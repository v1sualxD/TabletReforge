#include "TabletWorker.h"

#include <algorithm>
#include <unordered_set>

namespace TabletHelper {

namespace {

// Read the host's scan this long after requesting it (one host worker cycle):
// reading in the same tick returns the PREVIOUS scan.
constexpr int kReadDelayMs = 60;
// Scan cadence floor while no inventory or stash grid is drawn: enough to notice
// a panel opening, without asking the host for a full scan every 150 ms in maps.
constexpr int kIdleIntervalMs = 300;
// Modifier reads per pass: a full Tablet stash fills in within about a second.
constexpr int kModReadsPerPass = 40;
// A failed modifier read is retried after this long.
constexpr int kModRetryMs = 2000;
// A tablet not seen on screen for this long is forgotten (its entity address may
// be reused by another item later).
constexpr int kForgetAfterMs = 15000;
constexpr size_t kMaxCachedTablets = 4000;
// Without a DrawUI heartbeat for this long the worker stops scanning.
constexpr int64_t kHeartbeatTimeoutMs = 400;

int64_t SteadyMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

}  // namespace

void TabletWorker::Start() {
    if (m_thread.joinable()) return;
    {
        std::lock_guard<std::mutex> lk(m_wakeMutex);
        m_stop = false;
    }
    ResetState();
    m_thread = std::thread(&TabletWorker::Loop, this);
}

void TabletWorker::Stop() {
    {
        std::lock_guard<std::mutex> lk(m_wakeMutex);
        m_stop = true;
    }
    m_wakeCv.notify_all();
    if (m_thread.joinable()) m_thread.join();
    ResetState();
}

void TabletWorker::ResetState() {
    m_cache.clear();
    m_visible.clear();
    m_cacheArea = 0;
    m_evaluatedRev = ~0ull;
    m_requestPending = false;
    m_itemWindowOpen = false;
    m_publishedEmpty = true;
    std::lock_guard<std::mutex> lk(m_outMutex);
    m_frame.reset();
    m_stats = WorkerStats{};
}

void TabletWorker::SetFilter(std::shared_ptr<const FilterConfig> filter, int scanIntervalMs) {
    {
        std::lock_guard<std::mutex> lk(m_inMutex);
        m_filter = std::move(filter);
        ++m_filterRev;
    }
    m_scanIntervalMs.store(scanIntervalMs, std::memory_order_relaxed);
    m_wakeCv.notify_all();
}

void TabletWorker::Heartbeat(bool active, float displayW, float displayH, uint64_t areaCounter) {
    m_displayW.store(displayW, std::memory_order_relaxed);
    m_displayH.store(displayH, std::memory_order_relaxed);
    m_area.store(areaCounter, std::memory_order_relaxed);
    m_lastBeatMs.store(SteadyMs(), std::memory_order_relaxed);
    m_active.store(active, std::memory_order_release);
}

std::shared_ptr<const Frame> TabletWorker::Latest() const {
    std::lock_guard<std::mutex> lk(m_outMutex);
    return m_frame;
}

WorkerStats TabletWorker::Stats() const {
    std::lock_guard<std::mutex> lk(m_outMutex);
    return m_stats;
}

void TabletWorker::Loop() {
    for (;;) {
        bool active = false;
        {
            std::unique_lock<std::mutex> lk(m_wakeMutex);
            if (m_stop) return;
        }
        // This is the plugin's own thread: the host's SEH bridge only guards
        // host -> plugin calls. A structured fault or a C++ exception here would
        // end the whole program, so every tick is guarded twice and a faulting
        // tick only costs this pass.
        const int r = TickSEH(this);
        if (r == 0) {
            {
                std::lock_guard<std::mutex> lk(m_outMutex);
                ++m_stats.faults;
            }
            m_cache.clear();
            m_visible.clear();
            m_requestPending = false;
            m_faultBackoffUntil = Clock::now() + std::chrono::seconds(1);
        }
        active = m_active.load(std::memory_order_acquire) &&
                 SteadyMs() - m_lastBeatMs.load(std::memory_order_relaxed) < kHeartbeatTimeoutMs;
        std::unique_lock<std::mutex> lk(m_wakeMutex);
        if (m_stop) return;
        m_wakeCv.wait_for(lk, std::chrono::milliseconds(active ? 10 : 50));
    }
}

// SEH layer: no locals that need unwinding (C2712), it only forwards.
int TabletWorker::TickSEH(TabletWorker* self) {
    __try {
        return self->TickGuarded();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

int TabletWorker::TickGuarded() {
    try {
        Tick();
        return 1;
    } catch (...) {
        return 0;
    }
}

void TabletWorker::PublishEmpty(uint64_t area) {
    auto frame = std::make_shared<Frame>();
    frame->areaCounter = area;
    std::lock_guard<std::mutex> lk(m_outMutex);
    m_frame = std::move(frame);
    m_stats.visibleTablets = 0;
    m_stats.highlighted = 0;
    m_stats.pendingReads = 0;
    m_publishedEmpty = true;
}

void TabletWorker::Tick() {
    const auto now = Clock::now();
    if (now < m_faultBackoffUntil) return;

    const bool active = m_active.load(std::memory_order_acquire) &&
                        SteadyMs() - m_lastBeatMs.load(std::memory_order_relaxed) < kHeartbeatTimeoutMs;
    const uint64_t area = m_area.load(std::memory_order_relaxed);
    if (!active) {
        // Not in game, game in the background, plugin overlay not drawn: stop
        // scanning and drop the last result so nothing stale shows on return.
        m_requestPending = false;
        if (!m_publishedEmpty) PublishEmpty(area);
        return;
    }

    if (area != m_cacheArea) {
        // A new area re-creates every item entity: cached addresses would point
        // at other items.
        m_cache.clear();
        m_visible.clear();
        m_cacheArea = area;
        m_requestPending = false;
        PublishEmpty(area);
    }

    std::shared_ptr<const FilterConfig> filter;
    uint64_t rev = 0;
    {
        std::lock_guard<std::mutex> lk(m_inMutex);
        filter = m_filter;
        rev = m_filterRev;
    }
    if (!filter || !m_ctx) return;

    int interval = std::clamp(m_scanIntervalMs.load(std::memory_order_relaxed), 50, 1000);
    if (!m_itemWindowOpen) interval = (std::max)(interval, kIdleIntervalMs);
    const int readDelay = (std::min)(kReadDelayMs, interval / 2);
    const auto sinceRequest = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastRequest).count();

    if (!m_requestPending && sinceRequest >= interval) {
        m_ctx->Inventory.Scan(-1);  // only flags the host worker; returns at once
        m_lastRequest = now;
        m_requestPending = true;
        return;
    }
    if (m_requestPending && sinceRequest >= readDelay) {
        m_requestPending = false;
        ReadPass(now, *filter, m_displayW.load(std::memory_order_relaxed), m_displayH.load(std::memory_order_relaxed));
        EvaluateAndPublish(*filter, area);
        m_evaluatedRev = rev;
        return;
    }
    if (rev != m_evaluatedRev) {
        // Rules changed: re-evaluate the last read at once (no new scan needed).
        EvaluateAndPublish(*filter, area);
        m_evaluatedRev = rev;
    }
}

void TabletWorker::ReadPass(Clock::time_point now, const FilterConfig& filter, float displayW, float displayH) {
    const auto t0 = Clock::now();
    const std::vector<PluginSDK::Inventory> inventories = m_ctx->Inventory.GetAll();

    std::vector<Visible> visible;
    std::unordered_set<uintptr_t> seen;
    bool windowOpen = false;
    int budget = kModReadsPerPass;
    uint64_t reads = 0, failures = 0;
    int pending = 0;

    for (const auto& inv : inventories) {
        const bool itemRects = HasItemRects(inv);
        const bool grid = GridDrawn(inv, displayW, displayH);
        if (!itemRects && !grid) continue;  // not drawn now (closed panel, other tab, remembered entry)
        windowOpen = true;
        for (const auto& item : inv.Items) {
            if (item.Address == 0 || !LooksLikeTablet(item.Path, item.BaseTypeName)) continue;
            const auto rect = ResolveItemRect(inv, item, itemRects, grid, displayW, displayH);
            if (!rect) continue;
            if (!seen.insert(item.Address).second) continue;

            CacheEntry& entry = m_cache[item.Address];
            if (!entry.parsed || entry.path != item.Path || entry.baseType != item.BaseTypeName) {
                entry = CacheEntry{};
                entry.path = item.Path;
                entry.baseType = item.BaseTypeName;
                entry.parsed = std::make_shared<const ParsedTablet>(ParseTypeOnly(item.Path, item.BaseTypeName));
            }
            entry.lastSeen = now;
            if (filter.readMods && !entry.parsed->modsRead) {
                if (budget > 0 && now >= entry.nextReadAttempt) {
                    --budget;
                    ++reads;
                    const PluginSDK::ItemMods mods = m_ctx->Inventory.ReadItemMods(item.Address);
                    if (mods.Valid) {
                        entry.parsed =
                            std::make_shared<const ParsedTablet>(ParseWithMods(item.Path, item.BaseTypeName, mods));
                    } else {
                        ++failures;
                        entry.nextReadAttempt = now + std::chrono::milliseconds(kModRetryMs);
                    }
                }
                if (!entry.parsed->modsRead) ++pending;
            }
            visible.push_back(Visible{*rect, entry.parsed});
        }
    }

    // Forget tablets that have been off screen for a while.
    for (auto it = m_cache.begin(); it != m_cache.end();) {
        if (now - it->second.lastSeen > std::chrono::milliseconds(kForgetAfterMs)) it = m_cache.erase(it);
        else ++it;
    }
    if (m_cache.size() > kMaxCachedTablets) {
        for (auto it = m_cache.begin(); it != m_cache.end();) {
            if (it->second.lastSeen != now) it = m_cache.erase(it);
            else ++it;
        }
    }

    m_visible = std::move(visible);
    m_itemWindowOpen = windowOpen;

    const double ms = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
    std::lock_guard<std::mutex> lk(m_outMutex);
    ++m_stats.passes;
    m_stats.modReads += reads;
    m_stats.modReadFailures += failures;
    m_stats.lastPassMs = ms;
    m_stats.peakPassMs = (std::max)(m_stats.peakPassMs, ms);
    m_stats.visibleTablets = static_cast<int>(m_visible.size());
    m_stats.pendingReads = pending;
    m_stats.cachedTablets = static_cast<int>(m_cache.size());
    m_stats.itemWindowOpen = windowOpen;
}

void TabletWorker::EvaluateAndPublish(const FilterConfig& filter, uint64_t area) {
    auto frame = std::make_shared<Frame>();
    frame->areaCounter = area;
    frame->items.reserve(m_visible.size());
    for (const auto& v : m_visible) {
        if (!v.parsed) continue;
        Evaluation e = EvaluateTablet(filter, *v.parsed);
        if (!e.matched) continue;
        frame->items.push_back(TabletDraw{v.rect, std::move(e)});
    }
    const int highlighted = static_cast<int>(frame->items.size());
    std::lock_guard<std::mutex> lk(m_outMutex);
    m_frame = std::move(frame);
    m_stats.highlighted = highlighted;
    m_publishedEmpty = false;
}

std::string TabletWorker::BuildModDump(const PluginSDK::Context* ctx, int maxItems) {
    std::string s = "[Tablet Helper] tablet modifier dump\n";
    if (!ctx) return s + "no host context\n";
    const auto inventories = ctx->Inventory.GetAll();
    std::unordered_set<uintptr_t> seen;
    int dumped = 0;
    for (const auto& inv : inventories) {
        for (const auto& item : inv.Items) {
            if (dumped >= maxItems) return s + "\n...(item cap reached)\n";
            if (item.Address == 0 || !LooksLikeTablet(item.Path, item.BaseTypeName)) continue;
            if (!seen.insert(item.Address).second) continue;
            const auto mods = ctx->Inventory.ReadItemMods(item.Address);
            AppendModDump(s, ++dumped, item, mods, ctx->Inventory);
        }
    }
    if (dumped == 0) s += "no tablets listed: open a stash tab or inventory with tablets first.\n";
    return s;
}

}  // namespace TabletHelper

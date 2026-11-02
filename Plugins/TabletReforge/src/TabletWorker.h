#pragma once

// The scan worker: everything that reads the game runs here, off the render
// thread. It asks the host for an inventory scan, reads the result one host
// cycle later, finds the tablets drawn on screen, reads each new tablet's
// modifiers once (a few per pass), evaluates the filter rules and publishes an
// immutable Frame. DrawUI only draws the latest Frame.
//
// Inputs from the render thread: a heartbeat every DrawUI (in game, foreground,
// display size, area counter) and a copy of the filter rules on every change.
// Without a recent heartbeat the worker idles and stops scanning.

#include "ItemRects.h"
#include "TabletMatch.h"
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace TabletHelper {

struct TabletDraw {
    ScreenRect rect;
    Evaluation eval;
};

// One published result. Never modified after publication.
struct Frame {
    uint64_t areaCounter = 0;
    std::vector<TabletDraw> items;  // highlighted tablets only
};

struct WorkerStats {
    int visibleTablets = 0;      // tablets drawn on screen at the last read
    int highlighted = 0;         // of those, matched by the current rules
    int pendingReads = 0;        // visible tablets whose modifiers are not read yet
    int cachedTablets = 0;
    bool itemWindowOpen = false; // an inventory or stash grid was drawn at the last read
    uint64_t passes = 0;
    uint64_t modReads = 0;
    uint64_t modReadFailures = 0;
    uint64_t faults = 0;
    double lastPassMs = 0.0;
    double peakPassMs = 0.0;
};

class TabletWorker {
public:
    explicit TabletWorker(const PluginSDK::Context* ctx) : m_ctx(ctx) {}
    ~TabletWorker() { Stop(); }
    TabletWorker(const TabletWorker&) = delete;
    TabletWorker& operator=(const TabletWorker&) = delete;

    void Start();
    void Stop();

    // Render thread, on every change of the rules (and once after loading).
    void SetFilter(std::shared_ptr<const FilterConfig> filter, int scanIntervalMs);
    // Render thread, every DrawUI.
    void Heartbeat(bool active, float displayW, float displayH, uint64_t areaCounter);

    std::shared_ptr<const Frame> Latest() const;
    WorkerStats Stats() const;

    // Mod dump for the Diagnostics card: every tablet the host currently lists
    // (drawn or remembered), up to maxItems. Runs on the calling thread.
    static std::string BuildModDump(const PluginSDK::Context* ctx, int maxItems);

private:
    using Clock = std::chrono::steady_clock;

    struct CacheEntry {
        std::string path;
        std::string baseType;
        std::shared_ptr<const ParsedTablet> parsed;
        Clock::time_point lastSeen{};
        Clock::time_point nextReadAttempt{};
    };
    struct Visible {
        ScreenRect rect;
        std::shared_ptr<const ParsedTablet> parsed;
    };

    void Loop();
    static int TickSEH(TabletWorker* self);
    int TickGuarded();
    void Tick();
    void ReadPass(Clock::time_point now, const FilterConfig& filter, float displayW, float displayH);
    void EvaluateAndPublish(const FilterConfig& filter, uint64_t area);
    void PublishEmpty(uint64_t area);
    void ResetState();

    const PluginSDK::Context* m_ctx = nullptr;
    std::thread m_thread;
    std::mutex m_wakeMutex;
    std::condition_variable m_wakeCv;
    bool m_stop = false;

    // Inputs.
    mutable std::mutex m_inMutex;
    std::shared_ptr<const FilterConfig> m_filter;
    uint64_t m_filterRev = 0;
    std::atomic<int> m_scanIntervalMs{150};
    std::atomic<bool> m_active{false};
    std::atomic<int64_t> m_lastBeatMs{0};
    std::atomic<float> m_displayW{0.f};
    std::atomic<float> m_displayH{0.f};
    std::atomic<uint64_t> m_area{0};

    // Output.
    mutable std::mutex m_outMutex;
    std::shared_ptr<const Frame> m_frame;
    WorkerStats m_stats;

    // Worker-thread state.
    std::unordered_map<uintptr_t, CacheEntry> m_cache;
    std::vector<Visible> m_visible;
    uint64_t m_cacheArea = 0;
    uint64_t m_evaluatedRev = ~0ull;
    bool m_requestPending = false;
    bool m_itemWindowOpen = false;
    bool m_publishedEmpty = true;
    Clock::time_point m_lastRequest{};
    Clock::time_point m_faultBackoffUntil{};
};

}  // namespace TabletHelper

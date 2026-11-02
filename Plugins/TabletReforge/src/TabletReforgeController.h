#pragma once
#include "ItemRects.h"
#include "TabletEligibility.h"
#include "ReforgeUiDiscovery.h"
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <map>
#include <optional>


namespace TabletHelper {
struct TabletLabel { ScreenRect rect; std::string type; };
class TabletReforgeController {
    using Clock = std::chrono::steady_clock;
    enum class Phase { Idle, InitialScan, Transfer, WaitTransfer, Forge, WaitForge, PlaceResult, WaitResult };
    Phase phase_ = Phase::Idle;
    std::vector<ScreenRect> outlines_;
    std::vector<TabletLabel> labels_;
    size_t displayedCount_ = 0;
    uint64_t identityCacheArea_ = ~0ull, identityCacheStamp_ = 0;
    std::map<uintptr_t, ReforgeItem> identityCache_;
    bool pending_ = false, all_ = false;
    uint64_t selectionArea_ = ~0ull, area_ = 0, stamp_ = 0;
    HWND selectionWindow_ = nullptr, window_ = nullptr;
    LONG width_ = 0, height_ = 0;
    std::optional<BenchTargets> targets_;
    std::map<uintptr_t, ReforgeItem> expected_;
    std::vector<ReforgeBatch> batches_;
    size_t batch_ = 0, item_ = 0;
    Clock::time_point requestExpires_{}, actionAt_{}, deadline_{}, nextScan_{};
    std::string status_ = "Open an empty Reforging Bench, then press Start reforging.";
    static constexpr int kActionDelayMs = 700;
    int resultPickupDelayMs_ = 2000;
    enum class InputPhase { Idle, MoveAndModifier, ButtonDown, ButtonUp, ModifierUp };
    InputPhase inputPhase_ = InputPhase::Idle;
    bool ownedCtrl_ = false, ownedButton_ = false;
    Clock::time_point inputDue_{};
    static constexpr int kInputPulseMs = 40;


    static ReforgeItem Identity(const PluginSDK::Context* c, const PluginSDK::InventoryItem& i) {
        ReforgeItem result{i.Address, i.Path, i.BaseTypeName, i.Rarity, i.ItemLevel, i.SlotX, i.SlotY,
            i.IsIdentified, i.IsCorrupted};
        // Some POEFixer hosts leave rarity/identification unset in the inventory snapshot.
        // The item-mod summary is the authoritative fallback when it is available.
        if (c && i.Address) {
            const auto mods = c->Inventory.ReadItemMods(i.Address);
            if (mods.Valid) {
                result.rarity = mods.Rarity;
                result.level = mods.ItemLevel;
                result.identified = mods.IsIdentified;
                result.corrupted = mods.IsCorrupted;
            }
        }
        return result;
    }
    const std::map<uintptr_t, ReforgeItem>& Bag(const PluginSDK::Context* c,
        const PluginSDK::Inventory& inv, uint64_t area, uint64_t stamp) {
        if (identityCacheArea_ != area || identityCacheStamp_ != stamp) {
            identityCache_.clear();
            for (const auto& item : inv.Items) {
                if (item.Address) identityCache_.emplace(item.Address, Identity(c, item));
            }
            identityCacheArea_ = area;
            identityCacheStamp_ = stamp;
        }
        return identityCache_;
    }
    static void WriteScanDiagnostic(const PluginSDK::Context* c, const PluginSDK::Inventory& inv,
        const std::map<uintptr_t, ReforgeItem>& bag, const std::filesystem::path& directory) {
        try {
            const auto config = directory / "config";
            std::filesystem::create_directories(config);
            std::ofstream out(config / "tablet-scan.txt", std::ios::trunc);
            out << "TabletReforge 0.3.6 inventory scan\n"
                << "slot_x,slot_y,path,base_type,raw_rarity,raw_identified,mods_valid,mods_rarity,mods_identified,mods_corrupted,category,effective_rarity,effective_identified,effective_corrupted,eligible\n";
            for (const auto& item : inv.Items) {
                if (!LooksLikeTablet(item.Path, item.BaseTypeName)) continue;
                const auto found = bag.find(item.Address);
                const auto identity = found == bag.end() ? Identity(c, item) : found->second;
                const auto mods = c ? c->Inventory.ReadItemMods(item.Address) : PluginSDK::ItemMods{};
                out << item.SlotX << ',' << item.SlotY << ',' << item.Path << ',' << item.BaseTypeName << ','
                    << item.Rarity << ',' << item.IsIdentified << ',' << mods.Valid << ',' << mods.Rarity << ','
                    << mods.IsIdentified << ',' << mods.IsCorrupted << ',' << TabletCategory(identity) << ','
                    << identity.rarity << ',' << identity.identified << ',' << identity.corrupted << ','
                    << EligibleTablet(identity) << '\n';
            }
        } catch (...) {}
    }
    static bool Foreground(const PluginSDK::Context* c) {
        return c->Game.IsInGame() && !c->Game.IsMenuVisible() && c->Game.GetGameWindow() &&
            ::GetForegroundWindow() == c->Game.GetGameWindow();
    }
    static bool Contains(const ScreenRect& r, POINT p) {
        return p.x >= r.x && p.y >= r.y && p.x < r.x + r.w && p.y < r.y + r.h;
    }
    static std::optional<POINT> FirstFreeInventoryCell(const PluginSDK::Inventory& inv) {
        if (!inv.Grid.Valid || inv.Grid.CellSize <= 1.f || inv.TotalBoxesX <= 0 || inv.TotalBoxesY <= 0)
            return std::nullopt;
        const int columns = inv.TotalBoxesX;
        const int rows = inv.TotalBoxesY;
        std::vector<bool> occupied(static_cast<size_t>(columns * rows), false);
        for (const auto& item : inv.Items) {
            if (item.SlotX < 0 || item.SlotY < 0 || item.Width <= 0 || item.Height <= 0 ||
                item.SlotX + item.Width > columns || item.SlotY + item.Height > rows) continue;
            for (int y = item.SlotY; y < item.SlotY + item.Height; ++y)
                for (int x = item.SlotX; x < item.SlotX + item.Width; ++x)
                    occupied[static_cast<size_t>(y * columns + x)] = true;
        }
        for (int y = 0; y < rows; ++y) {
            for (int x = 0; x < columns; ++x) {
                if (occupied[static_cast<size_t>(y * columns + x)]) continue;
                const float px = inv.Grid.GridScreenX + (static_cast<float>(x) + 0.5f) * inv.Grid.CellSize;
                const float py = inv.Grid.GridScreenY + (static_cast<float>(y) + 0.5f) * inv.Grid.CellSize;
                return POINT{static_cast<LONG>(px), static_cast<LONG>(py)};
            }
        }
        return std::nullopt;
    }
    void ReadUi(const PluginSDK::Context* c, uintptr_t address, int parent,
                int depth, int& budget, std::vector<BenchNode>& nodes) {
        if (!address || depth > 24 || --budget < 0 || !c->Ui.IsVisible(address)) return;
        BenchNode node;
        node.address = address; node.parent = parent;
        node.name = c->Ui.GetControlName(address);
        node.text = c->Ui.GetStringId(address);
        if (!c->Ui.ComputeScreenRect(address, node.x, node.y, node.w, node.h) ||
            !std::isfinite(node.x) || !std::isfinite(node.y) || !std::isfinite(node.w) || !std::isfinite(node.h))
            node.w = node.h = 0;
        const int index = static_cast<int>(nodes.size());
        nodes.push_back(std::move(node));
        for (uintptr_t child : c->Ui.GetChildren(address)) {
            if (budget <= 0) break;
            ReadUi(c, child, index, depth + 1, budget, nodes);
        }
    }
    bool AttachBench(const PluginSDK::Context* c, const std::filesystem::path& directory) {
        window_ = c->Game.GetGameWindow();
        RECT client{};
        if (!::GetClientRect(window_, &client)) return false;
        width_ = client.right; height_ = client.bottom;
        std::vector<BenchNode> nodes;
        int budget = 4000;
        ReadUi(c, c->Ui.GetGameUiRoot(), -1, 0, budget, nodes);
        // A truncated scan cannot establish that the targets are unambiguous.
        targets_ = budget > 0 ? DiscoverBench(nodes, static_cast<float>(width_), static_cast<float>(height_)) : std::nullopt;
        if (targets_) return true;
        // Record public SDK UI observations for compatibility debugging, without extra menu controls.
        try {
            std::filesystem::create_directories(directory / "config");
            std::ofstream out(directory / "config" / "reforge-ui.txt", std::ios::trunc);
            out << "TabletReforge 0.3.6 UI discovery: " << nodes.size() << " nodes; budget=" << budget << '\n';
            for (size_t index = 0; index < nodes.size(); ++index) {
                const auto& n = nodes[index];
                const auto textId = NormalizeIdentifier(n.text);
                const auto text = (textId == "reforge" || textId == "reforgingbench") ? n.text : std::string();
                out << index << " parent=" << n.parent << ' ' << n.address << " [" << n.name << "] [" << text
                    << "] " << n.x << ',' << n.y << ' ' << n.w << 'x' << n.h << '\n';
            }
        } catch (...) {}
        return false;
    }
    bool LiveNode(const PluginSDK::Context* c, const BenchNode& expected, uintptr_t ancestor) const {
        const auto element = c->Ui.Read(expected.address);
        if (!element.Valid || !c->Ui.IsVisible(expected.address)) return false;
        if (!expected.name.empty() && c->Ui.GetControlName(expected.address) != expected.name) return false;
        if (!expected.text.empty() && c->Ui.GetStringId(expected.address) != expected.text) return false;
        float x = 0, y = 0, w = 0, h = 0;
        if (!c->Ui.ComputeScreenRect(expected.address, x, y, w, h) ||
            !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(w) || !std::isfinite(h) ||
            std::abs(x - expected.x) > 2 || std::abs(y - expected.y) > 2 ||
            std::abs(w - expected.w) > 2 || std::abs(h - expected.h) > 2) return false;
        uintptr_t current = expected.address;
        for (int depth = 0; current && depth < 32; ++depth) {
            if (!c->Ui.IsVisible(current)) return false;
            if (current == ancestor) return true;
            const auto node = c->Ui.Read(current);
            if (!node.Valid) return false;
            current = node.ParentAddress;
        }
        return false;
    }
    bool Ready(const PluginSDK::Context* c) const {
        if (!targets_ || !Foreground(c) || c->Game.GetGameWindow() != window_ ||
            c->Game.GetAreaChangeCounter() != area_ || c->Game.IsHostAutomationActive()) return false;
        RECT client{};
        // Reforging replaces child controls as tablets enter or leave the bench.
        // Keep the panel as the stable anchor; validating button/output nodes during every
        // 40 ms click pulse can falsely abort the Ctrl-click that is changing those nodes.
        return ::GetClientRect(window_, &client) && client.right == width_ && client.bottom == height_ &&
            LiveNode(c, targets_->panel, c->Ui.GetGameUiRoot());
    }
    bool Click(const PluginSDK::Context* c, POINT point, bool ctrl) {
        if (!Ready(c) || point.x < 0 || point.y < 0 || point.x >= width_ || point.y >= height_) return false;
        for (int key : {VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN, VK_RWIN, VK_LBUTTON, VK_RBUTTON})
            if (::GetAsyncKeyState(key) & 0x8000) return false;
        POINT screen = point;
        if (!::ClientToScreen(window_, &screen)) return false;
        const int sx = ::GetSystemMetrics(SM_XVIRTUALSCREEN), sy = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int sw = ::GetSystemMetrics(SM_CXVIRTUALSCREEN), sh = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (sw < 2 || sh < 2 || screen.x < sx || screen.y < sy || screen.x >= sx + sw || screen.y >= sy + sh) return false;
        INPUT input[2]{};
        UINT count = 0;
        input[count].type = INPUT_MOUSE;
        input[count].mi.dx = static_cast<LONG>((static_cast<double>(screen.x - sx) * 65535.0) / (sw - 1));
        input[count].mi.dy = static_cast<LONG>((static_cast<double>(screen.y - sy) * 65535.0) / (sh - 1));
        input[count++].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
        if (ctrl) { input[count].type = INPUT_KEYBOARD; input[count++].ki.wVk = VK_LCONTROL; }
        ownedCtrl_ = ctrl;
        if (::SendInput(count, input, sizeof(INPUT)) != count) { ReleaseInput(); return false; }
        inputPhase_ = InputPhase::MoveAndModifier;
        inputDue_ = Clock::now() + std::chrono::milliseconds(kInputPulseMs);
        return true;
    }
    void ReleaseInput() {
        INPUT release[2]{}; UINT count = 0;
        if (ownedButton_) { release[count].type = INPUT_MOUSE; release[count++].mi.dwFlags = MOUSEEVENTF_LEFTUP; }
        if (ownedCtrl_) {
            release[count].type = INPUT_KEYBOARD; release[count].ki.wVk = VK_LCONTROL;
            release[count++].ki.dwFlags = KEYEVENTF_KEYUP;
        }
        if (count) ::SendInput(count, release, sizeof(INPUT));
        ownedCtrl_ = ownedButton_ = false; inputPhase_ = InputPhase::Idle;
    }
    bool PumpInput(const PluginSDK::Context* c) {
        if (inputPhase_ == InputPhase::Idle) return false;
        if (!Ready(c)) { Stop("Stopped: game focus or bench panel changed during a click."); return true; }
        const auto now = Clock::now();
        if (now < inputDue_) return true;
        INPUT event{};
        if (inputPhase_ == InputPhase::MoveAndModifier) {
            event.type = INPUT_MOUSE; event.mi.dwFlags = MOUSEEVENTF_LEFTDOWN; ownedButton_ = true;
            inputPhase_ = InputPhase::ButtonDown;
        } else if (inputPhase_ == InputPhase::ButtonDown) {
            event.type = INPUT_MOUSE; event.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            inputPhase_ = InputPhase::ButtonUp;
        } else if (inputPhase_ == InputPhase::ButtonUp && ownedCtrl_) {
            event.type = INPUT_KEYBOARD; event.ki.wVk = VK_LCONTROL; event.ki.dwFlags = KEYEVENTF_KEYUP;
            inputPhase_ = InputPhase::ModifierUp;
        } else {
            ReleaseInput(); actionAt_ = now; deadline_ = now + std::chrono::seconds(6);
            c->Inventory.Scan(1); return true;
        }
        if (::SendInput(1, &event, sizeof(INPUT)) != 1) { Stop("Stopped: a mouse or keyboard event was blocked."); return true; }
        if (event.type == INPUT_MOUSE && event.mi.dwFlags == MOUSEEVENTF_LEFTUP) ownedButton_ = false;
        if (event.type == INPUT_KEYBOARD) ownedCtrl_ = false;
        inputDue_ = now + std::chrono::milliseconds(kInputPulseMs);
        return true;
    }
    void Wait(const PluginSDK::Context* c, Phase next, uint64_t stamp) {
        phase_ = next; stamp_ = stamp; actionAt_ = Clock::now();
        deadline_ = actionAt_ + std::chrono::seconds(6);
        c->Inventory.Scan(1);
    }
    void Preview(const PluginSDK::Context* c, bool all) {
        const uint64_t area = c->Game.GetAreaChangeCounter();
        if (area != selectionArea_ || selectionWindow_ != c->Game.GetGameWindow()) {
            selectionArea_ = area; selectionWindow_ = c->Game.GetGameWindow();
        }
        outlines_.clear(); labels_.clear(); displayedCount_ = 0;
        const auto snapshot = c->Inventory.ReadSnapshot(1);
        if (!snapshot.Ready || snapshot.AreaChangeCounter != area) return;
        const auto& bag = Bag(c, snapshot.Value, snapshot.AreaChangeCounter, snapshot.ScanStamp);
        for (const auto& [address, identity] : bag) { (void)address; if (EligibleTablet(identity)) ++displayedCount_; }
        if (!Foreground(c)) return;
        RECT client{};
        if (!::GetClientRect(c->Game.GetGameWindow(), &client)) return;
        const auto& inv = snapshot.Value;
        if (!GridDrawn(inv, static_cast<float>(client.right), static_cast<float>(client.bottom))) return;
        for (const auto& item : inv.Items) {
            const auto found = bag.find(item.Address);
            const auto identity = found == bag.end() ? Identity(c, item) : found->second;
            if (!LooksLikeTablet(item.Path, item.BaseTypeName)) continue;
            const auto rect = ResolveItemRect(inv, item, HasItemRects(inv), true,
                static_cast<float>(client.right), static_cast<float>(client.bottom));
            if (!rect) continue;
            const std::string type = ClassifyFromText(item.Path, item.BaseTypeName);
            labels_.push_back({*rect, type});
            if (all && EligibleTablet(identity)) outlines_.push_back(*rect);
        }
    }
public:
    ~TabletReforgeController() { ReleaseInput(); }
    bool Running() const { return phase_ != Phase::Idle; }
    bool Busy() const { return pending_ || Running(); }
    size_t SelectedCount() const { return displayedCount_; }
    const std::vector<ScreenRect>& Outlines() const { return outlines_; }
    const std::vector<TabletLabel>& Labels() const { return labels_; }
    const std::string& Status() const { return status_; }
    void Stop(const char* reason) { ReleaseInput(); phase_ = Phase::Idle; pending_ = false; targets_.reset(); status_ = reason; }
    void Reset() { Stop("Open an empty Reforging Bench, then press Start reforging."); outlines_.clear(); labels_.clear(); displayedCount_ = 0; }
    void RequestStart(const PluginSDK::Context* c, bool all, int resultPickupDelayMs) {
        if (Busy()) return;
        if (!all) { status_ = "Enable All tablets in inventory before starting."; return; }
        all_ = all; resultPickupDelayMs_ = std::clamp(resultPickupDelayMs, 200, 2000); pending_ = true;
        area_ = c->Game.GetAreaChangeCounter(); window_ = c->Game.GetGameWindow();
        requestExpires_ = Clock::now() + std::chrono::seconds(30);
        status_ = "Close the POEFixer menu to start. Keep the empty Reforging Bench open. Esc cancels.";
    }
    void Tick(const PluginSDK::Context* c, bool all, const std::filesystem::path& directory) {
        const auto now = Clock::now();
        if (Busy() && (::GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { Stop("Stopped. Recover any tablets left in the bench before restarting."); return; }
        if (Busy() && (!c->Game.IsInGame() || c->Game.GetAreaChangeCounter() != area_ || c->Game.GetGameWindow() != window_)) {
            Stop("Stopped: game window or area changed."); return;
        }
        if (pending_ && now > requestExpires_) { Stop("Start cancelled: the menu was not closed within 30 seconds."); return; }
        if (!c->Game.IsInGame()) { outlines_.clear(); labels_.clear(); displayedCount_ = 0; return; }
        if (PumpInput(c)) return;
        if (!Busy()) Preview(c, all); else { outlines_.clear(); labels_.clear(); }
        if (now >= nextScan_) { c->Inventory.Scan(1); nextScan_ = now + std::chrono::milliseconds(100); }
        if (!Foreground(c)) { if (Running()) Stop("Stopped: game lost focus or the POEFixer menu was opened."); return; }
        if (pending_) {
            pending_ = false;
            if (c->Game.IsHostAutomationActive()) { Stop("Another POEFixer automation is active."); return; }
            if (!AttachBench(c, directory) || !Ready(c)) {
                Stop("Reforging Bench controls not recognized. No clicks sent. See config/reforge-ui.txt."); return;
            }
            const auto snapshot = c->Inventory.ReadSnapshot(1);
            if (!snapshot.Supported) { Stop("This POEFixer version does not provide inventory snapshots."); return; }
            Wait(c, Phase::InitialScan, snapshot.ScanStamp);
            status_ = "Reading inventory tablets..."; return;
        }
        if (!Running()) return;
        if (!Ready(c)) { Stop("Stopped: bench closed, moved, resized, or another automation started."); return; }
        if (now > deadline_) { Stop("Stopped: no confirmation within 6 seconds. Check the bench before restarting."); return; }
        const int phaseDelayMs = phase_ == Phase::PlaceResult
            ? resultPickupDelayMs_ : kActionDelayMs;
        if (now - actionAt_ < std::chrono::milliseconds(phaseDelayMs)) return;
        const auto snapshot = c->Inventory.ReadSnapshot(1);
        if (!snapshot.Ready || snapshot.ScanStamp == stamp_) return;
        if (snapshot.AreaChangeCounter != area_) { Stop("Stopped: stale inventory from another area."); return; }
        const auto& inv = snapshot.Value;
        if (inv.TotalBoxesX != 12 || inv.TotalBoxesY != 5 || !GridDrawn(inv, static_cast<float>(width_), static_cast<float>(height_))) {
            Stop("Stopped: inventory grid is not visible."); return;
        }
        const auto& bag = Bag(c, inv, snapshot.AreaChangeCounter, snapshot.ScanStamp);
        if (phase_ == Phase::InitialScan) {
            std::vector<ReforgeItem> candidates;
            for (const auto& [address, identity] : bag) { (void)address; if (EligibleTablet(identity)) candidates.push_back(identity); }
            const size_t eligibleCount = candidates.size();
            batches_ = PlanReforge(std::move(candidates));
            expected_ = bag; batch_ = item_ = 0;
            if (batches_.empty()) {
                WriteScanDiagnostic(c, inv, bag, directory);
                const auto message = "No compatible trio found among " + std::to_string(eligibleCount) +
                    " eligible tablets. Requires three of the same category and rarity. See config/tablet-scan.txt.";
                Stop(message.c_str());
                return;
            }
            phase_ = Phase::Transfer;
        }
        if (phase_ == Phase::Transfer) {
            if (bag != expected_) { Stop("Stopped: inventory changed outside the sequence."); return; }
            const auto target = batches_[batch_][item_];
            const auto item = std::find_if(inv.Items.begin(), inv.Items.end(), [&](const auto& i) { return i.Address == target.address; });
            const auto current = bag.find(target.address);
            if (item == inv.Items.end() || current == bag.end() || current->second != target || !EligibleTablet(current->second)) { Stop("Stopped: selected tablet is missing or no longer eligible."); return; }
            const auto rect = ResolveItemRect(inv, *item, HasItemRects(inv), true, static_cast<float>(width_), static_cast<float>(height_));
            if (!rect || !Click(c, {static_cast<LONG>(rect->x + rect->w / 2), static_cast<LONG>(rect->y + rect->h / 2)}, true)) {
                Stop("Stopped: input was not sent. Release modifier keys and mouse buttons."); return;
            }
            status_ = "Reforging batch " + std::to_string(batch_ + 1) + " / " + std::to_string(batches_.size());
            Wait(c, Phase::WaitTransfer, snapshot.ScanStamp); return;
        }
        if (phase_ == Phase::WaitTransfer) {
            auto desired = expected_; desired.erase(batches_[batch_][item_].address);
            if (bag == expected_) return;
            if (bag != desired) { Stop("Stopped: unexpected item transfer. Check the bench."); return; }
            expected_ = std::move(desired);
            phase_ = ++item_ == 3 ? Phase::Forge : Phase::Transfer;
            actionAt_ = now; deadline_ = now + std::chrono::seconds(6); return;
        }
        if (phase_ == Phase::Forge) {
            const auto& button = targets_->button;
            if (bag != expected_ || !Click(c, {static_cast<LONG>(button.CenterX()), static_cast<LONG>(button.CenterY())}, false)) {
                Stop("Stopped before reforging: inventory or input changed."); return;
            }
            Wait(c, Phase::WaitForge, snapshot.ScanStamp); return;
        }
        if (phase_ == Phase::WaitForge) {
            const auto& output = targets_->output;
            if (bag != expected_ || !Click(c, {static_cast<LONG>(output.CenterX()), static_cast<LONG>(output.CenterY())}, false)) {
                Stop("Stopped before collecting the result."); return;
            }
            status_ = "Picking up reforge result...";
            Wait(c, Phase::PlaceResult, snapshot.ScanStamp); return;
        }
        if (phase_ == Phase::PlaceResult) {
            // Some bench versions do not quick-transfer the output on Ctrl+click.
            // A plain click picks it up; if that did not already add it to the bag,
            // place the carried item into a known empty inventory cell.
            if (bag != expected_) {
                phase_ = Phase::WaitResult;
                actionAt_ = now;
                stamp_ = snapshot.ScanStamp;
                deadline_ = now + std::chrono::seconds(6);
                return;
            }
            const auto emptyCell = FirstFreeInventoryCell(inv);
            if (!emptyCell || !Click(c, *emptyCell, false)) {
                Stop("Stopped: could not place the result in an empty inventory slot."); return;
            }
            Wait(c, Phase::WaitResult, snapshot.ScanStamp); return;
        }
        if (phase_ == Phase::WaitResult) {
            if (bag == expected_) return;
            auto additions = bag;
            for (const auto& [address, identity] : expected_) {
                const auto item = additions.find(address);
                if (item == additions.end() || item->second != identity) { Stop("Stopped: unexpected inventory contents."); return; }
                additions.erase(item);
            }
            if (additions.size() != 1 || TabletCategory(additions.begin()->second) != TabletCategory(batches_[batch_][0]) ||
                additions.begin()->second.rarity != batches_[batch_][0].rarity) {
                Stop("Stopped: unexpected result. Check the bench before restarting."); return;
            }
            expected_ = bag;
            if (++batch_ == batches_.size()) {
                const auto count = batches_.size(); Stop("Finished.");
                status_ = "Finished: " + std::to_string(count) + " batch(es). Results returned to inventory."; return;
            }
            item_ = 0; phase_ = Phase::Transfer; actionAt_ = now; deadline_ = now + std::chrono::seconds(6);
        }
    }
};
} // namespace TabletHelper

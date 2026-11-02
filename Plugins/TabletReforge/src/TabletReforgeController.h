#pragma once
#include "ItemRects.h"
#include "TabletSelection.h"
#include "ReforgeUiDiscovery.h"
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <map>
#include <optional>

namespace TabletHelper {
class TabletReforgeController {
    using Clock = std::chrono::steady_clock;
    enum class Phase { Idle, InitialScan, Transfer, WaitTransfer, Forge, WaitForge, WaitResult };
    Phase phase_ = Phase::Idle;
    TabletSelection selection_, requestedSelection_;
    std::vector<ScreenRect> outlines_;
    size_t displayedCount_ = 0;
    bool pending_ = false, all_ = false;
    uint64_t selectionArea_ = ~0ull, area_ = 0, stamp_ = 0;
    HWND selectionWindow_ = nullptr, window_ = nullptr;
    LONG width_ = 0, height_ = 0;
    std::optional<BenchTargets> targets_;
    std::map<uintptr_t, ReforgeItem> expected_;
    std::vector<ReforgeBatch> batches_;
    size_t batch_ = 0, item_ = 0;
    Clock::time_point requestExpires_{}, actionAt_{}, deadline_{}, nextScan_{};
    std::string status_ = "Hover a tablet and hold Ctrl + Alt to toggle selection.";
    static constexpr int kActionDelayMs = 750;

    static ReforgeItem Identity(const PluginSDK::InventoryItem& i) {
        return {i.Address, i.Path, i.Rarity, i.ItemLevel, i.SlotX, i.SlotY, i.IsIdentified, i.IsCorrupted};
    }
    static std::map<uintptr_t, ReforgeItem> Bag(const PluginSDK::Inventory& inv) {
        std::map<uintptr_t, ReforgeItem> bag;
        for (const auto& item : inv.Items) if (item.Address) bag.emplace(item.Address, Identity(item));
        return bag;
    }
    static bool Foreground(const PluginSDK::Context* c) {
        return c->Game.IsInGame() && !c->Game.IsMenuVisible() && c->Game.GetGameWindow() &&
            ::GetForegroundWindow() == c->Game.GetGameWindow();
    }
    static bool Contains(const ScreenRect& r, POINT p) {
        return p.x >= r.x && p.y >= r.y && p.x < r.x + r.w && p.y < r.y + r.h;
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
            out << "TabletReforge 0.2.0 UI discovery: " << nodes.size() << " nodes; budget=" << budget << '\n';
            for (const auto& n : nodes)
                if (!n.name.empty() || !n.text.empty()) out << n.parent << ' ' << n.address << " [" << n.name << "] [" << n.text
                    << "] " << n.x << ',' << n.y << ' ' << n.w << 'x' << n.h << '\n';
        } catch (...) {}
        return false;
    }
    bool LiveNode(const PluginSDK::Context* c, const BenchNode& expected, uintptr_t ancestor) const {
        const auto element = c->Ui.Read(expected.address);
        if (!element.Valid || !c->Ui.IsVisible(expected.address)) return false;
        if (!expected.name.empty() && c->Ui.GetControlName(expected.address) != expected.name) return false;
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
        return ::GetClientRect(window_, &client) && client.right == width_ && client.bottom == height_ &&
            LiveNode(c, targets_->panel, c->Ui.GetGameUiRoot()) &&
            LiveNode(c, targets_->button, targets_->panel.address) && LiveNode(c, targets_->output, targets_->panel.address);
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
        INPUT input[5]{};
        UINT count = 0;
        input[count].type = INPUT_MOUSE;
        input[count].mi.dx = static_cast<LONG>((static_cast<double>(screen.x - sx) * 65535.0) / (sw - 1));
        input[count].mi.dy = static_cast<LONG>((static_cast<double>(screen.y - sy) * 65535.0) / (sh - 1));
        input[count++].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
        if (ctrl) { input[count].type = INPUT_KEYBOARD; input[count++].ki.wVk = VK_LCONTROL; }
        input[count].type = INPUT_MOUSE; input[count++].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        input[count].type = INPUT_MOUSE; input[count++].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        if (ctrl) {
            input[count].type = INPUT_KEYBOARD; input[count].ki.wVk = VK_LCONTROL;
            input[count++].ki.dwFlags = KEYEVENTF_KEYUP;
        }
        if (::SendInput(count, input, sizeof(INPUT)) == count) return true;
        INPUT release[2]{};
        release[0].type = INPUT_MOUSE; release[0].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        release[1].type = INPUT_KEYBOARD; release[1].ki.wVk = VK_LCONTROL; release[1].ki.dwFlags = KEYEVENTF_KEYUP;
        ::SendInput(ctrl ? 2u : 1u, release, sizeof(INPUT));
        return false;
    }
    void Wait(const PluginSDK::Context* c, Phase next, uint64_t stamp) {
        phase_ = next; stamp_ = stamp; actionAt_ = Clock::now();
        deadline_ = actionAt_ + std::chrono::seconds(6);
        c->Inventory.Scan(1);
    }
    void Preview(const PluginSDK::Context* c, bool all) {
        const uint64_t area = c->Game.GetAreaChangeCounter();
        if (area != selectionArea_ || selectionWindow_ != c->Game.GetGameWindow()) {
            selection_.Clear(); selectionArea_ = area; selectionWindow_ = c->Game.GetGameWindow();
        }
        outlines_.clear(); displayedCount_ = 0;
        const bool chord = (::GetAsyncKeyState(VK_CONTROL) & 0x8000) && (::GetAsyncKeyState(VK_MENU) & 0x8000);
        if (!chord) selection_.ReleaseChord();
        const auto snapshot = c->Inventory.ReadSnapshot(1);
        if (!snapshot.Ready || snapshot.AreaChangeCounter != area) return;
        const auto bag = Bag(snapshot.Value);
        selection_.Refresh(bag);
        displayedCount_ = selection_.Candidates(bag, all).size();
        if (!Foreground(c)) return;
        RECT client{};
        if (!::GetClientRect(c->Game.GetGameWindow(), &client)) return;
        const auto& inv = snapshot.Value;
        if (!GridDrawn(inv, static_cast<float>(client.right), static_cast<float>(client.bottom))) return;
        POINT mouse{};
        const bool cursor = ::GetCursorPos(&mouse) && ::ScreenToClient(c->Game.GetGameWindow(), &mouse);
        for (const auto& item : inv.Items) {
            const auto identity = Identity(item);
            if (!EligibleTablet(identity)) continue;
            const auto rect = ResolveItemRect(inv, item, HasItemRects(inv), true,
                static_cast<float>(client.right), static_cast<float>(client.bottom));
            if (!rect) continue;
            if (!all && chord && cursor && Contains(*rect, mouse)) selection_.Touch(identity);
            if (all || selection_.Contains(identity)) outlines_.push_back(*rect);
        }
        displayedCount_ = selection_.Candidates(bag, all).size();
    }
public:
    bool Running() const { return phase_ != Phase::Idle; }
    bool Busy() const { return pending_ || Running(); }
    size_t SelectedCount() const { return displayedCount_; }
    const std::vector<ScreenRect>& Outlines() const { return outlines_; }
    const std::string& Status() const { return status_; }
    void Stop(const char* reason) { phase_ = Phase::Idle; pending_ = false; targets_.reset(); status_ = reason; }
    void Reset() { Stop("Hover a tablet and hold Ctrl + Alt to toggle selection."); selection_.Clear(); outlines_.clear(); displayedCount_ = 0; }
    void RequestStart(const PluginSDK::Context* c, bool all) {
        if (Busy()) return;
        all_ = all; requestedSelection_ = selection_; pending_ = true;
        area_ = c->Game.GetAreaChangeCounter(); window_ = c->Game.GetGameWindow();
        requestExpires_ = Clock::now() + std::chrono::seconds(30);
        status_ = "Close the POEFixer menu to start. Keep the empty Reforging Bench open. Esc cancels.";
    }
    void Tick(const PluginSDK::Context* c, bool all, const std::filesystem::path& directory) {
        const auto now = Clock::now();
        if (Busy() && (::GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { Stop("Stopped. Recover any tablets left in the bench before restarting."); return; }
        if (Busy() && (!c->Game.IsInGame() || c->Game.GetAreaChangeCounter() != area_ || c->Game.GetGameWindow() != window_)) {
            selection_.Clear(); Stop("Stopped: game window or area changed."); return;
        }
        if (pending_ && now > requestExpires_) { Stop("Start cancelled: the menu was not closed within 30 seconds."); return; }
        if (!c->Game.IsInGame()) { outlines_.clear(); displayedCount_ = 0; return; }
        if (!Busy()) Preview(c, all); else outlines_.clear();
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
            status_ = "Reading selected tablets..."; return;
        }
        if (!Running()) return;
        if (!Ready(c)) { Stop("Stopped: bench closed, moved, resized, or another automation started."); return; }
        if (now > deadline_) { Stop("Stopped: no confirmation within 6 seconds. Check the bench before restarting."); return; }
        if (now - actionAt_ < std::chrono::milliseconds(kActionDelayMs)) return;
        const auto snapshot = c->Inventory.ReadSnapshot(1);
        if (!snapshot.Ready || snapshot.ScanStamp == stamp_) return;
        if (snapshot.AreaChangeCounter != area_) { Stop("Stopped: stale inventory from another area."); return; }
        const auto& inv = snapshot.Value;
        if (inv.TotalBoxesX != 12 || inv.TotalBoxesY != 5 || !GridDrawn(inv, static_cast<float>(width_), static_cast<float>(height_))) {
            Stop("Stopped: inventory grid is not visible."); return;
        }
        const auto bag = Bag(inv);
        if (phase_ == Phase::InitialScan) {
            requestedSelection_.Refresh(bag);
            batches_ = PlanReforge(requestedSelection_.Candidates(bag, all_));
            expected_ = bag; batch_ = item_ = 0;
            if (batches_.empty()) { Stop("No compatible trio. Select three identified tablets of the same base and rarity."); return; }
            phase_ = Phase::Transfer;
        }
        if (phase_ == Phase::Transfer) {
            if (bag != expected_) { Stop("Stopped: inventory changed outside the sequence."); return; }
            const auto target = batches_[batch_][item_];
            const auto item = std::find_if(inv.Items.begin(), inv.Items.end(), [&](const auto& i) { return Identity(i) == target; });
            if (item == inv.Items.end() || !EligibleTablet(Identity(*item))) { Stop("Stopped: selected tablet is missing or no longer eligible."); return; }
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
            if (bag != expected_ || !Click(c, {static_cast<LONG>(output.CenterX()), static_cast<LONG>(output.CenterY())}, true)) {
                Stop("Stopped before collecting the result."); return;
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
            if (additions.size() != 1 || additions.begin()->second.path != batches_[batch_][0].path ||
                additions.begin()->second.rarity != batches_[batch_][0].rarity) {
                Stop("Stopped: unexpected result. Check the bench before restarting."); return;
            }
            expected_ = bag; selection_.Refresh(bag);
            if (++batch_ == batches_.size()) {
                const auto count = batches_.size(); Stop("Finished.");
                status_ = "Finished: " + std::to_string(count) + " batch(es). Results returned to inventory."; return;
            }
            item_ = 0; phase_ = Phase::Transfer; actionAt_ = now; deadline_ = now + std::chrono::seconds(6);
        }
    }
};
} // namespace TabletHelper

#pragma once
#include "ItemRects.h"
#include "TabletEligibility.h"
#include "TabletMatch.h"
#include "ReforgeUiDiscovery.h"
#include "TabletStashDiscovery.h"
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <map>
#include <optional>
#include <unordered_set>


namespace TabletHelper {
struct TabletLabel { ScreenRect rect; std::string type; int rarity = 0; };
struct TabletOutline { ScreenRect rect; int rarity = 0; };
class TabletReforgeController {
    using Clock = std::chrono::steady_clock;
    enum class Phase { Idle, InitialScan, Transfer, WaitTransfer, Forge, WaitForge, PlaceResult, WaitResult };
    enum class StashStage { Idle, PendingStart, NavigateTab, NavigateCategory, NavigatePage,
        ReadPage, Pull, WaitPull, AwaitBench, AwaitStash, Deposit, WaitDeposit, Finished };
    Phase phase_ = Phase::Idle;
    std::vector<TabletOutline> outlines_;
    std::vector<TabletLabel> labels_;
    size_t displayedCount_ = 0;
    uint64_t identityCacheArea_ = ~0ull, identityCacheStamp_ = 0;
    std::map<uintptr_t, ReforgeItem> identityCache_;
    std::map<uintptr_t, ParsedTablet> parsedCache_;
    bool pending_ = false, all_ = false;
    uint64_t selectionArea_ = ~0ull, area_ = 0, stamp_ = 0;
    HWND selectionWindow_ = nullptr, window_ = nullptr;
    LONG width_ = 0, height_ = 0;
    std::optional<BenchTargets> targets_;
    std::optional<TabletStashTargets> stashTargets_;
    StashStage stashStage_ = StashStage::Idle;
    bool stashCycle_ = false, stashInputMode_ = false, stashTabSelected_ = false, stashDepositInit_ = false;
    bool stashResumeForDeposit_ = false;
    uint8_t stashRarityMask_ = 0;
    std::vector<std::string> stashCategories_;
    std::vector<TabletHelperConfig::TypeConfig> stashTypeRules_;
    size_t stashCategory_ = 0, stashPage_ = 0;
    std::optional<ReforgeItem> stashPendingItem_;
    std::optional<uintptr_t> stashDepositItem_;
    std::map<uintptr_t, ReforgeItem> stashExpectedBag_;
    std::unordered_set<uintptr_t> stashSessionAddresses_;
    int stashUiCategory_ = -1, stashUiPage_ = -1;
    uint64_t stashBagStamp_ = 0;
    Clock::time_point stashActionAt_{};
    bool stashFromExhaustion_ = false;
    std::map<uintptr_t, ReforgeItem> expected_;
    std::vector<ReforgeBatch> batches_;
    size_t batch_ = 0, item_ = 0;
    Clock::time_point requestExpires_{}, actionAt_{}, deadline_{}, nextScan_{};
    std::string status_ = "Open an empty Reforging Bench, then press Start reforging.";
    static constexpr int kActionDelayMs = 700;
    int inputTransferDelayMs_ = 700;
    static constexpr int kResultPickupPauseMs = 700;
    enum class InputPhase { Idle, MoveAndModifier, ButtonDown, ButtonUp, ModifierUp };
    InputPhase inputPhase_ = InputPhase::Idle;
    bool ownedCtrl_ = false, ownedButton_ = false;
    Clock::time_point inputDue_{};
    int inputPulseMs_ = 40;


    static ReforgeItem Identity(const PluginSDK::Context* c, const PluginSDK::InventoryItem& i,
                                ParsedTablet* parsed = nullptr) {
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
                if (parsed) *parsed = ParseWithMods(i.Path, i.BaseTypeName, mods);
            } else if (parsed) {
                *parsed = ParseTypeOnly(i.Path, i.BaseTypeName);
            }
        }
        return result;
    }
    const std::map<uintptr_t, ReforgeItem>& Bag(const PluginSDK::Context* c,
        const PluginSDK::Inventory& inv, uint64_t area, uint64_t stamp) {
        if (identityCacheArea_ != area || identityCacheStamp_ != stamp) {
            identityCache_.clear();
            parsedCache_.clear();
            for (const auto& item : inv.Items) {
                if (item.Address) {
                    ParsedTablet parsed;
                    identityCache_.emplace(item.Address, Identity(c, item, &parsed));
                    parsedCache_.emplace(item.Address, std::move(parsed));
                }
            }
            identityCacheArea_ = area;
            identityCacheStamp_ = stamp;
        }
        return identityCache_;
    }
    bool ReforgeCandidate(const ReforgeItem& item, uint8_t rarityMask,
                          const std::vector<TabletHelperConfig::TypeConfig>& typeRules) const {
        if (stashCycle_ && !stashSessionAddresses_.contains(item.address)) return false;
        if (!EligibleTablet(item, rarityMask)) return false;
        const std::string category = TabletCategory(item);
        const auto rule = std::find_if(typeRules.begin(), typeRules.end(), [&](const auto& t) {
            return t.key == category;
        });
        if (rule == typeRules.end() || !rule->enabled) return false;
        const auto parsed = parsedCache_.find(item.address);
        if (parsed == parsedCache_.end() || !parsed->second.modsRead) return false;
                // Uses is an inclusion threshold: only tablets with enough uses are
        // reforged. Selected mods are the inverse keep filter: matching tablets
        // are protected from reforging.
        if (rule->minUsesLeft > 0 && parsed->second.usesLeft < rule->minUsesLeft) return false;
        if (rule->selectedBonusIds.empty()) return true;
        if (parsed->second.matchKeys.empty()) return false; // fail closed if mod identity is unavailable
        auto bonusRule = *rule;
        bonusRule.minUsesLeft = 0;
        return !TabletHelper::ConfigAccepts(bonusRule, parsed->second, true, category, nullptr);
    }
    static void WriteScanDiagnostic(const PluginSDK::Context* c, const PluginSDK::Inventory& inv,
        const std::map<uintptr_t, ReforgeItem>& bag, const std::filesystem::path& directory) {
        try {
            const auto config = directory / "config";
            std::filesystem::create_directories(config);
            std::ofstream out(config / "tablet-scan.txt", std::ios::trunc);
            out << "TabletReforge 0.5.3 inventory scan\n"
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
            out << "TabletReforge 0.3.9 UI discovery: " << nodes.size() << " nodes; budget=" << budget << '\n';
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
    bool AttachTabletStash(const PluginSDK::Context* c, const std::filesystem::path& directory) {
        window_ = c->Game.GetGameWindow();
        RECT client{};
        if (!window_ || !::GetClientRect(window_, &client)) return false;
        width_ = client.right; height_ = client.bottom;
        const auto tabs = c->Game.EnumerateStashTabs(PluginSDK::StashDomain::Own);
        const bool tabletTabActive = tabs.IsOpen() && std::any_of(tabs.Tabs.begin(), tabs.Tabs.end(),
            [](const auto& tab) { return tab.IsActive == 1 && NormalizeIdentifier(tab.Name) == "tabletfragments"; });
        if (!tabletTabActive) return false;
        std::vector<BenchNode> nodes;
        int budget = 8000;
        ReadUi(c, c->Ui.GetGameUiRoot(), -1, 0, budget, nodes);
        stashTargets_ = budget > 0 ? DiscoverTabletStash(nodes) : std::nullopt;
        if (stashTargets_) return true;
        try {
            std::filesystem::create_directories(directory / "config");
            std::ofstream out(directory / "tablet-stash-discovery.txt", std::ios::trunc);
            out << "TabletReforge stash discovery failed; nodes=" << nodes.size()
                << "; remaining_budget=" << budget << '\n';
            for (size_t i = 0; i < nodes.size(); ++i) {
                const auto& n = nodes[i];
                out << i << " parent=" << n.parent << " [" << n.name << "] [" << n.text << "] "
                    << n.x << ',' << n.y << ' ' << n.w << 'x' << n.h << '\n';
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
        if (stashInputMode_) {
            if (!stashTargets_ || !Foreground(c) || c->Game.GetGameWindow() != window_ ||
                c->Game.GetAreaChangeCounter() != area_ || c->Game.IsHostAutomationActive()) return false;
            const auto tabs = c->Game.EnumerateStashTabs(PluginSDK::StashDomain::Own);
            if (!tabs.IsOpen() || std::none_of(tabs.Tabs.begin(), tabs.Tabs.end(), [](const auto& tab) {
                return tab.IsActive == 1 && NormalizeIdentifier(tab.Name) == "tabletfragments";
            })) return false;
            RECT client{};
            return ::GetClientRect(window_, &client) && client.right == width_ && client.bottom == height_ &&
                LiveNode(c, stashTargets_->tabletTab, c->Ui.GetGameUiRoot());
        }
        if (!targets_ || !Foreground(c) || c->Game.GetGameWindow() != window_ ||
            c->Game.GetAreaChangeCounter() != area_ || c->Game.IsHostAutomationActive()) return false;
        RECT client{};
        // Reforging replaces child controls as tablets enter or leave the bench.
        // Keep the panel as the stable anchor; validating button/output nodes during every
        // 40 ms click pulse can falsely abort the Ctrl-click that is changing those nodes.
        return ::GetClientRect(window_, &client) && client.right == width_ && client.bottom == height_ &&
            LiveNode(c, targets_->panel, c->Ui.GetGameUiRoot());
    }
    bool MoveCursor(const PluginSDK::Context* c, POINT point) {
        if (!Ready(c) || point.x < 0 || point.y < 0 || point.x >= width_ || point.y >= height_) return false;
        for (int key : {VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN, VK_RWIN, VK_LBUTTON, VK_RBUTTON})
            if (::GetAsyncKeyState(key) & 0x8000) return false;
        POINT screen = point;
        if (!::ClientToScreen(window_, &screen)) return false;
        const int sx = ::GetSystemMetrics(SM_XVIRTUALSCREEN), sy = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int sw = ::GetSystemMetrics(SM_CXVIRTUALSCREEN), sh = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (sw < 2 || sh < 2 || screen.x < sx || screen.y < sy || screen.x >= sx + sw || screen.y >= sy + sh) return false;
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dx = static_cast<LONG>((static_cast<double>(screen.x - sx) * 65535.0) / (sw - 1));
        input.mi.dy = static_cast<LONG>((static_cast<double>(screen.y - sy) * 65535.0) / (sh - 1));
        input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
        return ::SendInput(1, &input, sizeof(INPUT)) == 1;
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
        inputDue_ = Clock::now() + std::chrono::milliseconds(inputPulseMs_);
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
        if (!Ready(c)) { Stop("Stopped: game focus or automation panel changed during a click."); return true; }
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
        inputDue_ = now + std::chrono::milliseconds(inputPulseMs_);
        return true;
    }
    void Wait(const PluginSDK::Context* c, Phase next, uint64_t stamp) {
        phase_ = next; stamp_ = stamp; actionAt_ = Clock::now();
        deadline_ = actionAt_ + std::chrono::seconds(6);
        c->Inventory.Scan(1);
    }
    void PauseCycleForBench(const char* message) {
        ReleaseInput(); phase_ = Phase::Idle; pending_ = false; targets_.reset();
        stashInputMode_ = false; stashTargets_.reset(); stashStage_ = StashStage::AwaitBench;
        stashActionAt_ = Clock::now(); status_ = message;
    }
    void PauseCycleForStash(const char* message) {
        ReleaseInput(); phase_ = Phase::Idle; pending_ = false; targets_.reset();
        stashInputMode_ = false; stashTargets_.reset(); stashStage_ = StashStage::AwaitStash;
        stashActionAt_ = Clock::now(); status_ = message;
    }
    bool CycleHasBatch(const PluginSDK::Context* c, const PluginSDK::InventorySnapshot& snapshot) {
        if (!snapshot.Ready) return false;
        const auto& bag = Bag(c, snapshot.Value, snapshot.AreaChangeCounter, snapshot.ScanStamp);
        std::vector<ReforgeItem> candidates;
        for (const auto& [address, item] : bag) {
            (void)address;
            if (ReforgeCandidate(item, stashRarityMask_, stashTypeRules_)) candidates.push_back(item);
        }
        return !PlanReforge(std::move(candidates)).empty();
    }
    void AdvanceStashPage(const PluginSDK::Context* c) {
        if (++stashPage_ >= 6) { stashPage_ = 0; ++stashCategory_; }
        stashUiCategory_ = stashUiPage_ = -1;
        if (stashCategory_ < stashCategories_.size()) {
            stashStage_ = StashStage::NavigateCategory;
            return;
        }
        stashFromExhaustion_ = true;
        const auto snapshot = c->Inventory.ReadSnapshot(1);
        if (CycleHasBatch(c, snapshot)) {
            PauseCycleForBench("Selected stash pages scanned. Open the Reforging Bench and click Continue with bench.");
        } else if (snapshot.Ready && std::none_of(snapshot.Value.Items.begin(), snapshot.Value.Items.end(), [&](const auto& item) {
            return stashSessionAddresses_.contains(item.Address);
        })) {
            stashStage_ = StashStage::Finished;
            stashCycle_ = false;
            status_ = "Finished: no compatible identified trio remained in the selected stash categories.";
        } else {
            PauseCycleForStash("No compatible trio remains. Return tablets to the selected stash pages, then click Continue at stash.");
        }
    }
    void TickStash(const PluginSDK::Context* c, const std::filesystem::path& directory) {
        if (stashStage_ == StashStage::AwaitBench || stashStage_ == StashStage::AwaitStash ||
            stashStage_ == StashStage::Finished || stashStage_ == StashStage::Idle) return;
        if (Clock::now() > requestExpires_) { Stop("Stash step cancelled: close the POEFixer menu within 30 seconds."); return; }
        if (!Foreground(c)) return;
        stashInputMode_ = true;
        if (PumpInput(c)) return;

        const auto now = Clock::now();
        if (stashStage_ == StashStage::PendingStart) {
            if (c->Game.IsHostAutomationActive() || !AttachTabletStash(c, directory)) {
                Stop("Tablet stash not recognized. Open TABLET+FRAGMENTS > Tablets; see config/tablet-stash-discovery.txt."); return;
            }
            const auto snapshot = c->Inventory.ReadSnapshot(1);
            if (!snapshot.Supported) { Stop("This POEFixer version does not provide inventory snapshots."); return; }
            if (!snapshot.Ready) { c->Inventory.Scan(1); return; }
            if (!FirstFreeInventoryCell(snapshot.Value)) {
                Stop("No free inventory slot. Clear space before starting the stash cycle."); return;
            }
            area_ = snapshot.AreaChangeCounter; stashExpectedBag_ = Bag(c, snapshot.Value,
                snapshot.AreaChangeCounter, snapshot.ScanStamp); stashSessionAddresses_.clear();
            stashTabSelected_ = false; stashUiCategory_ = stashUiPage_ = -1;
            stashStage_ = StashStage::NavigateTab; stashActionAt_ = now;
        }
        if (now - stashActionAt_ < std::chrono::milliseconds(250)) return;

        if (stashStage_ == StashStage::NavigateTab || stashStage_ == StashStage::NavigateCategory ||
            stashStage_ == StashStage::NavigatePage || stashStage_ == StashStage::ReadPage ||
            stashStage_ == StashStage::Pull || stashStage_ == StashStage::WaitPull ||
            stashStage_ == StashStage::Deposit || stashStage_ == StashStage::WaitDeposit) {
            // Discover the stash UI once per user handoff. Walking thousands of UI nodes
            // on every 250 ms state-machine tick can stall POEFixer's render thread.
            // The cached tablet-tab anchor is still validated before every input event.
            if (!stashTargets_ && !AttachTabletStash(c, directory)) {
                Stop("Tablet stash controls changed or could not be identified. No further clicks sent."); return;
            }
        }
        const auto snapshot = c->Inventory.ReadSnapshot(1);
        if (!snapshot.Supported || !snapshot.Ready || snapshot.AreaChangeCounter != area_) {
            c->Inventory.Scan(1); return;
        }
        const auto& bag = Bag(c, snapshot.Value, snapshot.AreaChangeCounter, snapshot.ScanStamp);

        if (stashStage_ == StashStage::NavigateTab) {
            const auto& tab = stashTargets_->tabletTab;
            if (!Click(c, {static_cast<LONG>(tab.CenterX()), static_cast<LONG>(tab.CenterY())}, false)) {
                Stop("Could not select the Tablet subtab. No items moved."); return;
            }
            stashTabSelected_ = true;
            stashStage_ = stashResumeForDeposit_ ? StashStage::Deposit : StashStage::NavigateCategory;
            stashResumeForDeposit_ = false; stashActionAt_ = now;
            return;
        }
        if (stashStage_ == StashStage::NavigateCategory) {
            if (stashCategory_ >= stashCategories_.size()) { AdvanceStashPage(c); return; }
            const auto canonical = std::find_if(StashCategoryOrder.begin(), StashCategoryOrder.end(), [&](const char* key) {
                return stashCategories_[stashCategory_] == key;
            });
            if (canonical == StashCategoryOrder.end()) { Stop("Unknown tablet category selection."); return; }
            const auto index = static_cast<size_t>(canonical - StashCategoryOrder.begin());
            if (stashUiCategory_ != static_cast<int>(index)) {
                const auto& button = stashTargets_->categories[index];
                if (!Click(c, {static_cast<LONG>(button.CenterX()), static_cast<LONG>(button.CenterY())}, false)) {
                    Stop("Could not select a Tablet stash category. No items moved."); return;
                }
                stashUiCategory_ = static_cast<int>(index); stashUiPage_ = -1; stashActionAt_ = now; return;
            }
            stashStage_ = StashStage::NavigatePage;
        }
        if (stashStage_ == StashStage::NavigatePage) {
            if (stashUiPage_ != static_cast<int>(stashPage_)) {
                const auto& button = stashTargets_->pages[stashPage_];
                if (!Click(c, {static_cast<LONG>(button.CenterX()), static_cast<LONG>(button.CenterY())}, false)) {
                    Stop("Could not select a Tablet stash page. No items moved."); return;
                }
                stashUiPage_ = static_cast<int>(stashPage_); stashActionAt_ = now; return;
            }
            stashStage_ = StashStage::ReadPage;
        }
        if (stashStage_ == StashStage::ReadPage || stashStage_ == StashStage::Pull) {
            if (FirstFreeInventoryCell(snapshot.Value) == std::nullopt) {
                if (CycleHasBatch(c, snapshot))
                    PauseCycleForBench("Inventory filled. Open the Reforging Bench and click Continue with bench.");
                else {
                    stashFromExhaustion_ = true;
                    PauseCycleForStash("Inventory is full but has no compatible trio. Return it to stash and adjust categories or rarity.");
                }
                return;
            }
            const auto match = MatchVisibleStashPage(*stashTargets_, c->Inventory.GetAll(), stashCategories_[stashCategory_]);
            if (match.state == TabletStashPageMatchState::Ambiguous || match.state == TabletStashPageMatchState::Missing) {
                Stop("Could not uniquely match the visible stash page to POEFixer inventory data. No item clicked."); return;
            }
            if (match.state == TabletStashPageMatchState::Empty) { AdvanceStashPage(c); return; }
            std::vector<const PluginSDK::InventoryItem*> candidates;
            for (const auto& item : match.inventory.Items) {
                                ParsedTablet parsed;
                const auto identity = Identity(c, item, &parsed);
                if (TabletCategory(identity) != stashCategories_[stashCategory_] ||
                    !EligibleTablet(identity, stashRarityMask_)) continue;

                const auto rule = std::find_if(stashTypeRules_.begin(), stashTypeRules_.end(), [&](const auto& type) {
                    return type.key == stashCategories_[stashCategory_] && type.enabled;
                });
                if (rule == stashTypeRules_.end()) continue;

                // Minimum uses is an inclusion threshold. Selected mods are keep
                // rules. If the host cannot read a configured rule, leave the
                // tablet untouched rather than risk moving the wrong one.
                if (rule->minUsesLeft > 0 && (!parsed.modsRead || parsed.usesLeft < rule->minUsesLeft)) continue;
                if (!rule->selectedBonusIds.empty()) {
                    if (!parsed.modsRead || parsed.matchKeys.empty()) continue;
                    auto bonusRule = *rule;
                    bonusRule.minUsesLeft = 0;
                    if (ConfigAccepts(bonusRule, parsed, true, stashCategories_[stashCategory_], nullptr)) continue;
                }
                candidates.push_back(&item);
            }
            std::sort(candidates.begin(), candidates.end(), [](const auto* a, const auto* b) {
                return a->SlotY == b->SlotY ? a->SlotX < b->SlotX : a->SlotY < b->SlotY;
            });
            if (candidates.empty()) { AdvanceStashPage(c); return; }
            const auto* item = candidates.front();
            const auto point = TabletStashSlotCenter(*stashTargets_, item->SlotX, item->SlotY);
            if (!point || !Click(c, *point, true)) { Stop("Could not safely Ctrl-click a stash tablet."); return; }
            stashPendingItem_ = Identity(c, *item); stashExpectedBag_ = bag;
            stashBagStamp_ = snapshot.ScanStamp;
            stashStage_ = StashStage::WaitPull; stashActionAt_ = now;
            status_ = "Withdrawing selected identified tablets from stash...";
            c->Inventory.Scan(1); return;
        }
        if (stashStage_ == StashStage::WaitPull) {
            if (snapshot.ScanStamp == stashBagStamp_) { c->Inventory.Scan(1); return; }
            if (bag.size() != stashExpectedBag_.size() + 1) {
                Stop("Stash Ctrl-click did not add exactly one inventory item. Check the inventory before restarting."); return;
            }
            auto additions = bag;
            for (const auto& [address, old] : stashExpectedBag_) {
                const auto it = additions.find(address);
                if (it == additions.end() || it->second != old) { Stop("Inventory changed unexpectedly during stash withdrawal."); return; }
                additions.erase(it);
            }
            const auto& received = additions.begin()->second;
            if (!stashPendingItem_ || TabletCategory(received) != TabletCategory(*stashPendingItem_) ||
                received.rarity != stashPendingItem_->rarity || !received.identified) {
                Stop("Withdrawn item did not match the identified tablet selected in stash."); return;
            }
            stashSessionAddresses_.insert(additions.begin()->first);
            stashExpectedBag_ = bag; stashPendingItem_.reset(); stashStage_ = StashStage::ReadPage;
            stashActionAt_ = now; return;
        }

        if (stashStage_ == StashStage::Deposit || stashStage_ == StashStage::WaitDeposit) {
            if (!stashDepositInit_) { stashExpectedBag_ = bag; stashDepositInit_ = true; }
            if (stashStage_ == StashStage::WaitDeposit) {
                if (snapshot.ScanStamp == stashBagStamp_) { c->Inventory.Scan(1); return; }
                if (bag.size() + 1 != stashExpectedBag_.size()) {
                    Stop("Stash deposit did not remove exactly one inventory item."); return;
                }
                auto remaining = stashExpectedBag_;
                const auto address = *stashDepositItem_;
                if (remaining.erase(address) != 1 || remaining.size() != bag.size()) {
                    Stop("Unexpected inventory change during stash deposit."); return;
                }
                for (const auto& [itemAddress, identity] : remaining) {
                    const auto it = bag.find(itemAddress);
                    if (it == bag.end() || it->second != identity) { Stop("Inventory changed during stash deposit."); return; }
                }
                stashSessionAddresses_.erase(address);
                stashExpectedBag_ = bag; stashDepositItem_.reset(); stashStage_ = StashStage::Deposit;
                stashActionAt_ = now;
            }
            const bool hasSessionItem = std::any_of(snapshot.Value.Items.begin(), snapshot.Value.Items.end(), [&](const auto& entry) {
                return stashSessionAddresses_.contains(entry.Address);
            });
            if (!hasSessionItem) {
                if (stashFromExhaustion_) {
                    stashStage_ = StashStage::Finished; stashCycle_ = false; stashInputMode_ = false;
                    status_ = "Finished: all selected stash pages were processed; results and leftovers returned.";
                } else {
                    stashCategory_ = stashPage_ = 0; stashUiCategory_ = stashUiPage_ = -1;
                    stashTabSelected_ = true; stashStage_ = StashStage::NavigateCategory;
                    stashActionAt_ = now; status_ = "Results returned. Scanning selected stash pages again...";
                }
                return;
            }
            const auto item = std::find_if(snapshot.Value.Items.begin(), snapshot.Value.Items.end(), [&](const auto& entry) {
                return stashSessionAddresses_.contains(entry.Address) && bag.find(entry.Address) != bag.end();
            });
            if (item == snapshot.Value.Items.end() || !LooksLikeTablet(item->Path, item->BaseTypeName)) {
                Stop("Only session tablets can be returned automatically; unexpected inventory item found."); return;
            }
            const std::string category = ClassifyFromText(item->Path, item->BaseTypeName);
            const auto categoryIt = std::find(StashCategoryOrder.begin(), StashCategoryOrder.end(), category);
            if (categoryIt == StashCategoryOrder.end()) { Stop("Could not identify the result tablet category for stash return."); return; }
            const size_t categoryIndex = static_cast<size_t>(categoryIt - StashCategoryOrder.begin());
            if (stashUiCategory_ != static_cast<int>(categoryIndex)) {
                const auto& button = stashTargets_->categories[categoryIndex];
                if (!Click(c, {static_cast<LONG>(button.CenterX()), static_cast<LONG>(button.CenterY())}, false)) {
                    Stop("Could not select the result's stash category."); return;
                }
                stashUiCategory_ = static_cast<int>(categoryIndex); stashUiPage_ = -1; stashActionAt_ = now; return;
            }
            if (stashUiPage_ < 0) {
                stashUiPage_ = 0;
                const auto& page = stashTargets_->pages[0];
                if (!Click(c, {static_cast<LONG>(page.CenterX()), static_cast<LONG>(page.CenterY())}, false)) {
                    Stop("Could not select a stash destination page."); return;
                }
                stashActionAt_ = now; return;
            }
            const auto pageMatch = MatchVisibleStashPage(*stashTargets_, c->Inventory.GetAll(), category);
            // Empty pages are valid deposit targets. Non-empty pages need a unique
            // page-to-inventory match before we can decide whether they have room.
            if (pageMatch.state == TabletStashPageMatchState::Ambiguous || pageMatch.state == TabletStashPageMatchState::Missing) {
                Stop("Could not verify Tablet stash space for returning results."); return;
            }
            if (pageMatch.state == TabletStashPageMatchState::Matched && pageMatch.inventory.Items.size() >= 96) {
                if (stashUiPage_ < 5) {
                    const size_t nextPage = static_cast<size_t>(stashUiPage_ + 1);
                    const auto& page = stashTargets_->pages[nextPage];
                    if (!Click(c, {static_cast<LONG>(page.CenterX()), static_cast<LONG>(page.CenterY())}, false)) {
                        Stop("Could not find free space on another stash page."); return;
                    }
                    stashUiPage_ = static_cast<int>(nextPage); stashActionAt_ = now; return;
                }
                Stop("All six pages of this Tablet category appear full. Free a stash slot and restart."); return;
            }
            const auto rect = ResolveItemRect(snapshot.Value, *item, HasItemRects(snapshot.Value), true,
                static_cast<float>(width_), static_cast<float>(height_));
            if (!rect || !Click(c, {static_cast<LONG>(rect->x + rect->w / 2),
                static_cast<LONG>(rect->y + rect->h / 2)}, true)) {
                Stop("Could not safely Ctrl-click a result back into the stash."); return;
            }
            stashExpectedBag_ = bag; stashDepositItem_ = item->Address; stashBagStamp_ = snapshot.ScanStamp;
            stashStage_ = StashStage::WaitDeposit; stashActionAt_ = now; c->Inventory.Scan(1); return;
        }
    }
    void Preview(const PluginSDK::Context* c, bool all, uint8_t rarityMask,
                 const std::vector<TabletHelperConfig::TypeConfig>& typeRules) {
        const uint64_t area = c->Game.GetAreaChangeCounter();
        if (area != selectionArea_ || selectionWindow_ != c->Game.GetGameWindow()) {
            selectionArea_ = area; selectionWindow_ = c->Game.GetGameWindow();
        }
        outlines_.clear(); labels_.clear(); displayedCount_ = 0;
        const auto snapshot = c->Inventory.ReadSnapshot(1);
        if (!snapshot.Ready || snapshot.AreaChangeCounter != area) return;
        const auto& bag = Bag(c, snapshot.Value, snapshot.AreaChangeCounter, snapshot.ScanStamp);
        for (const auto& [address, identity] : bag) { (void)address; if (all && ReforgeCandidate(identity, rarityMask, typeRules)) ++displayedCount_; }
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
            labels_.push_back({*rect, type, identity.rarity});
            if (identity.identified && identity.rarity >= 0 && identity.rarity <= 2)
                outlines_.push_back({*rect, identity.rarity});
        }
    }
    void CaptureStashDiagnosticsImpl(const PluginSDK::Context* c, const std::filesystem::path& directory) {
        if (!c) return;
        try {
            const auto config = directory / "config";
            std::filesystem::create_directories(config);
            std::ofstream out(config / "tablet-stash-ui.txt", std::ios::trunc);
            if (!out) { status_ = "Could not write config/tablet-stash-ui.txt."; return; }
            out << "TabletReforge stash UI diagnostics\n"
                << "Open the Tablet stash tab before capturing. Screen rectangles are client pixels.\n\n";

            const auto tabs = c->Game.EnumerateStashTabs(PluginSDK::StashDomain::Own);
            out << "Own stash tabs: supported=" << tabs.Supported << " status=" << tabs.Status << '\n';
            for (const auto& tab : tabs.Tabs)
                out << "tab index=" << tab.Index << " active=" << tab.IsActive << " onscreen=" << tab.OnScreen
                    << " rect=" << tab.X << ',' << tab.Y << ' ' << tab.W << 'x' << tab.H
                    << " name=" << tab.Name << '\n';
            out << "toggle onscreen=" << tabs.Toggle.OnScreen << " rect=" << tabs.Toggle.X << ','
                << tabs.Toggle.Y << ' ' << tabs.Toggle.W << 'x' << tabs.Toggle.H << "\n\n";

            out << "Inventories:\n";
            for (const auto& inv : c->Inventory.GetAll()) {
                size_t visible = 0;
                for (const auto& item : inv.Items) if (item.ScreenValid) ++visible;
                out << "id=" << inv.InventoryId << " name=" << c->Inventory.GetName(inv.InventoryId)
                    << " size=" << inv.TotalBoxesX << 'x' << inv.TotalBoxesY
                    << " grid=" << inv.Grid.Valid << ':' << inv.Grid.GridScreenX << ',' << inv.Grid.GridScreenY
                    << ',' << inv.Grid.CellSize << " items=" << inv.Items.size()
                    << " visible_items=" << visible << '\n';
                for (const auto& item : inv.Items) {
                    if (!LooksLikeTablet(item.Path, item.BaseTypeName)) continue;
                    const auto mods = c->Inventory.ReadItemMods(item.Address);
                    const auto identity = Identity(c, item);
                    out << "  tablet slot=" << item.SlotX << ',' << item.SlotY
                        << " screen_valid=" << item.ScreenValid << " rect="
                        << item.ScreenX << ',' << item.ScreenY << ' ' << item.ScreenW << 'x' << item.ScreenH
                        << " identified=" << identity.identified << " rarity=" << identity.rarity
                        << " mods_valid=" << mods.Valid
                        << " raw_identified=" << item.IsIdentified << " raw_rarity=" << item.Rarity
                        << " category=" << ClassifyFromText(item.Path, item.BaseTypeName)
                        << " path=" << item.Path << " base=" << item.BaseTypeName << '\n';
                }
            }

            out << "\nVisible UI tree (max 8000 nodes):\n";
            std::vector<BenchNode> nodes;
            int budget = 8000;
            ReadUi(c, c->Ui.GetGameUiRoot(), -1, 0, budget, nodes);
            for (size_t i = 0; i < nodes.size(); ++i) {
                const auto& n = nodes[i];
                out << i << " parent=" << n.parent << " addr=" << n.address
                    << " name=[" << n.name << "] string_id=[" << n.text << "] text=["
                    << c->Ui.GetText(n.address) << "] rect=" << n.x << ',' << n.y << ' '
                    << n.w << 'x' << n.h << '\n';
            }
            out << "remaining_node_budget=" << budget << '\n';
            status_ = "Stash UI diagnostics saved to config/tablet-stash-ui.txt.";
        } catch (...) {
            status_ = "Could not capture stash UI diagnostics.";
        }
    }
public:
    ~TabletReforgeController() { ReleaseInput(); }
    void CaptureStashDiagnostics(const PluginSDK::Context* c, const std::filesystem::path& directory) {
        CaptureStashDiagnosticsImpl(c, directory);
    }
    bool Running() const { return phase_ != Phase::Idle; }
    bool StashAwaitingBench() const { return stashCycle_ && stashStage_ == StashStage::AwaitBench; }
    bool StashAwaitingStash() const { return stashCycle_ && stashStage_ == StashStage::AwaitStash; }
    bool StashCycleActive() const { return stashCycle_ && stashStage_ != StashStage::Finished; }
    bool Busy() const {
        if (pending_ || Running()) return true;
        return stashCycle_ && stashStage_ != StashStage::Idle && stashStage_ != StashStage::AwaitBench &&
            stashStage_ != StashStage::AwaitStash && stashStage_ != StashStage::Finished;
    }
    size_t SelectedCount() const { return displayedCount_; }
    const std::vector<TabletOutline>& Outlines() const { return outlines_; }
    const std::vector<TabletLabel>& Labels() const { return labels_; }
    const std::string& Status() const { return status_; }
    void Stop(const char* reason) {
        ReleaseInput(); phase_ = Phase::Idle; pending_ = false; targets_.reset(); stashTargets_.reset();
        stashCycle_ = false; stashInputMode_ = false; stashStage_ = StashStage::Idle;
        stashPendingItem_.reset(); status_ = reason;
    }
    void Reset() { Stop("Open an empty Reforging Bench, then press Start reforging."); outlines_.clear(); labels_.clear(); displayedCount_ = 0; }
    void RequestStart(const PluginSDK::Context* c, bool all, int inputTransferDelayMs, int inputPulseMs) {
        if (Busy()) return;
        if (!all) { status_ = "Enable All tablets in inventory before starting."; return; }
        all_ = all; inputTransferDelayMs_ = std::clamp(inputTransferDelayMs, 10, 1900);
        inputPulseMs_ = std::clamp(inputPulseMs, 20, 40); pending_ = true;
        area_ = c->Game.GetAreaChangeCounter(); window_ = c->Game.GetGameWindow();
        requestExpires_ = Clock::now() + std::chrono::seconds(30);
        status_ = "Close the POEFixer menu to start. Keep the empty Reforging Bench open. Esc cancels.";
    }
    void RequestStashCycle(const PluginSDK::Context* c, const std::vector<TabletHelperConfig::TypeConfig>& typeRules,
                           uint8_t rarityMask, int transferDelayMs, int pulseMs) {
        if (Busy() || stashCycle_) return;
        stashCategories_.clear();
        for (const auto* key : StashCategoryOrder) {
            const auto rule = std::find_if(typeRules.begin(), typeRules.end(), [&](const auto& item) {
                return item.key == key && item.enabled;
            });
            if (rule != typeRules.end()) stashCategories_.emplace_back(key);
        }
        if (stashCategories_.empty() || rarityMask == 0) {
            status_ = "Select at least one tablet category and rarity before starting."; return;
        }
        stashCycle_ = true; stashStage_ = StashStage::PendingStart;
        stashTypeRules_ = typeRules;
        stashResumeForDeposit_ = false;
        stashRarityMask_ = rarityMask; stashCategory_ = stashPage_ = 0;
        stashTabSelected_ = false; stashPendingItem_.reset(); stashFromExhaustion_ = false;
        all_ = true; inputTransferDelayMs_ = std::clamp(transferDelayMs, 10, 1900);
        inputPulseMs_ = std::clamp(pulseMs, 20, 40);
        area_ = c->Game.GetAreaChangeCounter(); window_ = c->Game.GetGameWindow();
        requestExpires_ = Clock::now() + std::chrono::seconds(30);
        status_ = "Keep TABLET+FRAGMENTS open on Tablets. Close the POEFixer menu to scan selected categories.";
    }
    void RequestCycleBench(const PluginSDK::Context* c) {
        if (!StashAwaitingBench()) return;
        stashStage_ = StashStage::Idle; stashInputMode_ = false;
        stashFromExhaustion_ = false;
        RequestStart(c, true, inputTransferDelayMs_, inputPulseMs_);
        if (pending_) status_ = "Open the Reforging Bench and close the POEFixer menu. Esc cancels.";
    }
    void RequestCycleStash(const PluginSDK::Context* c) {
        if (!StashAwaitingStash()) return;
        stashInputMode_ = true; stashTabSelected_ = false;
        stashUiCategory_ = stashUiPage_ = -1; stashDepositInit_ = false;
        stashResumeForDeposit_ = true;
        if (!stashFromExhaustion_) stashCategory_ = stashPage_ = 0;
        stashStage_ = StashStage::NavigateTab;
        area_ = c->Game.GetAreaChangeCounter(); window_ = c->Game.GetGameWindow();
        stashActionAt_ = Clock::now();
        status_ = "Open TABLET+FRAGMENTS on Tablets, then close the POEFixer menu to return results and continue.";
        requestExpires_ = Clock::now() + std::chrono::seconds(30);
    }
    void Tick(const PluginSDK::Context* c, bool all, uint8_t rarityMask,
              const std::vector<TabletHelperConfig::TypeConfig>& typeRules,
              const std::filesystem::path& directory) {
        const auto now = Clock::now();
        if (Busy() && (::GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { Stop("Stopped. Recover any tablets left in the bench before restarting."); return; }
        if (Busy() && (!c->Game.IsInGame() || c->Game.GetAreaChangeCounter() != area_ || c->Game.GetGameWindow() != window_)) {
            Stop("Stopped: game window or area changed."); return;
        }
        if (pending_ && now > requestExpires_) { Stop("Start cancelled: the menu was not closed within 30 seconds."); return; }
        if (!c->Game.IsInGame()) { outlines_.clear(); labels_.clear(); displayedCount_ = 0; return; }
        if (stashCycle_ && stashStage_ != StashStage::Idle && stashStage_ != StashStage::AwaitBench &&
            stashStage_ != StashStage::AwaitStash && stashStage_ != StashStage::Finished) {
            TickStash(c, directory); return;
        }
        // The POEFixer settings window can take foreground focus while the game stays
        // attached. Do not read item mods or scan inventories from its UI/render loop.
        // Keep a pending start request alive so it can begin after the player returns
        // focus to the game.
        if (!Foreground(c)) {
            outlines_.clear(); labels_.clear(); displayedCount_ = 0;
            if (Running()) Stop("Stopped: game lost focus or the POEFixer menu was opened.");
            return;
        }
        if (PumpInput(c)) return;
        if (!Busy()) Preview(c, all, rarityMask, typeRules); else { outlines_.clear(); labels_.clear(); }
        if (now >= nextScan_) {
            c->Inventory.Scan(1);
            nextScan_ = now + std::chrono::milliseconds(Running() ? 25 : 100);
        }
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
        const int phaseDelayMs = phase_ == Phase::PlaceResult ? kResultPickupPauseMs
            : ((phase_ == Phase::Transfer || phase_ == Phase::WaitTransfer)
                ? inputTransferDelayMs_ : kActionDelayMs);
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
            for (const auto& [address, identity] : bag) { (void)address; if (all && ReforgeCandidate(identity, rarityMask, typeRules)) candidates.push_back(identity); }
            const size_t eligibleCount = candidates.size();
            batches_ = PlanReforge(std::move(candidates));
            expected_ = bag; batch_ = item_ = 0;
            if (batches_.empty()) {
                WriteScanDiagnostic(c, inv, bag, directory);
                const auto message = "No compatible trio found among " + std::to_string(eligibleCount) +
                    " eligible tablets. Requires three of the same category and rarity. See config/tablet-scan.txt.";
                if (stashCycle_) {
                    stashFromExhaustion_ = true;
                    PauseCycleForStash("No compatible trio in the inventory. Return the remaining tablets to stash to finish.");
                    return;
                }
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
            if (item == inv.Items.end() || current == bag.end() || current->second != target || !ReforgeCandidate(current->second, rarityMask, typeRules)) { Stop("Stopped: selected tablet is missing or no longer eligible."); return; }
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
            if (stashCycle_) {
                for (const auto& tablet : batches_[batch_]) stashSessionAddresses_.erase(tablet.address);
                stashSessionAddresses_.insert(additions.begin()->first);
            }
            expected_ = bag;
            if (++batch_ == batches_.size()) {
                if (stashCycle_) {
                    PauseCycleForStash("Reforge pass complete. Return all results to the Tablet stash, then click Continue at stash.");
                    return;
                }
                const auto count = batches_.size(); Stop("Finished.");
                status_ = "Finished: " + std::to_string(count) + " batch(es). Results returned to inventory."; return;
            }
            item_ = 0; phase_ = Phase::Transfer; actionAt_ = now; deadline_ = now + std::chrono::seconds(6);
            const auto nextAddress = batches_[batch_][item_].address;
            const auto nextItem = std::find_if(inv.Items.begin(), inv.Items.end(), [&](const auto& item) {
                return item.Address == nextAddress;
            });
            if (nextItem == inv.Items.end()) { Stop("Stopped: next planned tablet is missing."); return; }
            const auto nextRect = ResolveItemRect(inv, *nextItem, HasItemRects(inv), true,
                static_cast<float>(width_), static_cast<float>(height_));
            if (!nextRect || !MoveCursor(c, {static_cast<LONG>(nextRect->x + nextRect->w / 2),
                static_cast<LONG>(nextRect->y + nextRect->h / 2)})) {
                Stop("Stopped: could not move to the next planned tablet."); return;
            }
        }
    }
};
} // namespace TabletHelper

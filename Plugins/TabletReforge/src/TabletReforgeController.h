#pragma once
#include "ItemRects.h"
#include "TabletEligibility.h"
#include "TabletMatch.h"
#include "ReforgeUiDiscovery.h"
#include "TabletStashDiscovery.h"
#include <Windows.h>
#include <chrono>
#include <cstring>
#include <fstream>
#include <map>
#include <optional>
#include <unordered_set>


namespace TabletHelper {
struct TabletLabel { ScreenRect rect; std::string type; int rarity = 0; };
struct TabletOutline { ScreenRect rect; int rarity = 0; };
class TabletReforgeController {
    using Clock = std::chrono::steady_clock;
    enum class Phase { Idle, InitialScan, Transfer, WaitTransfer, Forge, WaitForge, WaitResult };
    enum class StashStage { Idle, PendingStart, NavigateTab, NavigateCategory, NavigatePage, AwaitPage,
        ReadPage, Pull, WaitPull, CheckTabletTooltip, WaitTabletClipboard, AwaitBench, AwaitStash, Deposit, WaitDeposit, Finished,
        CloseStashForBench, WaitStashClosed, FindBench, WaitBenchOpen, ForgePass,
        CloseBenchForStash, WaitBenchClosed, FindStash, WaitStashOpen };
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
    bool stashInventorySeeded_ = false;
    static constexpr int kInitialStashOpenDelayMs = 765;
    bool stashResumeForDeposit_ = false;
    uint8_t stashRarityMask_ = 0;
    std::vector<std::string> stashCategories_;
    std::vector<TabletHelperConfig::TypeConfig> stashTypeRules_;
    size_t stashCategory_ = 0, stashPage_ = 0;
    std::optional<ReforgeItem> stashPendingItem_;
    std::optional<uintptr_t> stashDepositItem_;
    std::map<uintptr_t, ReforgeItem> stashExpectedBag_;
    std::unordered_set<uintptr_t> stashSessionAddresses_;
    std::unordered_set<uintptr_t> stashSkippedAddresses_;
    int stashUiCategory_ = -1, stashUiPage_ = -1;
    int stashInventoryId_ = 0;
    uint64_t stashBagStamp_ = 0;
    std::optional<PluginSDK::Inventory> stashPageInventorySnapshot_;
    int stashPullRetries_ = 0;
    int stashEmptyConfirmCategory_ = -1, stashEmptyConfirmPage_ = -1;
    Clock::time_point stashEmptyConfirmAt_{};
    Clock::time_point stashActionAt_{}, stashPageRefreshAt_{};
    bool stashFromExhaustion_ = false;
    bool stashDepositIneligibleOnly_ = false;
    std::map<uintptr_t, ReforgeItem> expected_;
    std::vector<ReforgeBatch> batches_;
    size_t batch_ = 0, item_ = 0;
    Clock::time_point actionAt_{}, deadline_{}, nextScan_{};
    std::string status_ = "Open an empty Reforging Bench, then press Start reforging.";
    // Reduce wait intervals by about 1.3x while preserving the click pulse duration.
    static int FasterTransferDelayMs(int delayMs) {
        const int bounded = std::clamp(delayMs, 10, 1900);
        return std::clamp((bounded * 10 + 6) / 13, 10, 1900);
    }
    static constexpr int kActionDelayMs = 486;
    int inputTransferDelayMs_ = 486;
    // Let the reforger finish producing its result before the first pickup click.
    static constexpr int kForgeResultReadyPauseMs = 1615;
    enum class InputPhase { Idle, Hover, ModifierDown, ButtonDown, ButtonUp, ModifierUp };
    InputPhase inputPhase_ = InputPhase::Idle;
    bool ownedCtrl_ = false, ownedButton_ = false, queuedCtrl_ = false;
    Clock::time_point inputDue_{};
    int inputPulseMs_ = 40;
    int activeInputPulseMs_ = 40;
    static constexpr int kNormalClickPulseMs = 40;
    static constexpr int kHoverBeforeClickMs = 450;
    static constexpr int kStashTabletHoverMs = 238;
    enum class WorldClickPhase { Idle, Hover, ButtonDown };
    WorldClickPhase worldClickPhase_ = WorldClickPhase::Idle;
    POINT worldClickScreen_{};
    Clock::time_point worldClickDue_{};


    bool debugRecording_ = false;
    std::ofstream debugLog_;
    Clock::time_point debugStarted_{}, debugNextSample_{};
    std::string lastTooltipScanSummary_ = "not run";
    struct ClipboardFormatSnapshot { UINT format = 0; std::vector<unsigned char> bytes; };
    std::vector<ClipboardFormatSnapshot> clipboardBackup_;
    DWORD clipboardSequenceBeforeCopy_ = 0;
    Clock::time_point clipboardDeadline_{};
    bool clipboardSnapshotValid_ = false, clipboardVerified_ = false;

    bool BeginClipboardTabletRead(const PluginSDK::Context* c) {
        clipboardBackup_.clear();
        clipboardSnapshotValid_ = false;
        if (!Foreground(c) || !Ready(c)) return false;
        for (int key : {VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN, VK_RWIN, VK_LBUTTON, VK_RBUTTON})
            if (::GetAsyncKeyState(key) & 0x8000) return false;

        if (!::OpenClipboard(nullptr)) return false;
        bool captured = true;
        UINT format = 0;
        while ((format = ::EnumClipboardFormats(format)) != 0) {
            HANDLE handle = ::GetClipboardData(format);
            const SIZE_T size = handle ? ::GlobalSize(static_cast<HGLOBAL>(handle)) : 0;
            if (!size) { captured = false; break; }
            const auto* data = static_cast<const unsigned char*>(::GlobalLock(static_cast<HGLOBAL>(handle)));
            if (!data) { captured = false; break; }
            ClipboardFormatSnapshot entry;
            entry.format = format;
            entry.bytes.assign(data, data + size);
            ::GlobalUnlock(static_cast<HGLOBAL>(handle));
            clipboardBackup_.push_back(std::move(entry));
        }
        ::CloseClipboard();
        if (!captured) { clipboardBackup_.clear(); return false; }

        clipboardSnapshotValid_ = true;
        clipboardSequenceBeforeCopy_ = ::GetClipboardSequenceNumber();
        INPUT keys[4]{};
        keys[0].type = INPUT_KEYBOARD; keys[0].ki.wVk = VK_LCONTROL;
        keys[1].type = INPUT_KEYBOARD; keys[1].ki.wVk = 'C';
        keys[2].type = INPUT_KEYBOARD; keys[2].ki.wVk = 'C'; keys[2].ki.dwFlags = KEYEVENTF_KEYUP;
        keys[3].type = INPUT_KEYBOARD; keys[3].ki.wVk = VK_LCONTROL; keys[3].ki.dwFlags = KEYEVENTF_KEYUP;
        if (::SendInput(4, keys, sizeof(INPUT)) != 4) {
            INPUT release[2]{};
            release[0].type = INPUT_KEYBOARD; release[0].ki.wVk = 'C'; release[0].ki.dwFlags = KEYEVENTF_KEYUP;
            release[1].type = INPUT_KEYBOARD; release[1].ki.wVk = VK_LCONTROL; release[1].ki.dwFlags = KEYEVENTF_KEYUP;
            ::SendInput(2, release, sizeof(INPUT));
            RestoreClipboardSnapshot(::GetClipboardSequenceNumber());
            return false;
        }
        clipboardDeadline_ = Clock::now() + std::chrono::seconds(2);
        return true;
    }
    bool ReadCopiedTabletText(std::wstring& text, DWORD& sequence) {
        sequence = ::GetClipboardSequenceNumber();
        if (sequence == clipboardSequenceBeforeCopy_ || !::OpenClipboard(nullptr)) return false;
        HANDLE handle = ::GetClipboardData(CF_UNICODETEXT);
        if (handle) {
            const auto* data = static_cast<const wchar_t*>(::GlobalLock(static_cast<HGLOBAL>(handle)));
            if (data) { text.assign(data); ::GlobalUnlock(static_cast<HGLOBAL>(handle)); }
        } else if ((handle = ::GetClipboardData(CF_TEXT)) != nullptr) {
            const auto* data = static_cast<const char*>(::GlobalLock(static_cast<HGLOBAL>(handle)));
            if (data) {
                const int length = ::MultiByteToWideChar(CP_ACP, 0, data, -1, nullptr, 0);
                if (length > 1) {
                    std::wstring converted(static_cast<size_t>(length), L'\0');
                    ::MultiByteToWideChar(CP_ACP, 0, data, -1, converted.data(), length);
                    converted.resize(static_cast<size_t>(length - 1));
                    text = std::move(converted);
                }
                ::GlobalUnlock(static_cast<HGLOBAL>(handle));
            }
        }
        ::CloseClipboard();
        return !text.empty();
    }
    bool RestoreClipboardSnapshot(DWORD expectedSequence) {
        if (!clipboardSnapshotValid_) return true;
        std::vector<HGLOBAL> handles;
        handles.reserve(clipboardBackup_.size());
        for (const auto& entry : clipboardBackup_) {
            HGLOBAL memory = ::GlobalAlloc(GMEM_MOVEABLE, entry.bytes.size());
            if (!memory) {
                for (auto h : handles) ::GlobalFree(h);
                return false;
            }
            void* destination = ::GlobalLock(memory);
            if (!destination) {
                ::GlobalFree(memory);
                for (auto h : handles) ::GlobalFree(h);
                return false;
            }
            std::memcpy(destination, entry.bytes.data(), entry.bytes.size());
            ::GlobalUnlock(memory);
            handles.push_back(memory);
        }
        if (!::OpenClipboard(nullptr)) {
            for (auto h : handles) ::GlobalFree(h);
            return false;
        }
        if (::GetClipboardSequenceNumber() != expectedSequence) {
            ::CloseClipboard();
            for (auto h : handles) ::GlobalFree(h);
            clipboardBackup_.clear(); clipboardSnapshotValid_ = false;
            return true; // A newer clipboard change belongs to the user; leave it untouched.
        }
        bool restored = ::EmptyClipboard() != FALSE;
        for (size_t i = 0; restored && i < clipboardBackup_.size(); ++i) {
            if (!::SetClipboardData(clipboardBackup_[i].format, handles[i])) restored = false;
            else handles[i] = nullptr; // ownership transferred to the clipboard
        }
        ::CloseClipboard();
        for (auto h : handles) if (h) ::GlobalFree(h);
        clipboardBackup_.clear(); clipboardSnapshotValid_ = false;
        return restored;
    }
    static std::string ClipboardAscii(const std::wstring& text) {
        std::string result;
        result.reserve(text.size());
        for (wchar_t ch : text) result.push_back(ch >= 0x20 && ch <= 0x7e ? static_cast<char>(ch) : ' ');
        return result;
    }
    static bool ContainsSanctifiedText(const std::string& value) {
        return NormalizeIdentifier(value).find("sanctified") != std::string::npos;
    }
    static bool HasSanctifiedMarker(const PluginSDK::ItemMods& mods) {
        const auto contains = [](const std::vector<PluginSDK::Mod>& group) {
            return std::any_of(group.begin(), group.end(), [](const auto& mod) {
                return ContainsSanctifiedText(mod.Name) || ContainsSanctifiedText(mod.StatKey) ||
                    ContainsSanctifiedText(mod.AffixName) || ContainsSanctifiedText(mod.Id);
            });
        };
        return contains(mods.ImplicitMods) || contains(mods.ExplicitMods) ||
            contains(mods.EnchantMods) || contains(mods.HellscapeMods) || contains(mods.CrucibleMods);
    }
    static ReforgeItem Identity(const PluginSDK::Context* c, const PluginSDK::InventoryItem& i,
                                ParsedTablet* parsed = nullptr) {
        ReforgeItem result{i.Address, i.Path, i.BaseTypeName, i.Rarity, i.ItemLevel, i.SlotX, i.SlotY,
            i.IsIdentified, i.IsCorrupted};
        result.sanctified = ContainsSanctifiedText(i.Path) || ContainsSanctifiedText(i.BaseTypeName);
        // Some POEFixer hosts leave rarity/identification unset in the inventory snapshot.
        // The item-mod summary is the authoritative fallback when it is available.
        if (c && i.Address) {
            const auto mods = c->Inventory.ReadItemMods(i.Address);
            if (mods.Valid) {
                result.rarity = mods.Rarity;
                result.level = mods.ItemLevel;
                result.identified = mods.IsIdentified;
                result.corrupted = mods.IsCorrupted;
                result.sanctified = result.sanctified || HasSanctifiedMarker(mods);
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
    static bool SameTabletIdentity(const ReforgeItem& a, const ReforgeItem& b) {
        // Grid coordinates can change while the inventory/bench UI refreshes.
        // Address and item properties identify the tablet; x/y are only its current placement.
        return a.address == b.address && a.path == b.path && a.baseType == b.baseType &&
            a.rarity == b.rarity && a.level == b.level && a.identified == b.identified &&
            a.corrupted == b.corrupted && a.sanctified == b.sanctified;
    }
    static bool SameBagContents(const std::map<uintptr_t, ReforgeItem>& a,
                                const std::map<uintptr_t, ReforgeItem>& b) {
        if (a.size() != b.size()) return false;
        for (const auto& [address, item] : a) {
            const auto found = b.find(address);
            if (found == b.end() || !SameTabletIdentity(item, found->second)) return false;
        }
        return true;
    }
    bool ReforgeCandidate(const ReforgeItem& item, uint8_t rarityMask,
                          const std::vector<TabletHelperConfig::TypeConfig>& typeRules,
                          bool requireStashSession = true) const {
        if (requireStashSession && stashCycle_ && !stashSessionAddresses_.contains(item.address)) return false;
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
                << "slot_x,slot_y,path,base_type,raw_rarity,raw_identified,mods_valid,mods_rarity,mods_identified,mods_corrupted,sanctified_marker,category,effective_rarity,effective_identified,effective_corrupted,eligible\n";
            for (const auto& item : inv.Items) {
                if (!LooksLikeTablet(item.Path, item.BaseTypeName)) continue;
                const auto found = bag.find(item.Address);
                const auto identity = found == bag.end() ? Identity(c, item) : found->second;
                const auto mods = c ? c->Inventory.ReadItemMods(item.Address) : PluginSDK::ItemMods{};
                out << item.SlotX << ',' << item.SlotY << ',' << item.Path << ',' << item.BaseTypeName << ','
                    << item.Rarity << ',' << item.IsIdentified << ',' << mods.Valid << ',' << mods.Rarity << ','
                    << mods.IsIdentified << ',' << mods.IsCorrupted << ',' << identity.sanctified << ','
                    << TabletCategory(identity) << ','
                    << identity.rarity << ',' << identity.identified << ',' << identity.corrupted << ','
                    << EligibleTablet(identity) << '\n';
            }
        } catch (...) {}
    }
    static bool Foreground(const PluginSDK::Context* c) {
        return c->Game.IsInGame() && !c->Game.IsMenuVisible() && c->Game.GetGameWindow() &&
            ::GetForegroundWindow() == c->Game.GetGameWindow();
    }
    bool DismissHostWindowForCycle(const PluginSDK::Context* c) {
        const HWND gameWindow = c ? c->Game.GetGameWindow() : nullptr;
        HWND foreground = ::GetForegroundWindow();
        if (!gameWindow || !foreground) return false;
        if (foreground == gameWindow) return true;
        const HWND hostWindow = ::GetAncestor(foreground, GA_ROOT);
        DWORD hostPid = 0, gamePid = 0;
        if (!hostWindow || hostWindow == gameWindow) return false;
        ::GetWindowThreadProcessId(hostWindow, &hostPid);
        ::GetWindowThreadProcessId(gameWindow, &gamePid);
        if (hostPid != ::GetCurrentProcessId() || !gamePid) return false;

        // The one-shot Escape can be ignored by the POEFixer settings window.
        // Minimize only our own host window and explicitly hand focus back to PoE2;
        // this keeps the DLL loaded while allowing the game input sequence to run.
        ::AllowSetForegroundWindow(gamePid);
        ::SetForegroundWindow(gameWindow);
        ::ShowWindow(hostWindow, SW_MINIMIZE);
        ::SetForegroundWindow(gameWindow);
        return ::GetForegroundWindow() == gameWindow;
    }
    static bool Contains(const ScreenRect& r, POINT p) {
        return p.x >= r.x && p.y >= r.y && p.x < r.x + r.w && p.y < r.y + r.h;
    }
    static bool PlayerInventoryGridReady(const PluginSDK::Inventory& inv) {
        return inv.InventoryId == 1 && inv.Grid.Valid && inv.Grid.CellSize > 1.f &&
            inv.TotalBoxesX > 0 && inv.TotalBoxesY > 0;
    }
    static std::optional<POINT> FirstFreeInventoryCell(const PluginSDK::Inventory& inv) {
        if (!PlayerInventoryGridReady(inv)) return std::nullopt;
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
    void RecordDebugSample(const PluginSDK::Context* c) {
        if (!debugRecording_ || !debugLog_) return;
        const auto now = Clock::now();
        if (now < debugNextSample_) return;
        debugNextSample_ = now + std::chrono::milliseconds(450);
        try {
            POINT screen{};
            const bool cursorValid = ::GetCursorPos(&screen) != FALSE;
            POINT client = screen;
            const HWND gameWindow = c->Game.GetGameWindow();
            const bool clientValid = cursorValid && gameWindow && ::ScreenToClient(gameWindow, &client);
            const HWND foreground = ::GetForegroundWindow();
            DWORD foregroundPid = 0;
            if (foreground) ::GetWindowThreadProcessId(foreground, &foregroundPid);
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - debugStarted_).count();
            debugLog_ << "\n--- sample +" << elapsed << " ms ---\n"
                << "status=[" << status_ << "] phase=" << static_cast<int>(phase_)
                << " stash_stage=" << static_cast<int>(stashStage_)
                << " stash_cycle=" << stashCycle_ << " pending=" << pending_
                << " stash_category=" << (stashCategory_ < stashCategories_.size() ? stashCategories_[stashCategory_] : "done")
                << " stash_page=" << stashPage_
                << " foreground_hwnd=" << foreground << " foreground_pid=" << foregroundPid
                << " game_hwnd=" << gameWindow << " game_foreground=" << Foreground(c)
                << " cursor_screen=" << (cursorValid ? std::to_string(screen.x) + "," + std::to_string(screen.y) : "unavailable")
                << " cursor_client=" << (clientValid ? std::to_string(client.x) + "," + std::to_string(client.y) : "unavailable")
                << " tooltip_last=[" << lastTooltipScanSummary_ << "]\n";
            const auto player = c->Inventory.ReadSnapshot(1);
            debugLog_ << "player_inventory_snapshot supported=" << player.Supported
                << " ready=" << player.Ready << " id=" << player.Value.InventoryId
                << " grid=" << player.Value.Grid.Valid << ":" << player.Value.Grid.GridScreenX
                << "," << player.Value.Grid.GridScreenY << "," << player.Value.Grid.CellSize
                << " size=" << player.Value.TotalBoxesX << "x" << player.Value.TotalBoxesY
                << " items=" << player.Value.Items.size()
                << " has_free_cell=" << (player.Ready && FirstFreeInventoryCell(player.Value).has_value()) << "\n";
            if (stashTargets_) {
                debugLog_ << "tablet_stash_ui_grid=" << stashTargets_->grid.x << "," << stashTargets_->grid.y
                    << " size=" << stashTargets_->grid.w << "x" << stashTargets_->grid.h
                    << " cell=" << stashTargets_->cellSize
                    << " occupied_slots=" << stashTargets_->occupiedSlots.size() << "\n";
            }
            const auto tabs = c->Game.EnumerateStashTabs(PluginSDK::StashDomain::Own);
            debugLog_ << "stash_open=" << tabs.IsOpen() << " stash_status=" << tabs.Status << " active_tabs=";
            for (const auto& tab : tabs.Tabs)
                if (tab.IsActive == 1) debugLog_ << "[" << tab.Index << ":" << tab.Name << "]";
            debugLog_ << "\n";
            const auto inventories = c->Inventory.GetAll();
            for (const auto& inv : inventories) {
                size_t tabletCount = 0;
                for (const auto& item : inv.Items)
                    if (LooksLikeTablet(item.Path, item.BaseTypeName)) ++tabletCount;
                debugLog_ << "inventory id=" << inv.InventoryId << " name=[" << c->Inventory.GetName(inv.InventoryId)
                    << "] grid=" << inv.Grid.Valid << ":" << inv.Grid.GridScreenX << "," << inv.Grid.GridScreenY
                    << "," << inv.Grid.CellSize << " size=" << inv.TotalBoxesX << "x" << inv.TotalBoxesY
                    << " items=" << inv.Items.size() << " tablets=" << tabletCount << "\n";
                for (const auto& item : inv.Items) {
                    if (!LooksLikeTablet(item.Path, item.BaseTypeName)) continue;
                    debugLog_ << "  tablet slot=" << item.SlotX << "," << item.SlotY
                        << " screen_valid=" << item.ScreenValid << " rect=" << item.ScreenX << "," << item.ScreenY
                        << " " << item.ScreenW << "x" << item.ScreenH << " identified=" << item.IsIdentified
                        << " rarity=" << item.Rarity << " corrupted=" << item.IsCorrupted
                        << " category=[" << ClassifyFromText(item.Path, item.BaseTypeName) << "]"
                        << " base=[" << item.BaseTypeName << "] path=[" << item.Path << "]\n";
                }
            }
            if (stashTargets_ && stashCategory_ < stashCategories_.size()) {
                const auto match = MatchVisibleStashPage(*stashTargets_, inventories, stashCategories_[stashCategory_]);
                debugLog_ << "stash_page_match category=[" << stashCategories_[stashCategory_]
                    << "] ui_occupied=" << stashTargets_->occupiedSlots.size()
                    << " state=" << static_cast<int>(match.state)
                    << " inventory_id=" << match.inventory.InventoryId << "\n";
            }
            std::vector<BenchNode> nodes;
            int budget = 2000;
            ReadUi(c, c->Ui.GetGameUiRoot(), -1, 0, budget, nodes);
            size_t written = 0;
            for (const auto& node : nodes) {
                if (written >= 60) break;
                const auto text = c->Ui.GetText(node.address);
                const bool overlapsCursor = clientValid && client.x >= node.x && client.y >= node.y &&
                    client.x < node.x + node.w && client.y < node.y + node.h;
                const bool nearby = clientValid && node.w > 0 && node.h > 0 &&
                    std::abs(node.CenterX() - client.x) <= 420 && std::abs(node.CenterY() - client.y) <= 300;
                const auto normalized = NormalizeIdentifier(text + " " + node.text + " " + node.name);
                const bool relevant = overlapsCursor || nearby ||
                    normalized.find("tablet") != std::string::npos ||
                    normalized.find("sanctified") != std::string::npos ||
                    normalized.find("tooltip") != std::string::npos;
                if (!relevant || (text.empty() && node.text.empty() && node.name.empty())) continue;
                auto clean = [](std::string value) {
                    std::replace(value.begin(), value.end(), '\r', ' ');
                    std::replace(value.begin(), value.end(), '\n', ' ');
                    return value;
                };
                debugLog_ << "ui [" << clean(node.name) << "] id=[" << clean(node.text)
                    << "] text=[" << clean(text) << "] rect=" << node.x << "," << node.y
                    << " " << node.w << "x" << node.h << " cursor_overlap=" << overlapsCursor << "\n";
                ++written;
            }
            debugLog_ << "ui_nodes=" << nodes.size() << " remaining_budget=" << budget
                << " relevant_nodes_written=" << written << "\n";
            debugLog_.flush();
        } catch (...) {
            debugLog_ << "sample_error=exception\n";
            debugLog_.flush();
        }
    }
    bool AttachBench(const PluginSDK::Context* c, const std::filesystem::path& directory, bool writeDiagnostic = true) {
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
        if (!writeDiagnostic) return false;
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
    std::optional<POINT> FindBenchScreenPoint(const PluginSDK::Context* c) {
        if (!c || !Foreground(c)) return std::nullopt;
        RECT client{};
        const HWND gameWindow = c->Game.GetGameWindow();
        if (!gameWindow || !::GetClientRect(gameWindow, &client)) return std::nullopt;
        std::vector<POINT> matches;
        const auto snapshot = c->Game.GetSnapshot();
        for (const auto& entity : snapshot.Entities) {
            if (!entity.IsValid) continue;
            const auto path = NormalizeIdentifier(WideToUtf8(entity.Path));
            const auto targetPath = NormalizeIdentifier(entity.TgtPath);
            // POEFixer exposes the hideout prop as MiscellaneousObjects/Hideout/ReforgingBench.
            // PoE2DB's placeable-item metadata uses Items/Hideout/HideoutReforgingBench;
            // accept the live world path used by POEFixer as well.
            if (path.find("reforgingbench") == std::string::npos &&
                targetPath.find("reforgingbench") == std::string::npos) continue;
            if (!entity.Components.Targetable) continue;
            const auto targetable = c->Components.ReadTargetable(entity.Components.Targetable);
            if (!targetable.Valid || !targetable.IsTargetable || targetable.HiddenFromPlayer) continue;
            float x = 0, y = 0;
            if (!c->Render.WorldToScreen(entity.WorldX, entity.WorldY,
                entity.WorldZ + entity.ModelBoundsZ, x, y)) continue;
            if (!std::isfinite(x) || !std::isfinite(y) || x < 1 || y < 1 ||
                x >= client.right - 1 || y >= client.bottom - 1) continue;
            matches.push_back({static_cast<LONG>(std::lround(x)), static_cast<LONG>(std::lround(y))});
        }
        if (matches.size() == 1) return matches.front();
        if (!matches.empty()) return std::nullopt;

        // POEFixer also exposes the visible world-name label in its UI tree.
        // Use that live label as a fallback when the host omits the prop from
        // the targetable entity snapshot.
        std::vector<BenchNode> nodes;
        int budget = 2000;
        ReadUi(c, c->Ui.GetGameUiRoot(), -1, 0, budget, nodes);
        std::vector<POINT> labelMatches;
        for (const auto& node : nodes) {
            if (NormalizeIdentifier(node.name).find("reforgingbench") == std::string::npos ||
                node.w < 30 || node.h < 8 || !std::isfinite(node.x) || !std::isfinite(node.y) ||
                !std::isfinite(node.w) || !std::isfinite(node.h) || node.x < 1 || node.y < 1 ||
                node.x + node.w >= client.right - 1 || node.y + node.h >= client.bottom - 1) continue;
            labelMatches.push_back({static_cast<LONG>(std::lround(node.CenterX())),
                static_cast<LONG>(std::lround(node.CenterY()))});
        }
        if (labelMatches.size() != 1) return std::nullopt;
        return labelMatches.front();
    }
    std::optional<POINT> FindOwnStashScreenPoint(const PluginSDK::Context* c) {
        if (!c || !Foreground(c)) return std::nullopt;
        c->Overlay.SetEntityScanFlags(PluginSDK::EntityScan::StashObjects);
        const auto objects = c->Game.EnumerateStashObjects();
        if (!objects || objects->empty()) return std::nullopt;
        RECT client{};
        const HWND gameWindow = c->Game.GetGameWindow();
        if (!gameWindow || !::GetClientRect(gameWindow, &client)) return std::nullopt;
        struct Candidate { POINT point; int rank; float centerDistance; };
        std::vector<Candidate> matches;
        for (const auto& object : *objects) {
            if (object.Domain != PluginSDK::StashDomain::Own || object.Rank < 1 ||
                NormalizeIdentifier(object.Path).find("stash") == std::string::npos) continue;
            float x = 0, y = 0;
            if (!c->Render.WorldToScreen(object.WorldX, object.WorldY, object.WorldZ, x, y) ||
                !std::isfinite(x) || !std::isfinite(y) || x < 1 || y < 1 ||
                x >= client.right - 1 || y >= client.bottom - 1) continue;
            const float dx = x - client.right * .5f, dy = y - client.bottom * .5f;
            matches.push_back({{static_cast<LONG>(std::lround(x)), static_cast<LONG>(std::lround(y))},
                object.Rank, dx * dx + dy * dy});
        }
        if (matches.empty()) return std::nullopt;
        std::sort(matches.begin(), matches.end(), [](const auto& a, const auto& b) {
            return a.rank == b.rank ? a.centerDistance < b.centerDistance : a.rank > b.rank;
        });
        return matches.front().point;
    }
    bool BenchPanelVisible(const PluginSDK::Context* c) {
        window_ = c->Game.GetGameWindow();
        RECT client{};
        if (!window_ || !::GetClientRect(window_, &client)) return false;
        width_ = client.right; height_ = client.bottom;
        std::vector<BenchNode> nodes;
        int budget = 4000;
        ReadUi(c, c->Ui.GetGameUiRoot(), -1, 0, budget, nodes);
        auto found = budget > 0 ? DiscoverBench(nodes, static_cast<float>(width_), static_cast<float>(height_)) : std::nullopt;
        if (found) targets_ = std::move(found);
        return targets_.has_value();
    }
    static bool IsTabletStashTabName(const std::string& name) {
        return NormalizeIdentifier(name).find("tablet") != std::string::npos;
    }
    bool TabletStashPanelOpen(const PluginSDK::Context* c) const {
        return c->Game.EnumerateStashTabs(PluginSDK::StashDomain::Own).IsOpen();
    }
    void ReleaseWorldClick() {
        if (worldClickPhase_ == WorldClickPhase::ButtonDown) {
            INPUT release{}; release.type = INPUT_MOUSE; release.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            ::SendInput(1, &release, sizeof(INPUT));
        }
        worldClickPhase_ = WorldClickPhase::Idle;
    }
    bool PumpWorldClick(const PluginSDK::Context* c) {
        if (worldClickPhase_ == WorldClickPhase::Idle) return false;
        const HWND gameWindow = c ? c->Game.GetGameWindow() : nullptr;
        if (!c || !Foreground(c) || !gameWindow || gameWindow != window_ ||
            c->Game.GetAreaChangeCounter() != area_ || c->Game.IsHostAutomationActive()) {
            Stop("Stopped: game focus changed while waiting to click the stash or bench."); return true;
        }
        for (int key : {VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN, VK_RWIN})
            if (::GetAsyncKeyState(key) & 0x8000) {
                Stop("Stopped: a modifier key was held during the world click."); return true;
            }
        if (worldClickPhase_ == WorldClickPhase::Hover)
            for (int key : {VK_LBUTTON, VK_RBUTTON})
                if (::GetAsyncKeyState(key) & 0x8000) {
                    Stop("Stopped: a mouse button was held before the world click."); return true;
                }
        POINT cursor{};
        if (!::GetCursorPos(&cursor) || std::abs(cursor.x - worldClickScreen_.x) > 2 ||
            std::abs(cursor.y - worldClickScreen_.y) > 2) {
            Stop("Stopped: cursor moved before the stash or bench click."); return true;
        }
        const auto now = Clock::now();
        if (now < worldClickDue_) return true;
        if (worldClickPhase_ == WorldClickPhase::Hover) {
            INPUT press{}; press.type = INPUT_MOUSE; press.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            if (::SendInput(1, &press, sizeof(INPUT)) != 1) {
                Stop("Stopped: the stash or bench click was blocked."); return true;
            }
            worldClickPhase_ = WorldClickPhase::ButtonDown;
            worldClickDue_ = now + std::chrono::milliseconds(kNormalClickPulseMs);
            return true;
        }
        INPUT release{}; release.type = INPUT_MOUSE; release.mi.dwFlags = MOUSEEVENTF_LEFTUP;
        if (::SendInput(1, &release, sizeof(INPUT)) != 1) {
            Stop("Stopped: the stash or bench click could not be released."); return true;
        }
        worldClickPhase_ = WorldClickPhase::Idle;
        stashActionAt_ = now;
        c->Inventory.Scan(1);
        return true;
    }
    bool ClickWorldPoint(const PluginSDK::Context* c, POINT point) {
        if (!Foreground(c) || c->Game.IsHostAutomationActive()) return false;
        HWND gameWindow = c->Game.GetGameWindow();
        RECT client{};
        if (!gameWindow || !::GetClientRect(gameWindow, &client) || point.x < 0 || point.y < 0 ||
            point.x >= client.right || point.y >= client.bottom) return false;
        for (int key : {VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN, VK_RWIN, VK_LBUTTON, VK_RBUTTON})
            if (::GetAsyncKeyState(key) & 0x8000) return false;
        POINT screen = point;
        if (!::ClientToScreen(gameWindow, &screen)) return false;
        const int sx = ::GetSystemMetrics(SM_XVIRTUALSCREEN), sy = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int sw = ::GetSystemMetrics(SM_CXVIRTUALSCREEN), sh = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (sw < 2 || sh < 2 || screen.x < sx || screen.y < sy || screen.x >= sx + sw || screen.y >= sy + sh)
            return false;
        if (worldClickPhase_ != WorldClickPhase::Idle || inputPhase_ != InputPhase::Idle) return false;
        INPUT input{}; input.type = INPUT_MOUSE;
        input.mi.dx = static_cast<LONG>((static_cast<double>(screen.x - sx) * 65535.0) / (sw - 1));
        input.mi.dy = static_cast<LONG>((static_cast<double>(screen.y - sy) * 65535.0) / (sh - 1));
        input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
        if (::SendInput(1, &input, sizeof(INPUT)) != 1) return false;
        worldClickScreen_ = screen;
        worldClickPhase_ = WorldClickPhase::Hover;
        worldClickDue_ = Clock::now() + std::chrono::milliseconds(kHoverBeforeClickMs);
        return true;
    }
    bool PressEscape(const PluginSDK::Context* c) {
        if (!Foreground(c) || c->Game.IsHostAutomationActive() ||
            (::GetAsyncKeyState(VK_ESCAPE) & 0x8000)) return false;
        INPUT input[2]{};
        input[0].type = INPUT_KEYBOARD; input[0].ki.wVk = VK_ESCAPE;
        input[1].type = INPUT_KEYBOARD; input[1].ki.wVk = VK_ESCAPE; input[1].ki.dwFlags = KEYEVENTF_KEYUP;
        return ::SendInput(2, input, sizeof(INPUT)) == 2;
    }
    bool AttachTabletStash(const PluginSDK::Context* c, const std::filesystem::path& directory, bool writeDiagnostic = true) {
        window_ = c->Game.GetGameWindow();
        RECT client{};
        if (!window_ || !::GetClientRect(window_, &client)) return false;
        width_ = client.right; height_ = client.bottom;
        const auto tabs = c->Game.EnumerateStashTabs(PluginSDK::StashDomain::Own);
        const bool tabletTabActive = tabs.IsOpen() && std::any_of(tabs.Tabs.begin(), tabs.Tabs.end(),
            [](const auto& tab) { return tab.IsActive == 1 && IsTabletStashTabName(tab.Name); });
        if (!tabletTabActive) return false;
        std::vector<BenchNode> nodes;
        int budget = 8000;
        ReadUi(c, c->Ui.GetGameUiRoot(), -1, 0, budget, nodes);
        stashTargets_ = budget > 0 ? DiscoverTabletStash(nodes) : std::nullopt;
        if (stashTargets_) return true;
        if (!writeDiagnostic) return false;
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
    std::optional<bool> ReadHoveredTabletSanctified(const PluginSDK::Context* c, const ReforgeItem& item) {
        const auto expectedBase = NormalizeIdentifier(item.baseType);
        if (expectedBase.empty()) {
            lastTooltipScanSummary_ = "invalid_item_base";
            return std::nullopt;
        }

        // Some hosts expose the rendered item tooltip under the general UI root,
        // while stash controls are exposed under the game UI root. Search both roots.
        std::vector<BenchNode> nodes;
        std::unordered_set<uintptr_t> seenNodes;
        std::unordered_set<uintptr_t> seenRoots;
        std::vector<std::string> rootResults;
        for (const auto root : {c->Ui.GetUiRoot(), c->Ui.GetGameUiRoot()}) {
            if (!root || !seenRoots.insert(root).second) continue;
            std::vector<BenchNode> rootNodes;
            int budget = 8000;
            ReadUi(c, root, -1, 0, budget, rootNodes);
            rootResults.push_back("root=" + std::to_string(root) + " nodes=" +
                std::to_string(rootNodes.size()) + " remaining=" + std::to_string(budget));
            for (auto& node : rootNodes)
                if (seenNodes.insert(node.address).second) nodes.push_back(std::move(node));
        }

        bool tooltipTitleFound = false;
        bool sanctifiedMarkerFound = false;
        std::vector<std::string> traceNodes;
        POINT cursor{};
        const bool cursorValid = ::GetCursorPos(&cursor) && window_ && ::ScreenToClient(window_, &cursor);
        for (const auto& node : nodes) {
            const auto rawText = c->Ui.GetText(node.address);
            const auto displayText = NormalizeIdentifier(rawText);
            const auto stringId = NormalizeIdentifier(node.text);
            const auto controlName = NormalizeIdentifier(node.name);
            const auto nodeText = displayText + " " + stringId + " " + controlName;
            if (nodeText.find(expectedBase) != std::string::npos) tooltipTitleFound = true;
            if (displayText == "sanctified" || stringId == "sanctified" || controlName == "sanctified" ||
                nodeText.find("sanctified") != std::string::npos)
                sanctifiedMarkerFound = true;

            if (debugRecording_) {
                const bool overlapsCursor = cursorValid && cursor.x >= node.x && cursor.y >= node.y &&
                    cursor.x < node.x + node.w && cursor.y < node.y + node.h;
                const bool nearby = cursorValid && node.w > 0 && node.h > 0 &&
                    std::abs(node.CenterX() - cursor.x) <= 500 && std::abs(node.CenterY() - cursor.y) <= 400;
                const auto lower = NormalizeIdentifier(rawText + " " + node.name + " " + node.text);
                const bool keyword = lower.find(expectedBase) != std::string::npos ||
                    lower.find("sanctified") != std::string::npos || lower.find("tooltip") != std::string::npos ||
                    lower.find("itempopup") != std::string::npos || lower.find("itemtooltip") != std::string::npos;
                if ((overlapsCursor || nearby || keyword) &&
                    (!rawText.empty() || !node.text.empty() || !node.name.empty()) && traceNodes.size() < 180) {
                    auto clean = [](std::string value) {
                        std::replace(value.begin(), value.end(), '\r', ' ');
                        std::replace(value.begin(), value.end(), '\n', ' ');
                        return value;
                    };
                    traceNodes.push_back("ui [" + clean(node.name) + "] id=[" + clean(node.text) +
                        "] text=[" + clean(rawText) + "] rect=" + std::to_string(node.x) + "," +
                        std::to_string(node.y) + " " + std::to_string(node.w) + "x" +
                        std::to_string(node.h) + " cursor_overlap=" + std::to_string(overlapsCursor));
                }
            }
        }

        std::string rootSummary;
        for (const auto& result : rootResults) {
            if (!rootSummary.empty()) rootSummary += "; ";
            rootSummary += result;
        }
        lastTooltipScanSummary_ = "expected=" + expectedBase + " nodes=" + std::to_string(nodes.size()) +
            " title=" + std::to_string(tooltipTitleFound) + " sanctified=" +
            std::to_string(sanctifiedMarkerFound) + " " + rootSummary;
        if (debugRecording_ && debugLog_) {
            const auto mods = c->Inventory.ReadItemMods(item.address);
            debugLog_ << "\n--- hovered tablet verification ---\n"
                << "scan=[" << lastTooltipScanSummary_ << "] item_base=[" << item.baseType
                << "] rarity=" << item.rarity << " identified=" << item.identified
                << " corrupted=" << item.corrupted << " mods_valid=" << mods.Valid
                << " mods_identified=" << mods.IsIdentified << " mods_corrupted=" << mods.IsCorrupted
                << " mods_relic=" << mods.IsRelic << " mod_marker_sanctified=" << HasSanctifiedMarker(mods)
                << "\n";
            for (const auto& line : traceNodes) debugLog_ << line << "\n";
            debugLog_.flush();
        }
        if (!tooltipTitleFound) return std::nullopt;
        return sanctifiedMarkerFound;
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
                return tab.IsActive == 1 && IsTabletStashTabName(tab.Name);
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
    bool Click(const PluginSDK::Context* c, POINT point, bool ctrl, int pulseMs = 0, int hoverMs = 0) {
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
        if (::SendInput(1, &input, sizeof(INPUT)) != 1) return false;
        queuedCtrl_ = ctrl; ownedCtrl_ = ownedButton_ = false;
        inputPhase_ = InputPhase::Hover;
        // Only inventory-to-bench tablet clicks pass a speed override.
        // Stash and bench UI clicks keep a steady pulse and hover pause.
        activeInputPulseMs_ = pulseMs > 0 ? std::clamp(pulseMs, 12, 40) : kNormalClickPulseMs;
        inputDue_ = Clock::now() + std::chrono::milliseconds(
            hoverMs > 0 ? std::clamp(hoverMs, 100, kHoverBeforeClickMs) : kHoverBeforeClickMs);
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
        ownedCtrl_ = ownedButton_ = queuedCtrl_ = false; inputPhase_ = InputPhase::Idle;
    }
    bool PumpInput(const PluginSDK::Context* c) {
        if (inputPhase_ == InputPhase::Idle) return false;
        if (!Ready(c)) { Stop("Stopped: game focus or automation panel changed during a click."); return true; }
        const auto now = Clock::now();
        if (now < inputDue_) return true;
        INPUT event{};
        if (inputPhase_ == InputPhase::Hover && queuedCtrl_) {
            event.type = INPUT_KEYBOARD; event.ki.wVk = VK_LCONTROL;
            inputPhase_ = InputPhase::ModifierDown;
        } else if (inputPhase_ == InputPhase::Hover || inputPhase_ == InputPhase::ModifierDown) {
            event.type = INPUT_MOUSE; event.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
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
        if (event.type == INPUT_MOUSE && event.mi.dwFlags == MOUSEEVENTF_LEFTDOWN) ownedButton_ = true;
        if (event.type == INPUT_MOUSE && event.mi.dwFlags == MOUSEEVENTF_LEFTUP) ownedButton_ = false;
        if (event.type == INPUT_KEYBOARD && event.ki.dwFlags == 0) ownedCtrl_ = true;
        if (event.type == INPUT_KEYBOARD && (event.ki.dwFlags & KEYEVENTF_KEYUP)) ownedCtrl_ = false;
        inputDue_ = now + std::chrono::milliseconds(activeInputPulseMs_);
        return true;
    }
    void Wait(const PluginSDK::Context* c, Phase next, uint64_t stamp) {
        phase_ = next; stamp_ = stamp; actionAt_ = Clock::now();
        deadline_ = actionAt_ + std::chrono::seconds(6);
        c->Inventory.Scan(1);
    }
    void PauseCycleForBench(const char* message) {
        ReleaseInput(); phase_ = Phase::Idle; pending_ = false; targets_.reset();
        stashInputMode_ = false; stashTargets_.reset(); stashStage_ = StashStage::CloseStashForBench;
        stashFromExhaustion_ = false; stashActionAt_ = Clock::now(); status_ = message;
    }
    void PauseCycleForStash(const char* message) {
        ReleaseInput(); phase_ = Phase::Idle; pending_ = false; targets_.reset();
        stashActionAt_ = Clock::now(); status_ = message;
        if (stashInputMode_ && stashTargets_) {
            stashDepositInit_ = false; stashDepositItem_.reset(); stashStage_ = StashStage::Deposit;
            stashTabSelected_ = true;
        } else {
            stashInputMode_ = false; stashTargets_.reset(); stashResumeForDeposit_ = true;
            stashStage_ = StashStage::CloseBenchForStash;
        }
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
    std::optional<uintptr_t> FirstIneligibleBagTablet(const PluginSDK::Context* c,
        const PluginSDK::InventorySnapshot& snapshot) {
        if (!snapshot.Ready) return std::nullopt;
        const auto& bag = Bag(c, snapshot.Value, snapshot.AreaChangeCounter, snapshot.ScanStamp);
        for (const auto& item : snapshot.Value.Items) {
            if (!LooksLikeTablet(item.Path, item.BaseTypeName)) continue;
            const auto identity = bag.find(item.Address);
            if (identity == bag.end() ||
                !ReforgeCandidate(identity->second, stashRarityMask_, stashTypeRules_, false))
                return item.Address;
        }
        return std::nullopt;
    }
    void BeginIneligibleTabletCleanup(const PluginSDK::Context* c, const char* message) {
        const auto snapshot = c->Inventory.ReadSnapshot(1);
        if (!snapshot.Ready || !FirstIneligibleBagTablet(c, snapshot)) return;
        stashDepositIneligibleOnly_ = true;
        stashFromExhaustion_ = false;
        stashDepositInit_ = false;
        stashDepositItem_.reset();
        stashExpectedBag_.clear();
        stashTabSelected_ = true;
        stashInputMode_ = true;
        stashStage_ = StashStage::Deposit;
        stashActionAt_ = Clock::now();
        status_ = message;
    }
    void AdvanceStashPage(const PluginSDK::Context* c) {
        if (++stashPage_ >= 6) {
            stashPage_ = 0;
            ++stashCategory_;
            stashUiCategory_ = -1;
        }
        // Keep the active category while moving through its six pages. Only
        // reset the page control between pages; select a new category once when
        // the current category's six pages have all been scanned.
        stashUiPage_ = -1;
        stashPageInventorySnapshot_.reset();
        stashPageRefreshAt_ = {};
        stashEmptyConfirmCategory_ = stashEmptyConfirmPage_ = -1;
        stashEmptyConfirmAt_ = {};
        if (stashCategory_ < stashCategories_.size()) {
            stashStage_ = stashUiCategory_ < 0 ? StashStage::NavigateCategory : StashStage::NavigatePage;
            return;
        }
        stashFromExhaustion_ = true;
        const auto snapshot = c->Inventory.ReadSnapshot(1);
        if (CycleHasBatch(c, snapshot)) {
            stashFromExhaustion_ = false;
            PauseCycleForBench("Selected stash pages scanned. Finding the Reforging Bench...");
        } else if (snapshot.Ready && std::none_of(snapshot.Value.Items.begin(), snapshot.Value.Items.end(), [&](const auto& item) {
            return stashSessionAddresses_.contains(item.Address);
        })) {
            stashStage_ = StashStage::Finished;
            stashCycle_ = false;
            status_ = "Finished: no compatible identified trio remained in the selected stash categories.";
        } else {
            PauseCycleForStash("No compatible trio remains. Returning remaining tablets to the stash...");
        }
    }
    void TickStash(const PluginSDK::Context* c, const std::filesystem::path& directory) {
        if (stashStage_ == StashStage::AwaitBench || stashStage_ == StashStage::AwaitStash ||
            stashStage_ == StashStage::Finished || stashStage_ == StashStage::Idle ||
            stashStage_ == StashStage::ForgePass) return;
        if (stashStage_ == StashStage::PendingStart && !Foreground(c)) {
            if (!DismissHostWindowForCycle(c)) {
                status_ = "Hiding the POEFixer window and returning focus to Path of Exile 2...";
                return;
            }
        }
        if (!Foreground(c)) {
            if (stashCycle_)
                status_ = "Paused: return focus to Path of Exile 2 to continue the stash cycle. Press Esc to stop.";
            return;
        }
        stashInputMode_ = stashStage_ == StashStage::NavigateTab ||
            stashStage_ == StashStage::NavigateCategory || stashStage_ == StashStage::NavigatePage ||
            stashStage_ == StashStage::AwaitPage || stashStage_ == StashStage::ReadPage || stashStage_ == StashStage::Pull ||
            stashStage_ == StashStage::WaitPull || stashStage_ == StashStage::CheckTabletTooltip ||
            stashStage_ == StashStage::WaitTabletClipboard || stashStage_ == StashStage::Deposit ||
            stashStage_ == StashStage::WaitDeposit;
        if (PumpInput(c)) return;
        if (PumpWorldClick(c)) return;

        const auto now = Clock::now();
        if (now - stashActionAt_ < std::chrono::milliseconds(108)) return;
        if (stashStage_ == StashStage::CheckTabletTooltip &&
            now - stashActionAt_ < std::chrono::milliseconds(162)) return;
        if (stashStage_ == StashStage::CloseStashForBench) {
            if (TabletStashPanelOpen(c)) {
                if (!PressEscape(c)) { status_ = "Waiting to close the Tablet stash safely..."; return; }
                stashStage_ = StashStage::WaitStashClosed; stashActionAt_ = now; return;
            }
            stashStage_ = StashStage::FindBench;
        }
        if (stashStage_ == StashStage::WaitStashClosed) {
            if (TabletStashPanelOpen(c)) return;
            stashStage_ = StashStage::FindBench; stashActionAt_ = now;
        }
        if (stashStage_ == StashStage::FindBench) {
            const auto point = FindBenchScreenPoint(c);
            if (!point) { status_ = "Waiting for one visible, targetable Reforging Bench..."; return; }
            if (!ClickWorldPoint(c, *point)) { status_ = "Bench found; waiting to send a safe click..."; return; }
            stashStage_ = StashStage::WaitBenchOpen; stashActionAt_ = now;
            status_ = "Opening the detected Reforging Bench..."; return;
        }
        if (stashStage_ == StashStage::WaitBenchOpen) {
            // Give the character time to reach the prop. If the panel did not open,
            // retry the interaction after a pause instead of clicking rapidly.
            if (now - stashActionAt_ < std::chrono::milliseconds(1260)) return;
            if (!AttachBench(c, directory, false) || !Ready(c)) {
                const auto point = FindBenchScreenPoint(c);
                if (point && ClickWorldPoint(c, *point)) {
                    stashActionAt_ = now;
                    status_ = "Waiting for the Reforging Bench to open; retrying after a short pause...";
                } else {
                    stashActionAt_ = now;
                    status_ = "Waiting for the Reforging Bench to become available...";
                }
                return;
            }
            stashInputMode_ = false; stashStage_ = StashStage::ForgePass;
            pending_ = true; all_ = true; area_ = c->Game.GetAreaChangeCounter();
            window_ = c->Game.GetGameWindow(); status_ = "Bench opened. Starting the reforge pass...";
            return;
        }
        if (stashStage_ == StashStage::CloseBenchForStash) {
            if (now - stashActionAt_ < std::chrono::milliseconds(450)) return;
            stashActionAt_ = now;
            if (BenchPanelVisible(c)) {
                if (!PressEscape(c)) { status_ = "Waiting to close the Reforging Bench safely..."; return; }
                targets_.reset(); stashStage_ = StashStage::WaitBenchClosed; stashActionAt_ = now; return;
            }
            stashStage_ = StashStage::FindStash;
        }
        if (stashStage_ == StashStage::WaitBenchClosed) {
            if (now - stashActionAt_ < std::chrono::milliseconds(450)) return;
            stashActionAt_ = now;
            if (BenchPanelVisible(c)) return;
            targets_.reset(); stashStage_ = StashStage::FindStash; stashActionAt_ = now;
        }
        if (stashStage_ == StashStage::FindStash) {
            if (TabletStashPanelOpen(c)) { stashStage_ = StashStage::WaitStashOpen; stashActionAt_ = now; }
            else {
                const auto point = FindOwnStashScreenPoint(c);
                if (!point) { status_ = "Waiting for a visible own stash object..."; return; }
                if (!ClickWorldPoint(c, *point)) { status_ = "Stash found; waiting to send a safe click..."; return; }
                stashStage_ = StashStage::WaitStashOpen; stashActionAt_ = now;
                status_ = "Opening the detected own stash..."; return;
            }
        }
        if (stashStage_ == StashStage::WaitStashOpen) {
            // Initial interaction can send the character toward the stash. Wait for
            // that approach, then retry only if POEFixer still reports it closed.
            if (now - stashActionAt_ < std::chrono::milliseconds(1260)) return;
            const auto tabs = c->Game.EnumerateStashTabs(PluginSDK::StashDomain::Own);
            if (!tabs.IsOpen()) {
                const auto point = FindOwnStashScreenPoint(c);
                if (point && ClickWorldPoint(c, *point))
                    status_ = "Waiting for the stash to open; retrying after a short pause...";
                else
                    status_ = "Waiting for the stash interaction...";
                stashActionAt_ = now;
                return;
            }
            stashActionAt_ = now;
            const auto tabletTab = std::find_if(tabs.Tabs.begin(), tabs.Tabs.end(), [](const auto& tab) {
                return IsTabletStashTabName(tab.Name);
            });
            if (tabletTab == tabs.Tabs.end()) { status_ = "Waiting for the Tablet stash tab..."; return; }
            if (tabletTab->IsActive != 1) {
                const auto clickTab = tabletTab->OnScreen
                    ? std::optional<POINT>(POINT{static_cast<LONG>(tabletTab->CenterX()), static_cast<LONG>(tabletTab->CenterY())})
                    : (tabs.Toggle.OnScreen ? std::optional<POINT>(POINT{static_cast<LONG>(tabs.Toggle.CenterX()),
                        static_cast<LONG>(tabs.Toggle.CenterY())}) : std::nullopt);
                if (clickTab && ClickWorldPoint(c, *clickTab)) stashActionAt_ = now;
                status_ = "Selecting the Tablet stash tab..."; return;
            }
            if (!AttachTabletStash(c, directory, false)) return;
            stashInputMode_ = true; stashTabSelected_ = false; stashDepositInit_ = false;
            stashStage_ = StashStage::NavigateTab;
            stashUiCategory_ = stashUiPage_ = -1; stashActionAt_ = now;
            status_ = stashResumeForDeposit_
                ? "Stash reopened. Returning results and continuing the scan..."
                : "Tablet stash selected. Starting the scan..."; return;
        }
        if (stashStage_ == StashStage::PendingStart) {
            if (c->Game.IsHostAutomationActive()) { Stop("Another POEFixer automation is active."); return; }
            if (!stashInventorySeeded_) {
                const auto initial = c->Inventory.ReadSnapshot(1);
                if (!initial.Supported) { Stop("This POEFixer version does not provide inventory snapshots."); return; }
                if (!initial.Ready) { c->Inventory.Scan(1); return; }
                if (initial.AreaChangeCounter != area_) { Stop("The game area changed before the stash cycle started."); return; }

                const auto& initialBag = Bag(c, initial.Value, initial.AreaChangeCounter, initial.ScanStamp);
                stashExpectedBag_ = initialBag;
                stashSessionAddresses_.clear();
                stashSkippedAddresses_.clear();
                for (const auto& [address, item] : initialBag) {
                    if (ReforgeCandidate(item, stashRarityMask_, stashTypeRules_, false))
                        stashSessionAddresses_.insert(address);
                }
                stashInventorySeeded_ = true;

                if (CycleHasBatch(c, initial)) {
                    PauseCycleForBench("A compatible trio is already in your inventory. Finding the Reforging Bench...");
                    return;
                }
                if (!PlayerInventoryGridReady(initial.Value)) {
                    status_ = "Waiting for POEFixer to publish the player inventory grid; it is not being treated as full.";
                    c->Inventory.Scan(1);
                    return;
                }
            }
            const auto tabs = c->Game.EnumerateStashTabs(PluginSDK::StashDomain::Own);
            if (!tabs.IsOpen()) {
                if (now - stashActionAt_ < std::chrono::milliseconds(kInitialStashOpenDelayMs)) {
                    status_ = "Waiting briefly before the first stash click...";
                    return;
                }
                const auto point = FindOwnStashScreenPoint(c);
                if (!point) { status_ = "Waiting for a visible own stash object..."; return; }
                if (!ClickWorldPoint(c, *point)) { status_ = "Stash found; waiting to send a safe click..."; return; }
                stashStage_ = StashStage::WaitStashOpen; stashResumeForDeposit_ = false;
                stashActionAt_ = now; status_ = "Opening your stash..."; return;
            }
            const auto tabletTab = std::find_if(tabs.Tabs.begin(), tabs.Tabs.end(), [](const auto& tab) {
                return IsTabletStashTabName(tab.Name);
            });
            if (tabletTab == tabs.Tabs.end()) {
                status_ = "Waiting for your Tablet stash tab..."; return;
            }
            if (tabletTab->IsActive != 1) {
                const auto clickTab = tabletTab->OnScreen
                    ? std::optional<POINT>(POINT{static_cast<LONG>(tabletTab->CenterX()), static_cast<LONG>(tabletTab->CenterY())})
                    : (tabs.Toggle.OnScreen ? std::optional<POINT>(POINT{static_cast<LONG>(tabs.Toggle.CenterX()),
                        static_cast<LONG>(tabs.Toggle.CenterY())}) : std::nullopt);
                if (clickTab && ClickWorldPoint(c, *clickTab)) stashActionAt_ = now;
                status_ = "Selecting the Tablet stash tab..."; return;
            }
            if (!AttachTabletStash(c, directory)) {
                status_ = "Waiting for the Tablet subtab controls..."; return;
            }
            const auto snapshot = c->Inventory.ReadSnapshot(1);
            if (!snapshot.Supported) { Stop("This POEFixer version does not provide inventory snapshots."); return; }
            if (!snapshot.Ready) { c->Inventory.Scan(1); return; }
            if (snapshot.AreaChangeCounter != area_) { Stop("The game area changed before the stash scan started."); return; }
            if (!stashInventorySeeded_) {
                const auto& initialBag = Bag(c, snapshot.Value, snapshot.AreaChangeCounter, snapshot.ScanStamp);
                stashExpectedBag_ = initialBag; stashSessionAddresses_.clear(); stashSkippedAddresses_.clear();
                for (const auto& [address, item] : initialBag)
                    if (ReforgeCandidate(item, stashRarityMask_, stashTypeRules_, false))
                        stashSessionAddresses_.insert(address);
                stashInventorySeeded_ = true;
                if (CycleHasBatch(c, snapshot)) {
                    PauseCycleForBench("A compatible trio is already in your inventory. Finding the Reforging Bench...");
                    return;
                }
            }
            if (!PlayerInventoryGridReady(snapshot.Value)) {
                status_ = "Waiting for the player inventory grid; it is not being treated as full.";
                c->Inventory.Scan(1);
                return;
            }
            stashInputMode_ = true; stashTabSelected_ = false; stashUiCategory_ = stashUiPage_ = -1;
            if (FirstIneligibleBagTablet(c, snapshot)) {
                BeginIneligibleTabletCleanup(c,
                    "Returning filtered-out tablets from your inventory to the stash before scanning...");
                return;
            }
            if (!FirstFreeInventoryCell(snapshot.Value)) {
                Stop("Your inventory is full of eligible tablets but has no compatible trio. Adjust the selected categories or free bag space.");
                return;
            }
            stashStage_ = StashStage::NavigateTab; stashActionAt_ = now;
        }
        if (now - stashActionAt_ < std::chrono::milliseconds(108)) return;

        if (stashStage_ == StashStage::NavigateTab || stashStage_ == StashStage::NavigateCategory ||
            stashStage_ == StashStage::NavigatePage || stashStage_ == StashStage::ReadPage ||
            stashStage_ == StashStage::Pull || stashStage_ == StashStage::WaitPull ||
            stashStage_ == StashStage::Deposit || stashStage_ == StashStage::WaitDeposit) {
            // Refresh the grid and occupied cells while reading a page. POEFixer's
            // inventory enumeration can temporarily omit Grid.Valid after a page switch.
            const bool refreshPage = stashStage_ == StashStage::ReadPage &&
                !stashPageInventorySnapshot_ &&
                (stashPageRefreshAt_ == Clock::time_point{} ||
                    now - stashPageRefreshAt_ >= std::chrono::milliseconds(360));
            if (refreshPage || !stashTargets_) {
                const bool attached = AttachTabletStash(c, directory, false);
                if (refreshPage) stashPageRefreshAt_ = now;
                if (!attached) {
                    if (stashStage_ == StashStage::ReadPage) {
                        status_ = "Waiting for the selected Tablet stash page controls to become readable; no item will be clicked until they match.";
                        if (stashInventoryId_ > 0) c->Inventory.Scan(stashInventoryId_);
                        return;
                    }
                    Stop("Tablet stash controls changed or could not be identified. No further clicks sent."); return;
                }
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
                stashUiCategory_ = static_cast<int>(index); stashUiPage_ = -1;
                stashPageInventorySnapshot_.reset(); stashActionAt_ = now; return;
            }
            stashStage_ = StashStage::NavigatePage;
        }
        if (stashStage_ == StashStage::NavigatePage) {
            if (stashUiPage_ != static_cast<int>(stashPage_)) {
                const auto& button = stashTargets_->pages[stashPage_];
                if (!Click(c, {static_cast<LONG>(button.CenterX()), static_cast<LONG>(button.CenterY())}, false)) {
                    Stop("Could not select a Tablet stash page. No items moved."); return;
                }
                stashUiPage_ = -2;
                stashStage_ = StashStage::AwaitPage;
                stashActionAt_ = now;
                status_ = "Selecting Tablet stash page " + std::to_string(stashPage_ + 1) + " / 6...";
                return;
            }
            stashStage_ = StashStage::ReadPage;
        }
        if (stashStage_ == StashStage::AwaitPage) {
            // Wait until the click finishes, then bind to the inventory whose
            // live grid geometry matches the newly selected page. Scanning the
            // previous page ID can invalidate the active page during POEFixer's
            // transition and leave its visible cells temporarily unmatchable.
            if (inputPhase_ != InputPhase::Idle || worldClickPhase_ != WorldClickPhase::Idle) return;
            if (!AttachTabletStash(c, directory, false) || stashCategory_ >= stashCategories_.size()) {
                status_ = "Waiting for the selected Tablet stash page to become visible...";
                return;
            }
            const auto inventories = c->Inventory.GetAll();
            const auto match = MatchVisibleStashPage(*stashTargets_, inventories, stashCategories_[stashCategory_]);
            const auto& active = match.inventory;
            const float originTolerance = (std::max)(2.5f, stashTargets_->cellSize * .25f);
            const float cellTolerance = (std::max)(2.5f, stashTargets_->cellSize * .05f);
            const bool activeGrid =
                active.Grid.Valid && active.TotalBoxesX == 12 && active.TotalBoxesY == 8 &&
                std::abs(active.Grid.GridScreenX - stashTargets_->grid.x) <= originTolerance &&
                std::abs(active.Grid.GridScreenY - stashTargets_->grid.y) <= originTolerance &&
                std::abs(active.Grid.CellSize - stashTargets_->cellSize) <= cellTolerance;
            if ((match.state != TabletStashPageMatchState::Matched &&
                 match.state != TabletStashPageMatchState::Empty) || !activeGrid) {
                status_ = "Waiting for POEFixer to expose the active stash page grid...";
                return;
            }
            stashInventoryId_ = active.InventoryId;
            stashPageInventorySnapshot_ = active;
            stashUiPage_ = static_cast<int>(stashPage_);
            stashStage_ = StashStage::ReadPage;
            stashPageRefreshAt_ = {};
            stashActionAt_ = now;
            return;
        }
        if (stashStage_ == StashStage::ReadPage || stashStage_ == StashStage::Pull) {
            // Do not block page inspection on POEFixer's intermittent player-bag
            // grid loss. Empty pages can be advanced without needing a bag cell.
            TabletStashPageMatch match;
            if (stashPageInventorySnapshot_) {
                match.inventory = std::move(*stashPageInventorySnapshot_);
                stashPageInventorySnapshot_.reset();
                match.state = match.inventory.Items.empty()
                    ? TabletStashPageMatchState::Empty
                    : TabletStashPageMatchState::Matched;
            } else {
                const auto stashInventories = c->Inventory.GetAll();
                match = MatchVisibleStashPage(*stashTargets_, stashInventories, stashCategories_[stashCategory_]);
            }
            if (match.inventory.InventoryId > 0) stashInventoryId_ = match.inventory.InventoryId;
            if (match.state == TabletStashPageMatchState::Ambiguous || match.state == TabletStashPageMatchState::Missing) {
                stashEmptyConfirmCategory_ = stashEmptyConfirmPage_ = -1;
                stashEmptyConfirmAt_ = {};
                status_ = match.state == TabletStashPageMatchState::Ambiguous
                    ? "Waiting for POEFixer to resolve multiple matching Tablet stash grids; no item will be clicked."
                    : "Waiting for POEFixer to publish the selected Tablet stash page; no item will be clicked.";
                if (stashInventoryId_ > 0) c->Inventory.Scan(stashInventoryId_);
                return;
            }
            if (match.state == TabletStashPageMatchState::Empty) {
                if (stashEmptyConfirmCategory_ != static_cast<int>(stashCategory_) ||
                    stashEmptyConfirmPage_ != static_cast<int>(stashPage_)) {
                    stashEmptyConfirmCategory_ = static_cast<int>(stashCategory_);
                    stashEmptyConfirmPage_ = static_cast<int>(stashPage_);
                    stashEmptyConfirmAt_ = now;
                    stashPageRefreshAt_ = {};
                    status_ = "Confirming the Tablet stash page is really empty...";
                    if (stashInventoryId_ > 0) c->Inventory.Scan(stashInventoryId_);
                    return;
                }
                if (now - stashEmptyConfirmAt_ < std::chrono::milliseconds(1500)) {
                    // Re-scan this active stash inventory while confirming. A
                    // transient empty page during POEFixer's page switch must
                    // not cause pages 2–6 to be skipped.
                    if (stashInventoryId_ > 0) c->Inventory.Scan(stashInventoryId_);
                    return;
                }
                stashEmptyConfirmCategory_ = stashEmptyConfirmPage_ = -1;
                stashEmptyConfirmAt_ = {};
                AdvanceStashPage(c);
                return;
            }
            stashEmptyConfirmCategory_ = stashEmptyConfirmPage_ = -1;
            stashEmptyConfirmAt_ = {};
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
                if (stashSkippedAddresses_.contains(item.Address)) continue;
                candidates.push_back(&item);
            }
            std::sort(candidates.begin(), candidates.end(), [](const auto* a, const auto* b) {
                return a->SlotY == b->SlotY ? a->SlotX < b->SlotX : a->SlotY < b->SlotY;
            });
            if (candidates.empty()) { AdvanceStashPage(c); return; }
            if (!PlayerInventoryGridReady(snapshot.Value)) {
                status_ = "Waiting for POEFixer to expose the player bag grid before withdrawing a tablet.";
                c->Inventory.Scan(1);
                return;
            }
            if (!FirstFreeInventoryCell(snapshot.Value)) {
                if (CycleHasBatch(c, snapshot)) {
                    PauseCycleForBench("Inventory filled. Finding the Reforging Bench...");
                } else if (FirstIneligibleBagTablet(c, snapshot)) {
                    BeginIneligibleTabletCleanup(c,
                        "Returning filtered-out tablets from your inventory to free space...");
                } else {
                    Stop("Your inventory is full of eligible tablets but has no compatible trio. Adjust the selected categories or free bag space.");
                }
                return;
            }
            const auto* item = candidates.front();
            const auto point = TabletStashSlotCenter(*stashTargets_, item->SlotX, item->SlotY);
            if (!point) { Stop("Could not safely locate the stash tablet cell."); return; }
            if (!MoveCursor(c, *point)) {
                // UI controls and the bag grid can be republished between two
                // polls. No click has been sent yet, so refresh and retry safely.
                status_ = "Waiting for stable stash controls before hovering the tablet; no item clicked.";
                stashPageInventorySnapshot_.reset();
                stashPageRefreshAt_ = {};
                if (stashInventoryId_ > 0) c->Inventory.Scan(stashInventoryId_);
                return;
            }
            stashPendingItem_ = Identity(c, *item); stashExpectedBag_ = bag;
            stashBagStamp_ = snapshot.ScanStamp;
            stashStage_ = StashStage::CheckTabletTooltip; stashActionAt_ = now;
            status_ = "Checking tablet properties before withdrawing...";
            return;
        }
        if (stashStage_ == StashStage::WaitTabletClipboard) {
            if (!stashPendingItem_ || bag != stashExpectedBag_) {
                if (clipboardSnapshotValid_)
                    RestoreClipboardSnapshot(::GetClipboardSequenceNumber());
                Stop("Tablet inventory changed while checking its copied text."); return;
            }
            if (::GetClipboardSequenceNumber() == clipboardSequenceBeforeCopy_) {
                if (now < clipboardDeadline_) { status_ = "Waiting for the game to copy the hovered tablet text..."; return; }
                RestoreClipboardSnapshot(::GetClipboardSequenceNumber());
                Stop("The game did not copy the hovered tablet text. No stash item was clicked."); return;
            }
            std::wstring copiedText;
            DWORD copiedSequence = 0;
            if (!ReadCopiedTabletText(copiedText, copiedSequence)) {
                if (now < clipboardDeadline_) { status_ = "Reading the copied tablet text..."; return; }
                RestoreClipboardSnapshot(::GetClipboardSequenceNumber());
                Stop("Could not read the copied tablet text. No stash item was clicked."); return;
            }
            const auto normalizedText = NormalizeIdentifier(ClipboardAscii(copiedText));
            const auto expectedBase = NormalizeIdentifier(stashPendingItem_->baseType);
            const bool baseMatches = !expectedBase.empty() &&
                normalizedText.find(expectedBase) != std::string::npos;
            const bool isSanctified = normalizedText.find("sanctified") != std::string::npos;
            const bool clipboardRestored = RestoreClipboardSnapshot(copiedSequence);
            if (!clipboardRestored) {
                Stop("Could not restore the clipboard safely. No stash item was clicked."); return;
            }
            if (!baseMatches) {
                Stop("Copied item text did not match the hovered tablet. No stash item was clicked."); return;
            }
            if (isSanctified) {
                stashSkippedAddresses_.insert(stashPendingItem_->address);
                stashPendingItem_.reset(); stashStage_ = StashStage::ReadPage; stashActionAt_ = now;
                status_ = "Sanctified tablet left in the stash; checking the next tablet...";
                return;
            }
            clipboardVerified_ = true;
            stashStage_ = StashStage::CheckTabletTooltip; stashActionAt_ = now;
            status_ = "Tablet text verified; preparing the stash click...";
            return;
        }
        if (stashStage_ == StashStage::CheckTabletTooltip) {
            if (!stashPendingItem_ || bag != stashExpectedBag_) {
                Stop("Tablet inventory changed while checking its properties."); return;
            }
            std::optional<bool> sanctified;
            if (clipboardVerified_) { clipboardVerified_ = false; sanctified = false; }
            else sanctified = ReadHoveredTabletSanctified(c, *stashPendingItem_);
            if (!sanctified) {
                if (!BeginClipboardTabletRead(c)) {
                    Stop("Could not safely read the hovered tablet text from the clipboard. No item was clicked."); return;
                }
                stashStage_ = StashStage::WaitTabletClipboard; stashActionAt_ = now;
                status_ = "Checking the tablet text to identify Sanctified items...";
                return;
            }
            if (*sanctified) {
                stashSkippedAddresses_.insert(stashPendingItem_->address);
                stashPendingItem_.reset(); stashStage_ = StashStage::ReadPage; stashActionAt_ = now;
                status_ = "Sanctified tablet left in the stash; checking the next tablet...";
                return;
            }
            const auto match = MatchVisibleStashPage(*stashTargets_, c->Inventory.GetAll(), stashCategories_[stashCategory_]);
            const auto item = std::find_if(match.inventory.Items.begin(), match.inventory.Items.end(), [&](const auto& entry) {
                return entry.Address == stashPendingItem_->address;
            });
            if (match.state == TabletStashPageMatchState::Missing ||
                match.state == TabletStashPageMatchState::Ambiguous) {
                status_ = "Waiting to revalidate the hovered tablet against the active stash page; no click sent.";
                if (stashInventoryId_ > 0) c->Inventory.Scan(stashInventoryId_);
                return;
            }
            if (match.state != TabletStashPageMatchState::Matched || item == match.inventory.Items.end()) {
                status_ = "The inspected stash slot changed while the page refreshed; rescanning before continuing.";
                stashPendingItem_.reset();
                stashStage_ = StashStage::ReadPage;
                stashPageInventorySnapshot_.reset();
                stashPageRefreshAt_ = {};
                stashActionAt_ = now;
                if (stashInventoryId_ > 0) c->Inventory.Scan(stashInventoryId_);
                return;
            }
            const auto point = TabletStashSlotCenter(*stashTargets_, item->SlotX, item->SlotY);
            if (!point) { Stop("Could not safely locate the stash tablet."); return; }
            // Rescans may legitimately advance POEFixer's scan stamp while the
            // hovered tooltip is being read. Start transfer verification from
            // the newest unchanged inventory snapshot.
            stashBagStamp_ = snapshot.ScanStamp;
            if (!Click(c, *point, true, 0, kStashTabletHoverMs)) { Stop("Could not safely Ctrl-click a stash tablet."); return; }
            stashStage_ = StashStage::WaitPull; stashPullRetries_ = 0; stashActionAt_ = now;
            status_ = "Withdrawing selected identified tablets from stash...";
            c->Inventory.Scan(1); return;
        }
        if (stashStage_ == StashStage::WaitPull) {
            const bool inventoryUnchanged = bag == stashExpectedBag_;
            const auto retryUnchangedWithdrawal = [&]() {
                if (!inventoryUnchanged || stashPullRetries_ != 0 || !stashPendingItem_) return false;
                const auto page = MatchVisibleStashPage(*stashTargets_, c->Inventory.GetAll(),
                    TabletCategory(*stashPendingItem_));
                const auto stillPresent = std::find_if(page.inventory.Items.begin(), page.inventory.Items.end(),
                    [&](const auto& entry) { return entry.Address == stashPendingItem_->address; });
                const auto retryPoint = stillPresent == page.inventory.Items.end()
                    ? std::nullopt
                    : TabletStashSlotCenter(*stashTargets_, stillPresent->SlotX, stillPresent->SlotY);
                if (page.state != TabletStashPageMatchState::Matched || !retryPoint ||
                    !Click(c, *retryPoint, true, 0, kStashTabletHoverMs)) return false;
                ++stashPullRetries_;
                stashBagStamp_ = snapshot.ScanStamp;
                stashActionAt_ = now;
                status_ = "No inventory change yet; safely retrying the same stash Ctrl-click once...";
                c->Inventory.Scan(1);
                return true;
            };
            if (snapshot.ScanStamp == stashBagStamp_) {
                if (now - stashActionAt_ < std::chrono::milliseconds(585)) { c->Inventory.Scan(1); return; }
                if (retryUnchangedWithdrawal()) return;
                if (now - stashActionAt_ < std::chrono::milliseconds(2250)) {
                    status_ = "Waiting for POEFixer to confirm the stash withdrawal...";
                    c->Inventory.Scan(1);
                    return;
                }
                Stop(stashPullRetries_ == 0
                    ? "Stash withdrawal was not confirmed and could not be safely retried. Check the inventory before restarting."
                    : "Stash withdrawal was not confirmed after one safe retry. Check the inventory before restarting.");
                return;
            }
            if (bag.size() != stashExpectedBag_.size() + 1) {
                if (now - stashActionAt_ < std::chrono::milliseconds(585)) {
                    c->Inventory.Scan(1);
                    return;
                }
                if (retryUnchangedWithdrawal()) return;
                if (now - stashActionAt_ < std::chrono::milliseconds(2250)) {
                    status_ = "Waiting for POEFixer to confirm the stash withdrawal...";
                    c->Inventory.Scan(1);
                    return;
                }
                Stop(inventoryUnchanged
                    ? (stashPullRetries_ == 0
                        ? "Stash Ctrl-click did not transfer the tablet; no safe retry could be confirmed. Check the inventory."
                        : "Stash Ctrl-click did not transfer the tablet after one safe retry. Check the game input and inventory.")
                    : "Inventory changed unexpectedly during stash withdrawal. Check it before restarting.");
                return;
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
            stashExpectedBag_ = bag; stashPendingItem_.reset(); stashPullRetries_ = 0; stashStage_ = StashStage::ReadPage;
            stashActionAt_ = now; return;
        }

        if (stashStage_ == StashStage::Deposit || stashStage_ == StashStage::WaitDeposit) {
            if (!stashDepositInit_) { stashExpectedBag_ = bag; stashDepositInit_ = true; }
            if (stashStage_ == StashStage::WaitDeposit) {
                if (snapshot.ScanStamp == stashBagStamp_) {
                    if (now - stashActionAt_ < std::chrono::milliseconds(2250)) {
                        c->Inventory.Scan(1); return;
                    }
                    Stop("Stash Ctrl-click did not return the tablet. Check the stash and inventory before restarting.");
                    return;
                }
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
                    if (it == bag.end() || it->second != identity) {
                        Stop("Inventory changed during stash deposit."); return;
                    }
                }
                stashSessionAddresses_.erase(address);
                stashExpectedBag_ = bag; stashDepositItem_.reset(); stashStage_ = StashStage::Deposit;
                stashActionAt_ = now;
            }

            const auto tablet = std::find_if(snapshot.Value.Items.begin(), snapshot.Value.Items.end(), [&](const auto& entry) {
                if (!bag.contains(entry.Address) || !LooksLikeTablet(entry.Path, entry.BaseTypeName)) return false;
                if (!stashDepositIneligibleOnly_) return true;
                const auto identity = bag.find(entry.Address);
                return identity == bag.end() ||
                    !ReforgeCandidate(identity->second, stashRarityMask_, stashTypeRules_, false);
            });
            if (tablet == snapshot.Value.Items.end()) {
                if (stashDepositIneligibleOnly_) {
                    stashDepositIneligibleOnly_ = false;
                    stashDepositInit_ = false;
                    stashExpectedBag_ = bag;
                    stashTabSelected_ = true;
                    stashStage_ = StashStage::NavigateCategory;
                    stashActionAt_ = now;
                    status_ = "Filtered-out tablets returned from your inventory. Continuing the stash scan...";
                } else if (stashFromExhaustion_) {
                    stashStage_ = StashStage::Finished; stashCycle_ = false; stashInputMode_ = false;
                    status_ = "Finished: all tablets, results, and leftovers returned to the Tablet stash.";
                } else {
                    if (stashCategory_ >= stashCategories_.size()) stashCategory_ = stashPage_ = 0;
                    stashUiCategory_ = stashUiPage_ = -1;
                    stashTabSelected_ = true; stashStage_ = StashStage::NavigateCategory;
                    stashActionAt_ = now;
                    status_ = "All inventory tablets returned. Continuing the selected stash scan...";
                }
                return;
            }

            // Ctrl-clicking a tablet from the inventory lets the game place it
            // automatically in the open Tablet stash. No category or destination
            // page scan is needed for deposits.
            const auto rect = ResolveItemRect(snapshot.Value, *tablet, HasItemRects(snapshot.Value), true,
                static_cast<float>(width_), static_cast<float>(height_));
            if (!rect || !Click(c, {static_cast<LONG>(rect->x + rect->w / 2),
                static_cast<LONG>(rect->y + rect->h / 2)}, true)) {
                Stop("Could not safely Ctrl-click an inventory tablet into the Tablet stash."); return;
            }
            stashExpectedBag_ = bag; stashDepositItem_ = tablet->Address; stashBagStamp_ = snapshot.ScanStamp;
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
    static std::string WideToUtf8(const std::wstring& value) {
        if (value.empty()) return {};
        const int length = static_cast<int>(value.size());
        const int bytes = ::WideCharToMultiByte(CP_UTF8, 0, value.data(), length, nullptr, 0, nullptr, nullptr);
        if (bytes <= 0) return {};
        std::string result(static_cast<size_t>(bytes), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, value.data(), length, result.data(), bytes, nullptr, nullptr);
        return result;
    }
    void CaptureStashDiagnosticsImpl(const PluginSDK::Context* c, const std::filesystem::path& directory) {
        if (!c) return;
        try {
            const auto config = directory / "config";
            std::filesystem::create_directories(config);
            std::ofstream out(config / "tablet-reforge-diagnostics.txt", std::ios::trunc);
            if (!out) { status_ = "Could not write config/tablet-reforge-diagnostics.txt."; return; }
            out << "TabletReforge 0.6.9 diagnostics\n"
                << "Captured status: " << status_ << "\n"
                << "Controller phase=" << static_cast<int>(phase_)
                << " stash_stage=" << static_cast<int>(stashStage_)
                << " stash_cycle=" << stashCycle_ << " running=" << Running()
                << " pending=" << pending_ << "\n"
                << "stash_category_index=" << stashCategory_ << " stash_page_index=" << stashPage_
                << " tracked_session_items=" << stashSessionAddresses_.size()
                << " selected_categories=" << stashCategories_.size()
                << " rarity_mask=" << static_cast<int>(stashRarityMask_) << "\n"
                << "Screen rectangles are client pixels. Entity screen coordinates are projections.\n\n";

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
                        << " sanctified_text_marker=" << identity.sanctified
                        << " raw_identified=" << item.IsIdentified << " raw_rarity=" << item.Rarity
                        << " category=" << ClassifyFromText(item.Path, item.BaseTypeName)
                        << " path=" << item.Path << " base=" << item.BaseTypeName << '\n';
                }
            }

            out << "\nWorld entities (diagnostic snapshot):\n";
            const auto world = c->Game.GetSnapshot();
            out << "area=" << world.CurrentAreaName << " entity_count=" << world.Entities.size()
                << " game_window_foreground=" << world.GameWindowForeground << "\n";
            for (const auto& entity : world.Entities) {
                float sx = 0, sy = 0;
                const auto entityPath = WideToUtf8(entity.Path);
                const bool targetable = entity.Components.Targetable &&
                    c->Components.ReadTargetable(entity.Components.Targetable).IsTargetable;
                const bool projected = c->Render.WorldToScreen(entity.WorldX, entity.WorldY,
                    entity.WorldZ + entity.ModelBoundsZ, sx, sy);
                out << "entity id=" << entity.Id
                    << " type=" << static_cast<int>(entity.EntityType)
                    << " subtype=" << static_cast<int>(entity.EntitySubtype)
                    << " state=" << static_cast<int>(entity.EntityState)
                    << " valid=" << entity.IsValid << " targetable=" << targetable
                    << " targetable_component=" << entity.Components.Targetable
                    << " world=" << entity.WorldX << ',' << entity.WorldY << ',' << entity.WorldZ
                    << " screen_valid=" << projected << " screen=" << sx << ',' << sy
                    << " entity_path=[" << entityPath << "] targetable_path=[" << entity.TgtPath << "]\n";
            }
            const auto stashObjects = c->Game.EnumerateStashObjects();
            out << "\nStash objects available=" << stashObjects.has_value() << '\n';
            if (stashObjects) for (const auto& object : *stashObjects) {
                float sx = 0, sy = 0;
                const bool projected = c->Render.WorldToScreen(object.WorldX, object.WorldY, object.WorldZ, sx, sy);
                out << "stash entity=" << object.EntityId << " domain=" << static_cast<int>(object.Domain)
                    << " rank=" << object.Rank << " world=" << object.WorldX << ',' << object.WorldY
                    << ',' << object.WorldZ << " screen_valid=" << projected << " screen=" << sx << ',' << sy
                    << " path=[" << object.Path << "]\n";
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
            status_ = "Diagnostics saved to config/tablet-reforge-diagnostics.txt.";
        } catch (...) {
            status_ = "Could not capture TabletReforge diagnostics.";
        }
    }
public:
    ~TabletReforgeController() { StopDebugCapture(); ReleaseInput(); }
    bool DebugCaptureActive() const { return debugRecording_; }
    void StartDebugCapture(const std::filesystem::path& directory) {
        StopDebugCapture();
        try {
            const auto config = directory / "config";
            std::filesystem::create_directories(config);
            debugLog_.open(config / "tablet-reforge-live-debug.txt", std::ios::out | std::ios::trunc);
            if (!debugLog_) { status_ = "Could not open config/tablet-reforge-live-debug.txt."; return; }
            debugStarted_ = Clock::now();
            debugNextSample_ = debugStarted_;
            debugRecording_ = true;
            debugLog_ << "TabletReforge live debug capture 0.9.1\n"
                << "Started manually from Diagnostics. Samples every 500 ms until Stop is pressed.\n";
            status_ = "Debug recording started. Reproduce the issue, then press Stop recording.";
        } catch (...) {
            debugRecording_ = false;
            status_ = "Could not start live debug recording.";
        }
    }
    void StopDebugCapture() {
        if (debugLog_.is_open()) {
            debugLog_ << "\nCapture stopped manually.\n";
            debugLog_.flush();
            debugLog_.close();
        }
        const bool wasRecording = debugRecording_;
        debugRecording_ = false;
        if (wasRecording) status_ = "Debug recording saved to config/tablet-reforge-live-debug.txt.";
    }
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
        if (clipboardSnapshotValid_) {
            const DWORD current = ::GetClipboardSequenceNumber();
            if (current != clipboardSequenceBeforeCopy_) RestoreClipboardSnapshot(current);
            else { clipboardBackup_.clear(); clipboardSnapshotValid_ = false; }
        }
        clipboardVerified_ = false;
        ReleaseInput(); ReleaseWorldClick(); phase_ = Phase::Idle; pending_ = false; targets_.reset(); stashTargets_.reset();
        stashCycle_ = false; stashInputMode_ = false; stashStage_ = StashStage::Idle;
        stashInventorySeeded_ = false; stashDepositIneligibleOnly_ = false; stashFromExhaustion_ = false;
        stashPendingItem_.reset(); status_ = reason;
    }
    void Reset() { Stop("Open an empty Reforging Bench, then press Start reforging."); outlines_.clear(); labels_.clear(); displayedCount_ = 0; }
    void RequestStart(const PluginSDK::Context* c, bool all, int inputTransferDelayMs, int inputPulseMs) {
        if (Busy()) return;
        if (!all) { status_ = "Enable All tablets in inventory before starting."; return; }
        all_ = all; inputTransferDelayMs_ = FasterTransferDelayMs(inputTransferDelayMs);
        inputPulseMs_ = std::clamp(inputPulseMs, 12, 40); pending_ = true;
        area_ = c->Game.GetAreaChangeCounter(); window_ = c->Game.GetGameWindow();
        status_ = "Close the POEFixer menu to start. Esc cancels.";
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
        stashInventorySeeded_ = false; stashActionAt_ = Clock::now();
        stashInventoryId_ = 0; stashPageInventorySnapshot_.reset();
        stashSessionAddresses_.clear(); stashSkippedAddresses_.clear();
        stashTypeRules_ = typeRules;
        stashResumeForDeposit_ = false;
        stashRarityMask_ = rarityMask; stashCategory_ = stashPage_ = 0;
        stashTabSelected_ = false; stashPendingItem_.reset(); stashFromExhaustion_ = false;
        stashDepositIneligibleOnly_ = false; stashDepositInit_ = false; stashDepositItem_.reset();
        all_ = true; inputTransferDelayMs_ = FasterTransferDelayMs(transferDelayMs);
        inputPulseMs_ = std::clamp(pulseMs, 12, 40);
        area_ = c->Game.GetAreaChangeCounter(); window_ = c->Game.GetGameWindow();
        c->Overlay.SetEntityScanFlags(PluginSDK::EntityScan::StashObjects);
        const HWND gameWindow = c->Game.GetGameWindow();
        const HWND foreground = ::GetForegroundWindow();
        DWORD foregroundProcess = 0;
        if (foreground && foreground != gameWindow)
            ::GetWindowThreadProcessId(foreground, &foregroundProcess);
        bool closeRequested = false;
        if (foreground && foreground != gameWindow && foregroundProcess == ::GetCurrentProcessId()) {
            INPUT escape[2]{};
            escape[0].type = INPUT_KEYBOARD; escape[0].ki.wVk = VK_ESCAPE;
            escape[1].type = INPUT_KEYBOARD; escape[1].ki.wVk = VK_ESCAPE;
            escape[1].ki.dwFlags = KEYEVENTF_KEYUP;
            closeRequested = ::SendInput(2, escape, sizeof(INPUT)) == 2;
        }
        status_ = closeRequested
            ? "Closing the POEFixer window, then starting the stash cycle..."
            : "Starting the stash cycle. The stash will open automatically.";
    }
    void RequestCycleBench(const PluginSDK::Context* c) {
        if (!StashAwaitingBench()) return;
        stashStage_ = StashStage::Idle; stashInputMode_ = false;
        stashFromExhaustion_ = false;
        RequestStart(c, true, inputTransferDelayMs_, inputPulseMs_);
        if (pending_) status_ = "Close the POEFixer menu. The detected bench will open automatically. Esc cancels.";
    }
    void RequestCycleStash(const PluginSDK::Context* c) {
        if (!StashAwaitingStash()) return;
        stashInputMode_ = true; stashTabSelected_ = false;
        stashUiCategory_ = stashUiPage_ = -1; stashDepositInit_ = false;
        stashPageInventorySnapshot_.reset();
        stashResumeForDeposit_ = true;
        if (!stashFromExhaustion_) stashCategory_ = stashPage_ = 0;
        stashStage_ = StashStage::NavigateTab;
        area_ = c->Game.GetAreaChangeCounter(); window_ = c->Game.GetGameWindow();
        stashActionAt_ = Clock::now();
        status_ = "Reopening the Tablet stash automatically...";
    }
    void Tick(const PluginSDK::Context* c, bool all, uint8_t rarityMask,
              const std::vector<TabletHelperConfig::TypeConfig>& typeRules,
              const std::filesystem::path& directory) {
        RecordDebugSample(c);
        const auto now = Clock::now();
        if (Busy() && (::GetAsyncKeyState(VK_ESCAPE) & 0x8000)) { Stop("Stopped. Recover any tablets left in the bench before restarting."); return; }
        if (Busy() && (!c->Game.IsInGame() || c->Game.GetAreaChangeCounter() != area_ || c->Game.GetGameWindow() != window_)) {
            Stop("Stopped: game window or area changed."); return;
        }
        if (!c->Game.IsInGame()) { outlines_.clear(); labels_.clear(); displayedCount_ = 0; return; }
        if (stashCycle_ && stashStage_ != StashStage::Idle && stashStage_ != StashStage::AwaitBench &&
            stashStage_ != StashStage::AwaitStash && stashStage_ != StashStage::ForgePass &&
            stashStage_ != StashStage::Finished) {
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
        const int phaseDelayMs = phase_ == Phase::WaitForge ? kForgeResultReadyPauseMs
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
                    PauseCycleForStash("No compatible trio in the inventory. Returning tablets to stash...");
                    return;
                }
                Stop(message.c_str());
                return;
            }
            phase_ = Phase::Transfer;
        }
        if (phase_ == Phase::Transfer) {
            if (!SameBagContents(bag, expected_)) { Stop("Stopped: inventory changed outside the sequence."); return; }
            const auto target = batches_[batch_][item_];
            const auto item = std::find_if(inv.Items.begin(), inv.Items.end(), [&](const auto& i) { return i.Address == target.address; });
            const auto current = bag.find(target.address);
            if (item == inv.Items.end() || current == bag.end() || !SameTabletIdentity(current->second, target) || !ReforgeCandidate(current->second, rarityMask, typeRules)) { Stop("Stopped: selected tablet is missing or no longer eligible."); return; }
            const auto rect = ResolveItemRect(inv, *item, HasItemRects(inv), true, static_cast<float>(width_), static_cast<float>(height_));
            if (!rect || !Click(c, {static_cast<LONG>(rect->x + rect->w / 2), static_cast<LONG>(rect->y + rect->h / 2)}, true, inputPulseMs_)) {
                Stop("Stopped: input was not sent. Release modifier keys and mouse buttons."); return;
            }
            status_ = "Reforging batch " + std::to_string(batch_ + 1) + " / " + std::to_string(batches_.size());
            Wait(c, Phase::WaitTransfer, snapshot.ScanStamp); return;
        }
        if (phase_ == Phase::WaitTransfer) {
            auto desired = expected_; desired.erase(batches_[batch_][item_].address);
            if (SameBagContents(bag, expected_)) return;
            if (!SameBagContents(bag, desired)) { Stop("Stopped: unexpected item transfer. Check the bench."); return; }
            expected_ = std::move(desired);
            phase_ = ++item_ == 3 ? Phase::Forge : Phase::Transfer;
            actionAt_ = now; deadline_ = now + std::chrono::seconds(6); return;
        }
        if (phase_ == Phase::Forge) {
            const auto& button = targets_->button;
            if (!SameBagContents(bag, expected_) || !Click(c, {static_cast<LONG>(button.CenterX()), static_cast<LONG>(button.CenterY())}, false)) {
                Stop("Stopped before reforging: inventory or input changed."); return;
            }
            Wait(c, Phase::WaitForge, snapshot.ScanStamp); return;
        }
        if (phase_ == Phase::WaitForge) {
            const auto& output = targets_->output;
            if (!SameBagContents(bag, expected_) || !Click(c, {static_cast<LONG>(output.CenterX()), static_cast<LONG>(output.CenterY())}, true)) {
                Stop("Stopped before collecting the result."); return;
            }
            status_ = "Picking up reforge result...";
            Wait(c, Phase::WaitResult, snapshot.ScanStamp); return;
        }
        if (phase_ == Phase::WaitResult) {
            if (SameBagContents(bag, expected_)) return;
            auto additions = bag;
            for (const auto& [address, identity] : expected_) {
                const auto item = additions.find(address);
                if (item == additions.end() || !SameTabletIdentity(item->second, identity)) { Stop("Stopped: unexpected inventory contents."); return; }
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
                    PauseCycleForStash("Reforge pass complete. Returning results to the Tablet stash...");
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

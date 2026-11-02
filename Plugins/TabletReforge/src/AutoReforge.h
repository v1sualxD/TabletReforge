#pragma once
#include "ReforgePlan.h"
#include "ItemRects.h"
#include "TabletMatch.h"
#include <Windows.h>
#include <chrono>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace TabletHelper {
// Render-thread state machine. No sleeps, game-memory writes, packets or private offsets.
// Calibration is session-only: a recreated UI or changed viewport requires F6/F7 again.
class AutoReforge {
    using Clock = std::chrono::steady_clock;
    enum class Phase { Idle, InitialScan, Select, Transfer, WaitTransfer, Forge, WaitForge, WaitResult };
    struct Anchor { uintptr_t address = 0; int child = -1; };
    Phase phase_ = Phase::Idle;
    std::vector<Anchor> panelPath_;
    ScreenRect panel_{};
    POINT button_{}, output_{};
    HWND window_ = nullptr;
    LONG width_ = 0, height_ = 0;
    uint64_t area_ = 0, stamp_ = 0;
    bool outputSet_ = false;
    bool f6Down_ = false, f7Down_ = false;
    bool startRequested_ = false;
    FilterConfig keepFilter_;
    std::vector<std::string> enabledTypes_;
    std::vector<ReforgeItem> selection_, rejected_;
    size_t selectionIndex_ = 0;
    Clock::time_point requestExpires_{};
    bool emptyConfirmed_ = false;
    std::vector<ReforgeBatch> batches_;
    size_t batch_ = 0, item_ = 0;
    std::map<uintptr_t, ReforgeItem> expected_;
    Clock::time_point actionAt_{}, deadline_{}, nextScan_{};
    std::string status_ = "Desactive. Activer puis calibrer F6 / F7.";

    static bool Edge(int key, bool& previous) {
        const bool down = (::GetAsyncKeyState(key) & 0x8000) != 0;
        const bool result = down && !previous;
        previous = down;
        return result;
    }
    static ReforgeItem Identity(const PluginSDK::InventoryItem& i) {
        return {i.Address, i.Path, i.Rarity, i.ItemLevel, i.SlotX, i.SlotY, i.IsIdentified, i.IsCorrupted};
    }
    static std::map<uintptr_t, ReforgeItem> Bag(const PluginSDK::Inventory& inv) {
        std::map<uintptr_t, ReforgeItem> result;
        for (const auto& i : inv.Items) if (i.Address) result.emplace(i.Address, Identity(i));
        return result;
    }
    static bool Contains(const ScreenRect& r, POINT p) {
        return p.x >= r.x && p.y >= r.y && p.x < r.x + r.w && p.y < r.y + r.h;
    }
    static bool Rect(const PluginSDK::Context* c, uintptr_t a, ScreenRect& r) {
        return c->Ui.ComputeScreenRect(a, r.x, r.y, r.w, r.h) &&
            std::isfinite(r.x) && std::isfinite(r.y) && std::isfinite(r.w) &&
            std::isfinite(r.h) && r.w > 0 && r.h > 0;
    }
    bool Foreground(const PluginSDK::Context* c) const {
        return c->Game.IsInGame() && !c->Game.IsMenuVisible() &&
            c->Game.GetGameWindow() && ::GetForegroundWindow() == c->Game.GetGameWindow();
    }
    bool Ready(const PluginSDK::Context* c) const {
        if (!Foreground(c) || c->Game.GetGameWindow() != window_ || panelPath_.empty() ||
            c->Game.GetAreaChangeCounter() != area_ || c->Game.IsHostAutomationActive()) return false;
        RECT client{};
        if (!::GetClientRect(window_, &client) || client.right != width_ || client.bottom != height_) return false;
        uintptr_t parent = c->Ui.GetGameUiRoot();
        if (parent != panelPath_.front().address) return false;
        for (size_t i = 0; i < panelPath_.size(); ++i) {
            const auto& node = panelPath_[i];
            if (i && c->Ui.GetChildAt(parent, node.child) != node.address) return false;
            if (!c->Ui.IsVisible(node.address)) return false;
            parent = node.address;
        }
        ScreenRect current;
        if (!Rect(c, parent, current)) return false;
        return std::abs(current.x - panel_.x) < 2 && std::abs(current.y - panel_.y) < 2 &&
            std::abs(current.w - panel_.w) < 2 && std::abs(current.h - panel_.h) < 2;
    }
    // Search only visible UI branches under the cursor; never guess a fixed child index.
    void FindPanel(const PluginSDK::Context* c, uintptr_t node, POINT p,
                   std::vector<Anchor>& path, int depth, int& budget,
                   std::vector<Anchor>& best, ScreenRect& bestRect) {
        if (!node || depth > 24 || --budget < 0 || !c->Ui.IsVisible(node)) return;
        ScreenRect r;
        if (Rect(c, node, r) && Contains(r, p) && r.w >= 240 && r.h >= 250 &&
            r.w * r.h < static_cast<float>(width_) * static_cast<float>(height_) * 0.8f &&
            (best.empty() || r.w * r.h < bestRect.w * bestRect.h)) {
            best = path;
            bestRect = r;
        }
        const auto children = c->Ui.GetChildren(node);
        for (size_t i = 0; i < children.size() && budget > 0; ++i) {
            path.push_back({children[i], static_cast<int>(i)});
            FindPanel(c, children[i], p, path, depth + 1, budget, best, bestRect);
            path.pop_back();
        }
    }
    void Calibrate(const PluginSDK::Context* c) {
        Reset();
        window_ = c->Game.GetGameWindow();
        RECT client{};
        POINT cursor{};
        if (!::GetClientRect(window_, &client) || !::GetCursorPos(&cursor) ||
            !::ScreenToClient(window_, &cursor)) return;
        width_ = client.right; height_ = client.bottom;
        area_ = c->Game.GetAreaChangeCounter();
        button_ = cursor;
        std::vector<Anchor> path{{c->Ui.GetGameUiRoot(), -1}};
        int budget = 4000;
        FindPanel(c, path.front().address, cursor, path, 0, budget, panelPath_, panel_);
        status_ = panelPath_.empty() ? "Panneau non detecte. Replacer la souris sur Reforger puis F6." :
            "Bouton calibre. Survoler le resultat vide puis F7.";
    }
    bool Click(const PluginSDK::Context* c, POINT point, bool ctrl) {
        if (!Ready(c) || point.x < 0 || point.y < 0 || point.x >= width_ || point.y >= height_) return false;
        // Do not interfere with a key or mouse button held by the player.
        for (int key : {VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN, VK_RWIN, VK_LBUTTON, VK_RBUTTON})
            if (::GetAsyncKeyState(key) & 0x8000) return false;
        POINT screen = point;
        if (!::ClientToScreen(window_, &screen)) return false;
        const int sx = ::GetSystemMetrics(SM_XVIRTUALSCREEN), sy = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int sw = ::GetSystemMetrics(SM_CXVIRTUALSCREEN), sh = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if (sw < 2 || sh < 2) return false;
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
        // A partial submission must never leave our Ctrl or mouse button held.
        INPUT release[2]{};
        release[0].type = INPUT_MOUSE; release[0].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        release[1].type = INPUT_KEYBOARD; release[1].ki.wVk = VK_LCONTROL; release[1].ki.dwFlags = KEYEVENTF_KEYUP;
        ::SendInput(ctrl ? 2u : 1u, release, sizeof(INPUT));
        return false;
    }
    void Wait(const PluginSDK::Context* c, Phase next, uint64_t stamp) {
        phase_ = next; stamp_ = stamp; actionAt_ = Clock::now();
        deadline_ = actionAt_ + std::chrono::seconds(6);
        nextScan_ = actionAt_;
        c->Inventory.Scan(1);
    }
public:
    bool Running() const { return phase_ != Phase::Idle; }
    bool Pending() const { return startRequested_; }
    bool Busy() const { return Running() || Pending(); }
    bool Calibrated() const { return !panelPath_.empty() && outputSet_; }
    const std::string& Status() const { return status_; }
    bool& EmptyConfirmed() { return emptyConfirmed_; }
    void RequestStart(const FilterConfig& keepFilter, const std::vector<std::string>& types) {
        if (Busy()) return;
        if (!Calibrated() || !emptyConfirmed_ || !keepFilter.readMods || types.empty()) {
            status_ = "Calibrer, confirmer le reforger vide, activer la lecture des mods et choisir des categories.";
            return;
        }
        keepFilter_ = keepFilter;
        enabledTypes_ = types;
        startRequested_ = true;
        requestExpires_ = Clock::now() + std::chrono::seconds(30);
        status_ = "Pret : fermer le menu POEFixer pour lancer (30 s). Echap pour annuler.";
    }
    void Stop(const char* reason) {
        phase_ = Phase::Idle; startRequested_ = false; emptyConfirmed_ = false;
        status_ = reason;
    }
    void Reset() {
        Stop("Arrete. Calibrer F6 puis F7 dans le reforger.");
        panelPath_.clear(); outputSet_ = false; batches_.clear(); expected_.clear();
    }
    void Tick(const PluginSDK::Context* c, bool enabled, int delayMs) {
        const bool f6 = Edge(VK_F6, f6Down_), f7 = Edge(VK_F7, f7Down_);
        if (!enabled) { if (Busy()) Stop("Auto reforge desactive."); return; }
        if (Busy() && (::GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
            Stop("Arrete. Recuperer manuellement les objets restes au reforger."); return;
        }
        if (Pending() && Clock::now() > requestExpires_) { Stop("Lancement annule : delai de 30 s depasse."); return; }
        if (!Foreground(c)) { if (Running()) Stop("Arrete : jeu hors focus ou menu ouvert."); return; }
        if (!Running() && f6) { Calibrate(c); return; }
        if (!Running() && f7) {
            POINT p{};
            if (!Ready(c) || !::GetCursorPos(&p) || !::ScreenToClient(window_, &p) || !Contains(panel_, p) ||
                (std::abs(p.x - button_.x) < 25 && std::abs(p.y - button_.y) < 25)) {
                status_ = "Resultat invalide : calibrer F6 puis survoler son emplacement et F7."; return;
            }
            output_ = p; outputSet_ = true;
            status_ = "Calibre. Confirmer le reforger vide puis cliquer sur Lancer la reforge."; return;
        }
        if (!Running() && startRequested_) {
            startRequested_ = false;
            if (!Calibrated() || !Ready(c) || !emptyConfirmed_) {
                status_ = "Calibrer F6/F7 et confirmer le reforger vide dans les options."; return;
            }
            batch_ = item_ = 0;
            const auto snapshot = c->Inventory.ReadSnapshot(1);
            if (!snapshot.Supported) { Stop("Hote incompatible : snapshots d'inventaire indisponibles."); return; }
            Wait(c, Phase::InitialScan, snapshot.ScanStamp);
            status_ = "Lecture du sac et des filtres de conservation...";
            return;
        }
        if (!Running()) return;
        if (!Ready(c)) { Stop("Arrete : panneau ferme, resolution/zone modifiee ou automatisation concurrente."); return; }
        const auto now = Clock::now();
        if (now > deadline_) { Stop("Arrete : aucune confirmation sous 6 s. Verifier le reforger."); return; }
        if (now >= nextScan_) { c->Inventory.Scan(1); nextScan_ = now + std::chrono::milliseconds(100); }
        if (now - actionAt_ < std::chrono::milliseconds(std::clamp(delayMs, 250, 2000))) return;
        const auto snapshot = c->Inventory.ReadSnapshot(1);
        if (!snapshot.Supported || !snapshot.Ready || snapshot.ScanStamp == stamp_) return;
        if (snapshot.AreaChangeCounter != area_) { Stop("Arrete : inventaire d'une autre zone."); return; }
        const auto& inv = snapshot.Value;
        if (inv.TotalBoxesX != 12 || inv.TotalBoxesY != 5 || !GridDrawn(inv, static_cast<float>(width_), static_cast<float>(height_))) {
            Stop("Arrete : sac 12 x 5 non visible ou geometrie indisponible."); return;
        }
        const auto bag = Bag(inv);
        if (phase_ == Phase::InitialScan) {
            selection_.clear(); rejected_.clear(); selectionIndex_ = 0;
            for (const auto& [address, identity] : bag) {
                (void)address;
                if (identity.identified && !identity.corrupted && identity.rarity >= 0 && identity.rarity <= 2 &&
                    ContainsCI(identity.path, "toweraugment")) selection_.push_back(identity);
            }
            expected_ = bag; phase_ = Phase::Select;
            deadline_ = now + std::chrono::seconds(30);
        }
        if (phase_ == Phase::Select) {
            if (bag != expected_) { Stop("Arrete : sac modifie pendant la selection."); return; }
            // Bound expensive modifier reads per rendered frame. Invalid reads are protected.
            for (int budget = 4; budget > 0 && selectionIndex_ < selection_.size(); --budget) {
                const auto& candidate = selection_[selectionIndex_++];
                const auto mods = c->Inventory.ReadItemMods(candidate.address);
                if (!mods.Valid) continue;
                const auto parsed = ParseWithMods(candidate.path, "", mods);
                const bool categoryEnabled = std::find(enabledTypes_.begin(), enabledTypes_.end(), parsed.typeKey) != enabledTypes_.end();
                const bool keep = EvaluateTablet(keepFilter_, parsed).matched;
                if (CanReforgeRejected(candidate.identified, parsed.modsRead, IsRealType(parsed.typeKey), categoryEnabled, keep))
                    rejected_.push_back(candidate);
            }
            status_ = "Selection des rejets : " + std::to_string(selectionIndex_) + "/" + std::to_string(selection_.size());
            if (selectionIndex_ < selection_.size()) return;
            batches_ = PlanReforge(std::move(rejected_));
            if (batches_.empty()) { Stop("Termine : aucun trio de tablettes rejetees par les filtres."); return; }
            phase_ = Phase::Transfer; deadline_ = now + std::chrono::seconds(6);
        }
        if (phase_ == Phase::Transfer) {
            if (bag != expected_) { Stop("Arrete : inventaire modifie en dehors de la sequence."); return; }
            const auto& target = batches_[batch_][item_];
            const auto found = std::find_if(inv.Items.begin(), inv.Items.end(), [&](const auto& i) { return Identity(i) == target; });
            if (found == inv.Items.end()) { Stop("Arrete : tablette attendue absente."); return; }
            // Re-read immediately before consuming: missing data always stops the sequence.
            const auto mods = c->Inventory.ReadItemMods(found->Address);
            if (!mods.Valid) { Stop("Arrete : mods illisibles avant transfert."); return; }
            const auto parsed = ParseWithMods(found->Path, found->BaseTypeName, mods);
            if (!CanReforgeRejected(found->IsIdentified, parsed.modsRead, IsRealType(parsed.typeKey),
                std::find(enabledTypes_.begin(), enabledTypes_.end(), parsed.typeKey) != enabledTypes_.end(),
                EvaluateTablet(keepFilter_, parsed).matched)) {
                Stop("Arrete : tablette protegee par les filtres ou categorie exclue."); return;
            }
            const auto r = ResolveItemRect(inv, *found, HasItemRects(inv), true, static_cast<float>(width_), static_cast<float>(height_));
            if (!r || !Click(c, {static_cast<LONG>(r->x + r->w / 2), static_cast<LONG>(r->y + r->h / 2)}, true)) {
                Stop("Arrete : clic non envoye. Relacher les touches et verifier le jeu."); return;
            }
            status_ = "Lot " + std::to_string(batch_ + 1) + "/" + std::to_string(batches_.size()) + " : transfert...";
            Wait(c, Phase::WaitTransfer, snapshot.ScanStamp); return;
        }
        if (phase_ == Phase::WaitTransfer) {
            auto desired = expected_;
            desired.erase(batches_[batch_][item_].address);
            if (bag == expected_) return;
            if (bag != desired) { Stop("Arrete : transfert inattendu. Verifier les emplacements du reforger."); return; }
            expected_ = std::move(desired);
            phase_ = ++item_ == 3 ? Phase::Forge : Phase::Transfer;
            actionAt_ = now; deadline_ = now + std::chrono::seconds(6); return;
        }
        if (phase_ == Phase::Forge) {
            if (bag != expected_ || !Click(c, button_, false)) { Stop("Arrete avant reforge : inventaire ou entree modifie."); return; }
            Wait(c, Phase::WaitForge, snapshot.ScanStamp); return;
        }
        if (phase_ == Phase::WaitForge) {
            if (bag != expected_ || !Click(c, output_, true)) { Stop("Arrete avant recuperation du resultat."); return; }
            Wait(c, Phase::WaitResult, snapshot.ScanStamp); return;
        }
        if (phase_ == Phase::WaitResult) {
            if (bag == expected_) return;
            auto additions = bag;
            for (const auto& [address, identity] : expected_) {
                const auto i = additions.find(address);
                if (i == additions.end() || i->second != identity) { Stop("Arrete : contenu du sac inattendu."); return; }
                additions.erase(i);
            }
            if (additions.size() != 1 || additions.begin()->second.path != batches_[batch_][0].path ||
                additions.begin()->second.rarity != batches_[batch_][0].rarity) {
                Stop("Arrete : resultat inattendu. Verifier le reforger avant de relancer."); return;
            }
            expected_ = bag;
            if (++batch_ == batches_.size()) {
                const auto count = batches_.size();
                Stop("Termine."); status_ = "Termine : " + std::to_string(count) + " reforge(s). Resultats conserves."; return;
            }
            item_ = 0; phase_ = Phase::Transfer;
            actionAt_ = now; deadline_ = now + std::chrono::seconds(6);
        }
    }
};
} // namespace TabletHelper

// Tablet Helper - POEFixer plugin (SDK v6).
// Originally created by Omer Faruk ARPA (GPL-3.0); maintained by POEFixer.

#include "TabletHelperPlugin.h"   // PluginSDK.h
#include "lang_en.generated.h"    // the embedded English: once per DLL, after PluginSDK.h

#include "../../../POEFixer/imgui/imgui.h"

#include <algorithm>
#include <cstring>
#include <iterator>

// Plugin info: the host checks it before CreatePlugin; the publish tool reads it.
POEFIXER_PLUGIN_INFO(
    "TabletReforge",
    "Tablet Reforge",
    "0.1.0",
    "TabletReforge contributors; Omer Faruk ARPA; POEFixer",
    "Highlights tablets to keep and reforges rejected identified tablets in compatible batches of three.")

namespace TabletHelper {
const char* EnglishText(const char* key) {
    if (!key) return "";
    // kPluginLangEn is sorted by key (tools/plugin_i18n/gen.py).
    const auto* begin = std::begin(kPluginLangEn);
    const auto* end = std::end(kPluginLangEn);
    const auto* it = std::lower_bound(begin, end, key, [](const PluginSDK::I18n::Entry& e, const char* k) {
        return std::strcmp(e.key, k) < 0;
    });
    return (it != end && std::strcmp(it->key, key) == 0) ? it->text : key;
}
} // namespace TabletHelper

TabletHelperPlugin::~TabletHelperPlugin() {
    // The host normally calls OnDisable first; this covers an unload without it.
    if (m_worker) m_worker->Stop();
}

void TabletHelperPlugin::OnEnable(bool /*isGameAttached*/) {
    if (!HostCompatible()) {
        ctx()->Log.Error("Tablet Helper: this POEFixer version is not compatible with the plugin; it stays inactive.");
        return;
    }
    if (ctx()->ImGuiContext) ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));

    if (!m_settings.Load(DirectoryPath()))
        ctx()->Log.Warn("Tablet Helper: config/settings.json could not be read; defaults are used (a copy was kept as settings.json.corrupt).");
    m_loaded = true;
    ctx()->Log.Info(m_ranges.Load(DirectoryPath()).c_str());

    if (!m_worker) m_worker = std::make_unique<TabletHelper::TabletWorker>(ctx());
    PushFilter();
    m_worker->Start();
    m_renderer.Reset();
    m_running = true;
}

void TabletHelperPlugin::OnDisable() {
    m_running = false;
    m_autoReforge.Reset();
    if (m_worker) m_worker->Stop();
    m_renderer.Reset();
    SaveIfDue(true);
}

void TabletHelperPlugin::SaveSettings() { SaveIfDue(true); }

void TabletHelperPlugin::MarkChanged() {
    if (m_autoReforge.Busy()) m_autoReforge.Stop("Arrete : reglages modifies. Relancer avec les nouveaux filtres.");
    m_filterDirty = true;
    m_saveDirty = true;
    m_saveDue = Clock::now() + std::chrono::seconds(1);
}

void TabletHelperPlugin::PushFilter() {
    m_filterDirty = false;
    if (!m_worker) return;
    auto f = std::make_shared<TabletHelper::FilterConfig>();
    f->readMods = m_settings.readMods;
    f->types = m_settings.ActiveTypes();
    m_worker->SetFilter(std::move(f), m_settings.scanIntervalMs);
}

void TabletHelperPlugin::SaveIfDue(bool force) {
    if (!m_loaded || !m_saveDirty) return;
    if (!force && Clock::now() < m_saveDue) return;
    m_saveDirty = false;
    if (!m_settings.Save(DirectoryPath()))
        ctx()->Log.Warn("Tablet Helper: could not write config/settings.json.");
}

void TabletHelperPlugin::DrawUI() {
    if (!m_running || !m_worker || !ctx()->ImGuiContext) return;
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
    if (m_filterDirty) PushFilter();
    SaveIfDue(false);

    m_autoReforge.Tick(ctx(), m_settings.autoReforgeEnabled, m_settings.autoReforgeDelayMs);
    if (m_settings.autoReforgeEnabled && ctx()->Game.IsInGame() && ctx()->Game.IsForeground()) {
        ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.8f);
        ImGui::Begin("Auto reforge###TabletReforgeStatus", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
        ImGui::TextUnformatted(m_autoReforge.Status().c_str());
        if (m_autoReforge.Busy()) ImGui::TextUnformatted("Echap : arreter");
        ImGui::End();
    }

    // Cheap host scalars only; the worker does the reading.
    const bool active = ctx()->Game.IsInGame() && ctx()->Game.IsForeground();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const uint64_t area = ctx()->Game.GetAreaChangeCounter();
    m_worker->Heartbeat(active, display.x, display.y, area);
    if (!active) return;

    const auto frame = m_worker->Latest();
    if (!frame || frame->areaCounter != area || frame->items.empty()) return;
    // Over the game normally; behind the POEFixer window while it is open, so the
    // highlights never cover the menu.
    ImDrawList* dl = ctx()->Game.IsMenuVisible() ? ImGui::GetBackgroundDrawList() : ImGui::GetForegroundDrawList();
    m_renderer.Draw(dl, frame, m_settings);
}

extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin() { return new TabletHelperPlugin(); }
extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* p) { delete p; }

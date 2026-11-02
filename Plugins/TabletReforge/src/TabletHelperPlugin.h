#pragma once

// Tablet Helper: highlights Precursor Tablets in every open item window
// (inventory, stash tabs, the Tablet stash, guild stash, merchant), coloured per
// type and filtered by bonuses, roll values and uses left.
//
// Threads: the render thread owns the settings, the settings tab and drawing;
// TabletWorker reads the game and publishes immutable frames.

#include "HighlightRenderer.h"
#include "Settings.h"
#include "TabletRanges.h"
#include "TabletWorker.h"
#include "AutoReforge.h"
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace TabletHelper {
// The English text of a key of lang/en.json whatever the program language (the embedded
// English, defined next to it in TabletHelper.cpp); the key itself when it is not one.
const char* EnglishText(const char* key);
} // namespace TabletHelper

class TabletHelperPlugin : public PluginSDK::Plugin {
public:
    TabletHelperPlugin() = default;
    ~TabletHelperPlugin() override;

    const char* GetName() const override { return "Tablet Reforge"; }
    bool WantsOverlay() const override { return true; }
    void OnEnable(bool isGameAttached) override;
    void OnDisable() override;
    void DrawUI() override;
    void DrawSettings() override;
    void SaveSettings() override;

private:
    using Clock = std::chrono::steady_clock;

    // State changes from the settings tab: re-send the rules to the worker and
    // save about a second after the last edit.
    void MarkChanged();
    void PushFilter();
    void SaveIfDue(bool force);

    // Settings tab, one function per card (src/ui/SettingsUi.cpp).
    void DrawSectionStatus();
    void DrawSectionHighlight();
    void DrawSectionLabels();
    void DrawSectionDetection();
    void DrawSectionProfiles();
    void DrawSectionTypes();
    void DrawTypeEditor(TabletHelperConfig::TypeConfig& t);
    void DrawBonusTable(TabletHelperConfig::TypeConfig& t);
    const std::vector<const TabletHelper::Bonus*>& OrderedBonuses(const std::string& typeKey);
    bool DrawValueField(const char* id, int* v, int lo, int hi);
    void DrawSectionDiagnostics();

    TabletHelperConfig::Settings m_settings;
    TabletHelper::TabletRanges m_ranges;
    std::unique_ptr<TabletHelper::TabletWorker> m_worker;
    TabletHelper::HighlightRenderer m_renderer;
    TabletHelper::AutoReforge m_autoReforge;
    bool m_running = false;
    bool m_loaded = false;
    bool m_filterDirty = false;
    bool m_saveDirty = false;
    Clock::time_point m_saveDue{};

    // Settings-tab only state (not persisted).
    std::unordered_map<std::string, std::string> m_bonusSearch;  // per type key
    std::unordered_map<std::string, bool> m_onlySelected;        // per type key
    std::string m_openType;                                      // the type whose editor is open
    // Per type key: its bonuses by category, then by label in the program language;
    // rebuilt when PluginSDK::I18n::Revision() moves.
    std::unordered_map<std::string, std::vector<const TabletHelper::Bonus*>> m_bonusOrder;
    uint64_t m_bonusOrderRevision = ~0ull;
    // The last modifier-dump action, worded when drawn (in the program language).
    enum class DumpStatus { None, Appended, WriteFailed, Removed };
    DumpStatus m_dumpStatus = DumpStatus::None;
    std::string m_dumpPath;
    int m_dumpCount = 0;
};

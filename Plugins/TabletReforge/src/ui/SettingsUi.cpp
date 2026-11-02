// Tablet Helper settings tab (drawn under the host's Enabled header).
// One card per topic, one measured label column per card, widths from the
// available region, wrapped tooltips, dimmed (not hidden) dependent controls,
// stable ### ids. Every edit goes through MarkChanged(): the worker gets the new
// rules at once and the file is saved about a second after the last edit.
// Every text is a key of lang/en.json, looked up here in the program language
// (PluginI18n); nothing translated is kept across frames except the bonus order,
// rebuilt when the language changes.

#include "../TabletHelperPlugin.h"
#include "../TabletBonusCatalog.h"
#include "../TextUtil.h"

#include "../../../../POEFixer/imgui/IconsFontAwesome6.h"
#include "../../../../POEFixer/imgui/imgui.h"
#include "../../../../POEFixer/imgui/imgui_internal.h"  // ImTextInitClassifiers
#include "../../../../POEFixer/plugin_sdk/PluginUi.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace Ui = PluginUi;
namespace I18n = PluginSDK::I18n;
using TabletHelperConfig::TypeConfig;

namespace {

bool Contains(const std::vector<std::string>& v, const std::string& id) {
    return std::find(v.begin(), v.end(), id) != v.end();
}
void RemoveFrom(std::vector<std::string>& v, const std::string& id) {
    v.erase(std::remove(v.begin(), v.end(), id), v.end());
}

std::string PathToUtf8(const std::filesystem::path& p) {
    const std::u8string u = p.u8string();
    return std::string(u.begin(), u.end());
}

std::string Num(unsigned long long v) { return std::to_string(v); }
std::string Num(int v) { return std::to_string(v); }

// A number with two decimals for a TrF argument (the format is code, never a text).
std::string Num2(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", v);
    return buf;
}

// "<icon>  <text>###<id>": a button label whose ImGui id is the same in every language.
std::string IconLabel(const char* icon, const char* text, const char* id) {
    return std::string(icon) + "  " + text + "###" + id;
}

// Marks where a slider's own number goes in a translated text (a TrF argument); NumberFormat
// turns the first mark into the conversion (TextUtil.h).
constexpr const char* kValue = TabletHelper::kNumberMark;
using TabletHelper::NumberFormat;

using TabletHelper::FoldCase;

// The name of the shipped default profile ("Default") in the program language; a
// name the player gave is shown as it is.
const char* ShownProfileName(const std::string& name) {
    return name == "Default" ? I18n::Tr("profiles_default_name") : name.c_str();
}

// One line summing up a type's rule for the types table.
std::string RuleSummary(const TypeConfig& t, bool readMods) {
    if (!t.enabled) return I18n::Tr("types_rule_off");
    const int sel = static_cast<int>(t.selectedBonusIds.size());
    const int req = static_cast<int>(t.requiredBonusIds.size());
    std::string s;
    if (sel == 0) {
        s = I18n::Tr("types_rule_every_tablet");
    } else if (req > 0 && sel - req > 0) {
        s = I18n::TrF("types_rule_required_optional", { { "required", Num(req) }, { "optional", Num(sel - req) } });
    } else if (req > 0) {
        s = I18n::TrF("types_rule_required", { { "required", Num(req) } });
    } else {
        s = I18n::TrF("types_rule_bonuses", { { "count", Num(sel) } });
    }
    // Parts are joined by a middle dot drawn by code (as Waystone Helper does): a comma or a
    // space between two translated phrases reads wrong in some languages.
    constexpr const char* kSeparator = "  \xC2\xB7  ";
    if (t.minUsesLeft > 0) s += kSeparator + I18n::TrF("types_rule_uses", { { "count", Num(t.minUsesLeft) } });
    if (!readMods && (sel > 0 || t.minUsesLeft > 0)) s += std::string(kSeparator) + I18n::Tr("types_rule_paused");
    return s;
}

// The text, or its longest prefix plus "..." that fits maxW (UTF-8 safe).
std::string FitWithEllipsis(const std::string& text, float maxW) {
    if (ImGui::CalcTextSize(text.c_str()).x <= maxW) return text;
    std::string cut = text;
    while (!cut.empty()) {
        size_t n = cut.size() - 1;
        while (n > 0 && (static_cast<unsigned char>(cut[n]) & 0xC0) == 0x80) --n;
        cut.resize(n);
        while (!cut.empty() && cut.back() == ' ') cut.pop_back();
        const std::string candidate = cut + "...";
        if (ImGui::CalcTextSize(candidate.c_str()).x <= maxW) return candidate;
    }
    return "...";
}

// Width of the types table and the filter editor: the whole card when narrow,
// at most ~42 em so names, rules and buttons stay close together on wide panes.
float ListWidth() {
    return (std::min)(ImGui::GetContentRegionAvail().x, ImGui::GetFontSize() * 42.0f);
}

// A wrapped line in the kit's warning colour (the kit's Hint only has the
// secondary colour).
void WarningLine(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, Ui::Colors().Warning);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

}  // namespace

void TabletHelperPlugin::DrawSettings() {
    if (!ctx()->ImGuiContext) return;  // incompatible host: no ImGui context to draw into
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
    // ImGui 1.93 keeps its word-wrap character classes in a per-DLL static that
    // only the copy which builds the font atlas initialises (the host's). This
    // DLL's copy wraps our text, so set it up here (idempotent, one check).
    ImTextInitClassifiers();
    if (!m_loaded) {
        Ui::Hint(I18n::Tr("status_incompatible"));
        return;
    }

    ImGui::PushID("TabletReforge");
    ImGui::Spacing();
    DrawSectionStatus();

    Ui::BeginCard("##auto_reforge");
    ImGui::TextUnformatted("Auto reforge des tablettes (experimental)");
    if (ImGui::Checkbox("Activer l'auto reforge###auto_reforge_enabled", &m_settings.autoReforgeEnabled)) {
        m_autoReforge.Stop("Option modifiee. Verifier le reforger avant de lancer.");
        MarkChanged();
    }
    ImGui::TextWrapped("Les filtres de l'onglet Types de tablettes definissent les tablettes A GARDER : "
        "elles sont surlignees et protegees. Seules les autres tablettes identifiees des categories cochees "
        "ci-dessous seront reforgees. Les resultats non identifies ne sont jamais remis au reforger.");
    ImGui::TextUnformatted("Categories autorisees a la reforge (trios de meme base et rarete)");
    ImGui::BeginDisabled(m_autoReforge.Busy());
    for (const char* key : TabletHelper::kRealTypeKeys) {
        ImGui::PushID(key);
        bool selected = Contains(m_settings.autoReforgeTypes, key);
        if (ImGui::Checkbox(I18n::Tr(TabletHelper::TypeNameKey(key)), &selected)) {
            if (selected) m_settings.autoReforgeTypes.emplace_back(key);
            else RemoveFrom(m_settings.autoReforgeTypes, key);
            MarkChanged();
        }
        ImGui::PopID();
    }
    ImGui::EndDisabled();
    if (!m_settings.readMods) ImGui::TextWrapped("Activer Lire les mods des tablettes dans Detection pour pouvoir lancer.");
    if (ImGui::SliderInt("Delai entre actions (ms)###auto_reforge_delay", &m_settings.autoReforgeDelayMs, 250, 2000)) {
        m_autoReforge.Stop("Delai modifie. Verifier le reforger avant de lancer.");
        MarkChanged();
    }
    ImGui::TextWrapped("Une fois par session : ouvrir le reforger vide, fermer ce menu, "
        "survoler son bouton Reforger et appuyer sur F6, puis survoler l'emplacement du resultat et F7.");
    ImGui::BeginDisabled(!m_autoReforge.Calibrated() || m_autoReforge.Busy());
    ImGui::Checkbox("J'ai vide les 3 entrees et le resultat###auto_reforge_empty", &m_autoReforge.EmptyConfirmed());
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!m_settings.autoReforgeEnabled || !m_settings.readMods ||
        m_settings.autoReforgeTypes.empty() || !m_autoReforge.Calibrated() ||
        !m_autoReforge.EmptyConfirmed() || m_autoReforge.Busy());
    if (ImGui::Button("Lancer la reforge###auto_reforge_start")) {
        TabletHelper::FilterConfig filter;
        filter.readMods = m_settings.readMods;
        filter.types = m_settings.ActiveTypes();
        m_autoReforge.RequestStart(filter, m_settings.autoReforgeTypes);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Arreter###auto_reforge_stop")) m_autoReforge.Stop("Arrete depuis les options.");
    ImGui::TextWrapped("Apres Lancer la reforge, fermer ce menu pour demarrer. Echap pour arreter. "
        "Ne pas manipuler le sac ni ouvrir le chat pendant la sequence.");
    ImGui::TextWrapped("%s", m_autoReforge.Status().c_str());
    if (ImGui::Button("Arreter / effacer le calibrage###auto_reforge_reset")) m_autoReforge.Reset();
    Ui::EndCard();

    const bool twoColumns = Ui::WideEnoughForTwoColumns() && ImGui::BeginTable("##layout", 2, ImGuiTableFlags_SizingStretchSame);
    if (twoColumns) {
        ImGui::TableNextColumn();
        DrawSectionHighlight();
        DrawSectionDetection();
        ImGui::TableNextColumn();
        DrawSectionLabels();
        ImGui::EndTable();
    } else {
        DrawSectionHighlight();
        DrawSectionLabels();
        DrawSectionDetection();
    }
    DrawSectionProfiles();
    DrawSectionTypes();
    DrawSectionDiagnostics();
    ImGui::Spacing();
    PluginUi::Hint("TabletReforge - based on TabletHelper by Omer Faruk ARPA and POEFixer. GPL-3.0.");
    ImGui::Spacing();
    ImGui::PopID();

    if (m_filterDirty) PushFilter();
    SaveIfDue(false);
}

// ---------------------------------------------------------------------------
// Status: what the plugin does and what it sees right now
// ---------------------------------------------------------------------------
void TabletHelperPlugin::DrawSectionStatus() {
    Ui::BeginCard("##status");
    Ui::Heading(ICON_FA_GEM, I18n::Tr("status_title"), I18n::Tr("status_help"));
    Ui::Hint(I18n::Tr("status_hint"));
    ImGui::Spacing();

    const auto st = m_worker ? m_worker->Stats() : TabletHelper::WorkerStats{};
    const Ui::Palette pal = Ui::Colors();
    const ImU32 accent = ImGui::ColorConvertFloat4ToU32(pal.Accent);
    const ImU32 muted = ImGui::ColorConvertFloat4ToU32(pal.TextSecondary);
    const ImU32 warning = ImGui::ColorConvertFloat4ToU32(pal.Warning);
    if (!ctx()->Game.IsInGame()) {
        Ui::StatusPill(I18n::Tr("status_not_in_game"), muted);
    } else if (!st.itemWindowOpen) {
        Ui::StatusPill(I18n::Tr("status_no_window"), muted);
    } else {
        Ui::StatusPill(I18n::TrF("status_on_screen", { { "count", Num(st.visibleTablets) } }).c_str(), muted);
        ImGui::SameLine();
        Ui::StatusPill(I18n::TrF("status_highlighted", { { "count", Num(st.highlighted) } }).c_str(), accent);
        if (st.pendingReads > 0) {
            ImGui::SameLine();
            Ui::StatusPill(I18n::TrF("status_reading", { { "count", Num(st.pendingReads) } }).c_str(), warning);
            Ui::Tip(I18n::Tr("status_reading_tip"));
        }
    }
    Ui::EndCard();
    ImGui::Spacing();
}

// ---------------------------------------------------------------------------
// Highlight style
// ---------------------------------------------------------------------------
void TabletHelperPlugin::DrawSectionHighlight() {
    auto& s = m_settings;
    Ui::BeginCard("##highlight");
    Ui::Heading(ICON_FA_HIGHLIGHTER, I18n::Tr("highlight_title"));
    const char* styleLabel = I18n::Tr("highlight_style");
    const char* thicknessLabel = I18n::Tr("highlight_thickness");
    const Ui::LabelColumn col({styleLabel, thicknessLabel});

    col.Row(styleLabel);
    int mode = static_cast<int>(s.highlightMode);
    if (Ui::Combo("###style", mode,
                  {I18n::Tr("highlight_style_border"), I18n::Tr("highlight_style_cross"), I18n::Tr("highlight_style_star")})) {
        s.highlightMode = static_cast<TabletHelperConfig::HighlightMode>(mode);
        MarkChanged();
    }
    Ui::Tip(I18n::Tr("highlight_style_tip"));

    col.Row(thicknessLabel);
    const std::string pxFormat = NumberFormat(I18n::TrF("unit_px", { { "value", kValue } }));
    if (ImGui::SliderInt("###thickness", &s.borderThickness, TabletHelperConfig::kBorderThicknessMin,
                         TabletHelperConfig::kBorderThicknessMax, pxFormat.c_str()))
        MarkChanged();
    Ui::Tip(I18n::Tr("highlight_thickness_tip"));
    Ui::EndCard();
    ImGui::Spacing();
}

// ---------------------------------------------------------------------------
// Labels drawn on the tablets
// ---------------------------------------------------------------------------
void TabletHelperPlugin::DrawSectionLabels() {
    auto& s = m_settings;
    Ui::BeginCard("##labels");
    Ui::Heading(ICON_FA_TAG, I18n::Tr("labels_title"));

    if (Ui::Check(I18n::Tr("labels_type_name"), "showLabel", &s.showLabel, I18n::Tr("labels_type_name_tip")))
        MarkChanged();
    ImGui::Indent();
    ImGui::BeginDisabled(!s.showLabel);
    if (Ui::Check(I18n::Tr("labels_matched_bonuses"), "showBonuses", &s.showMatchedBonuses,
                  I18n::TrF("labels_matched_bonuses_tip", { { "check", "\xEF\x80\x8C" "N" } }).c_str()))
        MarkChanged();
    ImGui::EndDisabled();
    ImGui::Unindent();
    if (Ui::Check(I18n::Tr("labels_uses_left"), "showUses", &s.showUsesLeft,
                  I18n::TrF("labels_uses_left_tip", { { "badge", "\xC3\x97N" } }).c_str()))
        MarkChanged();

    ImGui::Spacing();
    const bool anyText = s.showLabel || s.showUsesLeft;
    ImGui::BeginDisabled(!anyText);
    const char* colourLabel = I18n::Tr("labels_text_colour");
    const char* sizeLabel = I18n::Tr("labels_text_size");
    const char* backgroundLabel = I18n::Tr("labels_background");
    const Ui::LabelColumn col({colourLabel, sizeLabel, backgroundLabel});
    col.Row(colourLabel);
    int tmode = static_cast<int>(s.labelTextMode);
    if (Ui::Combo("###textColour", tmode, {I18n::Tr("labels_colour_white"), I18n::Tr("labels_colour_tablet")})) {
        s.labelTextMode = static_cast<TabletHelperConfig::LabelTextMode>(tmode);
        MarkChanged();
    }
    Ui::Tip(I18n::Tr("labels_text_colour_tip"));

    col.Row(sizeLabel);
    int pct = static_cast<int>(s.labelScale * 100.0f + 0.5f);
    const std::string pctFormat = NumberFormat(I18n::TrF("unit_percent", { { "value", kValue } }));
    if (ImGui::SliderInt("###textSize", &pct, static_cast<int>(TabletHelperConfig::kLabelScaleMin * 100.0f),
                         static_cast<int>(TabletHelperConfig::kLabelScaleMax * 100.0f), pctFormat.c_str())) {
        s.labelScale = std::clamp(static_cast<float>(pct) / 100.0f, TabletHelperConfig::kLabelScaleMin,
                                  TabletHelperConfig::kLabelScaleMax);
        MarkChanged();
    }
    Ui::Tip(I18n::Tr("labels_text_size_tip"));

    col.Row(backgroundLabel);
    int alphaPct = static_cast<int>(static_cast<float>(s.labelBackgroundAlpha) * 100.0f / 255.0f + 0.5f);
    ImGui::BeginDisabled(!s.labelBackground);
    const std::string opaqueFormat = NumberFormat(I18n::TrF("labels_background_format", { { "value", kValue } }));
    if (ImGui::SliderInt("###bgAlpha", &alphaPct, 0, 100, opaqueFormat.c_str())) {
        s.labelBackgroundAlpha = std::clamp(static_cast<int>(static_cast<float>(alphaPct) * 255.0f / 100.0f + 0.5f),
                                            TabletHelperConfig::kLabelBgAlphaMin, TabletHelperConfig::kLabelBgAlphaMax);
        MarkChanged();
    }
    Ui::Tip(I18n::Tr("labels_background_tip"));
    ImGui::EndDisabled();

    if (Ui::Check(I18n::Tr("labels_dark_box"), "labelBg", &s.labelBackground, I18n::Tr("labels_dark_box_tip")))
        MarkChanged();
    if (Ui::Check(I18n::Tr("labels_outline"), "labelOutline", &s.labelOutline, I18n::Tr("labels_outline_tip")))
        MarkChanged();
    ImGui::EndDisabled();
    Ui::EndCard();
    ImGui::Spacing();
}

// ---------------------------------------------------------------------------
// Detection
// ---------------------------------------------------------------------------
void TabletHelperPlugin::DrawSectionDetection() {
    auto& s = m_settings;
    Ui::BeginCard("##detection");
    Ui::Heading(ICON_FA_MAGNIFYING_GLASS, I18n::Tr("detection_title"));

    if (Ui::Check(I18n::Tr("detection_read_mods"), "readMods", &s.readMods, I18n::Tr("detection_read_mods_tip")))
        MarkChanged();
    if (!s.readMods) WarningLine(I18n::Tr("detection_paused"));

    ImGui::Spacing();
    const char* refreshLabel = I18n::Tr("detection_refresh");
    const Ui::LabelColumn col({refreshLabel});
    col.Row(refreshLabel);
    const std::string msFormat = NumberFormat(I18n::TrF("unit_ms", { { "value", kValue } }));
    if (ImGui::SliderInt("###interval", &s.scanIntervalMs, TabletHelperConfig::kScanIntervalMinMs,
                         TabletHelperConfig::kScanIntervalMaxMs, msFormat.c_str()))
        MarkChanged();
    Ui::Tip(I18n::Tr("detection_refresh_tip"));
    Ui::EndCard();
    ImGui::Spacing();
}

// ---------------------------------------------------------------------------
// Filter profiles
// ---------------------------------------------------------------------------
void TabletHelperPlugin::DrawSectionProfiles() {
    auto& s = m_settings;
    auto& profiles = s.profiles;
    if (profiles.empty()) profiles.push_back(TabletHelperConfig::Profile{});
    if (s.activeProfile < 0 || s.activeProfile >= static_cast<int>(profiles.size())) s.activeProfile = 0;

    Ui::BeginCard("##profiles");
    Ui::Heading(ICON_FA_LAYER_GROUP, I18n::Tr("profiles_title"), I18n::Tr("profiles_help"));

    const char* activeLabel = I18n::Tr("profiles_active");
    const char* nameLabel = I18n::Tr("profiles_name");
    const Ui::LabelColumn col({activeLabel, nameLabel});
    col.Row(activeLabel);
    if (ImGui::BeginCombo("###profile", ShownProfileName(profiles[static_cast<size_t>(s.activeProfile)].name))) {
        for (int i = 0; i < static_cast<int>(profiles.size()); ++i) {
            ImGui::PushID(i);
            const bool sel = i == s.activeProfile;
            if (ImGui::Selectable(ShownProfileName(profiles[static_cast<size_t>(i)].name), sel) && !sel) {
                s.activeProfile = i;
                MarkChanged();
            }
            if (sel) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    Ui::Tip(I18n::Tr("profiles_active_tip"));

    col.Row(nameLabel);
    auto& name = profiles[static_cast<size_t>(s.activeProfile)].name;
    // The field shows the name as the combo does; an edit stores what was typed.
    std::string shownName = ShownProfileName(name);
    if (Ui::InputString("###profileName", I18n::Tr("profiles_name_hint"), shownName)) {
        name = TabletHelperConfig::TruncateUtf8(shownName, TabletHelperConfig::kMaxProfileNameLen);
        MarkChanged();
    }
    if (ImGui::IsItemDeactivatedAfterEdit() && name.empty()) {
        name = s.UniqueProfileName(I18n::Tr("profiles_new_name"));
        MarkChanged();
    }
    Ui::Tip(I18n::Tr("profiles_name_tip"));

    ImGui::Spacing();
    const int n = static_cast<int>(profiles.size());
    const bool atCap = n >= TabletHelperConfig::kMaxProfiles;
    ImGui::BeginDisabled(atCap);
    if (ImGui::Button(IconLabel(ICON_FA_PLUS, I18n::Tr("profiles_new"), "newProfile").c_str())) {
        TabletHelperConfig::Profile p;
        p.name = s.UniqueProfileName(I18n::Tr("profiles_new_name"));
        profiles.push_back(std::move(p));
        s.activeProfile = static_cast<int>(profiles.size()) - 1;
        MarkChanged();
    }
    Ui::Tip(I18n::Tr("profiles_new_tip"));
    ImGui::SameLine();
    if (Ui::SubtleButton(IconLabel(ICON_FA_COPY, I18n::Tr("profiles_duplicate"), "dupProfile").c_str())) {
        TabletHelperConfig::Profile p = profiles[static_cast<size_t>(s.activeProfile)];
        p.name = s.UniqueProfileName(I18n::TrF("profiles_copy_name", { { "name", ShownProfileName(p.name) } }));
        profiles.push_back(std::move(p));
        s.activeProfile = static_cast<int>(profiles.size()) - 1;
        MarkChanged();
    }
    Ui::Tip(I18n::Tr("profiles_duplicate_tip"));
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(n <= 1);
    if (Ui::SubtleButton(IconLabel(ICON_FA_TRASH, I18n::Tr("profiles_delete"), "delProfile").c_str()))
        ImGui::OpenPopup("##confirmDelete");
    Ui::Tip(I18n::Tr("profiles_delete_tip"));
    ImGui::EndDisabled();
    if (Ui::ConfirmPopup("##confirmDelete", I18n::Tr("profiles_delete_confirm"), I18n::Tr("profiles_delete_yes"),
                         I18n::Tr("profiles_delete_keep")) &&
        profiles.size() > 1) {
        profiles.erase(profiles.begin() + s.activeProfile);
        s.activeProfile = std::clamp(s.activeProfile, 0, static_cast<int>(profiles.size()) - 1);
        MarkChanged();
    }
    if (atCap) Ui::Hint(I18n::TrF("profiles_limit", { { "count", Num(TabletHelperConfig::kMaxProfiles) } }).c_str());
    Ui::EndCard();
    ImGui::Spacing();
}

// ---------------------------------------------------------------------------
// Tablet types: one row per type, one editor open at a time
// ---------------------------------------------------------------------------
void TabletHelperPlugin::DrawSectionTypes() {
    auto& types = m_settings.ActiveTypes();
    Ui::BeginCard("##types");
    Ui::Heading(ICON_FA_TABLE_CELLS, I18n::Tr("types_title"), I18n::Tr("types_help"));

    const ImGuiStyle& st = ImGui::GetStyle();
    const std::string closeLabel = IconLabel(ICON_FA_PEN, I18n::Tr("types_close"), "edit");
    const std::string editLabel = IconLabel(ICON_FA_PEN, I18n::Tr("types_edit"), "edit");
    const float editW = (std::max)(Ui::ButtonWidth(closeLabel.c_str()), Ui::ButtonWidth(editLabel.c_str()));
    if (ImGui::BeginTable("##typeTable", 4,
                          ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_PadOuterX |
                              ImGuiTableFlags_BordersInnerH,
                          ImVec2(ListWidth(), 0.0f))) {
        float nameW = 0.0f;
        for (const auto& t : types)
            nameW = (std::max)(nameW, ImGui::CalcTextSize(I18n::Tr(TabletHelper::TypeNameKey(t.key))).x);
        ImGui::TableSetupColumn("##on", ImGuiTableColumnFlags_WidthFixed,
                                ImGui::GetFrameHeight() * 2.0f + st.ItemInnerSpacing.x);
        ImGui::TableSetupColumn("##name", ImGuiTableColumnFlags_WidthFixed, nameW + st.ItemSpacing.x);
        ImGui::TableSetupColumn("##rule", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##edit", ImGuiTableColumnFlags_WidthFixed, editW);

        for (auto& t : types) {
            ImGui::PushID(t.key.c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Checkbox("##enabled", &t.enabled)) MarkChanged();
            Ui::Tip(I18n::Tr("types_enabled_tip"));
            ImGui::SameLine(0.0f, st.ItemInnerSpacing.x);
            if (Ui::Swatch("##colour", t.color)) MarkChanged();
            Ui::Tip(I18n::Tr("types_colour_tip"));

            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::BeginDisabled(!t.enabled);
            ImGui::TextUnformatted(I18n::Tr(TabletHelper::TypeNameKey(t.key)));
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            const std::string summary = RuleSummary(t, m_settings.readMods);
            ImGui::PushStyleColor(ImGuiCol_Text, Ui::Colors().TextSecondary);
            ImGui::TextUnformatted(FitWithEllipsis(summary, ImGui::GetContentRegionAvail().x).c_str());
            ImGui::PopStyleColor();
            Ui::Tip(summary.c_str());
            ImGui::EndDisabled();

            ImGui::TableNextColumn();
            const bool open = m_openType == t.key;
            if (Ui::SubtleButton(open ? closeLabel.c_str() : editLabel.c_str(), ImVec2(editW, 0.0f)))
                m_openType = open ? std::string() : t.key;
            Ui::Tip(open ? I18n::Tr("types_close_tip") : I18n::Tr("types_edit_tip"));
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    for (auto& t : types) {
        if (t.key != m_openType) continue;
        ImGui::Spacing();
        DrawTypeEditor(t);
    }
    Ui::EndCard();
    ImGui::Spacing();
}

void TabletHelperPlugin::DrawTypeEditor(TypeConfig& t) {
    ImGui::PushID(t.key.c_str());
    ImGui::PushID("editor");
    const std::string title = I18n::TrF("editor_title", { { "type", I18n::Tr(TabletHelper::TypeNameKey(t.key)) } });
    Ui::Heading(ICON_FA_FILTER, title.c_str());

    const bool readMods = m_settings.readMods;
    if (!readMods) WarningLine(I18n::Tr("editor_paused"));

    const int selCount = static_cast<int>(t.selectedBonusIds.size());
    const int reqCount = static_cast<int>(t.requiredBonusIds.size());
    const int optCount = selCount - reqCount;

    const char* minUsesLabel = I18n::Tr("editor_min_uses");
    const char* requiredLabel = I18n::Tr("editor_required");
    const char* optionalLabel = I18n::Tr("editor_optional");
    const Ui::LabelColumn col({minUsesLabel, requiredLabel, optionalLabel});
    ImGui::BeginDisabled(!readMods);
    col.Row(minUsesLabel);
    const std::string anyFormat = NumberFormat(I18n::Tr("editor_any"));
    if (ImGui::SliderInt("###minUses", &t.minUsesLeft, 0, TabletHelperConfig::kMinUsesMax,
                         t.minUsesLeft == 0 ? anyFormat.c_str() : "%d"))
        MarkChanged();
    Ui::Tip(I18n::Tr("editor_min_uses_tip"));

    auto poolSlider = [&](const char* label, const char* id, int* stored, int pool, const char* tip) {
        col.Row(label);
        if (pool <= 0) {
            // Nothing to count yet: an empty, dimmed slider (a 1..1 slider would
            // draw its grab across the whole frame; 0..100 keeps it minimal).
            ImGui::BeginDisabled();
            int zero = 0;
            ImGui::SliderInt(id, &zero, 0, 100, NumberFormat(I18n::Tr("editor_none_selected")).c_str());
            ImGui::EndDisabled();
            Ui::Tip(tip);
            return;
        }
        const int cap = std::clamp(pool, 1, TabletHelperConfig::kMinMatchedMax);
        int v = std::clamp(*stored, 1, cap);
        const std::string fmt = NumberFormat(I18n::TrF("editor_pool_format", { { "value", kValue }, { "total", Num(pool) } }));
        if (ImGui::SliderInt(id, &v, 1, cap, fmt.c_str())) {
            *stored = v;
            MarkChanged();
        }
        Ui::Tip(tip);
    };
    poolSlider(requiredLabel, "###minReq", &t.minRequiredBonuses, reqCount, I18n::Tr("editor_required_tip"));
    poolSlider(optionalLabel, "###minOpt", &t.minMatchedBonuses, optCount, I18n::Tr("editor_optional_tip"));
    ImGui::EndDisabled();

    if (selCount == 0) Ui::Hint(I18n::Tr("editor_no_bonus"));
    ImGui::Spacing();
    DrawBonusTable(t);
    ImGui::PopID();
    ImGui::PopID();
}

bool TabletHelperPlugin::DrawValueField(const char* id, int* v, int lo, int hi) {
    // A small integer field where clearing it means 0 ("no limit").
    char buf[12];
    if (*v == 0) buf[0] = '\0';
    else std::snprintf(buf, sizeof(buf), "%d", *v);
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("-0000").x + ImGui::GetStyle().FramePadding.x * 2.0f);
    bool changed = false;
    if (ImGui::InputTextWithHint(id, "-", buf, sizeof(buf),
                                 ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AutoSelectAll)) {
        int nv = (buf[0] == '\0' || (buf[0] == '-' && buf[1] == '\0')) ? 0 : std::atoi(buf);
        nv = std::clamp(nv, lo, hi);
        if (nv != *v) {
            *v = nv;
            changed = true;
        }
    }
    return changed;
}

// The type's bonuses in display order: by category, then by label in the program
// language. Rebuilt when the language changes.
const std::vector<const TabletHelper::Bonus*>& TabletHelperPlugin::OrderedBonuses(const std::string& typeKey) {
    const uint64_t revision = I18n::Revision();
    if (revision != m_bonusOrderRevision) {
        m_bonusOrder.clear();
        m_bonusOrderRevision = revision;
    }
    auto it = m_bonusOrder.find(typeKey);
    if (it != m_bonusOrder.end()) return it->second;
    std::vector<std::pair<std::string, const TabletHelper::Bonus*>> keyed;
    for (const auto& b : TabletHelper::GetBonusesFor(typeKey)) keyed.emplace_back(FoldCase(I18n::Tr(b.LabelKey.c_str())), &b);
    std::stable_sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) {
        if (a.second->CategoryRank != b.second->CategoryRank) return a.second->CategoryRank < b.second->CategoryRank;
        return a.first < b.first;
    });
    std::vector<const TabletHelper::Bonus*> ordered;
    ordered.reserve(keyed.size());
    for (const auto& k : keyed) ordered.push_back(k.second);
    return m_bonusOrder.emplace(typeKey, std::move(ordered)).first->second;
}

void TabletHelperPlugin::DrawBonusTable(TypeConfig& t) {
    const ImGuiStyle& st = ImGui::GetStyle();
    const auto& bonuses = OrderedBonuses(t.key);
    std::string& search = m_bonusSearch[t.key];
    bool& onlySelected = m_onlySelected[t.key];

    // Search row: field, "Selected only", "Clear"; the field gets its own line
    // when the row is too narrow for a useful field beside the other two.
    const float listW = ListWidth();
    const std::string clearLabel = IconLabel(ICON_FA_ERASER, I18n::Tr("bonuses_clear"), "clear");
    const char* onlyLabel = I18n::Tr("bonuses_selected_only");
    const float clearW = Ui::ButtonWidth(clearLabel.c_str());
    const float onlyW = ImGui::GetFrameHeight() + st.ItemInnerSpacing.x + ImGui::CalcTextSize(onlyLabel).x;
    const float fieldW = listW - clearW - onlyW - st.ItemSpacing.x * 2.0f;
    const bool stacked = fieldW < ImGui::GetFontSize() * 12.0f;
    ImGui::SetNextItemWidth(stacked ? listW : fieldW);
    Ui::InputString("###search", I18n::Tr("bonuses_search_hint"), search);
    Ui::Tip(I18n::Tr("bonuses_search_tip"));
    if (!stacked) ImGui::SameLine();
    Ui::Check(onlyLabel, "onlySel", &onlySelected, I18n::Tr("bonuses_selected_only_tip"));
    ImGui::SameLine();
    ImGui::BeginDisabled(t.selectedBonusIds.empty() && t.valueFilters.empty());
    if (Ui::SubtleButton(clearLabel.c_str())) {
        t.selectedBonusIds.clear();
        t.requiredBonusIds.clear();
        t.valueFilters.clear();
        MarkChanged();
    }
    Ui::Tip(I18n::Tr("bonuses_clear_tip"));
    ImGui::EndDisabled();

    const std::string needle = FoldCase(search);
    const float rowH = ImGui::GetFrameHeight() + st.CellPadding.y * 2.0f;
    const float tableH = rowH * 12.0f + ImGui::GetTextLineHeightWithSpacing();
    const bool narrow = listW < ImGui::GetFontSize() * 30.0f;
    const char* requiredHeader = narrow ? I18n::Tr("bonuses_col_required_short") : I18n::Tr("bonuses_col_required");
    const float reqW = (std::max)(ImGui::CalcTextSize(requiredHeader).x, ImGui::GetFrameHeight());
    const float valW = (std::max)((std::max)(ImGui::CalcTextSize(I18n::Tr("bonuses_col_min")).x,
                                             ImGui::CalcTextSize(I18n::Tr("bonuses_col_max")).x),
                                  ImGui::CalcTextSize("-0000").x + st.FramePadding.x * 2.0f);
    const ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersOuter |
                                  ImGuiTableFlags_PadOuterX | ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable("##bonuses", 4, flags, ImVec2(listW, tableH))) return;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(I18n::TrId("bonuses_col_bonus"), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(I18n::TrId("bonuses_col_required"), ImGuiTableColumnFlags_WidthFixed, reqW);
    ImGui::TableSetupColumn(I18n::TrId("bonuses_col_min"), ImGuiTableColumnFlags_WidthFixed, valW);
    ImGui::TableSetupColumn(I18n::TrId("bonuses_col_max"), ImGuiTableColumnFlags_WidthFixed, valW);
    // Header row by hand so each header explains its column.
    static constexpr const char* kHeaderTips[4] = {
        I18n::Key("bonuses_col_bonus_tip"), I18n::Key("bonuses_col_required_tip"), I18n::Key("bonuses_col_min_tip"),
        I18n::Key("bonuses_col_max_tip")};
    const std::string requiredShortHeader = std::string(requiredHeader) + "###bonuses_col_required";
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    for (int c = 0; c < 4; ++c) {
        ImGui::TableSetColumnIndex(c);
        // "Required" is the widest fixed header: abbreviated on narrow panes.
        ImGui::TableHeader(c == 1 && narrow ? requiredShortHeader.c_str() : ImGui::TableGetColumnName(c));
        Ui::Tip(I18n::Tr(kHeaderTips[c]));
    }

    const bool readMods = m_settings.readMods;
    int lastCategory = -1;
    int shown = 0;
    for (const TabletHelper::Bonus* bp : bonuses) {
        const TabletHelper::Bonus& b = *bp;
        const bool sel = Contains(t.selectedBonusIds, b.Id);
        if (onlySelected && !sel) continue;
        const char* label = I18n::Tr(b.LabelKey.c_str());
        if (!TabletHelper::BonusMatchesSearch(needle, label, TabletHelper::EnglishText(b.LabelKey.c_str()), b.Id))
            continue;
        if (b.CategoryRank != lastCategory) {
            lastCategory = b.CategoryRank;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, Ui::Colors().Accent);
            ImGui::TextUnformatted(I18n::Tr(b.CategoryKey));
            ImGui::PopStyleColor();
        }
        ImGui::PushID(b.Id.c_str());
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        bool on = sel;
        // The name is shortened with "..." to the column (the tooltip has it whole).
        const float nameW = ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - st.ItemInnerSpacing.x;
        const std::string checkLabel = FitWithEllipsis(label, nameW) + "###on";
        if (ImGui::Checkbox(checkLabel.c_str(), &on)) {
            if (on) {
                if (!Contains(t.selectedBonusIds, b.Id)) t.selectedBonusIds.push_back(b.Id);
            } else {
                RemoveFrom(t.selectedBonusIds, b.Id);
                RemoveFrom(t.requiredBonusIds, b.Id);
            }
            MarkChanged();
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
            ImGui::TextUnformatted(label);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextUnformatted(I18n::TrF("bonuses_mod_id", { { "id", b.Id } }).c_str());
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        ImGui::BeginDisabled(!on || !readMods);
        ImGui::TableNextColumn();
        bool req = Contains(t.requiredBonusIds, b.Id);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (reqW - ImGui::GetFrameHeight()) * 0.5f);
        if (ImGui::Checkbox("##req", &req)) {
            if (req) {
                if (!Contains(t.requiredBonusIds, b.Id)) t.requiredBonusIds.push_back(b.Id);
            } else {
                RemoveFrom(t.requiredBonusIds, b.Id);
            }
            MarkChanged();
        }
        Ui::Tip(I18n::Tr("bonuses_required_tip"));

        const TabletHelper::ModRange* rg = m_ranges.For(b.NormId);
        if (!rg && !b.NormIdStripped.empty()) rg = m_ranges.For(b.NormIdStripped);
        if (rg && rg->min != rg->max) {
            auto& vr = t.valueFilters[b.Id];
            const int lo = rg->min < 0 ? rg->min : 0;
            const int hi = (std::max)(rg->max, 1) * 4;  // generous: a patch may raise the rolls
            const bool pct = rg->unit == "percent";
            const bool secs = rg->unit == "seconds";
            auto withUnit = [&](int value) {
                if (pct) return I18n::TrF("unit_percent", { { "value", Num(value) } });
                if (secs) return I18n::TrF("unit_seconds", { { "value", Num(value) } });
                return Num(value);
            };
            const std::string minText = withUnit(rg->min);
            const std::string maxText = withUnit(rg->max);
            ImGui::TableNextColumn();
            if (DrawValueField("##min", &vr.min, lo, hi)) MarkChanged();
            Ui::Tip(I18n::TrF("bonuses_min_tip", { { "min", minText }, { "max", maxText } }).c_str());
            ImGui::TableNextColumn();
            if (DrawValueField("##max", &vr.max, lo, hi)) MarkChanged();
            Ui::Tip(I18n::TrF("bonuses_max_tip", { { "min", minText }, { "max", maxText } }).c_str());
            if (vr.min == 0 && vr.max == 0) t.valueFilters.erase(b.Id);
        } else {
            ImGui::TableNextColumn();
            ImGui::TextDisabled("-");
            Ui::Tip(I18n::Tr("bonuses_fixed_tip"));
            ImGui::TableNextColumn();
            ImGui::TextDisabled("-");
        }
        ImGui::EndDisabled();
        ImGui::PopID();
        ++shown;
    }
    if (shown == 0) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextUnformatted(onlySelected ? I18n::Tr("bonuses_none_selected") : I18n::Tr("bonuses_no_match"));
        ImGui::PopStyleColor();
    }
    ImGui::EndTable();
}

// ---------------------------------------------------------------------------
// Diagnostics (collapsed)
// ---------------------------------------------------------------------------
void TabletHelperPlugin::DrawSectionDiagnostics() {
    if (Ui::BeginDiagnostics(I18n::Tr("diag_tip"))) {
        const auto st = m_worker ? m_worker->Stats() : TabletHelper::WorkerStats{};
        const char* lastReadLabel = I18n::Tr("diag_last_read");
        const char* modReadsLabel = I18n::Tr("diag_mod_reads");
        const char* rememberedLabel = I18n::Tr("diag_remembered");
        const char* rangesLabel = I18n::Tr("diag_mod_ranges");
        const Ui::LabelColumn col({lastReadLabel, modReadsLabel, rememberedLabel, rangesLabel});
        ImGui::PushTextWrapPos(0.0f);  // values wrap inside the card on narrow panes
        col.RowNoWidth(lastReadLabel);
        ImGui::TextUnformatted(I18n::TrF("diag_last_read_value", { { "ms", Num2(st.lastPassMs) },
                                                                   { "peak", Num2(st.peakPassMs) },
                                                                   { "reads", Num(static_cast<unsigned long long>(st.passes)) } })
                                   .c_str());
        col.RowNoWidth(modReadsLabel);
        ImGui::TextUnformatted(I18n::TrF("diag_mod_reads_value",
                                         { { "count", Num(static_cast<unsigned long long>(st.modReads)) },
                                           { "failed", Num(static_cast<unsigned long long>(st.modReadFailures)) } })
                                   .c_str());
        col.RowNoWidth(rememberedLabel);
        ImGui::TextUnformatted(Num(st.cachedTablets).c_str());
        if (st.faults > 0) {
            ImGui::SameLine(0.0f, ImGui::GetFontSize());
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextUnformatted(
                I18n::TrF("diag_faults", { { "count", Num(static_cast<unsigned long long>(st.faults)) } }).c_str());
            ImGui::PopStyleColor();
        }
        col.RowNoWidth(rangesLabel);
        const std::string builtIn = Num(static_cast<unsigned long long>(m_ranges.BuiltInCount()));
        if (m_ranges.OverrideCount() > 0)
            ImGui::TextUnformatted(I18n::TrF("diag_ranges_overridden", { { "count", builtIn },
                                                                         { "overridden", Num(m_ranges.OverrideCount()) } })
                                       .c_str());
        else
            ImGui::TextUnformatted(I18n::TrF("diag_ranges_builtin", { { "count", builtIn } }).c_str());
        ImGui::PopTextWrapPos();

        ImGui::Spacing();
        Ui::Hint(I18n::Tr("diag_dump_hint"));
        const auto dir = DirectoryPath() / "config";
        const auto path = dir / "tablet_debug_dump.txt";
        if (Ui::SubtleButton(IconLabel(ICON_FA_BUG, I18n::Tr("diag_dump_write"), "dump").c_str())) {
            const std::string dump = TabletHelper::TabletWorker::BuildModDump(ctx(), 60);
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            std::ofstream out(path, std::ios::binary | std::ios::app);
            m_dumpPath = PathToUtf8(path);
            if (out.is_open()) {
                out << "\n===== DUMP #" << ++m_dumpCount << " =====\n" << dump;
                m_dumpStatus = DumpStatus::Appended;
            } else {
                m_dumpStatus = DumpStatus::WriteFailed;
            }
        }
        Ui::Tip(I18n::Tr("diag_dump_write_tip"));
        ImGui::SameLine();
        if (Ui::SubtleButton(IconLabel(ICON_FA_TRASH, I18n::Tr("diag_dump_clear"), "clearDump").c_str())) {
            std::error_code ec;
            std::filesystem::remove(path, ec);
            m_dumpCount = 0;
            m_dumpPath = PathToUtf8(path);
            m_dumpStatus = DumpStatus::Removed;
        }
        if (m_dumpStatus != DumpStatus::None) {
            std::string status;
            switch (m_dumpStatus) {
            case DumpStatus::Appended:
                status = I18n::TrF("diag_dump_appended", { { "number", Num(m_dumpCount) }, { "path", m_dumpPath } });
                break;
            case DumpStatus::WriteFailed:
                status = I18n::TrF("diag_dump_failed", { { "path", m_dumpPath } });
                break;
            case DumpStatus::Removed:
                status = I18n::TrF("diag_dump_removed", { { "path", m_dumpPath } });
                break;
            case DumpStatus::None:
                break;
            }
            ImGui::PushTextWrapPos(0.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextUnformatted(status.c_str());
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();
        }
        Ui::EndDiagnostics();
    }
    ImGui::Spacing();
}

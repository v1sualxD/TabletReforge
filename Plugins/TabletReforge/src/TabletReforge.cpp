// Derived from TabletHelper by Omer Faruk ARPA and POEFixer. GPL-3.0.
#include "TabletReforgeController.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "TabletRanges.h"
#include "../lib/nlohmann/json.hpp"
#include <fstream>
#include <stdexcept>
#include <array>
#include <iterator>
#include <unordered_map>

#include "lang_en.generated.h"

namespace {
using TabletHelperConfig::TypeConfig;

bool HasId(const std::vector<std::string>& ids, const std::string& id) {
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}
struct ReforgeProfile {
    std::string name = "Default";
    std::vector<TypeConfig> types = TabletHelperConfig::DefaultTypes();
    bool normal = true, magic = true, rare = true;
};
ReforgeProfile NewDefaultProfile(std::string name = "Default") {
    ReforgeProfile profile;
    profile.name = std::move(name);
    for (auto& rule : profile.types) rule.enabled = false;
    return profile;
}
void RemoveId(std::vector<std::string>& ids, const std::string& id) {
    ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
}
const char* TypeLabel(const std::string& key) {
    if (key == TabletHelper::TypeKeys::Irradiated) return "Irradiated Tablet";
    if (key == TabletHelper::TypeKeys::Breach) return "Breach Tablet";
    if (key == TabletHelper::TypeKeys::Delirium) return "Delirium Tablet";
    if (key == TabletHelper::TypeKeys::Expedition) return "Expedition Tablet";
    if (key == TabletHelper::TypeKeys::Abyss) return "Abyss Tablet";
    if (key == TabletHelper::TypeKeys::Ritual) return "Ritual Tablet";
    if (key == TabletHelper::TypeKeys::Overseer) return "Overseer Tablet";
    if (key == TabletHelper::TypeKeys::Temple) return "Temple Tablet";
    return "Unknown Tablet";
}
std::string BonusLabel(const std::string& key) {
    static const auto labels = [] {
        std::unordered_map<std::string, std::string> result;
        result.reserve(std::size(kPluginLangEn));
        for (const auto& entry : kPluginLangEn) result.emplace(entry.key, entry.text);
        return result;
    }();
    const auto it = labels.find(key);
    if (it != labels.end()) return it->second;
    return key;
}
std::string Lower(std::string value) {
    for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
} // namespace


POEFIXER_PLUGIN_INFO(
    "TabletReforge",
    "Tablet Reforge",
    "0.5.0",
    "TabletReforge contributors; Omer Faruk ARPA; POEFixer",
    "Reforge identified tablets in matching category and rarity trios, collect results without reusing unidentified tablets, and display tablet types.")


class TabletReforgePlugin : public PluginSDK::Plugin {
    TabletHelper::TabletReforgeController controller_;
    bool allTablets_ = true, showTabletTypes_ = true, running_ = false;
    int executionSpeed_ = 15;
    std::vector<ReforgeProfile> profiles_ = {NewDefaultProfile()};
    int activeProfile_ = 0;
    char profileName_[TabletHelperConfig::kMaxProfileNameLen + 1] = "Default";
    std::string editingType_;
    char bonusSearch_[128]{};
    TabletHelper::TabletRanges ranges_;

    ReforgeProfile& ActiveProfile() {
        if (profiles_.empty()) profiles_.push_back(NewDefaultProfile());
        activeProfile_ = std::clamp(activeProfile_, 0, static_cast<int>(profiles_.size()) - 1);
        return profiles_[static_cast<size_t>(activeProfile_)];
    }
    std::string UniqueProfileName(const std::string& requested) const {
        const std::string base = TabletHelperConfig::TruncateUtf8(requested.empty() ? "Profile" : requested,
            TabletHelperConfig::kMaxProfileNameLen - 4);
        auto used = [&](const std::string& name) {
            return std::any_of(profiles_.begin(), profiles_.end(), [&](const auto& p) { return p.name == name; });
        };
        if (!used(base)) return base;
        for (int i = 2; i < 1000; ++i) {
            const std::string name = base + " " + std::to_string(i);
            if (!used(name)) return name;
        }
        return base;
    }
    void SetProfileNameBuffer() {
        const auto& name = ActiveProfile().name;
        std::snprintf(profileName_, sizeof(profileName_), "%s", name.c_str());
    }
    uint8_t RarityMask() const {
        if (profiles_.empty()) return 0;
        const auto& profile = profiles_[static_cast<size_t>(std::clamp(activeProfile_, 0, static_cast<int>(profiles_.size()) - 1))];
        return static_cast<uint8_t>((profile.normal ? 1 : 0) | (profile.magic ? 2 : 0) | (profile.rare ? 4 : 0));
    }
    void DrawTypeEditor(TypeConfig& rule) {
        ImGui::Separator();
        ImGui::Text("Keep matching %s", TypeLabel(rule.key));
        ImGui::SliderInt("Minimum uses left", &rule.minUsesLeft, 0, TabletHelperConfig::kMinUsesMax,
            rule.minUsesLeft == 0 ? "Any" : "%d");
        if (ImGui::IsItemDeactivatedAfterEdit()) SaveSettings();

        const int requiredCount = static_cast<int>(rule.requiredBonusIds.size());
        const int optionalCount = static_cast<int>(rule.selectedBonusIds.size()) - requiredCount;
        auto drawThreshold = [&](const char* label, int& value, int pool) {
            ImGui::BeginDisabled(pool <= 0);
            const int maxValue = (std::max)(1, pool);
            value = std::clamp(value, 1, maxValue);
            if (ImGui::SliderInt(label, &value, 1, maxValue, "%d")) SaveSettings();
            ImGui::EndDisabled();
        };
        drawThreshold("Required bonuses", rule.minRequiredBonuses, requiredCount);
        drawThreshold("Optional bonuses", rule.minMatchedBonuses, optionalCount);
        ImGui::InputTextWithHint("Search bonuses", "Name or mod ID", bonusSearch_, sizeof(bonusSearch_));

        const auto& bonuses = TabletHelper::GetBonusesFor(rule.key);
        const float tableHeight = ImGui::GetTextLineHeightWithSpacing() * 11.0f;
        if (ImGui::BeginTable("TabletBonusFilters", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
            ImVec2(0, tableHeight))) {
            ImGui::TableSetupColumn("Keep bonus", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Required", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("Min", ImGuiTableColumnFlags_WidthFixed, 75.0f);
            ImGui::TableSetupColumn("Max", ImGuiTableColumnFlags_WidthFixed, 75.0f);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            const std::string needle = Lower(bonusSearch_);
            int category = -1;
            for (const auto& bonus : bonuses) {
                const std::string label = BonusLabel(bonus.LabelKey);
                if (!needle.empty() && Lower(label).find(needle) == std::string::npos &&
                    Lower(bonus.Id).find(needle) == std::string::npos) continue;
                if (category != bonus.CategoryRank) {
                    category = bonus.CategoryRank;
                    ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
                    ImGui::TextDisabled("%s", category == 0 ? "Common" : category == 1 ? "Mechanic" : "Unique");
                }
                ImGui::PushID(bonus.Id.c_str());
                bool selected = HasId(rule.selectedBonusIds, bonus.Id);
                bool required = HasId(rule.requiredBonusIds, bonus.Id);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
                if (ImGui::Checkbox(label.c_str(), &selected)) {
                    if (selected) rule.selectedBonusIds.push_back(bonus.Id);
                    else { RemoveId(rule.selectedBonusIds, bonus.Id); RemoveId(rule.requiredBonusIds, bonus.Id); }
                    SaveSettings();
                }
                ImGui::TableSetColumnIndex(1);
                ImGui::BeginDisabled(!selected);
                if (ImGui::Checkbox("##required", &required)) {
                    if (required) rule.requiredBonusIds.push_back(bonus.Id);
                    else RemoveId(rule.requiredBonusIds, bonus.Id);
                    SaveSettings();
                }
                const TabletHelper::ModRange* range = ranges_.For(bonus.NormId);
                if (!range && !bonus.NormIdStripped.empty()) range = ranges_.For(bonus.NormIdStripped);
                auto filter = rule.valueFilters.find(bonus.Id);
                TabletHelperConfig::ValueRange values = filter == rule.valueFilters.end()
                    ? TabletHelperConfig::ValueRange{} : filter->second;
                for (int column = 0; column < 2; ++column) {
                    ImGui::TableSetColumnIndex(2 + column);
                    if (selected && range && range->min != range->max) {
                        int& value = column == 0 ? values.min : values.max;
                        const int lo = range->min < 0 ? range->min : 0;
                        const int hi = (std::max)(range->max, 1) * 4;
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::InputInt(column == 0 ? "##min" : "##max", &value, 0, 0,
                            ImGuiInputTextFlags_CharsDecimal)) {
                            value = std::clamp(value, lo, hi);
                            if (values.min == 0 && values.max == 0) rule.valueFilters.erase(bonus.Id);
                            else rule.valueFilters[bonus.Id] = values;
                            SaveSettings();
                        }
                    } else {
                        ImGui::TextDisabled("-");
                    }
                }
                ImGui::EndDisabled();
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
public:
    const char* GetName() const override { return "Tablet Reforge"; }
    bool WantsOverlay() const override { return true; }
    void OnEnable(bool) override {
        if (!HostCompatible()) return;
        allTablets_ = true; showTabletTypes_ = true; executionSpeed_ = 15;
        profiles_ = {NewDefaultProfile()};
        activeProfile_ = 0;
        ranges_.Load(DirectoryPath());
        try {
            std::ifstream file(DirectoryPath() / "config" / "settings.json");
            if (file) {
                const auto settings = nlohmann::json::parse(file, nullptr, false);
                if (settings.is_object() && settings.contains("all_tablets") && settings["all_tablets"].is_boolean())
                    allTablets_ = settings["all_tablets"].get<bool>();
                if (settings.is_object() && settings.contains("show_tablet_types") && settings["show_tablet_types"].is_boolean())
                    showTabletTypes_ = settings["show_tablet_types"].get<bool>();
                if (settings.is_object() && settings.contains("execution_speed") && settings["execution_speed"].is_number_integer())
                    executionSpeed_ = std::clamp(settings["execution_speed"].get<int>(), 1, 20);
                bool hasProfiles = false;
                if (settings.is_object() && settings.contains("profiles") && settings["profiles"].is_array()) {
                    profiles_.clear();
                    for (const auto& entry : settings["profiles"]) {
                        if (!entry.is_object()) continue;
                        ReforgeProfile profile = NewDefaultProfile();
                        if (entry.contains("name") && entry["name"].is_string()) {
                            const auto name = TabletHelperConfig::TruncateUtf8(entry["name"].get<std::string>(),
                                TabletHelperConfig::kMaxProfileNameLen);
                            if (!name.empty()) profile.name = name;
                        }
                        if (entry.contains("normal") && entry["normal"].is_boolean()) profile.normal = entry["normal"].get<bool>();
                        if (entry.contains("magic") && entry["magic"].is_boolean()) profile.magic = entry["magic"].get<bool>();
                        if (entry.contains("rare") && entry["rare"].is_boolean()) profile.rare = entry["rare"].get<bool>();
                        if (entry.contains("types")) TabletHelperConfig::ApplyTypeOverrides(profile.types, entry["types"]);
                        profiles_.push_back(std::move(profile));
                        if (profiles_.size() >= TabletHelperConfig::kMaxProfiles) break;
                    }
                    hasProfiles = !profiles_.empty();
                }
                if (!hasProfiles) {
                    profiles_ = {NewDefaultProfile()};
                    auto& profile = profiles_.front();
                    if (settings.is_object() && settings.contains("rarity_normal") && settings["rarity_normal"].is_boolean()) profile.normal = settings["rarity_normal"].get<bool>();
                    if (settings.is_object() && settings.contains("rarity_magic") && settings["rarity_magic"].is_boolean()) profile.magic = settings["rarity_magic"].get<bool>();
                    if (settings.is_object() && settings.contains("rarity_rare") && settings["rarity_rare"].is_boolean()) profile.rare = settings["rarity_rare"].get<bool>();
                    if (settings.is_object() && settings.contains("reforge_types"))
                        TabletHelperConfig::ApplyTypeOverrides(profile.types, settings["reforge_types"]);
                }
                if (settings.is_object() && settings.contains("active_profile") && settings["active_profile"].is_number_integer())
                    activeProfile_ = std::clamp(settings["active_profile"].get<int>(), 0, static_cast<int>(profiles_.size()) - 1);
            }
        } catch (...) { ctx()->Log.Warn("TabletReforge: could not read settings; defaults are active."); }
        profiles_.reserve(TabletHelperConfig::kMaxProfiles);
        SetProfileNameBuffer();
        controller_.Reset(); running_ = true;
    }
    void OnDisable() override { running_ = false; controller_.Reset(); }
    void SaveSettings() override {
        try {
            const auto directory = DirectoryPath() / "config";
            std::filesystem::create_directories(directory);
            const auto temporary = directory / "settings.json.tmp";
            const auto destination = directory / "settings.json";
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            nlohmann::json settings{{"all_tablets", allTablets_}, {"show_tablet_types", showTabletTypes_},
                {"execution_speed", executionSpeed_}, {"active_profile", activeProfile_}};
            nlohmann::json profileArray = nlohmann::json::array();
            for (const auto& profile : profiles_) {
                profileArray.push_back({{"name", profile.name}, {"normal", profile.normal},
                    {"magic", profile.magic}, {"rare", profile.rare},
                    {"types", TabletHelperConfig::SerializeTypes(profile.types)}});
            }
            settings["profiles"] = std::move(profileArray);
            const auto& active = ActiveProfile();
            settings["rarity_normal"] = active.normal;
            settings["rarity_magic"] = active.magic;
            settings["rarity_rare"] = active.rare;
            settings["reforge_types"] = TabletHelperConfig::SerializeTypes(active.types);
            file << settings.dump(2);
            file.flush();
            if (!file.good()) throw std::runtime_error("Settings write failed");
            file.close();
            if (!::MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("Settings replacement failed");
        } catch (...) { ctx()->Log.Warn("TabletReforge: could not save settings."); }
    }
    void DrawSettings() override {
        if (!running_ || !ctx()->ImGuiContext) return;
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
        ImGui::PushID("TabletReforge");
        ImGui::TextWrapped("Choose tablet types to process. Tablets matching a type's Edit filters are kept; other eligible tablets are reforged in same-category and same-rarity groups of three.");
        ImGui::BeginDisabled(controller_.Busy());
        const char* activeName = ActiveProfile().name.c_str();
        if (ImGui::BeginCombo("Filter profile", activeName)) {
            for (int i = 0; i < static_cast<int>(profiles_.size()); ++i) {
                const bool selected = i == activeProfile_;
                if (ImGui::Selectable(profiles_[static_cast<size_t>(i)].name.c_str(), selected)) {
                    activeProfile_ = i;
                    editingType_.clear();
                    SetProfileNameBuffer();
                    SaveSettings();
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        auto& profile = ActiveProfile();
        if (ImGui::InputText("Profile name", profileName_, sizeof(profileName_)))
            profile.name = TabletHelperConfig::TruncateUtf8(profileName_, TabletHelperConfig::kMaxProfileNameLen);
        if (ImGui::IsItemDeactivatedAfterEdit()) SaveSettings();
        ImGui::BeginDisabled(profiles_.size() >= TabletHelperConfig::kMaxProfiles);
        if (ImGui::Button("New")) {
            auto created = NewDefaultProfile(UniqueProfileName("Profile"));
            profiles_.push_back(std::move(created));
            activeProfile_ = static_cast<int>(profiles_.size()) - 1;
            editingType_.clear(); SetProfileNameBuffer(); SaveSettings();
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate")) {
            auto copy = ActiveProfile();
            copy.name = UniqueProfileName(copy.name + " Copy");
            profiles_.push_back(std::move(copy));
            activeProfile_ = static_cast<int>(profiles_.size()) - 1;
            editingType_.clear(); SetProfileNameBuffer(); SaveSettings();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(profiles_.size() <= 1);
        bool deleteProfileRequested = false;
        if (ImGui::Button("Delete")) {
            deleteProfileRequested = true;
        }
        ImGui::EndDisabled();
        if (ImGui::Checkbox("All tablets in inventory", &allTablets_)) SaveSettings();
        ImGui::Text("Rarities to process");
        if (ImGui::Checkbox("Normal", &profile.normal)) SaveSettings();
        ImGui::SameLine();
        if (ImGui::Checkbox("Magic", &profile.magic)) SaveSettings();
        ImGui::SameLine();
        if (ImGui::Checkbox("Rare", &profile.rare)) SaveSettings();
        if (ImGui::SliderInt("Tablet transfer speed", &executionSpeed_, 1, 20)) SaveSettings();
        ImGui::TextDisabled("Highlights: Normal gray, Magic blue, Rare yellow. Result pickup waits 0.7 seconds.");
        ImGui::Separator();
        ImGui::TextUnformatted("Tablet types to process");
        for (auto& rule : profile.types) {
            if (rule.key == TabletHelper::TypeKeys::Global) continue;
            ImGui::PushID(rule.key.c_str());
            if (ImGui::Checkbox((std::string("Process ") + TypeLabel(rule.key)).c_str(), &rule.enabled)) SaveSettings();
            ImGui::SameLine();
            const bool open = editingType_ == rule.key;
            if (ImGui::Button(open ? "Close Edit" : "Edit")) {
                editingType_ = open ? std::string() : rule.key;
                bonusSearch_[0] = '\0';
            }
            ImGui::PopID();
        }
        auto editing = std::find_if(profile.types.begin(), profile.types.end(), [&](const auto& rule) {
            return rule.key == editingType_;
        });
        if (editing != profile.types.end()) DrawTypeEditor(*editing);
        if (ImGui::Checkbox("Highlight tablet types", &showTabletTypes_)) SaveSettings();
        ImGui::Text("Tablets to reforge: %llu", static_cast<unsigned long long>(controller_.SelectedCount()));
        const bool anyType = std::any_of(profile.types.begin(), profile.types.end(), [](const auto& rule) {
            return rule.enabled && rule.key != TabletHelper::TypeKeys::Global;
        });
        ImGui::BeginDisabled(!allTablets_ || RarityMask() == 0 || !anyType);
        if (ImGui::Button("Start reforging")) {
            const int transferDelayMs = executionSpeed_ <= 10
                ? 2100 - executionSpeed_ * 200
                : 100 - (executionSpeed_ - 10) * 9;
            const int inputPulseMs = executionSpeed_ <= 10 ? 40 : 40 - (executionSpeed_ - 10) * 2;
            controller_.RequestStart(ctx(), allTablets_, transferDelayMs, inputPulseMs);
        }
        ImGui::EndDisabled();
        if (deleteProfileRequested) {
            profiles_.erase(profiles_.begin() + activeProfile_);
            activeProfile_ = std::clamp(activeProfile_, 0, static_cast<int>(profiles_.size()) - 1);
            editingType_.clear(); SetProfileNameBuffer(); SaveSettings();
        }
        ImGui::EndDisabled();
        ImGui::TextWrapped("Open an empty Reforging Bench first. After starting, close this menu. "
            "Only trios of the same category and rarity are used. Unidentified results are never selected. Press Esc to stop.");
        ImGui::TextWrapped("%s", controller_.Status().c_str());
        ImGui::PopID();
    }
    void DrawUI() override {
        if (!running_ || !ctx()->ImGuiContext) return;
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
        controller_.Tick(ctx(), allTablets_, RarityMask(), ActiveProfile().types, DirectoryPath());
        if (!ctx()->Game.IsInGame() || !ctx()->Game.IsForeground()) return;
        auto* draw = ImGui::GetBackgroundDrawList();
        auto rarityColor = [](int rarity) -> ImU32 {
            if (rarity == 1) return IM_COL32(75, 155, 255, 255);
            if (rarity == 2) return IM_COL32(255, 220, 65, 255);
            return IM_COL32(175, 175, 175, 255);
        };
        for (const auto& outline : controller_.Outlines()) {
            const auto& r = outline.rect;
            const ImU32 color = rarityColor(outline.rarity);
            draw->AddRect(ImVec2(r.x + 2, r.y + 2), ImVec2(r.x + r.w - 2, r.y + r.h - 2), color, 2.0f, 3.0f, ImDrawFlags_None);
            draw->AddRectFilled(ImVec2(r.x + 2, r.y + 2), ImVec2(r.x + r.w - 2, r.y + r.h - 2), (color & 0x00FFFFFFu) | 0x18000000u, 2.0f);
        }
        if (showTabletTypes_) for (const auto& label : controller_.Labels()) {
            const auto& r = label.rect;
            const ImU32 color = rarityColor(label.rarity);
            auto* font = ImGui::GetFont();
            float size = (std::min)(ImGui::GetFontSize(), 12.0f);
            const float normalWidth = ImGui::CalcTextSize(label.type.c_str()).x;
            if (normalWidth > 0) size = (std::min)(size, ImGui::GetFontSize() * (r.w - 6.0f) / normalWidth);
            const ImVec2 text(r.x + 3.0f, r.y + 3.0f);
            draw->AddRect(ImVec2(r.x + 1, r.y + 1), ImVec2(r.x + r.w - 1, r.y + r.h - 1), color, 1.0f, 2.0f, ImDrawFlags_None);
            draw->AddRectFilled(ImVec2(r.x + 1, r.y + 1), ImVec2(r.x + r.w - 1, r.y + size + 6), IM_COL32(0,0,0,210));
            draw->AddText(font, size, text, color, label.type.c_str());
        }
        {
            ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_Always);
            ImGui::Begin("Tablet Reforge status", nullptr, ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextUnformatted(controller_.Status().c_str());
            if (controller_.Busy()) ImGui::TextUnformatted("Esc: stop");
            ImGui::End();
        }
    }
};
extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin() { return new TabletReforgePlugin(); }
extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* plugin) { delete plugin; }

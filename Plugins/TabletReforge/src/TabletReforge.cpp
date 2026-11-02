// Derived from TabletHelper by Omer Faruk ARPA and POEFixer. GPL-3.0.
#include "TabletReforgeController.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "../lib/nlohmann/json.hpp"
#include <fstream>
#include <stdexcept>

namespace {
using TabletHelperConfig::TypeConfig;

std::vector<TypeConfig> AllTabletTypes() {
    auto types = TabletHelperConfig::DefaultTypes();
    for (auto& type : types) {
        type.enabled = type.key != TabletHelper::TypeKeys::Global;
        type.minUsesLeft = 0;
        type.selectedBonusIds.clear();
        type.requiredBonusIds.clear();
        type.valueFilters.clear();
        type.minMatchedBonuses = 1;
        type.minRequiredBonuses = 1;
    }
    return types;
}

bool ReadBool(const nlohmann::json& settings, const char* key, bool fallback) {
    return settings.is_object() && settings.contains(key) && settings[key].is_boolean()
        ? settings[key].get<bool>() : fallback;
}

uint8_t MakeRarityMask(bool normal, bool magic, bool rare) {
    return static_cast<uint8_t>((normal ? 1 : 0) | (magic ? 2 : 0) | (rare ? 4 : 0));
}
} // namespace

POEFIXER_PLUGIN_INFO(
    "TabletReforge",
    "Tablet Reforge",
    "0.5.3",
    "TabletReforge contributors; Omer Faruk ARPA; POEFixer",
    "Reforge identified tablets by rarity in matching category trios, collect results without reusing unidentified tablets, and display tablet types.")

class TabletReforgePlugin : public PluginSDK::Plugin {
    TabletHelper::TabletReforgeController controller_;
    bool allTablets_ = true;
    bool showTabletTypes_ = true;
    bool running_ = false;
    bool normal_ = true;
    bool magic_ = true;
    bool rare_ = true;
    int executionSpeed_ = 15;
    std::vector<TypeConfig> types_ = AllTabletTypes();

    uint8_t RarityMask() const { return MakeRarityMask(normal_, magic_, rare_); }

public:
    const char* GetName() const override { return "Tablet Reforge"; }
    bool WantsOverlay() const override { return true; }

    void OnEnable(bool) override {
        if (!HostCompatible()) return;
        allTablets_ = true;
        showTabletTypes_ = true;
        normal_ = magic_ = rare_ = true;
        executionSpeed_ = 15;
        types_ = AllTabletTypes();
        try {
            std::ifstream file(DirectoryPath() / "config" / "settings.json");
            if (file) {
                const auto settings = nlohmann::json::parse(file, nullptr, false);
                if (settings.is_object()) {
                    allTablets_ = ReadBool(settings, "all_tablets", allTablets_);
                    showTabletTypes_ = ReadBool(settings, "show_tablet_types", showTabletTypes_);
                    if (settings.contains("execution_speed") && settings["execution_speed"].is_number_integer())
                        executionSpeed_ = std::clamp(settings["execution_speed"].get<int>(), 1, 20);

                    // Migrate rarity choices from the old active profile, if present.
                    const nlohmann::json* raritySettings = &settings;
                    if (settings.contains("profiles") && settings["profiles"].is_array() && !settings["profiles"].empty()) {
                        size_t index = 0;
                        if (settings.contains("active_profile") && settings["active_profile"].is_number_integer())
                            index = static_cast<size_t>(std::clamp(settings["active_profile"].get<int>(), 0,
                                static_cast<int>(settings["profiles"].size()) - 1));
                        const auto& profile = settings["profiles"][index];
                        if (profile.is_object()) raritySettings = &profile;
                    }
                    normal_ = ReadBool(*raritySettings, "normal", ReadBool(settings, "rarity_normal", normal_));
                    magic_ = ReadBool(*raritySettings, "magic", ReadBool(settings, "rarity_magic", magic_));
                    rare_ = ReadBool(*raritySettings, "rare", ReadBool(settings, "rarity_rare", rare_));
                }
            }
        } catch (...) { ctx()->Log.Warn("TabletReforge: could not read settings; defaults are active."); }
        controller_.Reset();
        running_ = true;
    }

    void OnDisable() override { running_ = false; controller_.Reset(); }

    void SaveSettings() override {
        try {
            const auto directory = DirectoryPath() / "config";
            std::filesystem::create_directories(directory);
            const auto temporary = directory / "settings.json.tmp";
            const auto destination = directory / "settings.json";
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            nlohmann::json settings{
                {"all_tablets", allTablets_},
                {"show_tablet_types", showTabletTypes_},
                {"execution_speed", executionSpeed_},
                {"rarity_normal", normal_},
                {"rarity_magic", magic_},
                {"rarity_rare", rare_}
            };
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
        ImGui::TextWrapped("Choose rarities to process. The plugin groups tablets by their category and rarity, then reforges matching trios of three.");
        ImGui::BeginDisabled(controller_.Busy());
        if (ImGui::Checkbox("All tablets in inventory", &allTablets_)) SaveSettings();
        ImGui::Text("Rarities to process");
        if (ImGui::Checkbox("Normal", &normal_)) SaveSettings();
        ImGui::SameLine();
        if (ImGui::Checkbox("Magic", &magic_)) SaveSettings();
        ImGui::SameLine();
        if (ImGui::Checkbox("Rare", &rare_)) SaveSettings();
        if (ImGui::SliderInt("Tablet transfer speed", &executionSpeed_, 1, 20)) SaveSettings();
        ImGui::TextDisabled("Highlights: Normal gray, Magic blue, Rare yellow. Result pickup waits 0.7 seconds.");
        if (ImGui::Checkbox("Highlight tablet types", &showTabletTypes_)) SaveSettings();
        ImGui::Text("Tablets to reforge: %llu", static_cast<unsigned long long>(controller_.SelectedCount()));
        ImGui::BeginDisabled(!allTablets_ || RarityMask() == 0);
        if (ImGui::Button("Start reforging")) {
            const int transferDelayMs = executionSpeed_ <= 10
                ? 2100 - executionSpeed_ * 200
                : 100 - (executionSpeed_ - 10) * 9;
            const int inputPulseMs = executionSpeed_ <= 10 ? 40 : 40 - (executionSpeed_ - 10) * 2;
            controller_.RequestStart(ctx(), allTablets_, transferDelayMs, inputPulseMs);
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        ImGui::TextWrapped("Open an empty Reforging Bench first. Only trios of the same category and rarity are used. Unidentified results are never selected. Press Esc to stop.");
        ImGui::TextWrapped("%s", controller_.Status().c_str());
        ImGui::PopID();
    }

    void DrawUI() override {
        if (!running_ || !ctx()->ImGuiContext) return;
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
        controller_.Tick(ctx(), allTablets_, RarityMask(), types_, DirectoryPath());
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
        ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_Always);
        ImGui::Begin("Tablet Reforge status", nullptr, ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted(controller_.Status().c_str());
        if (controller_.Busy()) ImGui::TextUnformatted("Esc: stop");
        ImGui::End();
    }
};

extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin() { return new TabletReforgePlugin(); }
extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* plugin) { delete plugin; }

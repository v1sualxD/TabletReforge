// Derived from TabletHelper by Omer Faruk ARPA and POEFixer. GPL-3.0.
#include "TabletReforgeController.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "../lib/nlohmann/json.hpp"
#include <fstream>
#include <stdexcept>

POEFIXER_PLUGIN_INFO(
    "TabletReforge",
    "Tablet Reforge",
    "0.3.0",
    "TabletReforge contributors; Omer Faruk ARPA; POEFixer",
    "Reforge matching inventory tablets in batches of three, collect unidentified results, and display tablet types.")

class TabletReforgePlugin : public PluginSDK::Plugin {
    TabletHelper::TabletReforgeController controller_;
    bool allTablets_ = true, showTabletTypes_ = true, running_ = false;
    int executionSpeed_ = 7;
public:
    const char* GetName() const override { return "Tablet Reforge"; }
    bool WantsOverlay() const override { return true; }
    void OnEnable(bool) override {
        if (!HostCompatible()) return;
        allTablets_ = true; showTabletTypes_ = true; executionSpeed_ = 7;
        try {
            std::ifstream file(DirectoryPath() / "config" / "settings.json");
            if (file) {
                const auto settings = nlohmann::json::parse(file, nullptr, false);
                if (settings.is_object() && settings.contains("all_tablets") && settings["all_tablets"].is_boolean())
                    allTablets_ = settings["all_tablets"].get<bool>();
                if (settings.is_object() && settings.contains("show_tablet_types") && settings["show_tablet_types"].is_boolean())
                    showTabletTypes_ = settings["show_tablet_types"].get<bool>();
                if (settings.is_object() && settings.contains("execution_speed") && settings["execution_speed"].is_number_integer())
                    executionSpeed_ = std::clamp(settings["execution_speed"].get<int>(), 1, 10);
            }
        } catch (...) { ctx()->Log.Warn("TabletReforge: could not read settings; defaults are active."); }
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
            file << nlohmann::json{{"all_tablets", allTablets_}, {"show_tablet_types", showTabletTypes_},
                {"execution_speed", executionSpeed_}}.dump(2);
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
        ImGui::TextWrapped("Reforge identified inventory tablets in matching groups of three. "
            "Unidentified tablets are never used.");
        ImGui::BeginDisabled(controller_.Busy());
        if (ImGui::Checkbox("All tablets in inventory", &allTablets_)) SaveSettings();
        if (ImGui::SliderInt("Execution speed", &executionSpeed_, 1, 10)) SaveSettings();
        ImGui::TextDisabled("1 = slow, 10 = fast (%d ms between actions)", 2100 - executionSpeed_ * 200);
        if (ImGui::Checkbox("Highlight tablet types", &showTabletTypes_)) SaveSettings();
        ImGui::Text("Eligible tablets: %llu", static_cast<unsigned long long>(controller_.SelectedCount()));
        ImGui::BeginDisabled(!allTablets_);
        if (ImGui::Button("Start reforging")) controller_.RequestStart(ctx(), allTablets_, executionSpeed_);
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        ImGui::TextWrapped("Open an empty Reforging Bench first. After starting, close this menu. "
            "Only trios of the same base and rarity are used. Unidentified results are never selected. Press Esc to stop.");
        ImGui::TextWrapped("%s", controller_.Status().c_str());
        ImGui::PopID();
    }
    void DrawUI() override {
        if (!running_ || !ctx()->ImGuiContext) return;
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
        controller_.Tick(ctx(), allTablets_, DirectoryPath());
        if (!ctx()->Game.IsInGame() || !ctx()->Game.IsForeground()) return;
        auto* draw = ImGui::GetBackgroundDrawList();
        for (const auto& r : controller_.Outlines()) {
            draw->AddRect(ImVec2(r.x + 2, r.y + 2), ImVec2(r.x + r.w - 2, r.y + r.h - 2), IM_COL32(70, 255, 120, 255), 2.0f, 3.0f, ImDrawFlags_None);
            draw->AddRectFilled(ImVec2(r.x + 2, r.y + 2), ImVec2(r.x + r.w - 2, r.y + r.h - 2), IM_COL32(70, 255, 120, 25), 2.0f);
        }
        if (showTabletTypes_) for (const auto& label : controller_.Labels()) {
            const auto& r = label.rect;
            ImU32 color = IM_COL32(220,220,220,255);
            if (label.type == "Abyss") color = IM_COL32(255,100,100,255);
            else if (label.type == "Expedition") color = IM_COL32(255,175,65,255);
            else if (label.type == "Ritual") color = IM_COL32(255,125,170,255);
            else if (label.type == "Breach") color = IM_COL32(190,120,255,255);
            else if (label.type == "Delirium") color = IM_COL32(85,220,220,255);
            else if (label.type == "Overseer") color = IM_COL32(255,225,75,255);
            else if (label.type == "Temple") color = IM_COL32(95,230,130,255);
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

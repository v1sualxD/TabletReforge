// Derived from TabletHelper by Omer Faruk ARPA and POEFixer. GPL-3.0.
#include "TabletReforgeController.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "../lib/nlohmann/json.hpp"
#include <fstream>

POEFIXER_PLUGIN_INFO(
    "TabletReforge",
    "Tablet Reforge",
    "0.2.0",
    "TabletReforge contributors; Omer Faruk ARPA; POEFixer",
    "Select inventory tablets with Ctrl+Alt, or choose all, then reforge compatible trios and collect the results.")

class TabletReforgePlugin : public PluginSDK::Plugin {
    TabletHelper::TabletReforgeController controller_;
    bool allTablets_ = false, running_ = false;
public:
    const char* GetName() const override { return "Tablet Reforge"; }
    bool WantsOverlay() const override { return true; }
    void OnEnable(bool) override {
        if (!HostCompatible()) return;
        allTablets_ = false;
        try {
            std::ifstream file(DirectoryPath() / "config" / "settings.json");
            if (file) {
                const auto settings = nlohmann::json::parse(file, nullptr, false);
                if (settings.is_object() && settings.contains("all_tablets") && settings["all_tablets"].is_boolean())
                    allTablets_ = settings["all_tablets"].get<bool>();
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
            file << nlohmann::json{{"all_tablets", allTablets_}}.dump(2);
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
        ImGui::TextWrapped("Hover an identified tablet in your inventory and hold Ctrl + Alt to toggle selection. "
            "Keep holding both keys and move over other tablets to select several. Selected tablets have a green outline.");
        ImGui::BeginDisabled(controller_.Busy());
        if (ImGui::Checkbox("All tablets in inventory", &allTablets_)) SaveSettings();
        ImGui::Text("Selected tablets: %llu", static_cast<unsigned long long>(controller_.SelectedCount()));
        ImGui::BeginDisabled(!allTablets_ && controller_.SelectedCount() < 3);
        if (ImGui::Button("Start reforging")) controller_.RequestStart(ctx(), allTablets_);
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
            draw->AddRect(ImVec2(r.x + 2, r.y + 2), ImVec2(r.x + r.w - 2, r.y + r.h - 2), IM_COL32(70, 255, 120, 255), 2, 0, 3);
            draw->AddRectFilled(ImVec2(r.x + 2, r.y + 2), ImVec2(r.x + r.w - 2, r.y + r.h - 2), IM_COL32(70, 255, 120, 25), 2);
        }
        if (controller_.Busy()) {
            ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_Always);
            ImGui::Begin("Tablet Reforge status", nullptr, ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextUnformatted(controller_.Status().c_str());
            ImGui::TextUnformatted("Esc: stop");
            ImGui::End();
        }
    }
};
extern "C" PLUGIN_API PluginSDK::Plugin* CreatePlugin() { return new TabletReforgePlugin(); }
extern "C" PLUGIN_API void DestroyPlugin(PluginSDK::Plugin* plugin) { delete plugin; }

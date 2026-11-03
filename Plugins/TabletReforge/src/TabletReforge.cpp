// Derived from TabletHelper by Omer Faruk ARPA and POEFixer. GPL-3.0.
#include "TabletReforgeController.h"
#include "TabletStashDiscovery.h"
#include "../../../POEFixer/imgui/imgui.h"
#include "../lib/nlohmann/json.hpp"
#include "lang_en.generated.h"
#include <array>
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


} // namespace

POEFIXER_PLUGIN_INFO(
    "TabletReforge",
    "Tablet Reforge",
    "0.9.9",
    "TabletReforge contributors; Omer Faruk ARPA; POEFixer",
    "Confirm each inventory tablet Ctrl-click before depositing the next tablet into stash.")

class TabletReforgePlugin : public PluginSDK::Plugin {
    TabletHelper::TabletReforgeController controller_;
    bool allTablets_ = true;
    bool running_ = false;
    int executionSpeed_ = 15;
    int minimumUses_ = 1;
    std::vector<TypeConfig> types_ = AllTabletTypes();
    std::array<char, 160> bonusSearch_{};

    uint8_t RarityMask() const { return 6; }

public:
    const char* GetName() const override { return "Tablet Reforge"; }
    bool WantsOverlay() const override { return true; }

    void OnEnable(bool) override {
        if (!HostCompatible()) return;
        allTablets_ = true;
        executionSpeed_ = 15;
        minimumUses_ = 1;
        types_ = AllTabletTypes();
        try {
            std::ifstream file(DirectoryPath() / "config" / "settings.json");
            if (file) {
                const auto settings = nlohmann::json::parse(file, nullptr, false);
                if (settings.is_object()) {
                    allTablets_ = ReadBool(settings, "all_tablets", allTablets_);
                    if (settings.contains("execution_speed") && settings["execution_speed"].is_number_integer())
                        executionSpeed_ = std::clamp(settings["execution_speed"].get<int>(), 0, 50);

                    if (settings.contains("minimum_uses") && settings["minimum_uses"].is_number_integer())
                        minimumUses_ = std::clamp(settings["minimum_uses"].get<int>(), 1, 10);
                    if (settings.contains("categories") && settings["categories"].is_object()) {
                        for (auto& type : types_)
                            type.enabled = ReadBool(settings["categories"], type.key.c_str(), type.enabled);
                    }
                    if (settings.contains("type_filters"))
                        TabletHelperConfig::ApplyTypeOverrides(types_, settings["type_filters"]);
                }
            }
        } catch (...) { ctx()->Log.Warn("TabletReforge: could not read settings; defaults are active."); }
        for (auto& type : types_)
            if (type.key != TabletHelper::TypeKeys::Global) type.minUsesLeft = minimumUses_;
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
                {"execution_speed", executionSpeed_},
                {"minimum_uses", minimumUses_}
            };
            settings["categories"] = nlohmann::json::object();
            for (const auto& type : types_)
                if (type.key != TabletHelper::TypeKeys::Global)
                    settings["categories"][type.key] = type.enabled;
            settings["type_filters"] = TabletHelperConfig::SerializeTypes(types_);
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

        ImGui::TextUnformatted("Tablet Reforge");
        ImGui::TextDisabled("Choose categories and filters, then start the full stash-to-bench cycle.");
        ImGui::Separator();

        ImGui::BeginDisabled(controller_.Busy() || controller_.StashCycleActive());
        if (ImGui::SliderInt("Transfer speed (0 = slowest)", &executionSpeed_, 0, 50)) SaveSettings();
        ImGui::TextDisabled("Speed affects inventory-to-bench transfers only. Stash tablet withdrawals use a faster fixed hover; other controls keep normal timing.");
        if (ImGui::SliderInt("Minimum uses to process", &minimumUses_, 1, 10)) {
            for (auto& type : types_)
                if (type.key != TabletHelper::TypeKeys::Global) type.minUsesLeft = minimumUses_;
            SaveSettings();
        }
        ImGui::TextDisabled("Minimum uses applies to stash withdrawal and inventory reforging.");

        ImGui::Separator();
        ImGui::TextUnformatted("Tablet categories");
        ImGui::Columns(4, "TabletReforgeCategories", false);
        for (const auto* category : TabletHelper::StashCategoryOrder) {
            const auto type = std::find_if(types_.begin(), types_.end(), [&](const auto& entry) {
                return entry.key == category;
            });
            if (type != types_.end() && ImGui::Checkbox(category, &type->enabled)) SaveSettings();
            ImGui::NextColumn();
        }
        ImGui::Columns(1);

        ImGui::EndDisabled();

        ImGui::Separator();
        ImGui::TextUnformatted("Stash keep filters");
        ImGui::TextDisabled("Tablets with selected mods stay in the stash; other eligible tablets can be withdrawn.");
        ImGui::BeginDisabled(controller_.StashCycleActive());
        ImGui::InputText("Search mods", bonusSearch_.data(), bonusSearch_.size());
        const std::string search = TabletHelper::ToLowerCopy(bonusSearch_.data());
        for (auto& type : types_) {
            if (type.key == TabletHelper::TypeKeys::Global) continue;
            const std::string header = type.key + " Tablet (" + std::to_string(type.selectedBonusIds.size()) + " keep mods)";
            if (!ImGui::CollapsingHeader(header.c_str())) continue;
            ImGui::PushID(type.key.c_str());
            if (type.selectedBonusIds.empty()) {
                ImGui::TextDisabled("No keep mods selected.");
            } else {
                const int maxMatched = (std::min)(TabletHelperConfig::kMinMatchedMax,
                    static_cast<int>(type.selectedBonusIds.size()));
                int minMatched = std::clamp(type.minMatchedBonuses, 1, maxMatched);
                if (ImGui::SliderInt("Minimum selected mods to keep", &minMatched, 1, maxMatched)) {
                    type.minMatchedBonuses = minMatched;
                    SaveSettings();
                }
                ImGui::TextDisabled("Tablets matching this many selected mods stay in the stash.");
            }
            const auto& bonuses = TabletHelper::GetBonusesFor(type.key);
            for (const auto& bonus : bonuses) {
                const char* translated = PluginSDK::I18n::Tr(bonus.LabelKey.c_str());
                const std::string label = translated ? translated : bonus.Id;
                if (!search.empty() && !TabletHelper::ContainsCI(label, search) &&
                    !TabletHelper::ContainsCI(bonus.Id, search)) continue;
                bool selected = std::find(type.selectedBonusIds.begin(), type.selectedBonusIds.end(), bonus.Id) != type.selectedBonusIds.end();
                ImGui::PushID(bonus.Id.c_str());
                if (ImGui::Checkbox(label.c_str(), &selected)) {
                    if (selected) type.selectedBonusIds.push_back(bonus.Id);
                    else {
                        type.selectedBonusIds.erase(std::remove(type.selectedBonusIds.begin(), type.selectedBonusIds.end(), bonus.Id), type.selectedBonusIds.end());
                        type.requiredBonusIds.erase(std::remove(type.requiredBonusIds.begin(), type.requiredBonusIds.end(), bonus.Id), type.requiredBonusIds.end());
                        type.valueFilters.erase(bonus.Id);
                    }
                    SaveSettings();
                }
                ImGui::PopID();
            }
            ImGui::PopID();
        }
        ImGui::EndDisabled();

        ImGui::Separator();
        ImGui::Text("Tablets to reforge: %llu", static_cast<unsigned long long>(controller_.SelectedCount()));
        ImGui::BeginDisabled(controller_.Busy() || RarityMask() == 0);
        if (ImGui::Button("Start automatic cycle", ImVec2(220, 32))) {
            const int speed = std::clamp(executionSpeed_, 0, 50);
            const int transferDelayMs = speed <= 10
                ? 2100 - speed * 200
                : std::max(10, 100 - (std::min(speed, 20) - 10) * 9);
            const int inputPulseMs = speed <= 10 ? 40
                : (speed <= 20 ? 40 - (speed - 10) * 2
                    : std::max(12, 20 - (speed - 20) / 3));
            controller_.RequestStashCycle(ctx(), types_, RarityMask(), transferDelayMs, inputPulseMs);
        }
        ImGui::EndDisabled();

        ImGui::BeginChild("TabletReforgeStatus", ImVec2(0, 64), true);
        ImGui::TextUnformatted("Status");
        ImGui::TextWrapped("%s", controller_.Status().c_str());
        ImGui::EndChild();
        ImGui::TextDisabled("Keep your character in the hideout. The single button opens your stash, selects Tablets, reforges, and returns results. Press Esc to stop.");

        ImGui::Separator();
        if (ImGui::CollapsingHeader("Diagnostics")) {
            ImGui::TextWrapped("Capture a report after an error, with the relevant game panel open. It records the current status, inventory, stash controls, visible UI and world entity paths.");
            if (!controller_.DebugCaptureActive()) {
                if (ImGui::Button("Start debug recording", ImVec2(210, 30)))
                    controller_.StartDebugCapture(DirectoryPath());
            } else {
                if (ImGui::Button("Stop recording", ImVec2(210, 30)))
                    controller_.StopDebugCapture();
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "Recording");
            }
            ImGui::TextDisabled("Live capture: config/tablet-reforge-live-debug.txt (samples every 500 ms).");
            if (ImGui::Button("Capture diagnostic report", ImVec2(210, 30)))
                controller_.CaptureStashDiagnostics(ctx(), DirectoryPath());
            ImGui::TextDisabled("Snapshot: config/tablet-reforge-diagnostics.txt.");
        }

        ImGui::PopID();
    }

    void DrawUI() override {
        if (!running_ || !ctx()->ImGuiContext) return;
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ctx()->ImGuiContext));
        controller_.Tick(ctx(), true, RarityMask(), types_, DirectoryPath());
        if (!ctx()->Game.IsInGame() || !ctx()->Game.IsForeground()) return;
        auto* draw = ImGui::GetBackgroundDrawList();
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

#pragma once

// Tablet Helper settings: plugin-owned JSON at <plugin>/config/settings.json, the
// same file and keys upstream releases wrote, so existing setups load unchanged.
//
// Load never throws and every key is read on its own with a type check: one
// damaged value keeps its default instead of aborting the rest of the file.
// Save writes a temporary file and swaps it in, so a crash mid-write cannot leave
// a truncated settings file.
//
// Model: display options shared by every profile, plus named filter profiles,
// each holding one block per tablet type and the cross-type "All types" block.

#include "TabletTypes.h"
#include "../lib/nlohmann/json.hpp"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace TabletHelperConfig {

// Control bounds: the single source shared by the settings tab and Load.
inline constexpr int   kBorderThicknessMin = 1;
inline constexpr int   kBorderThicknessMax = 8;
inline constexpr float kLabelScaleMin      = 0.5f;
inline constexpr float kLabelScaleMax      = 2.0f;
inline constexpr int   kLabelBgAlphaMin    = 0;
inline constexpr int   kLabelBgAlphaMax    = 255;
inline constexpr int   kScanIntervalMinMs  = 50;
inline constexpr int   kScanIntervalMaxMs  = 1000;
inline constexpr int   kMinUsesMax         = 10;   // ceiling of "Minimum uses left"
inline constexpr int   kMinMatchedMax      = 8;    // ceiling of the bonus-count thresholds
inline constexpr int   kValueFieldLimit    = 9999; // sanity bound of a stored min/max value

enum class HighlightMode { Border = 0, Cross = 1, Star = 2 };
enum class LabelTextMode { White = 0, TypeColor = 1 };

// Per-bonus value threshold. 0 on a side means "no limit" there; both 0 = off.
struct ValueRange {
    int min = 0;
    int max = 0;

    bool operator==(const ValueRange&) const = default;
};

// One tablet type's (or the "All types" block's) highlight rule.
struct TypeConfig {
    std::string key;          // TabletHelper::TypeKeys::* (JSON identity, ASCII; TypeNameKey names it)
    bool  enabled = true;
    float color[4] = {0.0f, 0.749f, 1.0f, 1.0f};
    int   minUsesLeft = 0;
    int   minMatchedBonuses = 1;   // threshold of the OPTIONAL pool
    int   minRequiredBonuses = 1;  // threshold of the REQUIRED pool
    std::vector<std::string> selectedBonusIds;  // empty = highlight every tablet of the type
    // Subset of selectedBonusIds forming the required pool. A tablet matches with
    // >= minRequiredBonuses of these AND >= minMatchedBonuses of the rest; each
    // threshold is clamped to its pool size.
    std::vector<std::string> requiredBonusIds;
    std::unordered_map<std::string, ValueRange> valueFilters;  // keyed by bonus Id

    bool operator==(const TypeConfig&) const = default;
};

inline std::vector<TypeConfig> DefaultTypes() {
    using namespace TabletHelper;
    auto make = [](const char* key, float r, float g, float b, bool enabled) {
        TypeConfig t;
        t.key = key;
        t.enabled = enabled;
        t.color[0] = r; t.color[1] = g; t.color[2] = b; t.color[3] = 1.0f;
        return t;
    };
    // Distinct colours so the out-of-the-box "every tablet by type" view reads at
    // a glance. "All types" is opt-in so it does not blanket-match every tablet
    // before the player gives it a filter.
    return {
        make(TypeKeys::Irradiated, 0.85f, 0.85f, 0.85f, true),
        make(TypeKeys::Breach,     0.65f, 0.30f, 0.90f, true),
        make(TypeKeys::Delirium,   0.20f, 0.80f, 0.80f, true),
        make(TypeKeys::Expedition, 0.95f, 0.55f, 0.15f, true),
        make(TypeKeys::Abyss,      0.80f, 0.15f, 0.15f, true),
        make(TypeKeys::Ritual,     0.95f, 0.35f, 0.20f, true),
        make(TypeKeys::Overseer,   0.95f, 0.80f, 0.20f, true),
        make(TypeKeys::Temple,     0.30f, 0.80f, 0.30f, true),
        make(TypeKeys::Global,     0.00f, 0.749f, 1.00f, false),
    };
}

namespace detail {

inline bool ReadBool(const nlohmann::json& j, const char* key, bool fallback) {
    const auto it = j.find(key);
    return (it != j.end() && it->is_boolean()) ? it->get<bool>() : fallback;
}
inline int ReadInt(const nlohmann::json& j, const char* key, int fallback, int lo, int hi) {
    const auto it = j.find(key);
    if (it == j.end() || !it->is_number()) return fallback;
    const double v = it->get<double>();
    if (!std::isfinite(v)) return fallback;
    return std::clamp(static_cast<int>(std::lround(std::clamp(v, -1.0e9, 1.0e9))), lo, hi);
}
inline float ReadFloat(const nlohmann::json& j, const char* key, float fallback, float lo, float hi) {
    const auto it = j.find(key);
    if (it == j.end() || !it->is_number()) return fallback;
    const double v = it->get<double>();
    if (!std::isfinite(v)) return fallback;
    return std::clamp(static_cast<float>(std::clamp(v, -1.0e9, 1.0e9)), lo, hi);
}
inline void ReadIdList(const nlohmann::json& e, const char* key, std::vector<std::string>& out) {
    const auto it = e.find(key);
    if (it == e.end() || !it->is_array()) return;
    out.clear();
    for (const auto& id : *it) {
        if (!id.is_string()) continue;
        std::string s = id.get<std::string>();
        if (s.empty() || s.size() > 128) continue;
        if (std::find(out.begin(), out.end(), s) == out.end()) out.push_back(std::move(s));
    }
}

inline TypeConfig* FindTypeIn(std::vector<TypeConfig>& types, const std::string& key) {
    for (auto& t : types)
        if (t.key == key) return &t;
    return nullptr;
}

}  // namespace detail

// Overlays saved per-type values (matched by key) onto a fresh DefaultTypes()
// list: the type list and its order stay canonical, only the
// editable fields come from JSON, each clamped to its control bounds.
inline void ApplyTypeOverrides(std::vector<TypeConfig>& types, const nlohmann::json& arr) {
    if (!arr.is_array()) return;
    for (const auto& e : arr) {
        if (!e.is_object()) continue;
        const auto keyIt = e.find("key");
        if (keyIt == e.end() || !keyIt->is_string()) continue;
        TypeConfig* t = detail::FindTypeIn(types, keyIt->get<std::string>());
        if (!t) continue;
        t->enabled = detail::ReadBool(e, "enabled", t->enabled);
        t->minUsesLeft = detail::ReadInt(e, "min_uses_left", t->minUsesLeft, 0, kMinUsesMax);
        t->minMatchedBonuses = detail::ReadInt(e, "min_matched_bonuses", t->minMatchedBonuses, 1, kMinMatchedMax);
        t->minRequiredBonuses = detail::ReadInt(e, "min_required_bonuses", t->minRequiredBonuses, 1, kMinMatchedMax);
        if (const auto c = e.find("color"); c != e.end() && c->is_array()) {
            for (int i = 0; i < 4 && i < static_cast<int>(c->size()); ++i) {
                const auto& v = (*c)[static_cast<size_t>(i)];
                if (!v.is_number()) continue;
                const double d = v.get<double>();
                if (std::isfinite(d)) t->color[i] = static_cast<float>(std::clamp(d, 0.0, 1.0));
            }
        }
        detail::ReadIdList(e, "selected_bonus_ids", t->selectedBonusIds);
        if (e.contains("required_bonus_ids")) {
            std::vector<std::string> req;
            detail::ReadIdList(e, "required_bonus_ids", req);
            // A required bonus must also be a selected one.
            t->requiredBonusIds.clear();
            for (auto& id : req)
                if (std::find(t->selectedBonusIds.begin(), t->selectedBonusIds.end(), id) != t->selectedBonusIds.end())
                    t->requiredBonusIds.push_back(std::move(id));
        }
        if (const auto vf = e.find("value_filters"); vf != e.end() && vf->is_object()) {
            t->valueFilters.clear();
            for (auto it = vf->begin(); it != vf->end(); ++it) {
                const auto& v = it.value();
                if (!v.is_array() || v.size() < 2 || it.key().empty() || it.key().size() > 128) continue;
                ValueRange r;
                if (v[0].is_number_integer()) r.min = std::clamp(v[0].get<int>(), -kValueFieldLimit, kValueFieldLimit);
                if (v[1].is_number_integer()) r.max = std::clamp(v[1].get<int>(), -kValueFieldLimit, kValueFieldLimit);
                if (r.min != 0 || r.max != 0) t->valueFilters[it.key()] = r;
            }
        }
    }
}

inline nlohmann::json SerializeTypes(const std::vector<TypeConfig>& types) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& t : types) {
        nlohmann::json e;
        e["key"] = t.key;
        e["enabled"] = t.enabled;
        e["color"] = {t.color[0], t.color[1], t.color[2], t.color[3]};
        e["min_uses_left"] = t.minUsesLeft;
        e["min_matched_bonuses"] = t.minMatchedBonuses;
        e["min_required_bonuses"] = t.minRequiredBonuses;
        e["selected_bonus_ids"] = t.selectedBonusIds;
        e["required_bonus_ids"] = t.requiredBonusIds;
        nlohmann::json vf = nlohmann::json::object();
        for (const auto& kv : t.valueFilters)
            if (kv.second.min != 0 || kv.second.max != 0) vf[kv.first] = {kv.second.min, kv.second.max};
        e["value_filters"] = std::move(vf);
        arr.push_back(std::move(e));
    }
    return arr;
}

inline constexpr int         kMaxProfiles       = 32;
inline constexpr std::size_t kMaxProfileNameLen = 63;

// A named filter profile: a complete set of per-type rules, switchable so a
// player can keep one setup per strategy.
struct Profile {
    std::string name = "Default";
    std::vector<TypeConfig> types = DefaultTypes();

    bool operator==(const Profile&) const = default;
};

// Cuts a UTF-8 string to at most maxBytes without splitting a character.
inline std::string TruncateUtf8(std::string s, std::size_t maxBytes) {
    if (s.size() <= maxBytes) return s;
    std::size_t n = maxBytes;
    while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
    s.resize(n);
    return s;
}

struct Settings {
    bool  showLabel = true;
    bool  showUsesLeft = false;
    bool  showMatchedBonuses = false;
    HighlightMode highlightMode = HighlightMode::Border;
    LabelTextMode labelTextMode = LabelTextMode::TypeColor;
    bool  labelBackground = true;
    bool  labelOutline = true;
    int   labelBackgroundAlpha = 170;
    int   borderThickness = 3;
    float labelScale = 1.0f;
    int   scanIntervalMs = 150;
    // Reads each tablet's modifiers (uses left, bonuses). Off = highlight by type
    // only; the uses-left and bonus filters are then ignored.
    bool  readMods = true;

    // Never empty (defaults and Load guarantee one profile).
    std::vector<Profile> profiles = {Profile{}};
    int activeProfile = 0;

    std::vector<TypeConfig>& ActiveTypes() {
        if (profiles.empty()) profiles.push_back(Profile{});
        if (activeProfile < 0 || activeProfile >= static_cast<int>(profiles.size())) activeProfile = 0;
        return profiles[static_cast<size_t>(activeProfile)].types;
    }
    const std::vector<TypeConfig>& ActiveTypes() const {
        static const std::vector<TypeConfig> kDefaults = DefaultTypes();
        if (profiles.empty()) return kDefaults;
        const int idx = (activeProfile >= 0 && activeProfile < static_cast<int>(profiles.size())) ? activeProfile : 0;
        return profiles[static_cast<size_t>(idx)].types;
    }

    // A profile name not used yet: base, "base 2", "base 3", ...
    std::string UniqueProfileName(const std::string& base) const {
        auto taken = [&](const std::string& n) {
            for (const auto& p : profiles)
                if (p.name == n) return true;
            return false;
        };
        const std::string root = TruncateUtf8(base.empty() ? std::string("Profile") : base, kMaxProfileNameLen - 4);
        if (!taken(root)) return root;
        for (int i = 2; i < 1000; ++i) {
            std::string n = root + " " + std::to_string(i);
            if (!taken(n)) return n;
        }
        return root;
    }

    static std::filesystem::path SettingsPath(const std::filesystem::path& pluginDir) {
        return pluginDir / "config" / "settings.json";
    }

    // Returns false only when the file exists but is not a JSON object (the
    // defaults are kept); a missing file is a normal first start.
    bool Load(const std::filesystem::path& pluginDir) {
        try {
            const auto path = SettingsPath(pluginDir);
            std::error_code ec;
            if (!std::filesystem::is_regular_file(path, ec)) return true;
            std::ifstream in(path, std::ios::binary);
            if (!in.is_open()) return false;
            const nlohmann::json j = nlohmann::json::parse(in, nullptr, /*allow_exceptions=*/false);
            in.close();
            if (j.is_discarded() || !j.is_object()) {
                // Keep the unreadable file beside the next save so a hand-edited
                // profile set is not lost without trace.
                auto corrupt = path;
                corrupt += L".corrupt";
                std::filesystem::copy_file(path, corrupt, std::filesystem::copy_options::overwrite_existing, ec);
                return false;
            }

            // "enabled" / "overlay_enabled" of upstream releases duplicated the
            // host's own Enabled switch and are ignored (and no longer written).
            showLabel = detail::ReadBool(j, "show_label", showLabel);
            showUsesLeft = detail::ReadBool(j, "show_uses_left", showUsesLeft);
            showMatchedBonuses = detail::ReadBool(j, "show_matched_bonuses", showMatchedBonuses);
            labelBackground = detail::ReadBool(j, "label_background", labelBackground);
            labelOutline = detail::ReadBool(j, "label_outline", labelOutline);
            readMods = detail::ReadBool(j, "read_mods", readMods);
            labelBackgroundAlpha = detail::ReadInt(j, "label_background_alpha", labelBackgroundAlpha,
                                                   kLabelBgAlphaMin, kLabelBgAlphaMax);
            borderThickness = detail::ReadInt(j, "border_thickness", borderThickness,
                                              kBorderThicknessMin, kBorderThicknessMax);
            labelScale = detail::ReadFloat(j, "label_scale", labelScale, kLabelScaleMin, kLabelScaleMax);
            scanIntervalMs = detail::ReadInt(j, "scan_interval_ms", scanIntervalMs,
                                             kScanIntervalMinMs, kScanIntervalMaxMs);
            highlightMode = static_cast<HighlightMode>(
                detail::ReadInt(j, "highlight_mode", static_cast<int>(highlightMode), 0, 2));
            labelTextMode = static_cast<LabelTextMode>(
                detail::ReadInt(j, "label_text_mode", static_cast<int>(labelTextMode), 0, 1));

            // Profiles: [{name, types:[...]}], each type list rebuilt from
            // DefaultTypes() and overlaid by key. A v1.0 file has a root-level
            // "types" array instead: it becomes the single "Default" profile.
            std::vector<Profile> loaded;
            if (const auto pit = j.find("profiles"); pit != j.end() && pit->is_array()) {
                for (const auto& pj : *pit) {
                    if (!pj.is_object()) continue;
                    Profile p;
                    if (const auto n = pj.find("name"); n != pj.end() && n->is_string()) {
                        std::string name = TruncateUtf8(n->get<std::string>(), kMaxProfileNameLen);
                        if (!name.empty()) p.name = std::move(name);
                    }
                    if (const auto t = pj.find("types"); t != pj.end()) ApplyTypeOverrides(p.types, *t);
                    loaded.push_back(std::move(p));
                    if (static_cast<int>(loaded.size()) >= kMaxProfiles) break;
                }
            } else if (const auto t = j.find("types"); t != j.end()) {
                Profile p;
                ApplyTypeOverrides(p.types, *t);
                loaded.push_back(std::move(p));
            }
            if (loaded.empty()) loaded.push_back(Profile{});
            profiles = std::move(loaded);
            activeProfile = detail::ReadInt(j, "active_profile", 0, 0, static_cast<int>(profiles.size()) - 1);
            return true;
        } catch (...) {
            // Unexpected content: keep what was read so far, never propagate.
            if (profiles.empty()) profiles.push_back(Profile{});
            return false;
        }
    }

    bool Save(const std::filesystem::path& pluginDir) const {
        try {
            if (pluginDir.empty()) return false;
            std::error_code ec;
            std::filesystem::create_directories(pluginDir / "config", ec);

            nlohmann::json j;
            j["show_label"] = showLabel;
            j["show_uses_left"] = showUsesLeft;
            j["show_matched_bonuses"] = showMatchedBonuses;
            j["label_background"] = labelBackground;
            j["label_outline"] = labelOutline;
            j["read_mods"] = readMods;
            j["label_background_alpha"] = labelBackgroundAlpha;
            j["border_thickness"] = borderThickness;
            j["label_scale"] = labelScale;
            j["scan_interval_ms"] = scanIntervalMs;
            j["highlight_mode"] = static_cast<int>(highlightMode);
            j["label_text_mode"] = static_cast<int>(labelTextMode);

            nlohmann::json parr = nlohmann::json::array();
            for (const auto& p : profiles) {
                nlohmann::json pj;
                pj["name"] = p.name;
                pj["types"] = SerializeTypes(p.types);
                parr.push_back(std::move(pj));
            }
            j["profiles"] = std::move(parr);
            j["active_profile"] = activeProfile;

            const std::string text = j.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
            const auto finalPath = SettingsPath(pluginDir);
            auto tmpPath = finalPath;
            tmpPath += L".tmp";
            {
                std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
                if (!out.is_open()) return false;
                out.write(text.data(), static_cast<std::streamsize>(text.size()));
                out.flush();
                if (!out.good()) {
                    out.close();
                    std::filesystem::remove(tmpPath, ec);
                    return false;
                }
            }
            if (!::MoveFileExW(tmpPath.c_str(), finalPath.c_str(),
                               MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                std::filesystem::remove(tmpPath, ec);
                return false;
            }
            return true;
        } catch (...) {
            return false;  // disk or serialisation failure must not cross the C ABI
        }
    }
};

}  // namespace TabletHelperConfig

#pragma once

// Turning one tablet's modifiers into what the matcher needs: the type, the uses
// left and the set of mod identities with their rolled values. Pure functions
// over PluginSDK data (no host calls), so the tests can drive them directly.

#include "TabletTypes.h"
#include "../../../POEFixer/plugin_sdk/PluginSDK.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace TabletHelper {

struct ParsedTablet {
    std::string typeKey = TypeKeys::Unknown;
    int usesLeft = 0;
    bool modsRead = false;                              // false: type only (mods off or not read yet)
    std::unordered_set<std::string> matchKeys;          // normalised mod Id / stat key (+ tier-stripped)
    std::unordered_map<std::string, int> matchValues;   // same keys -> rounded first value
};

inline void AddMatchKey(std::unordered_set<std::string>& keys, const std::string& raw) {
    std::string n = NormalizeIdentifier(raw);
    if (n.empty()) return;
    std::string s = StripTrailingDigits(n);
    if (!s.empty() && s != n) keys.insert(std::move(s));
    keys.insert(std::move(n));
}

inline int RoundValue(float v) {
    if (!std::isfinite(v)) return 0;
    const double d = static_cast<double>(v);
    return static_cast<int>(std::lround(d < -1.0e9 ? -1.0e9 : (d > 1.0e9 ? 1.0e9 : d)));
}

inline void AddMatchValue(std::unordered_map<std::string, int>& vals, const std::string& raw, float value) {
    std::string n = NormalizeIdentifier(raw);
    if (n.empty()) return;
    const int iv = RoundValue(value);
    std::string s = StripTrailingDigits(n);
    if (!s.empty() && s != n) vals[std::move(s)] = iv;
    vals[std::move(n)] = iv;
}

// Type from the metadata path alone (no mods read).
inline ParsedTablet ParseTypeOnly(const std::string& path, const std::string& baseType) {
    ParsedTablet p;
    p.typeKey = ClassifyFromText(path, baseType);
    return p;
}

// Full parse from a successful mod read. `mods.Valid` must be true.
inline ParsedTablet ParseWithMods(const std::string& path, const std::string& baseType,
                                  const PluginSDK::ItemMods& mods) {
    ParsedTablet p = ParseTypeOnly(path, baseType);
    p.modsRead = true;

    // Uses left = sum of the first value of the "adds N uses" IMPLICIT mods
    // (Mods.dat Id TowerAdd...). Only the implicit bucket counts: the explicit
    // "TowerAdditional..." map bonuses also contain "toweradd".
    for (const auto& m : mods.ImplicitMods) {
        if (ContainsCI(m.Name, "toweradd") || ContainsCI(m.Id, "toweradd")) {
            const int v = RoundValue(m.Value0);
            if (v > 0 && v < 1000) p.usesLeft += v;
        }
    }

    std::string implicitText;  // for the classification fallback
    auto addGroup = [&](const std::vector<PluginSDK::Mod>& g, bool implicit) {
        for (const auto& m : g) {
            AddMatchKey(p.matchKeys, m.Name);
            AddMatchKey(p.matchKeys, m.Id);
            AddMatchValue(p.matchValues, m.Name, m.Value0);
            AddMatchValue(p.matchValues, m.Id, m.Value0);
            if (implicit) {
                implicitText += ToLowerCopy(m.Id);
                implicitText += ' ';
                implicitText += ToLowerCopy(m.StatKey);
                implicitText += ' ';
            }
        }
    };
    addGroup(mods.ImplicitMods, true);
    addGroup(mods.ExplicitMods, false);
    addGroup(mods.EnchantMods, false);
    addGroup(mods.HellscapeMods, false);
    addGroup(mods.CrucibleMods, false);

    if (p.typeKey == std::string_view(TypeKeys::Unknown)) p.typeKey = ClassifyFromModText(implicitText);
    return p;
}

// Appends one tablet's raw modifiers to a diagnostics dump (ids, stat keys,
// values and the host's English stat text), for checking the catalog and the
// range table against the game after a patch.
inline void AppendModDump(std::string& s, int index, const PluginSDK::InventoryItem& item,
                          const PluginSDK::ItemMods& mods, const PluginSDK::InventoryService& inv) {
    char line[700];
    std::snprintf(line, sizeof(line), "\n#%d base='%s' path='%s' valid=%d\n", index, item.BaseTypeName.c_str(),
                  item.Path.c_str(), mods.Valid ? 1 : 0);
    s += line;
    auto group = [&](const char* tag, const std::vector<PluginSDK::Mod>& g) {
        for (const auto& m : g) {
            const std::string text = inv.FormatStat(m.StatKey, m.Value0, m.Value1);
            std::snprintf(line, sizeof(line), "  [%s] id='%s' stat='%s' v0=%.2f v1=%.2f :: %s\n", tag, m.Id.c_str(),
                          m.StatKey.c_str(), static_cast<double>(m.Value0), static_cast<double>(m.Value1), text.c_str());
            s += line;
        }
    };
    group("impl", mods.ImplicitMods);
    group("expl", mods.ExplicitMods);
    group("ench", mods.EnchantMods);
}

}  // namespace TabletHelper

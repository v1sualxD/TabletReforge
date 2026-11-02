#pragma once

// Per-mod roll ranges, used to bound the min/max value fields of the bonus
// filter. They are compiled in (PoE2 0.5.5, from poe2db), so the plugin needs no
// data file. An optional config/tablet_mod_ranges.json next to the DLL (the
// format upstream releases shipped) overrides single entries after a patch.
// Keyed by normalised mod id (+ the digit-stripped form). Only mods with a real
// range need an entry.

#include "TabletTypes.h"
#include "../lib/nlohmann/json.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace TabletHelper {

struct ModRange {
    int min = 0;
    int max = 0;
    std::string unit;  // percent | flat | seconds | none
};

struct RangeSeed { const char* id; int min; int max; const char* unit; };

inline const std::vector<RangeSeed>& BuiltInRanges() {
    static const std::vector<RangeSeed> v = {
        {"TowerDroppedItemRarityIncrease", 8, 12, "percent"},
        {"TowerMonsterEffectiveness", 10, 15, "percent"},
        {"TowerRareChestCount", 2, 3, "flat"},
        {"TowerExperienceGainIncrease", 12, 18, "percent"},
        {"TowerDroppedGoldIncrease", 25, 35, "percent"},
        {"TowerMonsterRarityIncrease", 15, 20, "percent"},
        {"TowerRarePackIncrease", 25, 35, "percent"},
        {"TowerMagicPackIncrease", 30, 40, "percent"},
        {"TowerPackSizeIncrease", 5, 7, "percent"},
        {"TowerMapDroppedMapsIncrease", 30, 40, "percent"},
        {"TowerRareMonsterSurpassing", 50, 80, "percent"},
        {"TowerMapAdditionalModifier", 1, 2, "flat"},
        {"TowerAdditionalAzmeriWisp", 1, 2, "flat"},
        {"TowerAdditionalEssence", 1, 2, "flat"},
        {"TowerAdditionalShrine", 1, 2, "flat"},
        {"TowerAdditionalStrongbox", 1, 2, "flat"},
        {"TowerAdditionalExileChance", 70, 100, "percent"},
        {"TowerAdditionalSpiritChance", 70, 100, "percent"},
        {"TowerAdditionalEssenceChance", 70, 100, "percent"},
        {"TowerAdditionalShrineChance", 70, 100, "percent"},
        {"TowerAdditionalStrongboxChance", 70, 100, "percent"},
        {"TowerStoneCircleChance", 70, 100, "percent"},

        {"TowerMapBossExperience", 40, 80, "percent"},
        {"TowerMapBossWaystoneChance", 18, 30, "percent"},
        {"TowerMapBossRarity", 35, 60, "percent"},
        {"TowerMapBossQuantity", 13, 20, "percent"},

        {"TowerBreachBossChance", 20, 50, "percent"},
        {"TowerBreachWombgiftLevelChance", 10, 30, "percent"},
        {"TowerBreachWombgiftQuantity", 30, 60, "percent"},
        {"TowerBreachHivebloodQuantity", 30, 60, "percent"},
        {"TowerBreachRareMonsterPotency", 5, 20, "percent"},
        {"TowerBreachMonsterQuantity", 5, 15, "percent"},
        {"TowerBreachAdditionalRares", 1, 3, "flat"},

        {"TowerExpeditionExplosionRadius", 5, 10, "percent"},
        {"TowerExpeditionLogbookIncrease", 15, 25, "percent"},
        {"TowerExpeditionRunicMonsters", 15, 25, "percent"},
        {"TowerExpeditionIncreasedVerisium", 15, 25, "percent"},
        {"TowerExpeditionUnearthedRares", 1, 2, "flat"},
        {"TowerExpeditionAdditionalSentinels", 1, 2, "flat"},
        {"TowerExpeditionIncreasedMonsterRarity", 15, 25, "percent"},
        {"TowerExpeditionFrozenBosses", 1, 2, "flat"},
        {"TowerExpeditionBuriedStrongboxes", 1, 2, "flat"},
        {"TowerExpeditionVaalRemnants", 1, 2, "flat"},
        {"TowerExpeditionChanceForVerisiumRemnant", 30, 40, "percent"},
        {"TowerExpeditionChanceForTwinnedElites", 30, 40, "percent"},
        {"TowerExpedition2RunicModPassoverChance", 25, 35, "percent"},
        {"UniqueExpeditionExplosionRemovesLife", 1, 1, "flat"},
        {"UniqueExpeditionTwinnedElites", 1, 1, "flat"},

        {"TowerDeliriumAdditionalShardsChance", 12, 26, "percent"},
        {"TowerDeliriumRareMonsterPause", 3, 5, "seconds"},
        {"TowerDeliriumDoodadsIncrease", 15, 30, "percent"},
        {"TowerDeliriumPackSizeIncrease", 15, 30, "percent"},
        {"TowerDeliriumDifficultyIncrease", 15, 30, "percent"},
        {"TowerDeliriumFogPersistence", 20, 30, "percent"},
        {"TowerDeliriumFogDissipationDelayNew", 6, 12, "seconds"},
        {"TowerDeliriumMonsterSplinterIncrease", 15, 30, "percent"},
        {"TowerDeliriumBossChance", 15, 30, "percent"},

        {"TowerAbyss4AdditionalChance", 20, 40, "percent"},
        {"TowerAbyssExtraTickets", 20, 30, "percent"},
        {"TowerAbyssExtraModifiers", 20, 30, "percent"},
        {"TowerAbyssDepthsChance", 10, 20, "percent"},
        {"TowerAbyssEffectivenessPerChasm", 8, 12, "percent"},
        {"TowerAbyssRareMonsterIncrease", 1, 2, "flat"},
        {"TowerAbyssMonsterIncrease", 20, 30, "percent"},

        {"TowerRitualOmenChance", 35, 70, "percent"},
        {"TowerRitualMagicMonsters", 25, 40, "percent"},
        {"TowerRitualRareMonsters", 35, 70, "percent"},
        {"TowerRitualChanceForNoCost", 3, 6, "percent"},
        {"TowerRitualAdditionalReroll", 1, 3, "flat"},
        {"TowerRitualDeferSpeed", 25, 40, "percent"},
        {"TowerRitualDeferCostIncrease", 20, 30, "percent"},
        {"TowerRitualRerollCostIncrease", 20, 30, "percent"},
        {"TowerRitualTributeIncrease", 18, 30, "percent"},

        {"TowerIncursionRareChestChance", 30, 60, "percent"},
        {"TowerIncursionBossChance", 10, 25, "percent"},
        {"TowerIncursionTokenChance", 5, 10, "percent"},
        {"TowerIncursionSecondaryEncounters", 25, 50, "percent"},
        {"TowerIncursionExtraPacksChance", 30, 60, "percent"},
        {"TowerIncursionPackSize", 10, 30, "percent"},

        {"UniqueBreachHiveAdditionalWaves", 2, 5, "flat"},
        {"UniqueBreachUnstableAdditionalRares", 2, 5, "flat"},
        {"UniqueTowerBreachDensityIncrease", -10, 20, "percent"},
        {"UniqueDeliriumDifficultyIncrease", -10, 10, "percent"},
        {"UniqueRitualTributeCostIncrease", 10, 15, "percent"},
    };
    return v;
}

class TabletRanges {
public:
    // Built-in table, then the optional override file. Never throws. Returns a
    // one-line English summary for the log (the Diagnostics card words the counts
    // in the program language).
    std::string Load(const std::filesystem::path& pluginDir) {
        m_ranges.clear();
        for (const auto& s : BuiltInRanges()) Insert(s.id, ModRange{s.min, s.max, s.unit});
        m_builtIn = m_ranges.size();
        m_overrides = 0;

        try {
            const auto path = pluginDir / "config" / "tablet_mod_ranges.json";
            std::error_code ec;
            if (std::filesystem::is_regular_file(path, ec)) {
                std::ifstream in(path, std::ios::binary);
                nlohmann::json j = nlohmann::json::parse(in, nullptr, /*allow_exceptions=*/false);
                if (!j.is_discarded() && j.is_object() && j.contains("mods") && j["mods"].is_array()) {
                    // The same id may appear twice (upstream's file listed the
                    // Breach and the common pack-size roll under one id): the
                    // entries of one file are merged into the widest range, so
                    // a value field never refuses a roll the game can produce.
                    std::unordered_map<std::string, ModRange> fileRanges;
                    for (const auto& e : j["mods"]) {
                        if (!e.is_object() || !e.contains("id") || !e["id"].is_string()) continue;
                        ModRange r;
                        if (e.contains("min") && e["min"].is_number_integer()) r.min = e["min"].get<int>();
                        if (e.contains("max") && e["max"].is_number_integer()) r.max = e["max"].get<int>();
                        if (e.contains("unit") && e["unit"].is_string()) r.unit = e["unit"].get<std::string>();
                        if (r.max < r.min) std::swap(r.min, r.max);
                        const std::string id = e["id"].get<std::string>();
                        auto it = fileRanges.find(id);
                        if (it == fileRanges.end()) {
                            fileRanges.emplace(id, r);
                        } else {
                            it->second.min = (std::min)(it->second.min, r.min);
                            it->second.max = (std::max)(it->second.max, r.max);
                        }
                    }
                    for (const auto& [id, r] : fileRanges) Insert(id, r);
                    m_overrides = static_cast<int>(fileRanges.size());
                }
            }
        } catch (...) {
            // A damaged override file keeps the built-in table.
        }

        char buf[128];
        if (m_overrides > 0)
            std::snprintf(buf, sizeof(buf), "Tablet Helper: %zu built-in mod ranges, %d overridden by tablet_mod_ranges.json.",
                          m_builtIn, m_overrides);
        else
            std::snprintf(buf, sizeof(buf), "Tablet Helper: %zu built-in mod ranges.", m_builtIn);
        m_summary = buf;
        return m_summary;
    }

    const std::string& Summary() const { return m_summary; }   // English, for the log
    std::size_t BuiltInCount() const { return m_builtIn; }
    int OverrideCount() const { return m_overrides; }

    const ModRange* For(const std::string& normId) const {
        auto it = m_ranges.find(normId);
        return it == m_ranges.end() ? nullptr : &it->second;
    }

private:
    void Insert(const std::string& id, const ModRange& r) {
        std::string n = NormalizeIdentifier(id);
        if (n.empty()) return;
        m_ranges[n] = r;
        std::string s = StripTrailingDigits(n);
        if (!s.empty() && s != n) m_ranges[s] = r;
    }

    std::unordered_map<std::string, ModRange> m_ranges;
    std::size_t m_builtIn = 0;
    int m_overrides = 0;
    std::string m_summary;
};

}  // namespace TabletHelper

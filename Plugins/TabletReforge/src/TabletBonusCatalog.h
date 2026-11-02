#pragma once

// Precursor Tablet bonus catalog.
// A bonus maps a stable internal mod Id (e.g. "TowerBreachBossChance") to the
// lang/en.json keys of its label and category. Matching is by internal mod
// identity only (never by translated tooltip text), because several bonuses can
// share wording.
//
// Label keys are built here: "bonus_<Id>", or "bonus_<Id>_<type>" where a type
// words a common bonus its own way (lang/dynamic_prefixes.txt lists "bonus_", and
// tests/test_logic.cpp checks that every key built exists in lang/en.json). The
// English labels in lang/en.json follow the in-game tooltip text (patch 0.5.5).
//
// Per-type list = that type's specific bonuses + the shared common bonuses,
// de-duplicated by Id (first wins, so a type may override a common label) and
// ordered by category, then Id; the settings tab sorts each category by its
// label in the program language. Global = the union across every type.

#include "TabletTypes.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace TabletHelper {

struct Bonus {
    std::string Id;
    std::string LabelKey;          // lang/en.json key of the label ("bonus_<Id>")
    const char* CategoryKey = "";  // lang/en.json key of the category (static string)
    int CategoryRank = 0;          // the categories' display order
    std::string NormId;          // NormalizeIdentifier(Id)
    std::string NormIdStripped;  // StripTrailingDigits(NormId)
};

// A bonus is present on a tablet if its normalized Id (or the trailing-digit-
// stripped form, to collapse tier variants) is in the tablet's mod key set.
inline bool BonusMatches(const Bonus& b,
                         const std::unordered_set<std::string>& keys) {
    if (keys.find(b.NormId) != keys.end()) return true;
    if (!b.NormIdStripped.empty() && b.NormIdStripped != b.NormId
        && keys.find(b.NormIdStripped) != keys.end())
        return true;
    return false;
}

namespace detail {

enum Category { kCommon = 0, kMechanic = 1, kUnique = 2 };

inline const char* CategoryKey(Category c) {
    switch (c) {
    case kCommon:   return Key("category_common");
    case kMechanic: return Key("category_mechanic");
    case kUnique:   return Key("category_unique");
    }
    return Key("category_common");
}

// A bonus labelled "bonus_<id>", or "bonus_<id>_<labelSuffix>" where a type words a
// shared bonus its own way.
inline Bonus B(const char* id, Category category, const char* labelSuffix = nullptr) {
    Bonus b;
    b.Id = id;
    b.LabelKey = std::string("bonus_") + id;
    if (labelSuffix) b.LabelKey += std::string("_") + labelSuffix;
    b.CategoryKey = CategoryKey(category);
    b.CategoryRank = static_cast<int>(category);
    b.NormId = NormalizeIdentifier(id);
    b.NormIdStripped = StripTrailingDigits(b.NormId);
    return b;
}

inline const std::vector<Bonus>& CommonBonuses() {
    static const std::vector<Bonus> v = {
        B("TowerDroppedItemRarityIncrease", kCommon),
        B("TowerMapDroppedMapsIncrease", kCommon),
        B("TowerDroppedGoldIncrease", kCommon),
        B("TowerExperienceGainIncrease", kCommon),
        B("TowerMonsterEffectiveness", kCommon),
        B("TowerMonsterRarityIncrease", kCommon),
        B("TowerRarePackIncrease", kCommon),
        B("TowerMagicPackIncrease", kCommon),
        B("TowerPackSizeIncrease", kCommon),
        B("TowerRareMonsterSurpassing", kCommon),
        B("TowerReducedPackSize", kCommon),
        B("TowerRareChestCount", kCommon),
        B("TowerAdditionalStoneCircle", kCommon),
        B("TowerAdditionalExile", kCommon),
        B("TowerAdditionalAzmeriWisp", kCommon),
        B("TowerAdditionalEssence", kCommon),
        B("TowerAdditionalShrine", kCommon),
        B("TowerAdditionalStrongbox", kCommon),
        B("TowerStoneCircleChance", kCommon),
        B("TowerAdditionalExileChance", kCommon),
        B("TowerAdditionalSpiritChance", kCommon),
        B("TowerAdditionalEssenceChance", kCommon),
        B("TowerAdditionalShrineChance", kCommon),
        B("TowerAdditionalStrongboxChance", kCommon),
        B("TowerMapAdditionalModifier", kCommon),
        B("TowerMapAdditionalUniqueMonsterModifier", kCommon),
    };
    return v;
}

inline std::vector<Bonus> SpecificBonuses(const std::string& typeKey) {
    if (typeKey == TypeKeys::Irradiated) {
        return {
            B("UniqueBiomeTabletForest", kUnique),
            B("UniqueBiomeTabletMountain", kUnique),
            B("UniqueBiomeTabletWater", kUnique),
            B("UniqueBiomeTabletDesert", kUnique),
            B("UniqueBiomeTabletGrass", kUnique),
            B("UniqueMapsAddIrridiationWhenCompleting", kUnique),
        };
    }
    if (typeKey == TypeKeys::Breach) {
        return {
            B("TowerPackSizeIncrease", kCommon, "breach"),
            B("TowerBreachAdditionalRares", kMechanic),
            B("TowerBreachBossChance", kMechanic),
            B("TowerBreachWombgiftLevelChance", kMechanic),
            B("TowerBreachWombgiftQuantity", kMechanic),
            B("TowerBreachHivebloodQuantity", kMechanic),
            B("TowerBreachRareMonsterPotency", kMechanic),
            B("TowerBreachMonsterQuantity", kMechanic),
            B("UniqueBreachHiveAdditionalWaves", kUnique),
            B("UniqueBreachMinimumRadius", kUnique),
            B("UniqueBreachUnstableAdditionalRares", kUnique),
            B("UniqueTowerBreachDensityIncrease", kUnique),
        };
    }
    if (typeKey == TypeKeys::Delirium) {
        return {
            B("TowerDeliriumAdditionalShardsChance", kMechanic),
            B("TowerDeliriumRareMonsterPause", kMechanic),
            B("TowerDeliriumDoodadsIncrease", kMechanic),
            B("TowerDeliriumPackSizeIncrease", kMechanic),
            B("TowerDeliriumDifficultyIncrease", kMechanic),
            B("TowerDeliriumFogPersistence", kMechanic),
            B("TowerDeliriumFogDissipationDelayNew", kMechanic),
            B("TowerDeliriumMonsterSplinterIncrease", kMechanic),
            B("TowerDeliriumBossChance", kMechanic),
            B("UniqueDeliriumDifficultyIncrease", kUnique),
            B("UniqueDeliriumEndlessFog", kUnique),
        };
    }
    if (typeKey == TypeKeys::Expedition) {
        return {
            B("TowerExpeditionExplosionRadius", kMechanic),
            B("TowerExpeditionLogbookIncrease", kMechanic),
            B("TowerExpeditionRunicMonsters", kMechanic),
            B("TowerExpeditionIncreasedVerisium", kMechanic),
            B("TowerExpeditionUnearthedRares", kMechanic),
            B("TowerExpeditionAdditionalSentinels", kMechanic),
            B("TowerExpeditionIncreasedMonsterRarity", kMechanic),
            B("TowerExpeditionFrozenBosses", kMechanic),
            B("TowerExpeditionBuriedStrongboxes", kMechanic),
            B("TowerExpeditionVaalRemnants", kMechanic),
            B("TowerExpeditionChanceForVerisiumRemnant", kMechanic),
            B("TowerExpeditionChanceForTwinnedElites", kMechanic),
            B("TowerExpedition2RunicModPassoverChance", kMechanic),
            B("UniqueExpeditionExplosionRemovesLife", kUnique),
            B("UniqueExpeditionTwinnedElites", kUnique),
        };
    }
    if (typeKey == TypeKeys::Abyss) {
        return {
            B("TowerAbyssAdditionalChance", kMechanic),
            B("TowerAbyss4AdditionalChance", kMechanic),
            B("TowerAbyssExtraTickets", kMechanic),
            B("TowerAbyssExtraModifiers", kMechanic),
            B("TowerAbyssIncreasedRewards", kMechanic),
            B("TowerAbyssDepthsChance", kMechanic),
            B("TowerAbyssEffectivenessPerChasm", kMechanic),
            B("TowerAbyssRareMonsterIncrease", kMechanic),
            B("TowerAbyssMonsterIncrease", kMechanic),
        };
    }
    if (typeKey == TypeKeys::Ritual) {
        return {
            B("TowerRitualOmenChance", kMechanic),
            B("TowerRitualMagicMonsters", kMechanic),
            B("TowerRitualRareMonsters", kMechanic),
            B("TowerRitualChanceForNoCost", kMechanic),
            B("TowerRitualAdditionalReroll", kMechanic),
            B("TowerRitualDeferSpeed", kMechanic),
            B("TowerRitualDeferCostIncrease", kMechanic),
            B("TowerRitualRerollCostIncrease", kMechanic),
            B("TowerRitualTributeIncrease", kMechanic),
            B("UniqueRitualTributeCostIncrease", kUnique),
            B("UniqueRitualUnlimitedRerolls", kUnique),
        };
    }
    if (typeKey == TypeKeys::Overseer) {
        return {
            B("TowerMapBossExperience", kMechanic),
            B("TowerMapBossWaystoneChance", kMechanic),
            B("TowerMapBossRarity", kMechanic),
            B("TowerMapBossQuantity", kMechanic),
            B("TowerMapBossAdditionalSpirit", kMechanic),
            B("TowerMapBossAdditionalEssence", kMechanic),
            B("TowerMapBossAdditionalShrine", kMechanic),
            B("TowerMapBossAdditionalStrongbox", kMechanic),
            B("UniqueMapBossAdditionalModifier", kUnique),
            B("UniqueMapBossPossession", kUnique),
        };
    }
    if (typeKey == TypeKeys::Temple) {
        return {
            B("TowerIncursionRareChestChance", kMechanic),
            B("TowerIncursionBossChance", kMechanic),
            B("TowerIncursionTokenChance", kMechanic),
            B("TowerIncursionSecondaryEncounters", kMechanic),
            B("TowerIncursionExtraPacksChance", kMechanic),
            B("TowerIncursionExtraPacks", kMechanic),
            B("TowerIncursionPackSize", kMechanic),
        };
    }
    return {};
}

// De-duplicate by Id (case-insensitive, first wins) then order by category, then Id.
inline std::vector<Bonus> DistinctSorted(std::vector<Bonus> in) {
    std::vector<Bonus> out;
    std::unordered_set<std::string> seen;
    for (auto& b : in) {
        std::string idLow = ToLowerCopy(b.Id);
        if (!seen.insert(idLow).second) continue;
        out.push_back(std::move(b));
    }
    std::sort(out.begin(), out.end(), [](const Bonus& a, const Bonus& b) {
        if (a.CategoryRank != b.CategoryRank) return a.CategoryRank < b.CategoryRank;
        return a.Id < b.Id;
    });
    return out;
}

inline const std::unordered_map<std::string, std::vector<Bonus>>& Catalog() {
    static const std::unordered_map<std::string, std::vector<Bonus>> cat = [] {
        std::unordered_map<std::string, std::vector<Bonus>> m;
        std::vector<Bonus> unionAll;
        for (const char* key : kRealTypeKeys) {
            std::vector<Bonus> list = SpecificBonuses(key);
            const auto& common = CommonBonuses();
            list.insert(list.end(), common.begin(), common.end());
            auto sorted = DistinctSorted(std::move(list));
            unionAll.insert(unionAll.end(), sorted.begin(), sorted.end());
            m.emplace(key, std::move(sorted));
        }
        m.emplace(TypeKeys::Global, DistinctSorted(std::move(unionAll)));
        return m;
    }();
    return cat;
}

}  // namespace detail

// Bonuses selectable for a given tablet type (or Global). Empty if the key is
// not a known tablet type.
inline const std::vector<Bonus>& GetBonusesFor(const std::string& typeKey) {
    static const std::vector<Bonus> kEmpty;
    const auto& cat = detail::Catalog();
    auto it = cat.find(typeKey);
    return it != cat.end() ? it->second : kEmpty;
}

inline bool IsKnownType(const std::string& key) {
    return detail::Catalog().find(key) != detail::Catalog().end();
}

// Look up a single bonus definition by Id within a type's (or Global's) list.
// Hashed by normalised id, built once (the matcher calls this for every selected
// bonus of every tablet it evaluates).
inline const Bonus* FindBonus(const std::string& typeKey, const std::string& id) {
    using Index = std::unordered_map<std::string, std::unordered_map<std::string, const Bonus*>>;
    static const Index index = [] {
        Index m;
        for (const auto& [key, list] : detail::Catalog()) {
            auto& byId = m[key];
            for (const auto& b : list) byId.emplace(b.NormId, &b);
        }
        return m;
    }();
    if (id.empty()) return nullptr;
    const auto typeIt = index.find(typeKey);
    if (typeIt == index.end()) return nullptr;
    const auto it = typeIt->second.find(NormalizeIdentifier(id));
    return it == typeIt->second.end() ? nullptr : it->second;
}

}  // namespace TabletHelper

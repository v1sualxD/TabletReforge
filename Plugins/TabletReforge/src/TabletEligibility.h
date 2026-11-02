#pragma once
#include "ReforgePlan.h"


namespace TabletHelper {
inline bool EligibleTablet(const ReforgeItem& item) {
    return item.address && item.identified && !item.corrupted && !item.sanctified && item.rarity >= 1 &&
        item.rarity <= 2 && LooksLikeTablet(item.path, item.baseType) &&
        IsRealType(TabletCategory(item));
}

// Rarity values follow the game's item rarity enum: 0 = Normal, 1 = Magic, 2 = Rare.
inline bool EligibleTablet(const ReforgeItem& item, uint8_t rarityMask) {
    return EligibleTablet(item) && (rarityMask & (uint8_t{1} << item.rarity)) != 0;
}
} // namespace TabletHelper

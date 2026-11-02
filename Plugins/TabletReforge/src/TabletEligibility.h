#pragma once
#include "ReforgePlan.h"


namespace TabletHelper {
inline bool EligibleTablet(const ReforgeItem& item) {
    return item.address && item.identified && !item.corrupted && item.rarity >= 0 &&
        item.rarity <= 2 && LooksLikeTablet(item.path, item.baseType) &&
        IsRealType(TabletCategory(item));
}
} // namespace TabletHelper

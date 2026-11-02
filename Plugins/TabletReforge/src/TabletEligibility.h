#pragma once
#include "ReforgePlan.h"

namespace TabletHelper {
inline bool EligibleTablet(const ReforgeItem& item) {
    return item.address && item.identified && !item.corrupted && item.rarity >= 0 &&
        item.rarity <= 2 && ContainsCI(item.path, "toweraugment");
}
} // namespace TabletHelper

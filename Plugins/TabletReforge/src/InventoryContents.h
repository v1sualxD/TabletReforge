#pragma once
#include "ReforgePlan.h"

namespace TabletHelper {
inline bool SameTabletProperties(const ReforgeItem& a, const ReforgeItem& b) {
    // POEFixer can rebuild item objects while rescanning. Runtime addresses and
    // grid coordinates are transient; base/path and item properties are stable.
    return a.path == b.path && a.baseType == b.baseType && a.rarity == b.rarity &&
        a.level == b.level && a.identified == b.identified &&
        a.corrupted == b.corrupted && a.sanctified == b.sanctified;
}
inline bool SameBagContents(const std::map<uintptr_t, ReforgeItem>& a,
                            const std::map<uintptr_t, ReforgeItem>& b) {
    if (a.size() != b.size()) return false;
    bool sameAddresses = true;
    for (const auto& [address, item] : a) {
        const auto found = b.find(address);
        if (found == b.end() || !SameTabletProperties(item, found->second)) {
            sameAddresses = false; break;
        }
    }
    if (sameAddresses) return true;
    std::vector<const ReforgeItem*> unmatched;
    unmatched.reserve(b.size());
    for (const auto& [address, item] : b) { (void)address; unmatched.push_back(&item); }
    for (const auto& [address, item] : a) {
        (void)address;
        const auto found = std::find_if(unmatched.begin(), unmatched.end(),
            [&](const ReforgeItem* other) { return SameTabletProperties(item, *other); });
        if (found == unmatched.end()) return false;
        unmatched.erase(found);
    }
    return unmatched.empty();
}
} // namespace TabletHelper

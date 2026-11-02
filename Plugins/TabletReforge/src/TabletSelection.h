#pragma once
#include "ReforgePlan.h"
#include <map>
#include <set>

namespace TabletHelper {
inline bool EligibleTablet(const ReforgeItem& item) {
    return item.address && item.identified && !item.corrupted && item.rarity >= 0 &&
        item.rarity <= 2 && ContainsCI(item.path, "toweraugment");
}
inline bool SameTablet(const ReforgeItem& a, const ReforgeItem& b) {
    return a.address == b.address && a.path == b.path && a.rarity == b.rarity &&
        a.level == b.level && a.identified == b.identified && a.corrupted == b.corrupted;
}
class TabletSelection {
    std::map<uintptr_t, ReforgeItem> selected_;
    std::set<uintptr_t> touched_;
public:
    void Clear() { selected_.clear(); touched_.clear(); }
    void ReleaseChord() { touched_.clear(); }
    size_t Count() const { return selected_.size(); }
    bool Contains(const ReforgeItem& item) const {
        const auto it = selected_.find(item.address);
        return it != selected_.end() && SameTablet(it->second, item) && EligibleTablet(item);
    }
    void Touch(const ReforgeItem& item) {
        if (!EligibleTablet(item) || !touched_.insert(item.address).second) return;
        if (Contains(item)) selected_.erase(item.address);
        else selected_[item.address] = item;
    }
    void Refresh(const std::map<uintptr_t, ReforgeItem>& bag) {
        for (auto it = selected_.begin(); it != selected_.end();) {
            const auto current = bag.find(it->first);
            if (current == bag.end() || !EligibleTablet(current->second) || !SameTablet(current->second, it->second))
                it = selected_.erase(it);
            else { it->second = current->second; ++it; }
        }
    }
    std::vector<ReforgeItem> Candidates(const std::map<uintptr_t, ReforgeItem>& bag, bool all) const {
        std::vector<ReforgeItem> items;
        for (const auto& [address, item] : bag) {
            (void)address;
            if (EligibleTablet(item) && (all || Contains(item))) items.push_back(item);
        }
        return items;
    }
};
} // namespace TabletHelper

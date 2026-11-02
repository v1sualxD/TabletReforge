#pragma once
#include "TabletTypes.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <tuple>
#include <vector>

namespace TabletHelper {
struct ReforgeItem {
    uintptr_t address = 0;
    std::string path;
    int rarity = 0, level = 0, x = 0, y = 0;
    bool identified = false, corrupted = false;
    bool operator==(const ReforgeItem&) const = default;
};
using ReforgeBatch = std::array<ReforgeItem, 3>;

// A failed read or an unknown/disabled category must never become a rejection.
inline bool CanReforgeRejected(bool identified, bool modsRead, bool knownType,
                               bool categoryEnabled, bool matchesKeepRules) {
    return identified && modsRead && knownType && categoryEnabled && !matchesKeepRules;
}

// Freeze the initial inventory: results are collected but never put back in the queue.
// Caller supplies only tablets rejected by the keep filters. Match exact base/rarity.
inline std::vector<ReforgeBatch> PlanReforge(std::vector<ReforgeItem> items) {
    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
        return std::tie(a.y, a.x, a.address) < std::tie(b.y, b.x, b.address);
    });
    std::map<std::pair<std::string, int>, std::vector<ReforgeItem>> groups;
    std::set<uintptr_t> seen;
    for (const auto& item : items) {
        if (!item.address || !seen.insert(item.address).second || item.corrupted ||
            !item.identified || item.rarity < 0 || item.rarity > 2 ||
            !ContainsCI(item.path, "toweraugment")) continue;
        groups[{item.path, item.rarity}].push_back(item);
    }
    std::vector<ReforgeBatch> batches;
    for (const auto& [key, group] : groups) {
        (void)key;
        for (size_t i = 0; i + 2 < group.size(); i += 3)
            batches.push_back({group[i], group[i + 1], group[i + 2]});
    }
    return batches;
}
} // namespace TabletHelper

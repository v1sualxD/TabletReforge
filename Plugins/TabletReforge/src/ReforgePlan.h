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
    std::string baseType;
    int rarity = 0, level = 0, x = 0, y = 0;
    bool identified = false, corrupted = false;
    bool operator==(const ReforgeItem&) const = default;
};
using ReforgeBatch = std::array<ReforgeItem, 3>;

inline std::string TabletCategory(const ReforgeItem& item) {
    return ClassifyFromText(item.path, item.baseType);
}


// Freeze the initial inventory: results are collected but never put back in the queue.
// Group by the visible tablet category and rarity; metadata paths can vary within a category.
inline std::vector<ReforgeBatch> PlanReforge(std::vector<ReforgeItem> items) {
    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
        return std::tie(a.y, a.x, a.address) < std::tie(b.y, b.x, b.address);
    });
    std::map<std::pair<std::string, int>, std::vector<ReforgeItem>> groups;
    std::set<uintptr_t> seen;
    for (const auto& item : items) {
        if (!item.address || !seen.insert(item.address).second || item.corrupted ||
            !item.identified || item.rarity < 0 || item.rarity > 2 ||
            !LooksLikeTablet(item.path, item.baseType)) continue;
        const std::string category = TabletCategory(item);
        if (!IsRealType(category)) continue;
        groups[{category, item.rarity}].push_back(item);
    }
    std::vector<ReforgeBatch> batches;
    for (const auto& [key, group] : groups) {

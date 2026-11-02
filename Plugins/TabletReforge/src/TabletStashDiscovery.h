#pragma once
#include "ReforgeUiDiscovery.h"
#include <array>
#include <limits>
#include <set>

namespace TabletHelper {

// POE2's Tablet tab has eight tablet categories followed by a non-tablet
// control. Keep this list in the order confirmed from the user's stash UI.
inline constexpr std::array<const char*, 8> StashCategoryOrder{
    "Irradiated", "Ritual", "Expedition", "Delirium",
    "Breach", "Abyss", "Temple", "Overseer"};

struct TabletStashSlot {
    int x = -1;
    int y = -1;
    BenchNode cell;
};

struct TabletStashTargets {
    BenchNode panel;
    BenchNode grid;
    BenchNode tabletTab;
    std::vector<BenchNode> categories;
    std::vector<BenchNode> pages;
    std::vector<TabletStashSlot> occupiedSlots;
    float cellSize = 0;
};

enum class TabletStashPageMatchState { Matched, Empty, Missing, Ambiguous };
struct TabletStashPageMatch {
    TabletStashPageMatchState state = TabletStashPageMatchState::Missing;
    PluginSDK::Inventory inventory;
};

// POEFixer exposes stash items without per-item screen rectangles. The active
// stash page is identified by its live inventory-grid geometry; inactive tablet
// categories/pages have Grid.Valid=false. Do not use occupiedSlots here: those
// are a UI snapshot captured before category/page navigation and become stale.
inline TabletStashPageMatch MatchVisibleStashPage(const TabletStashTargets& targets,
    const std::vector<PluginSDK::Inventory>& inventories, const std::string& category) {
    std::optional<PluginSDK::Inventory> match;
    for (const auto& inventory : inventories) {
        if (inventory.TotalBoxesX != 12 || inventory.TotalBoxesY != 8 || !inventory.Grid.Valid)
            continue;
        const auto withinTolerance = [](float a, float b) { return std::abs(a - b) <= 2.5f; };
        if (!withinTolerance(inventory.Grid.GridScreenX, targets.grid.x) ||
            !withinTolerance(inventory.Grid.GridScreenY, targets.grid.y) ||
            !withinTolerance(inventory.Grid.CellSize, targets.cellSize)) continue;
        if (match) return {TabletStashPageMatchState::Ambiguous, {}};
        match = inventory;
    }
    if (!match) return {TabletStashPageMatchState::Missing, {}};
    if (match->Items.empty()) return {TabletStashPageMatchState::Empty, {}};

    // The active grid geometry uniquely identifies the visible inventory. Verify
    // its contents before allowing any cursor input.
    for (const auto& item : match->Items) {
        if (!LooksLikeTablet(item.Path, item.BaseTypeName) ||
            ClassifyFromText(item.Path, item.BaseTypeName) != category ||
            item.SlotX < 0 || item.SlotX >= 12 || item.SlotY < 0 || item.SlotY >= 8)
            return {TabletStashPageMatchState::Missing, {}};
    }
    return {TabletStashPageMatchState::Matched, std::move(*match)};
}

inline std::optional<POINT> TabletStashSlotCenter(const TabletStashTargets& targets, int x, int y) {
    if (x < 0 || x >= 12 || y < 0 || y >= 8 || targets.cellSize <= 1.f) return std::nullopt;
    const float px = targets.grid.x + (static_cast<float>(x) + .5f) * targets.cellSize;
    const float py = targets.grid.y + (static_cast<float>(y) + .5f) * targets.cellSize;
    return POINT{static_cast<LONG>(std::lround(px)), static_cast<LONG>(std::lround(py))};
}

inline bool SameControl(const BenchNode& a, const BenchNode& b, float tolerance = 3.f) {
    return std::abs(a.CenterX() - b.CenterX()) <= tolerance &&
        std::abs(a.CenterY() - b.CenterY()) <= tolerance;
}

inline bool IsNumericText(const std::string& text) {
    return !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char c) {
        return c >= '0' && c <= '9';
    });
}

// Discover controls from the live UI tree and geometry. We don't depend on
// the hideout position or window resolution. The ninth icon is deliberately
// excluded because it is not one of the eight tablet categories.
inline std::optional<TabletStashTargets> DiscoverTabletStash(
    const std::vector<BenchNode>& nodes) {
    std::optional<TabletStashTargets> found;
    for (size_t panelIndex = 0; panelIndex < nodes.size(); ++panelIndex) {
        const auto& panel = nodes[panelIndex];
        if (panel.name != "pages_container" || panel.parent < 0 || panel.w < 400 || panel.h < 250)
            continue;

        std::vector<size_t> grids;
        for (size_t i = 0; i < nodes.size(); ++i) {
            const auto& node = nodes[i];
            if (node.w < 250 || node.h < 150 || node.w / node.h < 1.42f || node.w / node.h > 1.56f)
                continue;
            if (!Descendant(nodes, i, panelIndex)) continue;
            grids.push_back(i);
        }
        if (grids.empty()) continue;

        // Prefer the innermost 12x8 rectangle. Several nested UI controls can
        // share its bounds, so the actual tablet cells below disambiguate it.
        std::optional<size_t> gridIndex;
        size_t bestCellCount = 0;
        float bestArea = (std::numeric_limits<float>::max)();
        for (const size_t candidate : grids) {
            const auto& grid = nodes[candidate];
            const float cellW = grid.w / 12.f, cellH = grid.h / 8.f;
            if (std::abs(cellW - cellH) > (std::max)(2.f, cellW * .08f)) continue;
            size_t count = 0;
            for (size_t i = 0; i < nodes.size(); ++i) {
                if (!Descendant(nodes, i, candidate) || i == candidate) continue;
                const auto& cell = nodes[i];
                if (std::abs(cell.w - cellW) <= cellW * .08f &&
                    std::abs(cell.h - cellH) <= cellH * .08f &&
                    cell.x >= grid.x - 1 && cell.y >= grid.y - 1 &&
                    cell.x + cell.w <= grid.x + grid.w + 1 &&
                    cell.y + cell.h <= grid.y + grid.h + 1) ++count;
            }
            const float area = grid.w * grid.h;
            if (!gridIndex || count > bestCellCount || (count == bestCellCount && area < bestArea)) {
                bestCellCount = count; bestArea = area; gridIndex = candidate;
            }
        }
        if (!gridIndex) continue;

        const auto& grid = nodes[*gridIndex];
        const float cell = grid.w / 12.f;
        TabletStashTargets current;
        current.panel = panel;
        current.grid = grid;
        current.cellSize = cell;

        // Stash category counters are children of their clickable square.
        for (size_t i = 0; i < nodes.size(); ++i) {
            const auto& label = nodes[i];
            if (label.parent < 0 || !IsNumericText(label.text)) continue;
            const auto& button = nodes[static_cast<size_t>(label.parent)];
            if (button.w < cell * .9f || button.w > cell * 1.35f ||
                std::abs(button.w - button.h) > cell * .15f ||
                button.CenterY() >= grid.y - cell || button.CenterY() <= grid.y - cell * 3.5f)
                continue;
            if (std::none_of(current.categories.begin(), current.categories.end(),
                [&](const auto& old) { return SameControl(old, button); })) current.categories.push_back(button);
        }
        std::sort(current.categories.begin(), current.categories.end(),
            [](const auto& a, const auto& b) { return a.CenterX() < b.CenterX(); });
        if (current.categories.size() < StashCategoryOrder.size()) continue;
        current.categories.resize(StashCategoryOrder.size());

        // Page numbers are children of the clickable page buttons. Deduplicate
        // the selected-page overlay by screen position, not transient address.
        for (size_t i = 0; i < nodes.size(); ++i) {
            const auto& label = nodes[i];
            if (label.parent < 0 || label.text.size() != 1 || label.text[0] < '1' || label.text[0] > '6')
                continue;
            int ancestor = label.parent;
            const BenchNode* buttonPtr = nullptr;
            for (int depth = 0; ancestor >= 0 && depth < 4; ++depth) {
                const auto& candidate = nodes[static_cast<size_t>(ancestor)];
                if (candidate.w >= cell * .8f && candidate.CenterY() < grid.y &&
                    candidate.CenterY() > grid.y - cell) { buttonPtr = &candidate; break; }
                ancestor = candidate.parent;
            }
            if (!buttonPtr) continue;
            const auto& button = *buttonPtr;
            if (std::none_of(current.pages.begin(), current.pages.end(),
                [&](const auto& old) { return SameControl(old, button, cell * .2f); })) current.pages.push_back(button);
        }
        std::sort(current.pages.begin(), current.pages.end(),
            [](const auto& a, const auto& b) { return a.CenterX() < b.CenterX(); });
        if (current.pages.size() != 6) continue;

        // Collect occupied grid slots from the same cell geometry. Use unique
        // logical coordinates to collapse nested controls sharing one tile.
        std::set<std::pair<int, int>> slots;
        for (size_t i = 0; i < nodes.size(); ++i) {
            const auto& tile = nodes[i];
            if (i == *gridIndex || !Descendant(nodes, i, *gridIndex) ||
                std::abs(tile.w - cell) > cell * .08f || std::abs(tile.h - cell) > cell * .08f)
                continue;
            const int x = static_cast<int>(std::lround((tile.CenterX() - grid.x) / cell - .5f));
            const int y = static_cast<int>(std::lround((tile.CenterY() - grid.y) / cell - .5f));
            if (x >= 0 && x < 12 && y >= 0 && y < 8) slots.emplace(x, y);
        }
        for (const auto& slot : slots) {
            const float x = grid.x + (slot.first + .5f) * cell;
            const float y = grid.y + (slot.second + .5f) * cell;
            const auto tile = std::find_if(nodes.begin(), nodes.end(), [&](const auto& node) {
                return std::abs(node.CenterX() - x) < cell * .1f &&
                    std::abs(node.CenterY() - y) < cell * .1f &&
                    std::abs(node.w - cell) < cell * .08f && std::abs(node.h - cell) < cell * .08f;
            });
            if (tile != nodes.end()) current.occupiedSlots.push_back({slot.first, slot.second, *tile});
        }

        // The Tablet subtab is a fixed sibling control above the category row.
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (NormalizeIdentifier(nodes[i].text) != "tablets" || nodes[i].parent < 0) continue;
            const auto& button = nodes[static_cast<size_t>(nodes[i].parent)];
            if (button.w > 70 && button.h > 22 && button.CenterY() < grid.y - cell * 2)
                current.tabletTab = button;
        }
        if (current.tabletTab.address == 0) continue;

        // Same tab may contain nested pages_container controls. Keep the
        // candidate with the most concrete occupied tiles; ambiguity is an
        // error because clicking the wrong grid can move unrelated items.
        if (!found || current.occupiedSlots.size() > found->occupiedSlots.size()) found = std::move(current);
        else if (current.occupiedSlots.size() == found->occupiedSlots.size() &&
            !SameControl(current.grid, found->grid)) return std::nullopt;
    }
    return found;
}

} // namespace TabletHelper


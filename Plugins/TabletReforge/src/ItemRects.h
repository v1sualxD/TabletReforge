#pragma once

// On-screen item rectangles. The host publishes every inventory it knows (the
// backpack, every stash tab ever opened, the open guild tab, the merchant), but
// only the inventories currently drawn carry screen geometry: a regular grid has
// Grid.Valid (its origin and cell size), a special tab (Tablet, currency,
// fragment, ...) gives each item its own screen rectangle (ScreenValid) because
// its slots are not laid out on a uniform grid. Entries the host carried over
// from an earlier scan have neither.
//
// Order matters: the per-item rectangle first, then grid maths, and grid maths
// only on an inventory that has no per-item rectangles at all (on a special tab
// the logical grid does not match the visual layout).

#include "../../../POEFixer/plugin_sdk/PluginSDK.h"

#include <optional>

namespace TabletHelper {

struct ScreenRect {
    float x = 0.f;
    float y = 0.f;
    float w = 0.f;
    float h = 0.f;
};

inline bool CenterOnScreen(float x, float y, float w, float h, float displayW, float displayH) {
    if (w <= 0.f || h <= 0.f || displayW <= 0.f || displayH <= 0.f) return false;
    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f;
    return cx >= 0.f && cy >= 0.f && cx < displayW && cy < displayH;
}

// True when at least one item of the inventory carries its own rectangle: the
// inventory is a special tab being drawn right now.
inline bool HasItemRects(const PluginSDK::Inventory& inv) {
    for (const auto& item : inv.Items)
        if (item.ScreenValid) return true;
    return false;
}

// A uniform grid the host measured this scan, inside the display. The host sets
// Grid.Valid only while the grid's UI element is visible.
inline bool GridDrawn(const PluginSDK::Inventory& inv, float displayW, float displayH) {
    if (!inv.Grid.Valid || inv.Grid.CellSize <= 1.f) return false;
    if (displayW <= 0.f || displayH <= 0.f || inv.TotalBoxesX <= 0 || inv.TotalBoxesY <= 0) return false;
    const float w = static_cast<float>(inv.TotalBoxesX) * inv.Grid.CellSize;
    const float h = static_cast<float>(inv.TotalBoxesY) * inv.Grid.CellSize;
    // A grid larger than the display is a mis-measured special layout.
    if (w > displayW + 4.f || h > displayH + 4.f) return false;
    return CenterOnScreen(inv.Grid.GridScreenX, inv.Grid.GridScreenY, w, h, displayW, displayH);
}

inline std::optional<ScreenRect> ResolveItemRect(const PluginSDK::Inventory& inv,
                                                 const PluginSDK::InventoryItem& item,
                                                 bool inventoryHasItemRects, bool gridDrawn,
                                                 float displayW, float displayH) {
    if (item.ScreenValid) {
        if (!CenterOnScreen(item.ScreenX, item.ScreenY, item.ScreenW, item.ScreenH, displayW, displayH))
            return std::nullopt;
        return ScreenRect{item.ScreenX, item.ScreenY, item.ScreenW, item.ScreenH};
    }
    if (inventoryHasItemRects || !gridDrawn) return std::nullopt;
    if (item.SlotX < 0 || item.SlotY < 0 || item.Width <= 0 || item.Height <= 0) return std::nullopt;
    if (item.SlotX + item.Width > inv.TotalBoxesX || item.SlotY + item.Height > inv.TotalBoxesY) return std::nullopt;
    const float cell = inv.Grid.CellSize;
    ScreenRect r{inv.Grid.GridScreenX + static_cast<float>(item.SlotX) * cell,
                 inv.Grid.GridScreenY + static_cast<float>(item.SlotY) * cell,
                 static_cast<float>(item.Width) * cell, static_cast<float>(item.Height) * cell};
    if (!CenterOnScreen(r.x, r.y, r.w, r.h, displayW, displayH)) return std::nullopt;
    return r;
}

}  // namespace TabletHelper

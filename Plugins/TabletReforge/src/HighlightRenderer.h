#pragma once

// In-game drawing of the highlighted tablets (render thread only).
//
// Layout (label text, fitted font sizes, positions) is computed once per worker
// Frame and font size and reused every frame; a frame then costs one rectangle,
// line or star per tablet plus the cached text runs. Everything stays inside the
// tablet's own cell: every type name of a window is drawn at one shared size
// (a name too long for its cell uses a three-letter code), bonus names are
// shortened with an ellipsis, a "+N" line stands for those that do not fit, and
// a cell with no room for names shows a check-mark count instead. Coordinates
// are pixel-snapped. Names are translated here, on the render thread, into the
// program language (PluginI18n); a language switch or a font reload rebuilds the
// layouts.

#include "Settings.h"
#include "TabletWorker.h"
#include "../../../POEFixer/imgui/imgui.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace TabletHelper {

class HighlightRenderer {
public:
    void Draw(ImDrawList* dl, const std::shared_ptr<const Frame>& frame, const TabletHelperConfig::Settings& s);
    void Reset() {
        m_frame.reset();
        m_layouts.clear();
    }

private:
    struct TextRun {
        std::string text;
        float fontSize = 0.f;
        ImVec2 pos;   // top-left of the text
        ImVec2 size;  // text size
    };
    struct ItemLayout {
        ImVec2 min, max;           // snapped cell rectangle
        ImU32 color = 0;           // highlight colour (with the rule's alpha)
        ImU32 typeColor = 0;       // the same colour, opaque (label text)
        std::vector<TextRun> lines;
        bool hasBadge = false;
        TextRun badge;             // uses left, bottom-right corner
        bool hasCount = false;
        TextRun count;             // matched-bonus count, bottom-left (when names do not fit)
    };

    void BuildLayouts(const Frame& frame, const TabletHelperConfig::Settings& s, float baseFont);

    std::shared_ptr<const Frame> m_frame;
    float m_baseFont = 0.f;
    ImFont* m_font = nullptr;        // the font the layouts were measured with
    uint64_t m_textRevision = 0;     // PluginSDK::I18n::Revision() of their texts
    bool m_showLabel = false, m_showUses = false, m_showBonuses = false;
    TabletHelperConfig::HighlightMode m_mode = TabletHelperConfig::HighlightMode::Border;
    int m_thickness = 0;
    std::vector<ItemLayout> m_layouts;
};

}  // namespace TabletHelper

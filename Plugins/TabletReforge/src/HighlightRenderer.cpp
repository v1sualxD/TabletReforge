#include "HighlightRenderer.h"
#include "TextUtil.h"

#include "../../../POEFixer/plugin_sdk/PluginUi.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>

namespace TabletHelper {

namespace {

namespace I18n = PluginSDK::I18n;

// The label drawn on the tablet, in the program language.
const char* ShortName(std::string_view typeKey) { return I18n::Tr(TypeShortKey(typeKey)); }

// The three-letter label used when the short name does not fit the cell.
const char* Abbreviation(std::string_view typeKey) {
    const char* key = TypeAbbreviationKey(typeKey);
    return key ? I18n::Tr(key) : "?";
}

constexpr float kPi = 3.14159265358979323846f;

ImU32 ToU32(const float c[4], bool opaque) {
    auto ch = [](float v) { return static_cast<int>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
    return IM_COL32(ch(c[0]), ch(c[1]), ch(c[2]), opaque ? 255 : ch(c[3]));
}

ImVec2 Measure(ImFont* font, float size, const char* text) {
    return font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
}

// Largest size in [minSize, size] at which text fits maxW; 0 when even minSize
// does not fit.
float FitSize(ImFont* font, const char* text, float maxW, float size, float minSize) {
    const float w = Measure(font, size, text).x;
    if (w <= maxW) return size;
    if (w <= 0.0f) return size;
    const float s = std::floor(size * maxW / w);
    if (s >= minSize && Measure(font, s, text).x <= maxW) return s;
    return 0.0f;
}

// Shortens text with "..." until it fits maxW (UTF-8 safe). Empty when not even
// one character fits.
std::string Ellipsize(ImFont* font, float size, const std::string& text, float maxW) {
    if (Measure(font, size, text.c_str()).x <= maxW) return text;
    std::string cut = text;
    while (!cut.empty()) {
        size_t n = cut.size() - 1;
        while (n > 0 && (static_cast<unsigned char>(cut[n]) & 0xC0) == 0x80) --n;
        cut.resize(n);
        while (!cut.empty() && cut.back() == ' ') cut.pop_back();
        const std::string candidate = cut + "...";
        if (Measure(font, size, candidate.c_str()).x <= maxW) return candidate;
    }
    return std::string();
}

ImVec2 Snap(ImVec2 v) { return ImVec2(std::floor(v.x), std::floor(v.y)); }

void DrawText(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 col, const char* text, bool outline) {
    if (outline) PluginUi::TextOutlined(dl, font, size, pos, col, text);
    else dl->AddText(font, size, pos, col, text);
}

void StarPath(ImDrawList* dl, ImVec2 c, float outer, float inner) {
    for (int i = 0; i < 10; ++i) {
        const float a = -kPi / 2.0f + static_cast<float>(i) * kPi / 5.0f;
        const float r = (i % 2 == 0) ? outer : inner;
        dl->PathLineTo(ImVec2(c.x + std::cos(a) * r, c.y + std::sin(a) * r));
    }
}

}  // namespace

void HighlightRenderer::BuildLayouts(const Frame& frame, const TabletHelperConfig::Settings& s, float baseFont) {
    m_layouts.clear();
    m_layouts.reserve(frame.items.size());
    ImFont* font = ImGui::GetFont();
    const float F = std::max(8.0f, std::floor(baseFont));
    const float Fmin = std::max(8.0f, std::floor(F * 0.7f));
    const float Fsmall = std::max(Fmin, std::floor(F * 0.85f));
    const int thickness = std::clamp(s.borderThickness, TabletHelperConfig::kBorderThicknessMin,
                                     TabletHelperConfig::kBorderThicknessMax);
    const bool border = s.highlightMode == TabletHelperConfig::HighlightMode::Border;
    const bool star = s.highlightMode == TabletHelperConfig::HighlightMode::Star;

    auto cellOf = [&](const TabletDraw& item, ImVec2& mn, ImVec2& mx, float& pad, float& availW) {
        mn = Snap(ImVec2(item.rect.x, item.rect.y));
        mx = Snap(ImVec2(item.rect.x + item.rect.w, item.rect.y + item.rect.h));
        const float w = mx.x - mn.x, h = mx.y - mn.y;
        pad = std::max(2.0f, std::floor(std::min(w, h) * 0.06f)) + (border ? static_cast<float>(thickness) : 0.0f);
        availW = w - 2.0f * pad;
    };

    // One type-name size for the whole window, so every label reads alike: the
    // largest size (up to F) at which every full name that fits at all still
    // fits. Names that do not fit at that size use their three-letter code.
    float typeSize = F;
    if (s.showLabel) {
        for (const auto& item : frame.items) {
            ImVec2 mn, mx;
            float pad = 0.0f, availW = 0.0f;
            cellOf(item, mn, mx, pad, availW);
            if (availW <= 4.0f) continue;
            const float fit = FitSize(font, ShortName(item.eval.typeKey), availW, F, Fmin);
            if (fit > 0.0f) typeSize = std::min(typeSize, fit);
        }
    }

    for (const auto& item : frame.items) {
        ItemLayout L;
        float pad = 0.0f, availW = 0.0f;
        cellOf(item, L.min, L.max, pad, availW);
        L.color = ToU32(item.eval.color, false);
        L.typeColor = ToU32(item.eval.color, true);
        const float cellW = L.max.x - L.min.x;
        const float cellH = L.max.y - L.min.y;
        if (cellW < 4.0f || cellH < 4.0f || availW <= 4.0f) {
            m_layouts.push_back(std::move(L));
            continue;
        }
        const float bottom = L.max.y - pad;

        // Uses-left badge, bottom-right.
        if (s.showUsesLeft && item.eval.usesLeft >= 0) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "\xC3\x97%d", item.eval.usesLeft);  // "×N"
            const float size = FitSize(font, buf, availW, Fsmall, Fmin);
            if (size > 0.0f) {
                L.hasBadge = true;
                L.badge.text = buf;
                L.badge.fontSize = size;
                L.badge.size = Measure(font, size, buf);
                L.badge.pos = Snap(ImVec2(L.max.x - pad - L.badge.size.x, bottom - L.badge.size.y));
            }
        }
        // The bottom row belongs to the badges and the star.
        float reserved = L.hasBadge ? L.badge.size.y + 2.0f : 0.0f;
        if (star) reserved = std::max(reserved, std::clamp(std::min(cellW, cellH) * 0.40f, 12.0f, 30.0f));
        const float textTop = L.min.y + pad;
        const float textBottom = bottom - reserved;

        size_t bonusesShown = 0;
        if (s.showLabel) {
            // Type name at the shared size, else its code, else nothing (a cell
            // too small for text keeps its coloured frame).
            const char* name = ShortName(item.eval.typeKey);
            float size = typeSize;
            if (Measure(font, size, name).x > availW) {
                name = Abbreviation(item.eval.typeKey);
                if (Measure(font, size, name).x > availW) size = FitSize(font, name, availW, size, Fmin);
            }
            const ImVec2 sz = size > 0.0f ? Measure(font, size, name) : ImVec2(0.0f, 0.0f);
            if (size > 0.0f && textTop + sz.y <= textBottom + 0.5f) {
                TextRun typeRun;
                typeRun.text = name;
                typeRun.fontSize = size;
                typeRun.size = sz;
                typeRun.pos = Snap(ImVec2(L.min.x + pad, textTop));
                float y = textTop + sz.y;
                L.lines.push_back(std::move(typeRun));

                if (s.showMatchedBonuses && !item.eval.bonuses.empty()) {
                    const float lineH = Measure(font, Fsmall, "Ag").y;
                    const size_t total = item.eval.bonuses.size();
                    for (size_t i = 0; i < total; ++i) {
                        if (y + lineH > textBottom + 0.5f) break;
                        const bool lastRoom = y + 2.0f * lineH > textBottom + 0.5f;
                        const size_t left = total - i;
                        std::string text;
                        bool more = lastRoom && left > 1;  // the last line that fits counts the rest
                        if (!more) {
                            text = Ellipsize(font, Fsmall, I18n::Tr(item.eval.bonuses[i]->LabelKey.c_str()), availW);
                            more = text.empty();
                        }
                        // A first name cut to a few letters says nothing: leave the
                        // cell to the check-mark count below instead (letters, not bytes).
                        if (i == 0 && (more || CutBelow(text, 8)))
                            break;
                        if (more) text = "+" + std::to_string(left);
                        TextRun run;
                        run.fontSize = Fsmall;
                        run.size = Measure(font, Fsmall, text.c_str());
                        run.pos = Snap(ImVec2(L.min.x + pad, y));
                        run.text = std::move(text);
                        y += lineH;
                        L.lines.push_back(std::move(run));
                        bonusesShown = total;  // listed or counted
                        if (more) break;
                    }
                }
            }
        }

        // No room for bonus names: a check-mark count in the lower-left corner
        // (the star's place in Star mode, so not there).
        if (s.showMatchedBonuses && bonusesShown == 0 && !item.eval.bonuses.empty() && !star) {
            const std::string text = std::string("\xEF\x80\x8C") + std::to_string(item.eval.bonuses.size());  // FA check
            const float roomW = availW - (L.hasBadge ? L.badge.size.x + 3.0f : 0.0f);
            const float size = roomW > 4.0f ? FitSize(font, text.c_str(), roomW, Fsmall, Fmin) : 0.0f;
            const ImVec2 sz = size > 0.0f ? Measure(font, size, text.c_str()) : ImVec2(0.0f, 0.0f);
            const float topLimit = L.lines.empty() ? textTop : L.lines.back().pos.y + L.lines.back().size.y;
            if (size > 0.0f && bottom - sz.y >= topLimit - 0.5f) {
                L.hasCount = true;
                L.count.text = text;
                L.count.fontSize = size;
                L.count.size = sz;
                L.count.pos = Snap(ImVec2(L.min.x + pad, bottom - sz.y));
            }
        }
        m_layouts.push_back(std::move(L));
    }
}

void HighlightRenderer::Draw(ImDrawList* dl, const std::shared_ptr<const Frame>& frame,
                             const TabletHelperConfig::Settings& s) {
    if (!dl || !frame || frame->items.empty()) return;
    const float baseFont = ImGui::GetFontSize() *
                           std::clamp(s.labelScale, TabletHelperConfig::kLabelScaleMin, TabletHelperConfig::kLabelScaleMax);
    // The layouts hold translated, measured text: a language switch or a font
    // reload (the host reloads the fonts for another game language) rebuilds them.
    ImFont* const currentFont = ImGui::GetFont();
    const uint64_t textRevision = I18n::Revision();
    if (frame != m_frame || baseFont != m_baseFont || s.showLabel != m_showLabel || s.showUsesLeft != m_showUses ||
        s.showMatchedBonuses != m_showBonuses || s.highlightMode != m_mode || s.borderThickness != m_thickness ||
        currentFont != m_font || textRevision != m_textRevision) {
        m_frame = frame;
        m_baseFont = baseFont;
        m_font = currentFont;
        m_textRevision = textRevision;
        m_showLabel = s.showLabel;
        m_showUses = s.showUsesLeft;
        m_showBonuses = s.showMatchedBonuses;
        m_mode = s.highlightMode;
        m_thickness = s.borderThickness;
        BuildLayouts(*frame, s, baseFont);
    }

    ImFont* font = ImGui::GetFont();
    const float t = static_cast<float>(std::clamp(s.borderThickness, TabletHelperConfig::kBorderThicknessMin,
                                                  TabletHelperConfig::kBorderThicknessMax));
    const ImU32 white = IM_COL32(255, 255, 255, 255);
    const ImU32 dark = IM_COL32(0, 0, 0, 170);
    const ImU32 labelBg = IM_COL32(0, 0, 0, std::clamp(s.labelBackgroundAlpha, 0, 255));
    const bool typeColoredText = s.labelTextMode == TabletHelperConfig::LabelTextMode::TypeColor;

    for (const auto& L : m_layouts) {
        const float w = L.max.x - L.min.x;
        const float h = L.max.y - L.min.y;
        if (w < 4.0f || h < 4.0f) continue;

        switch (s.highlightMode) {
        case TabletHelperConfig::HighlightMode::Cross: {
            const float inset = std::max(2.0f, t);
            const ImVec2 a(L.min.x + inset, L.min.y + inset), b(L.max.x - inset, L.max.y - inset);
            const ImVec2 c(L.max.x - inset, L.min.y + inset), d(L.min.x + inset, L.max.y - inset);
            dl->AddLine(a, b, dark, t + 2.0f);
            dl->AddLine(c, d, dark, t + 2.0f);
            dl->AddLine(a, b, L.color, t);
            dl->AddLine(c, d, L.color, t);
            break;
        }
        case TabletHelperConfig::HighlightMode::Star: {
            const float outer = std::clamp(std::min(w, h) * 0.18f, 5.0f, 14.0f);
            const float inner = outer * 0.45f;
            const float pad = std::max(2.0f, std::floor(std::min(w, h) * 0.06f));
            const ImVec2 c(std::floor(L.min.x + pad + outer) + 0.5f, std::floor(L.max.y - pad - outer) + 0.5f);
            StarPath(dl, ImVec2(c.x + 1.0f, c.y + 1.0f), outer, inner);
            dl->PathStroke(dark, std::max(2.0f, t) + 2.0f, ImDrawFlags_Closed);
            StarPath(dl, c, outer, inner);
            dl->PathStroke(L.color, std::max(2.0f, t), ImDrawFlags_Closed);
            break;
        }
        default: {
            // The frame fills the cell's outer t pixels (AddRect strokes inside
            // its rectangle), plus a thin dark line just inside it so the frame
            // reads on bright item art too.
            dl->AddRect(L.min, L.max, L.color, 0.0f, t);
            if (w > 2.0f * t + 4.0f && h > 2.0f * t + 4.0f)
                dl->AddRect(ImVec2(L.min.x + t, L.min.y + t), ImVec2(L.max.x - t, L.max.y - t), dark, 0.0f, 1.0f);
            break;
        }
        }

        if (L.lines.empty() && !L.hasBadge && !L.hasCount) continue;
        const ImU32 textCol = typeColoredText ? L.typeColor : white;
        dl->PushClipRect(L.min, L.max, true);
        auto run = [&](const TextRun& r, ImU32 col) {
            if (s.labelBackground && labelBg != 0)
                dl->AddRectFilled(ImVec2(r.pos.x - 2.0f, r.pos.y), ImVec2(r.pos.x + r.size.x + 2.0f, r.pos.y + r.size.y),
                                  labelBg, 2.0f);
            DrawText(dl, font, r.fontSize, r.pos, col, r.text.c_str(), s.labelOutline);
        };
        for (size_t i = 0; i < L.lines.size(); ++i) run(L.lines[i], i == 0 ? textCol : white);
        if (L.hasBadge) run(L.badge, white);
        if (L.hasCount) run(L.count, white);
        dl->PopClipRect();
    }
}

}  // namespace TabletHelper

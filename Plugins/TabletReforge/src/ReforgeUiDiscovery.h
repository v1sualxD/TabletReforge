#pragma once
#include "TabletTypes.h"
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace TabletHelper {
struct BenchNode {
    uintptr_t address = 0;
    int parent = -1;
    std::string name, text;
    float x = 0, y = 0, w = 0, h = 0;
    float CenterX() const { return x + w / 2; }
    float CenterY() const { return y + h / 2; }
};
struct BenchTargets { BenchNode panel, button, output; };
inline bool Descendant(const std::vector<BenchNode>& nodes, size_t child, size_t parent) {
    for (size_t budget = 0; budget < nodes.size() && child < nodes.size(); ++budget) {
        if (child == parent) return true;
        if (nodes[child].parent < 0) return false;
        child = static_cast<size_t>(nodes[child].parent);
    }
    return false;
}
inline bool SameUiPoint(const BenchNode& a, const BenchNode& b) {
    return std::abs(a.CenterX() - b.CenterX()) <= 12 && std::abs(a.CenterY() - b.CenterY()) <= 12;
}
inline bool UiInside(const BenchNode& outer, const BenchNode& inner) {
    return inner.x >= outer.x && inner.y >= outer.y &&
        inner.x + inner.w <= outer.x + outer.w + 1 && inner.y + inner.h <= outer.y + outer.h + 1;
}
// Semantic discovery only: never fall back to guessed coordinates or child offsets.
// These tokens are a conservative compatibility heuristic, not verified game control IDs.
inline std::optional<BenchTargets> DiscoverBench(const std::vector<BenchNode>& nodes, float width, float height) {
    std::optional<BenchTargets> found;
    for (size_t p = 0; p < nodes.size(); ++p) {
        const auto& panel = nodes[p];
        const std::string identity = ToLowerCopy(panel.name + " " + panel.text);
        if (!ContainsCI(identity, "reforg") ||
            !(ContainsCI(identity, "panel") || ContainsCI(identity, "bench") || ContainsCI(identity, "window")) ||
            panel.w < 200 || panel.h < 200 || panel.x < 0 || panel.y < 0 ||
            panel.x + panel.w > width + 1 || panel.y + panel.h > height + 1 ||
            panel.w * panel.h >= width * height * 0.8f) continue;
        std::vector<BenchNode> buttons, outputs;
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (i == p || !Descendant(nodes, i, p)) continue;
            const auto& node = nodes[i];
            if (!UiInside(panel, node) || node.w < 10 || node.h < 10) continue;
            const auto name = ToLowerCopy(node.name);
            const auto text = NormalizeIdentifier(node.text);
            bool button = (ContainsCI(name, "button") || ContainsCI(name, "confirm")) && ContainsCI(name, "reforg");
            if (!button && (ContainsCI(name, "button") || ContainsCI(name, "confirm"))) {
                for (size_t k = 0; k < nodes.size(); ++k)
                    if (nodes[k].parent == static_cast<int>(i) && NormalizeIdentifier(nodes[k].text) == "reforge") button = true;
            }
            if ((button || text == "reforge") && node.w >= 35 && node.h <= 150 && node.w <= 500) buttons.push_back(node);
            if ((ContainsCI(name, "result") || ContainsCI(name, "output")) && node.w <= 200 && node.h <= 200)
                outputs.push_back(node);
        }
        if (buttons.empty() || outputs.empty()) continue;
        auto choose = [](const std::vector<BenchNode>& choices) -> std::optional<BenchNode> {
            BenchNode best = choices.front();
            for (const auto& candidate : choices) {
                if (!SameUiPoint(best, candidate)) return std::nullopt;
                if (candidate.w * candidate.h < best.w * best.h) best = candidate;
            }
            return best;
        };
        const auto button = choose(buttons), output = choose(outputs);
        if (!button || !output || SameUiPoint(*button, *output)) continue;
        const BenchTargets candidate{panel, *button, *output};
        if (found && (!SameUiPoint(found->button, candidate.button) || !SameUiPoint(found->output, candidate.output)))
            return std::nullopt;
        if (!found || panel.w * panel.h < found->panel.w * found->panel.h) found = candidate;
    }
    return found;
}
} // namespace TabletHelper

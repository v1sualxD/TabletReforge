#pragma once
#include "TabletTypes.h"
#include <algorithm>
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
inline bool SameUiPoint(const BenchNode& a, const BenchNode& b);
// Observed in the live English PoE2 bench: three tall input widgets form a row,
// with one matching output widget above the middle input. The widgets themselves
// have no control names. Resolve their actual rectangles, not fixed coordinates.
inline std::optional<BenchNode> StructuralBenchOutput(std::vector<BenchNode> raw, const BenchNode& button) {
    std::vector<BenchNode> slots;
    for (const auto& node : raw) {
        if (node.w < 28 || node.w > 240 || node.h < 28 || node.h > 320 ||
            node.w / node.h < 0.40f || node.w / node.h > 1.20f ||
            node.y + node.h >= button.CenterY()) continue;
        auto existing = std::find_if(slots.begin(), slots.end(), [&](const auto& old) { return SameUiPoint(old, node); });
        if (existing == slots.end()) slots.push_back(node);
        else if (node.w * node.h > existing->w * existing->h) *existing = node;
    }
    std::optional<BenchNode> found;
    auto similar = [](const BenchNode& a, const BenchNode& b) {
        return std::abs(a.w - b.w) <= a.w * 0.20f && std::abs(a.h - b.h) <= a.h * 0.20f;
    };
    for (const auto& left : slots) for (const auto& middle : slots) for (const auto& right : slots) {
        if (!(left.CenterX() < middle.CenterX() && middle.CenterX() < right.CenterX()) ||
            !similar(left, middle) || !similar(middle, right) ||
            std::abs(left.CenterY() - middle.CenterY()) > 10 || std::abs(right.CenterY() - middle.CenterY()) > 10) continue;
        const float firstGap = middle.CenterX() - left.CenterX(), secondGap = right.CenterX() - middle.CenterX();
        if (firstGap < middle.w * 0.9f || firstGap > middle.w * 1.8f ||
            std::abs(firstGap - secondGap) > middle.w * 0.15f) continue;
        for (const auto& output : slots) {
            if (!similar(output, middle) || output.y + output.h >= middle.y ||
                std::abs(output.CenterX() - middle.CenterX()) > middle.w * 0.30f ||
                std::abs(button.CenterX() - middle.CenterX()) > middle.w * 0.40f) continue;
            if (found && !SameUiPoint(*found, output)) return std::nullopt;
            found = output;
        }
    }
    return found;
}
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
        bool benchIdentity = ContainsCI(identity, "reforg") &&
            (ContainsCI(identity, "panel") || ContainsCI(identity, "bench") || ContainsCI(identity, "window"));
        if (!benchIdentity) {
            for (size_t i = 0; i < nodes.size(); ++i)
                if (Descendant(nodes, i, p) && NormalizeIdentifier(nodes[i].text) == "reforgingbench") benchIdentity = true;
        }
        if (!benchIdentity || panel.w < 200 || panel.h < 200 || panel.x < 0 || panel.y < 0 ||
            panel.x + panel.w > width + 1 || panel.y + panel.h > height + 1 ||
            panel.w * panel.h >= width * height * 0.8f) continue;
        std::vector<BenchNode> buttons, outputs, descendants;
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (i == p || !Descendant(nodes, i, p)) continue;
            const auto& node = nodes[i];
            if (!UiInside(panel, node) || node.w < 10 || node.h < 10) continue;
            descendants.push_back(node);
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
        if (buttons.empty()) continue;
        auto choose = [](const std::vector<BenchNode>& choices) -> std::optional<BenchNode> {
            if (choices.empty()) return std::nullopt;
            BenchNode best = choices.front();
            for (const auto& candidate : choices) {
                if (!SameUiPoint(best, candidate)) return std::nullopt;
                if (candidate.w * candidate.h < best.w * best.h) best = candidate;
            }
            return best;
        };
        const auto button = choose(buttons);
        const auto output = outputs.empty() && button ? StructuralBenchOutput(descendants, *button) : choose(outputs);
        if (!button || !output || SameUiPoint(*button, *output)) continue;
        const BenchTargets candidate{panel, *button, *output};
        if (found && (!SameUiPoint(found->button, candidate.button) || !SameUiPoint(found->output, candidate.output)))
            return std::nullopt;
        if (!found || panel.w * panel.h < found->panel.w * found->panel.h) found = candidate;
    }
    return found;
}
} // namespace TabletHelper

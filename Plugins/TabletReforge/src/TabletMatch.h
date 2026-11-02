#pragma once

// Match engine: given the filter rules and a parsed tablet, decide whether to
// highlight it, in which colour, and which of the selected bonuses it carries.
//
// A tablet is tested against two scopes: its own type's rule and the "All types"
// rule. A scope accepts the tablet when it is enabled, the tablet has enough uses
// left, and either the scope selects no bonuses (highlight by type) or enough of
// its selected bonuses are present. The own type's colour wins when both accept.
// Unknown-type tablets are never highlighted.
//
// When reading modifiers is switched off, the uses-left and bonus filters are
// ignored and every enabled type is highlighted by type. When reading is on but
// this tablet's modifiers have not been read yet, a scope with filters waits
// (no highlight) instead of flashing a match that the read may withdraw.

#include "Settings.h"
#include "TabletBonusCatalog.h"
#include "TabletScanner.h"
#include "TabletTypes.h"

#include <string>
#include <vector>

namespace TabletHelper {

// The part of the settings the scan worker needs, copied on every change.
struct FilterConfig {
    bool readMods = true;
    std::vector<TabletHelperConfig::TypeConfig> types;  // the active profile

    const TabletHelperConfig::TypeConfig* Find(std::string_view key) const {
        for (const auto& t : types)
            if (t.key == key) return &t;
        return nullptr;
    }
};

struct Evaluation {
    bool matched = false;
    float color[4] = {1.f, 1.f, 1.f, 1.f};    // the winning scope's colour
    const char* typeKey = TypeKeys::Unknown;  // static string
    int usesLeft = -1;                        // -1 = not read
    std::vector<const Bonus*> bonuses;        // matched selected bonuses, required ones first
};

inline bool HasFilters(const TabletHelperConfig::TypeConfig& c) {
    return c.minUsesLeft > 0 || !c.selectedBonusIds.empty();
}

inline bool IsRequired(const TabletHelperConfig::TypeConfig& c, const std::string& id) {
    for (const auto& r : c.requiredBonusIds)
        if (r == id) return true;
    return false;
}

// A bonus with a value threshold only counts as present when the tablet's
// rolled value for it is inside the threshold. No threshold: always fine.
inline bool BonusValueOk(const TabletHelperConfig::TypeConfig& c, const Bonus& b, const ParsedTablet& t) {
    const auto fit = c.valueFilters.find(b.Id);
    if (fit == c.valueFilters.end()) return true;
    const auto& r = fit->second;
    if (r.min == 0 && r.max == 0) return true;

    auto vit = t.matchValues.find(b.NormId);
    if (vit == t.matchValues.end() && !b.NormIdStripped.empty()) vit = t.matchValues.find(b.NormIdStripped);
    if (vit == t.matchValues.end()) return false;
    const int val = vit->second;
    if (r.min != 0 && val < r.min) return false;
    if (r.max != 0 && val > r.max) return false;
    return true;
}

inline bool ConfigAccepts(const TabletHelperConfig::TypeConfig& c, const ParsedTablet& t, bool readMods,
                          const std::string& bonusScope, std::vector<const Bonus*>* matchedOut) {
    if (!readMods) return true;               // type-only mode: filters are ignored
    if (!t.modsRead) return !HasFilters(c);   // not read yet: only a filter-free rule can answer
    if (t.usesLeft < c.minUsesLeft) return false;
    if (c.selectedBonusIds.empty()) return true;

    // Required and optional pools; an empty pool imposes no condition.
    std::vector<const Bonus*> reqMatched, optMatched;
    int reqTotal = 0, optTotal = 0;
    for (const auto& id : c.selectedBonusIds) {
        const bool req = IsRequired(c, id);
        (req ? reqTotal : optTotal) += 1;
        const Bonus* b = FindBonus(bonusScope, id);
        if (b && BonusMatches(*b, t.matchKeys) && BonusValueOk(c, *b, t)) (req ? reqMatched : optMatched).push_back(b);
    }
    // Each threshold is clamped to its pool so a stale value cannot make a rule
    // impossible.
    auto enough = [](int have, int total, int want) {
        if (total <= 0) return true;
        int need = want > total ? total : want;
        if (need < 1) need = 1;
        return have >= need;
    };
    if (!enough(static_cast<int>(reqMatched.size()), reqTotal, c.minRequiredBonuses)) return false;
    if (!enough(static_cast<int>(optMatched.size()), optTotal, c.minMatchedBonuses)) return false;

    if (matchedOut) {
        reqMatched.insert(reqMatched.end(), optMatched.begin(), optMatched.end());
        *matchedOut = std::move(reqMatched);
    }
    return true;
}

inline const char* StaticTypeKey(std::string_view key) {
    for (const char* k : kRealTypeKeys)
        if (key == k) return k;
    return TypeKeys::Unknown;
}

inline Evaluation EvaluateTablet(const FilterConfig& f, const ParsedTablet& t) {
    Evaluation e;
    e.typeKey = StaticTypeKey(t.typeKey);
    if (!IsRealType(t.typeKey)) return e;
    e.usesLeft = t.modsRead ? t.usesLeft : -1;

    const auto* own = f.Find(t.typeKey);
    const auto* glob = f.Find(TypeKeys::Global);
    std::vector<const Bonus*> ownMatched, globMatched;
    const bool ownOk = own && own->enabled && ConfigAccepts(*own, t, f.readMods, t.typeKey, &ownMatched);
    const bool globOk = !ownOk && glob && glob->enabled &&
                        ConfigAccepts(*glob, t, f.readMods, TypeKeys::Global, &globMatched);
    if (!ownOk && !globOk) return e;

    e.matched = true;
    const float* src = ownOk ? own->color : glob->color;
    for (int i = 0; i < 4; ++i) e.color[i] = src[i];
    e.bonuses = ownOk ? std::move(ownMatched) : std::move(globMatched);
    return e;
}

}  // namespace TabletHelper

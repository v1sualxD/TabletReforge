#include "../Plugins/TabletReforge/src/ReforgePlan.h"
#include <cstdlib>
#include <iostream>
#include <set>


using namespace TabletHelper;
static void Check(bool ok, const char* name) {
    if (!ok) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); }
}
static ReforgeItem Item(uintptr_t id, const char* path = "Metadata/Items/TowerAugment/BreachAugment",
    int rarity = 1, const char* baseType = "Breach Tablet") {
    return {id, path, baseType, rarity, 80, static_cast<int>(id % 12), static_cast<int>(id / 12), true, false};
}
int main() {
    Check(PlanReforge({}).empty(), "empty inventory");
    Check(PlanReforge({Item(1), Item(2)}).empty(), "incomplete trio stays in inventory");
    auto p = PlanReforge({Item(1), Item(2), Item(3), Item(4), Item(5), Item(6), Item(7)});
    Check(p.size() == 2, "seven tablets give two batches and one leftover");
    std::set<uintptr_t> selected;
    for (const auto& batch : p) for (const auto& item : batch) selected.insert(item.address);
    Check(selected.size() == 6, "each original consumed at most once");
    Check(PlanReforge({Item(1), Item(1), Item(2)}).empty(), "duplicate snapshots do not create a trio");
    Check(PlanReforge({Item(1), Item(2), Item(3, "Metadata/Items/TowerAugment/GenericAugment", 1, "Irradiated Tablet")}).empty(), "different categories stay separate");
    Check(PlanReforge({Item(1), Item(2), Item(3, "Metadata/Items/TowerAugment/BreachAugment", 2)}).empty(), "different rarities stay separate");
    auto abyssA = Item(20, "Metadata/Items/TowerAugment/AbyssAugment");
    auto abyssB = Item(21, "Metadata/Items/TowerAugment/AbyssAugmentTier2");
    auto abyssC = Item(22, "Metadata/Items/TowerAugment/MapAbyssAugment");
    auto categoryBatch = PlanReforge({abyssA, abyssB, abyssC});
    Check(categoryBatch.size() == 1 && TabletCategory(categoryBatch[0][0]) == TypeKeys::Abyss,
        "three same-category variants form a batch despite different metadata paths");
    for (int pass = 0; pass < 20; ++pass) {
        auto repeated = PlanReforge({abyssA, abyssB, abyssC});
        Check(repeated.size() == 1 && TabletCategory(repeated[0][0]) == TabletCategory(repeated[0][1]) &&
            TabletCategory(repeated[0][1]) == TabletCategory(repeated[0][2]) && repeated[0][0].rarity == repeated[0][2].rarity,
            "same-category trio remains stable across 20 planner checks");
    }
    auto baseOnlyA = Item(30, "", 1, "Abyss Tablet");
    auto baseOnlyB = Item(31, "", 1, "Abyss Tablet");
    auto baseOnlyC = Item(32, "", 1, "Abyss Tablet");
    Check(EligibleTablet(baseOnlyA) && PlanReforge({baseOnlyA, baseOnlyB, baseOnlyC}).size() == 1,
        "tablet name fallback is eligible when metadata path is missing");
    for (int variant = 0; variant < 6; ++variant) {
        auto bad = Item(3);
        if (variant == 0) bad.corrupted = true;
        if (variant == 1) bad.rarity = 3;
        if (variant == 2) bad.identified = false;
        if (variant == 3) { bad.path = "Metadata/Items/Currency/CurrencyRerollRare"; bad.baseType = "Orb"; }
        if (variant == 4) bad.address = 0;
        if (variant == 5) { bad.path.clear(); bad.baseType.clear(); }
        Check(PlanReforge({Item(1), Item(2), bad}).empty(), "unsupported items excluded");
    }
    p = PlanReforge({Item(9), Item(8), Item(7), Item(6), Item(5), Item(4)});
    Check(p.size() == 2 && p[0][0].address == 4 && p[1][2].address == 9, "stable inventory order");
    std::cout << "ReforgePlan: all checks passed\n";
}

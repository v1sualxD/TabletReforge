#include "../Plugins/TabletReforge/src/ReforgePlan.h"
#include <cstdlib>
#include <iostream>
#include <set>

using namespace TabletHelper;
static void Check(bool ok, const char* name) {
    if (!ok) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); }
}
static ReforgeItem Item(uintptr_t id, const char* path = "Metadata/Items/TowerAugment/BreachAugment", int rarity = 1) {
    return {id, path, rarity, 80, static_cast<int>(id % 12), static_cast<int>(id / 12), true, false};
}
int main() {
    Check(CanReforgeRejected(true, true, true, true, false), "identified rejected tablet is eligible");
    Check(!CanReforgeRejected(true, true, true, true, true), "highlighted keeper is protected");
    Check(!CanReforgeRejected(false, true, true, true, false), "unidentified result is protected");
    Check(!CanReforgeRejected(true, false, true, true, false), "failed modifier read is protected");
    Check(!CanReforgeRejected(true, true, false, true, false), "unknown category is protected");
    Check(!CanReforgeRejected(true, true, true, false, false), "unchecked category is protected");
    Check(PlanReforge({}).empty(), "empty inventory");
    Check(PlanReforge({Item(1), Item(2)}).empty(), "incomplete trio stays in inventory");
    auto p = PlanReforge({Item(1), Item(2), Item(3), Item(4), Item(5), Item(6), Item(7)});
    Check(p.size() == 2, "seven tablets give two batches and one leftover");
    std::set<uintptr_t> selected;
    for (const auto& batch : p) for (const auto& item : batch) selected.insert(item.address);
    Check(selected.size() == 6, "each original consumed at most once");
    Check(PlanReforge({Item(1), Item(1), Item(2)}).empty(), "duplicate snapshots do not create a trio");
    Check(PlanReforge({Item(1), Item(2), Item(3, "Metadata/Items/TowerAugment/GenericAugment")}).empty(), "different bases stay separate");
    Check(PlanReforge({Item(1), Item(2), Item(3, "Metadata/Items/TowerAugment/BreachAugment", 2)}).empty(), "different rarities stay separate");
    for (int variant = 0; variant < 6; ++variant) {
        auto bad = Item(3);
        if (variant == 0) bad.corrupted = true;
        if (variant == 1) bad.rarity = 3;
        if (variant == 2) bad.identified = false;
        if (variant == 3) bad.path = "Metadata/Items/Currency/CurrencyRerollRare";
        if (variant == 4) bad.address = 0;
        if (variant == 5) bad.path.clear();
        Check(PlanReforge({Item(1), Item(2), bad}).empty(), "unsupported items excluded");
    }
    p = PlanReforge({Item(9), Item(8), Item(7), Item(6), Item(5), Item(4)});
    Check(p.size() == 2 && p[0][0].address == 4 && p[1][2].address == 9, "stable inventory order");
    std::cout << "ReforgePlan: all checks passed\n";
}

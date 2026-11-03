#include "../Plugins/TabletReforge/src/InventoryContents.h"
#include <cstdlib>
#include <iostream>
using namespace TabletHelper;
using Bag = std::map<uintptr_t, ReforgeItem>;
static void Check(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
static ReforgeItem Item(uintptr_t address, int rarity = 1) {
    return {address, "Metadata/Items/TowerAugment/AbyssAugment", "Abyss Tablet",
        rarity, 80, 0, 0, true, false, false};
}
int main() {
    Check(SameBagContents({}, {}), "empty bags match");
    Bag original{{1,Item(1)},{2,Item(2,2)}};
    Check(SameBagContents(original, original), "stable-address fast path");
    Bag rebuilt{{11,Item(11)},{12,Item(12,2)}};
    rebuilt.at(11).x = 7; rebuilt.at(12).y = 3;
    Check(SameBagContents(original, rebuilt), "address and position refresh preserves contents");
    Bag duplicates{{1,Item(1)},{2,Item(2)}};
    Check(!SameBagContents(original, duplicates), "duplicate count cannot hide missing rarity");
    rebuilt.at(12).identified = false;
    Check(!SameBagContents(original, rebuilt), "identification change is detected");
    rebuilt = {{11,Item(11)},{12,Item(12,2)}};
    rebuilt.at(12).sanctified = true;
    Check(!SameBagContents(original, rebuilt), "Sanctified change is detected");
    rebuilt.at(12).sanctified = false; rebuilt.at(12).corrupted = true;
    Check(!SameBagContents(original, rebuilt), "corruption change is detected");
    rebuilt.erase(12);
    Check(!SameBagContents(original, rebuilt), "removed item is detected");
    original.erase(2);
    Check(SameBagContents(original, rebuilt), "expected single removal can be confirmed");
    std::cout << "Inventory contents tests passed\n";
}

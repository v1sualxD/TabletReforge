#include "../Plugins/TabletReforge/src/TabletSelection.h"
#include "../Plugins/TabletReforge/src/ReforgeUiDiscovery.h"
#include <cstdlib>
#include <iostream>
using namespace TabletHelper;
static void Check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
static ReforgeItem Item(uintptr_t id, const char* category = "Abyss", int rarity = 1) {
    return {id, std::string("Metadata/Items/TowerAugment/") + category + "Augment", rarity, 80,
        static_cast<int>(id % 12), static_cast<int>(id / 12), true, false};
}
int main() {
    TabletSelection selected;
    std::map<uintptr_t, ReforgeItem> bag{{1, Item(1)}, {2, Item(2)}, {3, Item(3)}, {4, Item(4, "Expedition")}};
    selected.Touch(bag[1]); selected.Touch(bag[1]);
    Check(selected.Count() == 1, "hovering for multiple frames toggles once per chord");
    selected.Touch(bag[2]); selected.Touch(bag[3]);
    Check(PlanReforge(selected.Candidates(bag, false)).size() == 1, "three manually selected Abyss tablets form one batch");
    selected.ReleaseChord(); selected.Touch(bag[1]);
    Check(!selected.Contains(bag[1]) && selected.Count() == 2, "a new chord deselects the hovered tablet");
    Check(selected.Candidates(bag, true).size() == 4, "all-inventory mode includes unselected eligible tablets");
    bag[2].x = 10; selected.Refresh(bag);
    Check(selected.Contains(bag[2]), "selection follows the same item when it moves slots");
    bag[2].path = Item(2, "Ritual").path; selected.Refresh(bag);
    Check(!selected.Contains(bag[2]), "reused addresses do not inherit selection");
    bag[3].identified = false; selected.Refresh(bag);
    Check(selected.Count() == 0, "unidentified tablets are removed from selection");
    auto unid = Item(5); unid.identified = false; selected.Touch(unid);
    Check(selected.Count() == 0, "unidentified results cannot be selected");
    auto mixed = PlanReforge({Item(1,"Abyss"), Item(2,"Abyss"), Item(3,"Abyss"),
        Item(4,"Expedition"), Item(5,"Expedition"), Item(6,"Expedition"),
        Item(7,"Ritual"), Item(8,"Ritual"), Item(9,"Ritual"), Item(10,"Ritual")});
    Check(mixed.size() == 3, "Abyss, Expedition and Ritual each form their own batch; leftovers stay");
    for (const auto& batch : mixed)
        Check(batch[0].path == batch[1].path && batch[1].path == batch[2].path &&
            batch[0].rarity == batch[1].rarity && batch[1].rarity == batch[2].rarity, "never mix categories or rarities");

    std::vector<BenchNode> nodes{
        {100,-1,"game_ui","",0,0,1920,1080},
        {101,0,"reforging_bench_panel","",10,10,600,800},
        {102,1,"reforge_button","Reforge",100,700,160,40},
        {103,1,"result_slot","",148,500,64,64}};
    Check(DiscoverBench(nodes,1920,1080).has_value(), "semantic discovery recognizes synthetic named bench controls");
    nodes.push_back({104,1,"result_slot_two","",300,500,64,64});
    Check(!DiscoverBench(nodes,1920,1080), "ambiguous output slots produce no target");
    nodes.pop_back(); nodes[1].name = "vendor_panel";
    Check(!DiscoverBench(nodes,1920,1080), "other windows are not accepted as a reforging bench");
    nodes[1].name = "reforging_bench_panel"; nodes[3].name.clear();
    Check(!DiscoverBench(nodes,1920,1080), "missing output identity never falls back to coordinates");
    std::cout << "Selection, category grouping and UI discovery: all checks passed\n";
}

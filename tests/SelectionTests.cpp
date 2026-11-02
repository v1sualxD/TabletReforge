#include "../Plugins/TabletReforge/src/TabletEligibility.h"
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
    Check(EligibleTablet(Item(1)), "identified tablets are eligible");
    auto unid = Item(5); unid.identified = false;
    Check(!EligibleTablet(unid), "unidentified results are excluded");
    auto corrupted = Item(6); corrupted.corrupted = true;
    Check(!EligibleTablet(corrupted), "corrupted tablets are excluded");
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
    // Live client layout observed on 2026-11-02: unnamed bench widgets, title and
    // a lowercase text button. The output is above three tall input slots.
    std::vector<BenchNode> liveLayout{
        {200,-1,"game_ui","",0,0,1920,1080},
        {201,0,"","",350,150,560,740},
        {202,1,"","Reforging Bench",405,167,445,28},
        {203,1,"","reforge",592,854,70,22},
        {204,1,"","",435,582,122,227},
        {205,1,"","",566,582,122,227},
        {206,1,"","",698,582,122,227},
        {207,1,"","",566,295,122,227}};
    const auto liveTargets = DiscoverBench(liveLayout,1920,1080);
    Check(liveTargets && liveTargets->output.address == 207 && liveTargets->button.address == 203,
        "unnamed live bench layout resolves output above three inputs");
    liveLayout.pop_back();
    Check(!DiscoverBench(liveLayout,1920,1080), "missing structural output never generates a guessed target");
    liveLayout.push_back({207,1,"","",566,295,122,227});
    liveLayout[6].y = 650;
    Check(!DiscoverBench(liveLayout,1920,1080), "non-aligned input slots are rejected");
    std::cout << "Selection, category grouping and UI discovery: all checks passed\n";
}

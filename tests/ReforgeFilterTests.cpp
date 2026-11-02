#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "../Plugins/TabletReforge/src/TabletMatch.h"
#include "../Plugins/TabletReforge/src/ReforgePlan.h"
#include <cstdlib>
#include <iostream>

using namespace TabletHelper;
static void Check(bool value, const char* message) {
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
int main() {
    const auto& bonus = GetBonusesFor(TypeKeys::Abyss).front();
    TabletHelperConfig::TypeConfig keepRule;
    keepRule.key = TypeKeys::Abyss;
    keepRule.enabled = true;
    keepRule.selectedBonusIds = {bonus.Id};
    keepRule.valueFilters[bonus.Id] = {12, 0};
    FilterConfig filter{true, {keepRule}};
    ParsedTablet tablet;
    tablet.typeKey = TypeKeys::Abyss;
    tablet.modsRead = true;
    tablet.matchKeys.insert(bonus.NormId);
    tablet.matchValues[bonus.NormId] = 14;
    Check(EvaluateTablet(filter, tablet).matched, "wanted bonus above min is highlighted");
    Check(!CanReforgeRejected(true, true, true, true, EvaluateTablet(filter, tablet).matched), "highlighted tablet is never consumed");
    tablet.matchValues[bonus.NormId] = 8;
    Check(!EvaluateTablet(filter, tablet).matched, "roll below min is rejected");
    Check(CanReforgeRejected(true, true, true, true, EvaluateTablet(filter, tablet).matched), "identified rejected tablet can be queued");
    Check(!CanReforgeRejected(false, true, true, true, EvaluateTablet(filter, tablet).matched), "unidentified result cannot be queued on a later run");
    tablet.modsRead = false;
    Check(!CanReforgeRejected(true, tablet.modsRead, true, true, EvaluateTablet(filter, tablet).matched), "unread modifiers are protected despite absent highlight");
    tablet.modsRead = true;
    TabletHelperConfig::TypeConfig global;
    global.key = TypeKeys::Global;
    global.enabled = true;
    filter.types.push_back(global);
    Check(EvaluateTablet(filter, tablet).matched, "all-types keep rule protects a rejected own-type rule");
    Check(!CanReforgeRejected(true, true, true, true, EvaluateTablet(filter, tablet).matched), "global keep rule prevents consumption");
    filter.types = {keepRule};
    filter.types[0].selectedBonusIds.clear();
    Check(EvaluateTablet(filter, tablet).matched, "enabled category with no bonus selection keeps every tablet");
    std::cout << "Reforge filters: all checks passed\n";
}

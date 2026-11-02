#pragma once
#include "ReforgeUiDiscovery.h"
#include <array>
namespace TabletHelper {
inline constexpr std::array<const char*, 8> StashCategoryOrder{
    "Irradiated", "Ritual", "Expedition", "Delirium", "Breach", "Abyss", "Temple", "Overseer"};
struct TabletStashTargets {
    BenchNode grid;
    std::vector<BenchNode> categories, pages;
};
// Use the game's live controls, including their current scale and position.
inline std::optional<TabletStashTargets> DiscoverTabletStash(const std::vector<BenchNode>& nodes) {
    std::optional<TabletStashTargets> result;
    for (size_t i=0; i<nodes.size(); ++i) {
        const auto& label=nodes[i];
        if (label.name!="tablet_use_label" || label.parent<0) continue;
        const auto& item=nodes[static_cast<size_t>(label.parent)];
        if(item.parent<0 || item.w<20 || item.h<20) continue;
        const auto& grid=nodes[static_cast<size_t>(item.parent)];
        if(std::abs(grid.w/item.w-12)>0.1f || std::abs(grid.h/item.h-8)>0.1f) continue;
        if(result && result->grid.address!=grid.address) return std::nullopt;
        result=TabletStashTargets{grid,{},{}};
    }
    if(!result) return std::nullopt;
    const auto& grid=result->grid;
    const float cell=grid.w/12;
    for(const auto& node:nodes) {
        if(node.parent<0) continue;
        const auto& parent=nodes[static_cast<size_t>(node.parent)];
        if(node.text.size()==1 && node.text[0]>='1' && node.text[0]<='6' &&
            parent.CenterY()<grid.y && parent.CenterY()>grid.y-cell && parent.w>cell*.8f)
            result->pages.push_back(parent);
        if(!node.text.empty() && std::all_of(node.text.begin(),node.text.end(),[](unsigned char c){return c>='0'&&c<='9';}) &&
            parent.w>cell && parent.w<cell*1.3f && std::abs(parent.w-parent.h)<2 &&
            parent.y<grid.y-cell && parent.y>grid.y-cell*3)
            result->categories.push_back(parent);
    }
    auto sortUnique=[](auto& controls){
        std::sort(controls.begin(),controls.end(),[](const auto&a,const auto&b){return a.x<b.x;});
        controls.erase(std::unique(controls.begin(),controls.end(),[](const auto&a,const auto&b){return a.address==b.address;}),controls.end());
    };
    sortUnique(result->categories); sortUnique(result->pages);
    if(result->categories.size()<8 || result->pages.empty() || result->pages.size()>6) return std::nullopt;
    return result;
}
}

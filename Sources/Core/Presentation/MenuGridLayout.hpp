#pragma once
#include "Core/Presentation/DisplayLayout.hpp"
#include <array>
#include <algorithm>
#include <cmath>
namespace hm2::menu {
// Phone201 original menu positions, expressed in HD-skin UI units (x1.5).
// HD item/page indices and the game callbacks remain unchanged.
inline constexpr Vec2 inventoryGrid{400.5f,330},stockGrid{82.5f,330};
inline constexpr Vec2 equipmentRoot{139.5f,316.5f},botRoot{291,318};
inline constexpr std::array<Vec2,6> equipmentOffsets{{{0,0},{0,-84},{-55.5f,-7.5f},{55.5f,-7.5f},{55.5f,-84},{-55.5f,-84}}};
inline constexpr ClipRect inventoryWindow{370.5f,120,300,240},stockWindow{52.5f,120,240,240};
inline constexpr Vec2 tradeAction{333,237}; // Phone GuiShop, relative position (-18,-2) x1.5
inline constexpr Size tradeActionSize{48,85.5f},detailTextureSize{384,192},detailHitSize{309,192};
inline constexpr float stockPagerY=stockGrid.y-231,inventoryPagerY=inventoryGrid.y-234;
inline constexpr float categoryY=402,pagerHitHeight=48,categoryHitHeight=60;
inline constexpr float inventoryOverflow=60,stockOverflow=120;
inline float menuScale(const DisplayConfiguration& c){return std::min(std::max(c.canvas.width/480,c.canvas.height/320),c.canvas.height/200);}
inline float menuHeader(const DisplayConfiguration& c){return 60*menuScale(c);}
inline float menuFooter(const DisplayConfiguration& c){return 50*menuScale(c);}
struct ShopDetailLayout {
    Vec2 center;
    float scale;
};
inline ShopDetailLayout shopDetail(const DisplayLayout& display,Vec2 preferred,bool stock){
    const auto transform=display.ui(DisplayRegion::phoneMenu);
    const auto area=display.configuration().content();
    const auto low=transform.inverse({area.x,area.y});
    const auto high=transform.inverse({area.x+area.width,area.y+area.height});
    const float stockPagerTop=display.ui(DisplayRegion::phoneStockHeader).map({0,stockPagerY+pagerHitHeight*.5f}).y;
    const float inventoryPagerTop=display.ui(DisplayRegion::phoneInventoryPages).map({0,inventoryPagerY+pagerHitHeight*.5f}).y;
    const float categoryBottom=display.ui(DisplayRegion::phoneCategoryHeader).map({0,categoryY-categoryHitHeight*.5f}).y;
    const float gap=4;
    const float left=stock?low.x:std::max(low.x,tradeAction.x+tradeActionSize.width*.5f+gap);
    const float right=stock?std::min(high.x,tradeAction.x-tradeActionSize.width*.5f-gap):high.x;
    const float bottom=std::max(low.y,transform.inverse({0,std::max(stockPagerTop,inventoryPagerTop)}).y+gap);
    const float top=std::min(high.y,transform.inverse({0,categoryBottom}).y-gap);
    // On an unusually constrained surface, keep the detail off the other
    // column even if the space between fixed controls is too short to draw it.
    const float scale=std::clamp(std::min((right-left)/detailTextureSize.width,(top-bottom)/detailTextureSize.height),0.f,1.f);
    if(scale==0)return {preferred,0};
    const float halfWidth=detailTextureSize.width*scale*.5f;
    const float halfHeight=detailTextureSize.height*scale*.5f;
    return {{std::clamp(preferred.x,left+halfWidth,std::max(left+halfWidth,right-halfWidth)),
             std::clamp(preferred.y,bottom+halfHeight,std::max(bottom+halfHeight,top-halfHeight))},scale};
}
inline unsigned visibleRows(const DisplayConfiguration& c){
    if(!c.enabled)return 4;
    // Original40-unit rows and32-unit icons must clear the original footer.
    // A shorter landscape surface changes the button page, never scrolls it.
    const float scale=menuScale(c),first=c.canvas.height-menuHeader(c)-35;
    return unsigned(std::clamp(int(std::floor((first-124-36*scale)/(40*scale)))+1,1,4));
}
inline unsigned subpageForCell(unsigned cell,unsigned cellsPerPage,unsigned cellCount){
    return cellCount==0?0:std::min(cell,cellCount-1)/cellsPerPage;
}
inline bool menuRegion(DisplayRegion r){return r==DisplayRegion::phoneMenu||r==DisplayRegion::phoneStock||r==DisplayRegion::phoneInventory||r==DisplayRegion::phoneStockHeader||r==DisplayRegion::phoneEquipment||r==DisplayRegion::phoneFooter||r==DisplayRegion::phoneHeader||r==DisplayRegion::phoneCategoryHeader||r==DisplayRegion::phoneInventoryPages||r==DisplayRegion::phoneCommandList||r==DisplayRegion::phoneCommandInfo||r==DisplayRegion::phoneTrade;}
inline Vec2 originalHDInventory(Vec2 point,bool maintenance){
    const Vec2 root{384,maintenance?512.f:312.f};
    return {root.x+(maintenance?27.f:24.f)+point.x-inventoryGrid.x,root.y+(maintenance?63.f:120.f)+point.y-inventoryGrid.y};
}
}

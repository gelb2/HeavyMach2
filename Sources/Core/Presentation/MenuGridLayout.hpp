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
inline constexpr float inventoryOverflow=60,stockOverflow=120;
inline float menuScale(const DisplayConfiguration& c){return std::min(std::max(c.canvas.width/480,c.canvas.height/320),c.canvas.height/200);}
inline float menuHeader(const DisplayConfiguration& c){return 60*menuScale(c);}
inline float menuFooter(const DisplayConfiguration& c){return 50*menuScale(c);}
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

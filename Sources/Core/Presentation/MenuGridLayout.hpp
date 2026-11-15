#pragma once
#include "Core/Presentation/DisplayLayout.hpp"
#include <array>
namespace hm2::menu {
// Phone201 original menu positions, expressed in HD-skin UI units (x1.5).
// HD item/page indices and the game callbacks remain unchanged.
inline constexpr Vec2 inventoryGrid{400.5f,330},stockGrid{82.5f,330};
inline constexpr Vec2 equipmentRoot{139.5f,316.5f},botRoot{291,318};
inline constexpr std::array<Vec2,6> equipmentOffsets{{{0,0},{0,-84},{-55.5f,-7.5f},{55.5f,-7.5f},{55.5f,-84},{-55.5f,-84}}};
inline constexpr ClipRect inventoryWindow{370.5f,120,300,240},stockWindow{52.5f,120,240,240};
inline constexpr float inventoryOverflow=60,stockOverflow=120;
inline bool menuRegion(DisplayRegion r){return r==DisplayRegion::phoneMenu||r==DisplayRegion::phoneStock||r==DisplayRegion::phoneInventory||r==DisplayRegion::phoneStockHeader||r==DisplayRegion::phoneEquipment;}
inline Vec2 originalHDInventory(Vec2 point,bool maintenance){
    const Vec2 root{384,maintenance?512.f:312.f};
    return {root.x+(maintenance?27.f:24.f)+point.x-inventoryGrid.x,root.y+(maintenance?63.f:120.f)+point.y-inventoryGrid.y};
}
}

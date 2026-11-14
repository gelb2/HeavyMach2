#pragma once
#include "Core/Presentation/DisplayLayout.hpp"
#include <array>
#include <algorithm>
#include <stdexcept>
namespace hm2::menu {
// Phone201 original menu positions, expressed in HD-skin UI units (x1.5).
// HD item/page indices and the game callbacks remain unchanged.
inline constexpr Vec2 inventoryGrid{400.5f,330},stockGrid{82.5f,330};
inline constexpr Vec2 equipmentRoot{139.5f,316.5f},botRoot{291,318};
inline constexpr std::array<Vec2,6> equipmentOffsets{{{0,0},{0,-84},{-55.5f,-7.5f},{55.5f,-7.5f},{55.5f,-84},{-55.5f,-84}}};
inline constexpr ClipRect inventoryWindow{370.5f,120,300,240},stockWindow{52.5f,120,240,240};
// The illustration and its facility/cell coordinates share one uniform
// transform. Extra HD cells are menu content in the two side panels; they do
// not extend the original Phone artwork or change the HD page indices.
struct PhoneLayout {
    ClipRect picture,leftRail,rightRail,footer;
    DisplayTransform artwork,skin;
    explicit PhoneLayout(const DisplayConfiguration& c){
        const auto safe=c.content();
        const float footerHeight=std::max(56.f,c.insets.bottom+44.f);
        const float rail=102.f;
        const float scale=std::min(std::max(1.f,c.canvas.height-footerHeight)/320.f,
                                   std::max(1.f,safe.width-rail*2)/480.f);
        picture={safe.x+(safe.width-480*scale)*.5f,
                 footerHeight+(c.canvas.height-footerHeight-320*scale)*.5f,480*scale,320*scale};
        artwork={scale,{picture.x,picture.y}};skin={scale*(2.f/3.f),artwork.offset};
        leftRail={safe.x,footerHeight,picture.x-safe.x,c.canvas.height-footerHeight};
        rightRail={picture.x+picture.width,footerHeight,safe.x+safe.width-picture.x-picture.width,c.canvas.height-footerHeight};
        footer={safe.x,0,safe.width,footerHeight};
    }
    [[nodiscard]] Vec2 extraPosition(unsigned index,bool inventory)const{
        const auto& rail=inventory?rightRail:leftRail;
        return {rail.x+rail.width*.5f+(index%2?23.f:-23.f),rail.y+rail.height-64.f-46.f*float(index/2)};
    }
    [[nodiscard]] ClipRect cell(unsigned index,bool inventory)const{
        if(index>=(inventory?25u:24u))throw std::out_of_range("Phone menu cell");
        const unsigned original=inventory?20u:16u;
        const auto origin=inventory?inventoryGrid:stockGrid;
        const auto center=index<original?skin.map({origin.x+60.f*float(index%(inventory?5:4)),origin.y-60.f*float(index/(inventory?5:4))}):extraPosition(index-original,inventory);
        const float size=index<original?60.f*skin.scale:44.f;
        return {center.x-size*.5f,center.y-size*.5f,size,size};
    }
    [[nodiscard]] Vec2 footerPosition(float fraction)const{return {footer.x+footer.width*fraction,footer.y+std::max(28.f,footer.height-22.f)};}
};
inline bool menuRegion(DisplayRegion r){return r==DisplayRegion::phoneMenu||r==DisplayRegion::phoneStock||r==DisplayRegion::phoneInventory||r==DisplayRegion::phoneStockHeader||r==DisplayRegion::phoneEquipment;}
inline Vec2 originalHDInventory(Vec2 point,bool maintenance){
    const Vec2 root{384,maintenance?512.f:312.f};
    return {root.x+(maintenance?27.f:24.f)+point.x-inventoryGrid.x,root.y+(maintenance?63.f:120.f)+point.y-inventoryGrid.y};
}
}

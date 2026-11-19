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
inline constexpr float pagerY=96;
inline constexpr float categoryY=402,pagerHitHeight=27,categoryHitHeight=60;
inline constexpr unsigned phoneShopRows=4;
inline constexpr float equipmentFooterY=76.5f,disarmY=124.5f;
inline float phoneReferenceScale(const DisplayConfiguration& c){
    return std::min(c.canvas.width/480,c.canvas.height/320);
}
inline float phoneReferenceUIScale(const DisplayConfiguration& c){
    return phoneReferenceScale(c)*2.f/3.f;
}
inline DisplayTransform phoneReferenceUI(const DisplayConfiguration& c){
    const float scale=phoneReferenceUIScale(c);
    return {scale,{c.canvas.width*.5f-360*scale,c.canvas.height*.5f-240*scale}};
}
inline float menuScale(const DisplayConfiguration& c){return std::min(std::max(c.canvas.width/480,c.canvas.height/320),c.canvas.height/200);}
inline float menuHeader(const DisplayConfiguration& c){return 60*menuScale(c);}
inline float menuFooter(const DisplayConfiguration& c){return 50*menuScale(c);}
struct ShopDetailLayout {
    Vec2 center;
    float scale;
};
inline ShopDetailLayout shopDetail(const DisplayConfiguration& display,Vec2 preferred,bool stock,bool equipment){
    const auto transform=phoneReferenceUI(display);
    const auto area=display.content();
    const auto low=transform.inverse({area.x,area.y});
    const auto high=transform.inverse({area.x+area.width,area.y+area.height});
    const float left=stock?low.x:std::max(low.x,equipment?360.f:tradeAction.x+tradeActionSize.width*.5f);
    const float right=stock?std::min(high.x,tradeAction.x-tradeActionSize.width*.5f):high.x;
    // The stock and inventory pager rows share the same baseline as the
    // original artwork. Their ink and receivers end just below the detail.
    const float lowerEdge=pagerY+pagerHitHeight*.5f;
    const float upperEdge=equipment?360.f:categoryY-categoryHitHeight*.5f;
    const float bottom=std::max(low.y,lowerEdge);
    const float top=std::min(high.y,upperEdge);
    const float scale=std::clamp(std::min((right-left)/detailHitSize.width,(top-bottom)/detailHitSize.height),0.f,1.f);
    if(scale==0)return {preferred,0};
    // ItemDesc has transparent columns outside its 206-unit visible/active
    // width. The padding may cross the edge of the reference canvas.
    const float halfWidth=detailHitSize.width*scale*.5f;
    const float halfHeight=detailHitSize.height*scale*.5f;
    return {{std::clamp(preferred.x,left+halfWidth,std::max(left+halfWidth,right-halfWidth)),
             std::clamp(preferred.y,bottom+halfHeight,std::max(bottom+halfHeight,top-halfHeight))},scale};
}
inline bool menuRegion(DisplayRegion r){return r==DisplayRegion::phoneMenu||r==DisplayRegion::phoneStock||r==DisplayRegion::phoneInventory||r==DisplayRegion::phoneStockHeader||r==DisplayRegion::phoneEquipment||r==DisplayRegion::phoneFooter||r==DisplayRegion::phoneHeader||r==DisplayRegion::phoneCategoryHeader||r==DisplayRegion::phoneInventoryPages||r==DisplayRegion::phoneCommandList||r==DisplayRegion::phoneCommandInfo||r==DisplayRegion::phoneTrade||r==DisplayRegion::phoneEquipmentFooter||r==DisplayRegion::phoneStockFooter;}
inline Vec2 originalHDInventory(Vec2 point,bool maintenance){
    const Vec2 root{384,maintenance?512.f:312.f};
    return {root.x+(maintenance?27.f:24.f)+point.x-inventoryGrid.x,root.y+(maintenance?63.f:120.f)+point.y-inventoryGrid.y};
}
}

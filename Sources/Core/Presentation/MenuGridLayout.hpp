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
struct EquipmentLayout {
    struct Band { DisplayTransform artwork; ClipRect clip; };
    std::array<Band,7> bands;
    DisplayTransform equipment,tab,inventory,pages,header,footer;
    ClipRect inventoryClip,pagesClip;
    unsigned inventoryRows;
    float disarmY;
};
inline EquipmentLayout equipmentLayout(const DisplayConfiguration& c){
    const auto [width,height]=c.canvas;
    // The left half contains labeled slots, three stat rows and the tabs.
    // The source bands [0,51], [68,240], [253,320] contain those elements.
    // On wider displays the bands consume the two gaps between them.
    constexpr float divider=240, leftTop=67,leftBottom=51;
    const float leftScale=std::min(width/480,height/290);
    const float split=divider*leftScale;
    const float leftHeader=height-leftTop*leftScale,leftFooter=leftBottom*leftScale;
    const float removed=std::clamp(320-height/leftScale,0.f,30.f);
    const float leftMiddleSource=51+removed*17.f/30;
    const DisplayTransform leftMiddle{leftScale,{0,leftFooter-leftMiddleSource*leftScale}};

    // The original inventory half covers the remaining width. Only rows
    // with room for artwork and input enter the current button page.
    const float rightScale=(width-split)/(480-divider);
    const float rightX=split-divider*rightScale;
    const float rightHeader=height-std::min(45*rightScale,height*.20f);
    const float rightFooter=std::min(48*rightScale,height*.24f);
    const float pagerHalf=std::min(16*rightScale,height*.055f);
    const float gridTop=rightHeader;
    const float available=gridTop-rightFooter-2*pagerHalf;
    const unsigned rows=unsigned(std::clamp(int(std::floor(available/(40*rightScale))),1,4));
    const float gridSpan=std::min(available,40*rightScale*rows);
    const float gridBottom=gridTop-gridSpan;
    const float pagerCenter=(rightFooter+gridBottom)*.5f;
    const float firstRow=gridTop-20*rightScale;
    const DisplayTransform rightMiddle{rightScale,{rightX,firstRow-220*rightScale}};
    const auto hudUnits=[](DisplayTransform art){art.scale*=2.f/3.f;return art;};
    const DisplayTransform rightTop{rightScale,{rightX,rightHeader-251*rightScale}};
    const DisplayTransform rightBottom{rightScale,{rightX,0}};
    const DisplayTransform rightPages{rightScale,{rightX,pagerCenter-64*rightScale}};
    EquipmentLayout result{
        {{
            {{leftScale,{0,height-320*leftScale}},{0,leftHeader,split,height-leftHeader}},
            {leftMiddle,{0,leftFooter,split,leftHeader-leftFooter}},
            {{leftScale,{}},{0,0,split,leftFooter}},
            {rightTop,{split,rightHeader,width-split,height-rightHeader}},
            {rightMiddle,{split,gridBottom,width-split,gridSpan}},
            {rightPages,{split,rightFooter,width-split,gridBottom-rightFooter}},
            {rightBottom,{split,0,width-split,rightFooter}},
        }},
        hudUnits(leftMiddle),{leftScale*2.f/3.f,{}},hudUnits(rightMiddle),hudUnits(rightPages),
        hudUnits(rightTop),hudUnits(rightBottom),
        {rightMiddle.map({247,0}).x,gridBottom,200*rightScale,gridSpan},
        {split,pagerCenter-pagerHalf,width-split,2*pagerHalf},rows,
        std::max(83.f,leftMiddleSource+21.f)*1.5f,
    };
    return result;
}
struct ShopDetailLayout {
    Vec2 center;
    float scale;
};
inline ShopDetailLayout shopDetail(const DisplayLayout& display,Vec2 preferred,bool stock,const ClipRect* equipmentInventory=nullptr){
    const auto transform=display.ui(DisplayRegion::phoneMenu);
    const auto area=display.configuration().content();
    const auto low=transform.inverse({area.x,area.y});
    const auto high=transform.inverse({area.x+area.width,area.y+area.height});
    const float stockPagerTop=display.ui(DisplayRegion::phoneStockHeader).map({0,stockPagerY+pagerHitHeight*.5f}).y;
    const float inventoryPagerTop=display.ui(DisplayRegion::phoneInventoryPages).map({0,inventoryPagerY+pagerHitHeight*.5f}).y;
    const float categoryBottom=display.ui(DisplayRegion::phoneCategoryHeader).map({0,categoryY-categoryHitHeight*.5f}).y;
    const float gap=4;
    const float left=equipmentInventory?std::max(low.x,transform.inverse({equipmentInventory->x,0}).x+gap)
        :stock?low.x:std::max(low.x,tradeAction.x+tradeActionSize.width*.5f+gap);
    const float right=stock?std::min(high.x,tradeAction.x-tradeActionSize.width*.5f-gap):high.x;
    const float lowerEdge=equipmentInventory?equipmentInventory->y:std::max(stockPagerTop,inventoryPagerTop);
    const float upperEdge=equipmentInventory?equipmentInventory->y+equipmentInventory->height:categoryBottom;
    const float bottom=std::max(low.y,transform.inverse({0,lowerEdge}).y+gap);
    const float top=std::min(high.y,transform.inverse({0,upperEdge}).y-gap);
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
inline bool menuRegion(DisplayRegion r){return r==DisplayRegion::phoneMenu||r==DisplayRegion::phoneStock||r==DisplayRegion::phoneInventory||r==DisplayRegion::phoneStockHeader||r==DisplayRegion::phoneEquipment||r==DisplayRegion::phoneFooter||r==DisplayRegion::phoneHeader||r==DisplayRegion::phoneCategoryHeader||r==DisplayRegion::phoneInventoryPages||r==DisplayRegion::phoneCommandList||r==DisplayRegion::phoneCommandInfo||r==DisplayRegion::phoneTrade||r==DisplayRegion::phoneEquipmentFooter||r==DisplayRegion::phoneStockFooter;}
inline Vec2 originalHDInventory(Vec2 point,bool maintenance){
    const Vec2 root{384,maintenance?512.f:312.f};
    return {root.x+(maintenance?27.f:24.f)+point.x-inventoryGrid.x,root.y+(maintenance?63.f:120.f)+point.y-inventoryGrid.y};
}
}

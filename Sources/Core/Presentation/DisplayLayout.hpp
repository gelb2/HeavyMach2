#pragma once
#include "Core/Types.hpp"
#include <optional>

namespace hm2 {
// Presentation only: original HD world units, saved indices and clocks never
// change with these values. Insets and canvas use positive-Y-up logical points.
struct DisplayInsets {float left=0,bottom=0,right=0,top=0;};
enum class DisplayDevice {automatic,phone,tablet};
struct DisplayConfiguration {
    Size canvas{768,1024};
    DisplayInsets insets;
    bool enabled=false,compact=false,focusZoom=false;
    DisplayDevice device=DisplayDevice::automatic;
    static DisplayConfiguration surface(Size,DisplayInsets={});
    [[nodiscard]] ClipRect content()const;
};
struct DisplayTransform {
    float scale=1;Vec2 offset;
    [[nodiscard]] Vec2 map(Vec2)const;
    [[nodiscard]] Vec2 inverse(Vec2)const;
    [[nodiscard]] bool identity()const{return scale==1&&offset.x==0&&offset.y==0;}
};
enum class DisplayRegion {content,backdrop,title,slot0,slot1,slot2,bottom,top,
    panel,maintenance,list,chooser,shopStock,shopBackdrop,shopInventory,shopFooter,banner,
    world,surface,baseDescription,artBackdrop,artScene,artContent,phoneMenu,phoneStock,phoneInventory,phoneStockHeader,phoneEquipment,titleBackdrop,artBanner,phoneFooter,phoneHeader,phoneCategoryHeader,phoneInventoryPages,phoneCommandList,phoneCommandInfo,phoneTrade,phoneEquipmentFooter,phoneStockFooter,tabletMenu};
class DisplayLayout {
    DisplayConfiguration configuration_;
public:
    explicit DisplayLayout(DisplayConfiguration c={}):configuration_(c){(void)c.content();}
    [[nodiscard]] const DisplayConfiguration& configuration()const{return configuration_;}
    [[nodiscard]] DisplayTransform ui(DisplayRegion,float horizontalPan=0)const;
    [[nodiscard]] std::optional<ClipRect> clip(DisplayRegion,float horizontalPan=0)const;
    [[nodiscard]] float shopPanLimit()const;
    // A camera result already contains the full HD-world-to-point scale.
    [[nodiscard]] Size worldViewportAtScale(float finalScale)const;
    [[nodiscard]] DisplayTransform worldAtScale(float finalScale)const;
    [[nodiscard]] Vec2 baseButtonPosition(unsigned index,unsigned count)const;
    [[nodiscard]] Vec2 keepControlVisible(DisplayRegion,Vec2,ClipRect relativeBounds)const;
};
// Runs after the original node/rounding/atlas assembly. Never mutates textures.
void transformSprite(Sprite&,const DisplayTransform&,std::optional<ClipRect> = {});
void transformFrame(Frame&,const DisplayTransform&,Size,std::optional<ClipRect> = {});
}

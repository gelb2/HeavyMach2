#include "Core/Presentation/DisplayLayout.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include "Core/Presentation/MenuGridLayout.hpp"
#include "Core/Presentation/MenuArtwork.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace hm2 {
namespace {
void finite(float x){if(!std::isfinite(x))throw std::invalid_argument("Nonfinite display configuration");}
DisplayTransform anchored(Vec2 reference,Vec2 destination,float scale){
    return {scale,{destination.x-reference.x*scale,destination.y-reference.y*scale}};
}
ClipRect intersection(ClipRect a,ClipRect b){
    const float x=std::max(a.x,b.x),y=std::max(a.y,b.y);
    return {x,y,std::max(0.f,std::min(a.x+a.width,b.x+b.width)-x),std::max(0.f,std::min(a.y+a.height,b.y+b.height)-y)};
}
}
DisplayConfiguration DisplayConfiguration::surface(Size size,DisplayInsets insets){
    DisplayConfiguration result{size,insets,true};(void)result.content();
    result.compact=std::min(size.width,size.height)<600;
    result.focusZoom=result.compact;return result;
}
ClipRect DisplayConfiguration::content()const{
    for(float value:{canvas.width,canvas.height,insets.left,insets.right,insets.top,insets.bottom})finite(value);
    if(canvas.width<=0||canvas.height<=0||insets.left<0||insets.right<0||insets.top<0||insets.bottom<0||
       insets.left+insets.right>=canvas.width||insets.top+insets.bottom>=canvas.height)
        throw std::invalid_argument("Invalid display extent or safe area");
    return {insets.left,insets.bottom,canvas.width-insets.left-insets.right,canvas.height-insets.bottom-insets.top};
}
Vec2 DisplayTransform::map(Vec2 p)const{return {p.x*scale+offset.x,p.y*scale+offset.y};}
Vec2 DisplayTransform::inverse(Vec2 p)const{
    if(!std::isfinite(scale)||scale<=0)throw std::invalid_argument("Invalid display scale");
    return {(p.x-offset.x)/scale,(p.y-offset.y)/scale};
}
DisplayTransform DisplayLayout::ui(DisplayRegion region,float pan)const{
    if(!configuration_.enabled)return {};
    const auto a=configuration_.compact?configuration_.content():ClipRect{0,0,configuration_.canvas.width,configuration_.canvas.height};const Vec2 center{a.x+a.width*.5f,a.y+a.height*.5f};
    // HD widgets retain their 768x1024 source layout, as before the Phone
    // adaptation; a physical display point is not one original HUD unit.
    const float nominal=configuration_.compact?2.f/3.f:std::min(a.width/768,a.height/1024);
    const auto fit=[&](float w,float h){return std::min({nominal,a.width/w,a.height/h});};
    const auto centered=[&](float w,float h){return anchored({384,512},center,fit(w,h));};
    switch(region){
    // One HD menu projection for artwork, controls, text and inverse input.
    // World projection and movable HUD sizing remain separate consumers of
    // the original HD canvas, not extra per-platform UI implementations.
    case DisplayRegion::tabletMenu:{const auto c=configuration_.canvas;return anchored({384,512},{c.width*.5f,c.height*.5f},std::max(c.width/768,c.height/1024));}
    case DisplayRegion::titleBackdrop:{const auto c=configuration_.canvas;return anchored({384,1024},{c.width*.5f,c.height},std::max(c.width/768,c.height/1024));}
    case DisplayRegion::artBanner:{const auto c=configuration_.canvas;return anchored({240,160},{c.width*.5f,c.height*.5f},std::min(c.width/480,c.height/320));}
    case DisplayRegion::phoneEquipmentFooter:case DisplayRegion::phoneStockFooter:case DisplayRegion::phoneFooter:{auto t=ui(DisplayRegion::phoneMenu);t.offset.y=0;return t;}
    case DisplayRegion::phoneInventoryPages:case DisplayRegion::phoneStockHeader:{auto t=ui(DisplayRegion::phoneMenu);t.offset.y=72*menu::phoneReferenceScale(configuration_)-96*t.scale;return t;}
    case DisplayRegion::phoneCategoryHeader:case DisplayRegion::phoneHeader:{auto t=ui(DisplayRegion::phoneMenu);t.offset.y=configuration_.canvas.height-480*t.scale;return t;}
    case DisplayRegion::phoneTrade:return menu::phoneReferenceUI(configuration_);
    case DisplayRegion::phoneCommandList:case DisplayRegion::phoneCommandInfo:{
        const auto c=configuration_.canvas;const float header=menu::menuHeader(configuration_),footer=menu::menuFooter(configuration_);const float s=std::min(menu::menuScale(configuration_)*2.f/3.f,std::max(1.f,c.height-header-footer-12)/275.f);
        if(region==DisplayRegion::phoneCommandList)return anchored({222,342},{c.width*148.f/480,c.height-header-24},s);
        return anchored({540,240},{c.width*.75f,footer+(c.height-header-footer)*.5f},s);
    }
    case DisplayRegion::phoneStock:case DisplayRegion::phoneInventory:case DisplayRegion::phoneEquipment:{
        auto t=ui(DisplayRegion::phoneMenu);t.offset.y=configuration_.canvas.height-menu::menuHeader(configuration_)-35-330*t.scale;return t;
    }
    case DisplayRegion::phoneMenu:{
        return anchored({360,240},{configuration_.canvas.width*.5f,configuration_.canvas.height*.5f},menu::menuScale(configuration_)*2.f/3.f);
    }
    case DisplayRegion::artBackdrop:case DisplayRegion::artScene:{
        const auto canvas=configuration_.canvas;const float scale=std::max(canvas.width/480,canvas.height/320);
        const float travel=std::max(0.f,320*scale-canvas.height)*.5f;
        return anchored({240,160},{canvas.width*.5f,canvas.height*.5f+(region==DisplayRegion::artScene?std::clamp(pan,-travel,travel):0.f)},scale);
    }
    case DisplayRegion::artContent:return anchored({240,160},center,std::min(a.width/480,a.height/320));
    case DisplayRegion::backdrop:{const auto c=configuration_.compact?ClipRect{0,0,configuration_.canvas.width,configuration_.canvas.height}:a;return anchored({384,512},{c.x+c.width*.5f,c.y+c.height*.5f},std::max(c.width/768,c.height/1024));}
    case DisplayRegion::title:return configuration_.compact?anchored({384,202},center,std::min(nominal,a.width/560)):centered(768,1024);
    case DisplayRegion::slot0:case DisplayRegion::slot1:case DisplayRegion::slot2:{
        const auto index=unsigned(region)-unsigned(DisplayRegion::slot0);
        if(!configuration_.compact)return centered(768,1024);
        const float scale=std::min({nominal,std::max(1.f,a.width-24)/(3*512+32),std::max(1.f,a.height-72)/256});
        return anchored({384,760-float(index)*240},{a.x+a.width*(float(index)+.5f)/3,a.y+a.height*.57f},scale);
    }
    case DisplayRegion::bottom:return configuration_.compact?anchored({384,0},{center.x,0.f},std::min({.75f,a.width/768,a.height/256})):anchored({384,0},{configuration_.canvas.width*.5f,0},nominal);
    case DisplayRegion::top:return anchored({384,1024},{center.x,a.y+a.height},ui(DisplayRegion::bottom).scale);
    case DisplayRegion::banner:return anchored({384,512},menu::phone(configuration_)?Vec2{configuration_.canvas.width*.5f,configuration_.canvas.height*.5f}:center,configuration_.compact?ui(DisplayRegion::bottom).scale:nominal);
    case DisplayRegion::panel:return centered(768,560);
    case DisplayRegion::maintenance:return centered(640,440);
    case DisplayRegion::list:{if(!configuration_.compact)return centered(768,1024);const float s=std::min(nominal,a.width/768);return anchored({384,1024},{center.x,a.y+a.height+pan},s);}
    case DisplayRegion::chooser:{const auto t=ui(DisplayRegion::bottom);return {t.scale,{t.offset.x,t.offset.y-pan}};}
    case DisplayRegion::shopBackdrop:case DisplayRegion::shopStock:case DisplayRegion::shopInventory:{
        if(!configuration_.compact)return centered(768,1024);
        const float scale=std::min({nominal,std::max(1.f,a.height-56)/512,std::max(.5f,(a.width-24)/1536)});
        const float offset=std::clamp(pan,0.f,std::max(0.f,1536*scale+16-a.width));
        const bool inventory=region==DisplayRegion::shopInventory;
        return anchored({384,inventory?312.f:768.f},{a.x+384*scale+(inventory?768*scale+16:0)-offset,a.y+28+(a.height-28)*.5f},scale);
    }
    case DisplayRegion::shopFooter:{
        if(!configuration_.compact)return centered(768,1024);
        return anchored({640,52},{a.x+a.width-48,a.y+18},.5f);
    }
    case DisplayRegion::content:return centered(768,1024);
    case DisplayRegion::world:return worldAtScale(1);
    case DisplayRegion::surface:return {};
    case DisplayRegion::baseDescription:{
        if(!configuration_.compact)return centered(768,560);
        const float rail=std::min(220.f,a.width*.38f),gap=16;
        const float width=std::max(1.f,a.width-rail-gap);
        return anchored({384,312},{a.x+rail+gap+width*.5f,center.y},std::min({.75f,width/512,(a.height-24)/256}));
    }
    }
    throw std::logic_error("Unknown presentation region");
}
std::optional<ClipRect> DisplayLayout::clip(DisplayRegion region,float pan)const{
    if(!configuration_.enabled)return {};
    if(region==DisplayRegion::world||region==DisplayRegion::tabletMenu||region==DisplayRegion::titleBackdrop||region==DisplayRegion::artBanner||(menu::phone(configuration_)&&region==DisplayRegion::banner))return ClipRect{0,0,configuration_.canvas.width,configuration_.canvas.height};
    if(configuration_.compact&&(region==DisplayRegion::world||region==DisplayRegion::backdrop||region==DisplayRegion::artBackdrop||region==DisplayRegion::artScene))return ClipRect{0,0,configuration_.canvas.width,configuration_.canvas.height};
    if(configuration_.compact&&menu::menuRegion(region)){
        const auto canvas=ClipRect{0,0,configuration_.canvas.width,configuration_.canvas.height};
        if(region!=DisplayRegion::phoneStock&&region!=DisplayRegion::phoneInventory)return canvas;
        const auto source=region==DisplayRegion::phoneStock?menu::stockWindow:menu::inventoryWindow;const auto t=ui(region,pan);const auto p=t.map({source.x,source.y});return intersection(canvas,{p.x,p.y,source.width*t.scale,source.height*t.scale});
    }
    auto result=region==DisplayRegion::bottom?ClipRect{0,0,configuration_.canvas.width,configuration_.canvas.height}:configuration_.content();
    if(region==DisplayRegion::bottom){
        // HD gauge tiles slide through the original screen's left edge.
        // Move that768-wide boundary with HUD instead of exposing the hidden
        // tile portion when a wider window centers the original controls.
        const auto hud=ui(region,pan);const auto left=hud.map({0,0}),right=hud.map({768,0});
        result=intersection(result,{left.x,result.y,right.x-left.x,result.height});
    }
    if(configuration_.compact&&(region==DisplayRegion::list||region==DisplayRegion::chooser)){const float footer=std::min(60.f,result.height*.25f);result.y+=footer;result.height-=footer;}
    if(configuration_.compact&&region==DisplayRegion::shopBackdrop){
        const auto t=ui(region,pan);const auto lower=t.map({0,512});
        result=intersection(result,{lower.x,lower.y,768*t.scale,512*t.scale});
    }
    return result;
}
float DisplayLayout::shopPanLimit()const{
    if(!configuration_.enabled||!configuration_.compact)return 0;
    const auto t=ui(DisplayRegion::shopStock);return std::max(0.f,1536*t.scale+16-configuration_.content().width);
}
Size DisplayLayout::worldViewportAtScale(float scale)const{
    finite(scale);if(scale<=0)throw std::invalid_argument("Nonpositive final world scale");
    if(!configuration_.enabled)return {768,1024};
    const auto a=configuration_.compact?ClipRect{0,0,configuration_.canvas.width,configuration_.canvas.height}:configuration_.content();return {a.width/scale,a.height/scale};
}
DisplayTransform DisplayLayout::worldAtScale(float scale)const{
    finite(scale);if(scale<=0)throw std::invalid_argument("Nonpositive final world scale");
    if(!configuration_.enabled)return {};
    const auto a=configuration_.content();return {scale,configuration_.compact?Vec2{}:Vec2{a.x,a.y}};
}
Vec2 DisplayLayout::baseButtonPosition(unsigned index,unsigned count)const{
    const auto a=configuration_.content();const float rail=std::min(220.f,a.width*.38f);
    return {a.x+rail*.5f,a.y+a.height*(1.f-(float(index)+.5f)/std::max(1u,count))};
}
Vec2 DisplayLayout::keepControlVisible(DisplayRegion region,Vec2 point,ClipRect bounds)const{
    if(!configuration_.enabled)return point;
    const auto t=ui(region);const auto p=t.map(point);const auto a=configuration_.content();
    const float left=a.x-bounds.x*t.scale,right=a.x+a.width-(bounds.x+bounds.width)*t.scale;
    const float bottom=a.y-bounds.y*t.scale,top=a.y+a.height-(bounds.y+bounds.height)*t.scale;
    return t.inverse({left<=right?std::clamp(p.x,left,right):p.x,bottom<=top?std::clamp(p.y,bottom,top):p.y});
}
void transformSprite(Sprite& sprite,const DisplayTransform& t,std::optional<ClipRect> clip){
    if(!t.identity()){
        sprite.center=t.map(sprite.center);sprite.size={sprite.size.width*t.scale,sprite.size.height*t.scale};
        for(auto& vertex:sprite.strip)vertex.position=t.map(vertex.position);
        if(sprite.atlasBatch){auto copy=std::make_shared<AtlasBatch>(*sprite.atlasBatch);for(auto& quad:copy->quads)for(auto& vertex:quad.vertices)vertex.position=t.map(vertex.position);sprite.atlasBatch=std::move(copy);}
        if(sprite.clip){const auto p=t.map({sprite.clip->x,sprite.clip->y});sprite.clip=ClipRect{p.x,p.y,sprite.clip->width*t.scale,sprite.clip->height*t.scale};}
    }
    if(clip)sprite.clip=sprite.clip?intersection(*sprite.clip,*clip):*clip;
}
void transformFrame(Frame& frame,const DisplayTransform& t,Size canvas,std::optional<ClipRect> clip){
    for(auto& sprite:frame.sprites)transformSprite(sprite,t,clip);frame.width=canvas.width;frame.height=canvas.height;
}
}

#include "Core/Presentation/DisplayLayout.hpp"
#include "Core/Presentation/VisualLayer.hpp"
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
    const auto a=configuration_.content();const Vec2 center{a.x+a.width*.5f,a.y+a.height*.5f};
    const float nominal=configuration_.compact?2.f/3.f:1.f;
    const auto fit=[&](float w,float h){return std::min({nominal,a.width/w,a.height/h});};
    const auto centered=[&](float w,float h){return anchored({384,512},center,fit(w,h));};
    switch(region){
    case DisplayRegion::backdrop:return anchored({384,512},center,std::max(a.width/768,a.height/1024));
    case DisplayRegion::title:return configuration_.compact?anchored({384,202},center,std::min(nominal,a.width/560)):centered(768,1024);
    case DisplayRegion::slot0:case DisplayRegion::slot1:case DisplayRegion::slot2:{
        const auto index=unsigned(region)-unsigned(DisplayRegion::slot0);
        if(!configuration_.compact)return centered(768,1024);
        const float scale=std::min({nominal,std::max(1.f,a.width-24)/(3*512+32),std::max(1.f,a.height-72)/256});
        return anchored({384,760-float(index)*240},{a.x+a.width*(float(index)+.5f)/3,a.y+a.height*.57f},scale);
    }
    case DisplayRegion::bottom:return anchored({384,0},{center.x,a.y},configuration_.compact?.5f:1.f);
    case DisplayRegion::top:return anchored({384,1024},{center.x,a.y+a.height},1.f);
    case DisplayRegion::banner:return anchored({384,512},center,configuration_.compact?.5f:1.f);
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
    }
    throw std::logic_error("Unknown presentation region");
}
std::optional<ClipRect> DisplayLayout::clip(DisplayRegion region,float pan)const{
    if(!configuration_.enabled)return {};
    auto result=configuration_.content();
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
float DisplayLayout::worldScale(float zoom)const{
    finite(zoom);if(zoom<=0)throw std::invalid_argument("Nonpositive world display zoom");
    return !configuration_.enabled?1.f:(configuration_.compact?.5f:1.f)*zoom;
}
Size DisplayLayout::worldViewport(float zoom)const{const auto a=configuration_.content();const auto s=worldScale(zoom);return {a.width/s,a.height/s};}
DisplayTransform DisplayLayout::world(float zoom)const{const auto a=configuration_.content();return {worldScale(zoom),{a.x,a.y}};}
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

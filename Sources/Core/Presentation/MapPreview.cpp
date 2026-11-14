#include "Core/Presentation/MapPreview.hpp"
#include "Assets/Image.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace hm2 {
namespace {
void valid(const MapCamera& c){if(!std::isfinite(c.viewport.width)||!std::isfinite(c.viewport.height)||c.viewport.width<=0||c.viewport.height<=0||!std::isfinite(c.center.x)||!std::isfinite(c.center.y)||!std::isfinite(c.offset.x)||!std::isfinite(c.offset.y))throw std::runtime_error("Invalid map camera");}
Size imageSize(ResourceAccess& r,std::string_view name){const auto image=decodeImage(r.read(name));return {float(image.width),float(image.height)};}
void appendVisibleTile(Frame& frame,Sprite sprite){
    const float sourceLeft=sprite.center.x-sprite.size.width*.5f,sourceBottom=sprite.center.y-sprite.size.height*.5f;
    const float sourceRight=sourceLeft+sprite.size.width,sourceTop=sourceBottom+sprite.size.height;
    const float left=std::max(0.f,sourceLeft),bottom=std::max(0.f,sourceBottom),right=std::min(frame.width,sourceRight),top=std::min(frame.height,sourceTop);
    if(left>=right||bottom>=top)return;
    if(left!=sourceLeft||bottom!=sourceBottom||right!=sourceRight||top!=sourceTop){
        // Intersect the submitted quad with the existing view, interpolating
        // the same image UVs. No camera, world size, image scale or pixels
        // change. Avoid the reproduced wide GLES clip/rasterization defect.
        const auto vertex=[&](float x,float y){return StripVertex{{x,y},{(x-sourceLeft)/sprite.size.width,(sourceTop-y)/sprite.size.height},sprite.color};};
        sprite.strip={vertex(left,top),vertex(left,bottom),vertex(right,top),vertex(right,bottom)};
    }
    frame.sprites.push_back(std::move(sprite));
}
}
Vec2 MapCamera::toScreen(Vec2 p)const {valid(*this);return {p.x-center.x+viewport.width/2-offset.x,p.y-center.y+viewport.height/2-offset.y};}
Vec2 MapCamera::toWorld(Vec2 p)const {valid(*this);return {p.x+center.x-viewport.width/2+offset.x,p.y+center.y-viewport.height/2+offset.y};}
MapPreview::MapPreview(ResourceAccess& r,std::string_view name):maps_(Maps::parse(Chunk::parse(r.read("MapInfo.mcb")))),mapName_(name){
    (void)map();
    // GobWorld::openMap: optional name+BG.jpg then mandatory name.jpg/name.png.
    background_=mapName_+"BG.jpg";
    if(r.exists(background_))backgroundSize_=imageSize(r,background_);else background_.clear();
    foreground_=mapName_+".jpg";
    if(!r.exists(foreground_))foreground_=mapName_+".png";
    foregroundSize_=imageSize(r,foreground_);
}
Frame MapPreview::compose(const MapCamera& camera)const{
    valid(camera);Frame frame;frame.width=camera.viewport.width;frame.height=camera.viewport.height;
    const auto p=camera.toScreen({});
    if(!background_.empty()){
        const auto center=camera.toScreen(camera.backgroundWorld);Size size{backgroundSize_.width*2,backgroundSize_.height*2};
        if(camera.fillSurface){
            // The original parallax backdrop can be narrower than a wide
            // canvas. Cover uniformly with the complete image, preserving its
            // aspect ratio and UVs; world/terrain/unit scale stays unchanged.
            const float cover=std::max({1.f,camera.viewport.width/size.width,camera.viewport.height/size.height});
            size={size.width*cover,size.height*cover};
            const float halfX=size.width*.5f,halfY=size.height*.5f;
            auto bounded=center;
            if(size.width>=camera.viewport.width)bounded.x=std::clamp(bounded.x,camera.viewport.width-halfX,halfX);
            if(size.height>=camera.viewport.height)bounded.y=std::clamp(bounded.y,camera.viewport.height-halfY,halfY);
            appendVisibleTile(frame,{background_,bounded,size});
        }else
        frame.sprites.push_back({background_,center,size});
    }
    // setFixedPos:YES on HD nonretina rounds the translated terrain center to
    // integral original coordinates. Background and other units remain unrounded.
    Sprite terrain{foreground_,{std::round(p.x),std::round(p.y)},foregroundSize_};
    if(camera.fillSurface)appendVisibleTile(frame,std::move(terrain));
    else frame.sprites.push_back(std::move(terrain));
    return frame;
}
}

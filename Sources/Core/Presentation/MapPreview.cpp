#include "Core/Presentation/MapPreview.hpp"
#include "Assets/Image.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace hm2 {
namespace {
void valid(const MapCamera& c){if(!std::isfinite(c.viewport.width)||!std::isfinite(c.viewport.height)||c.viewport.width<=0||c.viewport.height<=0||!std::isfinite(c.center.x)||!std::isfinite(c.center.y)||!std::isfinite(c.offset.x)||!std::isfinite(c.offset.y))throw std::runtime_error("Invalid map camera");}
Size imageSize(ResourceAccess& r,std::string_view name){const auto image=decodeImage(r.read(name));return {float(image.width),float(image.height)};}
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
        if(camera.fillSurface){const float cover=std::max({1.f,(camera.viewport.width+2*std::abs(center.x-camera.viewport.width*.5f))/size.width,(camera.viewport.height+2*std::abs(center.y-camera.viewport.height*.5f))/size.height});size={size.width*cover,size.height*cover};}
        frame.sprites.push_back({background_,center,size});
    }
    // setFixedPos:YES on HD nonretina rounds the translated terrain center to
    // integral original coordinates. Background and other units remain unrounded.
    Sprite terrain{foreground_,{std::round(p.x),std::round(p.y)},foregroundSize_};
    if(camera.fillSurface&&background_.empty()){
        // Preserve every original terrain pixel/coordinate. Only the camera's
        // edge overscan samples the existing GPU CLAMP_TO_EDGE texture; no
        // new terrain, mirrored tiles, assets or collision data are generated.
        const float left=terrain.center.x-foregroundSize_.width*.5f,bottom=terrain.center.y-foregroundSize_.height*.5f;
        const float x0=std::min(0.f,left),y0=std::min(0.f,bottom),x1=std::max(camera.viewport.width,left+foregroundSize_.width),y1=std::max(camera.viewport.height,bottom+foregroundSize_.height);
        for(Vec2 point:{Vec2{x0,y0},Vec2{x1,y0},Vec2{x0,y1},Vec2{x1,y1}})terrain.strip.push_back({point,{(point.x-left)/foregroundSize_.width,1-(point.y-bottom)/foregroundSize_.height},{1,1,1,1}});
    }
    frame.sprites.push_back(std::move(terrain));
    return frame;
}
}

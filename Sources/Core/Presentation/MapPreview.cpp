#include "Core/Presentation/MapPreview.hpp"
#include "Assets/Image.hpp"
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
    if(!background_.empty())frame.sprites.push_back({background_,p,{backgroundSize_.width*2,backgroundSize_.height*2}});
    // setFixedPos:YES on HD nonretina rounds the translated terrain center to
    // integral original coordinates. Background and other units remain unrounded.
    frame.sprites.push_back({foreground_,{std::round(p.x),std::round(p.y)},foregroundSize_});
    return frame;
}
}

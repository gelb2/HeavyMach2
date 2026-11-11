#include "Core/Presentation/MovementPathVisual.hpp"
#include <cmath>
#include <stdexcept>

namespace hm2 {
namespace {
void finite(Vec2 p){if(!std::isfinite(p.x)||!std::isfinite(p.y))throw std::runtime_error("Nonfinite original movement visual point");}
}
void MovementPathVisual::validate(const MovementPathState& state){
    if(state.start>=12||state.count>12)throw std::runtime_error("Invalid authoritative movement ring");
    for(unsigned i=0;i<state.count;++i)finite(state.points[(state.start+i)%12]);
}
void MovementPathVisual::prepare(ResourceAccess& resources,PreparedTiles& tiles){
    for(const auto* name:{"MovePath.png","DestPos.png","TargetPath.png"})
        if(!tiles.prepare(resources,name))throw std::runtime_error(std::string("Missing original path image: ")+name);
    tiles.lookup("DestPos.png")->columns=2;tiles.lookup("DestPos.png")->rows=1;
    tiles_= &tiles;
    for(auto& line:lines_)if(line.vertices.capacity()<130)line.vertices.reserve(130);
    if(targetLine_.vertices.capacity()<130)targetLine_.vertices.reserve(130);
    const auto* mark=tiles.lookup("DestPos.png");
    const float halfWidth=mark->pixels.width/4.f,halfHeight=mark->pixels.height/2.f;
    for(auto& state:visibility_){state.bounds={-halfWidth,-halfHeight,halfWidth,halfHeight};state.visual=true;}
}
void MovementPathVisual::acceptedSetMovePath(const MovementPathState& after,Vec2 player,bool reset){
    validate(after);finite(player);if(!after.count||(!reset&&after.count<2))throw std::runtime_error("Accepted path has no destination/predecessor");
    if(reset){for(auto& mark:marks_){mark.shown=false;mark.lineShown=false;}}
    const unsigned index=(after.start+after.count-1)%12;
    auto& mark=marks_[index];mark.position=after.points[index];
    mark.lineEnd=reset?player:after.points[(after.start+after.count-2)%12];
    updateLine(lines_[index],"MovePath.png",mark.position,mark.lineEnd,8);
    mark.tile=index==after.start?1u:0u;mark.alpha=1;mark.scale.x=5;
    // Original writes Y through old _pathCnt, not the physical tail index.
    marks_[after.count-1].scale.y=5;
    mark.shown=true;mark.lineShown=true;
}
void MovementPathVisual::clearMovePath(unsigned start,unsigned count){
    if(start>=12||count>12)throw std::runtime_error("Invalid clear path physical range");
    for(unsigned i=0;i<count;++i){auto& mark=marks_[(start+i)%12];mark.shown=false;mark.lineShown=false;}
}
void MovementPathVisual::processMyMach(Vec2 player,double worldTime,const MovementPathState& before,
                                     const MovementPathState& after){
    validate(before);validate(after);finite(player);
    if(!std::isfinite(worldTime))throw std::runtime_error("Nonfinite original world time");
    if(!before.count){if(after.count)throw std::runtime_error("Path inserted inside arrival phase");return;}
    marks_[before.start].lineEnd=player;
    updateLine(lines_[before.start],"MovePath.png",marks_[before.start].position,player,8);
    if(after.count==before.count-1&&after.start==(before.start+1)%12){
        marks_[before.start].shown=false;marks_[before.start].lineShown=false;
        if(after.count){auto& head=marks_[after.start];head.tile=1;head.alpha=1;head.scale.x=1.5f;marks_[after.count].scale.y=1.5f;}
    }else if(after.count!=before.count||after.start!=before.start){
        throw std::runtime_error("Movement visual arrival observer skipped original mutation");
    }
    // The original enters this block using BEFORE count, so it also animates
    // all twelve (hidden included) after consuming the final destination.
    for(unsigned i=0;i<12;++i){auto& mark=marks_[i];
        mark.alpha+=((i==after.start?.5f:.3f)-mark.alpha)/10.f;
        mark.scale.x+=((i==after.start?1.f:.5f)-mark.scale.x)/5.f;mark.scale.y=mark.scale.x;
    }
    marks_[after.start].angle=std::sin(float(worldTime+worldTime));
}
void MovementPathVisual::processTarget(Vec2 player,std::optional<Vec2> target){
    finite(player);
    if(target){finite(*target);targetStart_=player;targetEnd_=*target;updateLine(targetLine_,"TargetPath.png",player,*target,4);}
    else targetShown_=false;
}
void MovementPathVisual::updateLine(Line& line,std::string_view name,Vec2 from,Vec2 to,float width){
    if(!tiles_)throw std::runtime_error("Movement visual not prepared");
    const auto* tile=tiles_->lookup(name);line.origin=from;line.vertices.clear();if(!tile)return;
    const float dx=to.x-from.x,dy=to.y-from.y,length=std::sqrt(dx*dx+dy*dy);
    // Original hidden constructor divides zero. A zero-area visible strip has
    // no pixels; omit it instead of submitting NaN to the modern adapter.
    if(length==0)return;
    if(!std::isfinite(length))throw std::runtime_error("Nonfinite original path length");
    const float blocks=length/32.f;
    if(double(blocks)>double(line.vertices.max_size()/2-2))throw std::runtime_error("Original path exceeds addressable adapter storage");
    const auto full=std::size_t(blocks),vertices=(full+2)*2;
    // Modern memory safety adapter: retain every original block/UV/vertex;
    // grow beyond the original fixed 64-block allocation without clipping.
    if(line.vertices.capacity()<vertices)line.vertices.reserve(vertices);
    const float nx=dx/length,ny=dy/length,px=ny*width*.5f,py=nx*width*.5f;
    const auto visual=tiles_->visual(*tile);const float maxT=visual.content.height/visual.storage.height;
    const float remainder=float(std::fmod(double(length),32.));
    const float partial=(remainder/32.f)*maxT;
    float cx=0,cy=0;
    for(std::size_t i=0;i<=full+1;++i){
        const float v=i==full+1?((i&1)?1.f-partial:partial):((i&1)?0.f:maxT);
        line.vertices.push_back({{cx-px,cy+py},{0,v/maxT},{}});
        line.vertices.push_back({{cx+px,cy-py},{1,v/maxT},{}});
        const float step=i<full?32.f:remainder;cx+=nx*step;cy+=ny*step;
    }
}
void MovementPathVisual::submitLine(VisualLayer& layer,std::string_view name,const Line& line,
                                    const MapCamera* camera)const{
    if(line.vertices.empty())return;
    const auto* tile=tiles_->lookup(name);if(!tile)return;
    Sprite sprite;sprite.image=tile->name;sprite.textureSource=tile->textureSource;
    sprite.blend={BlendFactor::sourceAlpha,BlendFactor::one};
    const Vec2 origin=camera?camera->toScreen(line.origin):line.origin;
    sprite.strip=line.vertices;
    for(auto& vertex:sprite.strip){vertex.position.x+=origin.x;vertex.position.y+=origin.y;}
    layer.submitNonAtlas(std::move(sprite));
}
void MovementPathVisual::appendWorldLayers(VisualLayer& layer1,VisualLayer& layer7,const MapCamera* camera)const{
    if(!tiles_)throw std::runtime_error("Movement visual not prepared");
    const auto* tile=tiles_->lookup("DestPos.png");
    for(unsigned i=0;i<12;++i){const auto& mark=marks_[i];
        if(mark.lineShown)submitLine(layer1,"MovePath.png",lines_[i],camera);
        auto visibility=visibility_[i];visibility.show=mark.shown;
        refreshVisibility(visibility,mark.position,camera);
        VisualImage image;image.tile=tiles_->visual(*tile);image.tileNo=mark.tile;image.blendType=2;
        image.screenPosition=camera?camera->toScreen(mark.position):mark.position;
        image.scale=mark.scale;image.alpha=mark.alpha;image.angle=mark.angle;
        image.show=mark.shown;image.isInScreen=visibility.inScreen;layer1.submitImage(image);
    }
    if(targetShown_)submitLine(layer7,"TargetPath.png",targetLine_,camera);
}
std::array<std::size_t,13> MovementPathVisual::lineCapacities()const{
    std::array<std::size_t,13> out{};for(unsigned i=0;i<12;++i)out[i]=lines_[i].vertices.capacity();out[12]=targetLine_.vertices.capacity();return out;
}
void MovementPathVisual::appendPreparation(Frame& frame)const{
    if(!tiles_)throw std::runtime_error("Movement visual not prepared");
    for(const auto* name:{"MovePath.png","DestPos.png","TargetPath.png"}){const auto* tile=tiles_->lookup(name);
        Sprite sprite;sprite.image=name;sprite.size=tile->pixels;sprite.textureSource=tile->textureSource;frame.sprites.push_back(std::move(sprite));}
}
}

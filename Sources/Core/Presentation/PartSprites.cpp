#include "Core/Presentation/PartSprites.hpp"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <stdexcept>

namespace hm2 {
namespace {std::string image(const Part& p){return p.tileCount?p.name+".png":"PartsDummy.png";}}
void PartSprites::prepare(ResourceAccess& r,std::span<const PartPose> poses){
    for(const auto& pose:poses){const auto& p=*pose.part;const auto name=image(p);
        // Verified original DummyParts root: getTile returns nil; atlas skips
        // nil and fallback sends no GL draw. Do not hide other missing assets.
        auto* tile=tiles_.prepare(r,name);
        if(!tile&&p.name!="DummyParts")throw std::runtime_error("Missing original part image: "+name);
        if(tile){tile->columns=std::max(1u,p.tileCount);tile->rows=1;}
        if(p.tileCount)tiles_.prepareShadow(r,name);
    }
}
void collectOriginalPartShadow(VisualLayer& layer,const PreparedTiles& tiles,const PartPose& pose,
                               Vec2 center,const MapCamera* camera,VisibilityState& state,MachineKind kind){
    if(!pose.part||!pose.part->tileCount||state.removed)return;
    const auto* tile=tiles.lookupShadow(image(*pose.part));if(!tile)return;
    const float w=tile->pixels.width/float(tile->columns),h=tile->pixels.height/float(tile->rows);
    state.bounds={-w*.5f,-h*.5f,w*.5f,h*.5f};state.visual=true;
    // addParts calls setHvMach before setPartsInfo creates this child.
    // machType2 sets shadowLen10 first; partsType2's later shadowLen2 write
    // does not move the already-created child. Neither offset is rotated.
    const float length=kind==MachineKind::aircraft?10.f:4.f;
    const Vec2 world{center.x+length,center.y-length};
    VisualImage shadow;shadow.tile=tiles.visual(*tile);shadow.screenPosition=camera?camera->toScreen(world):world;
    shadow.angle=pose.angle;shadow.reverse=pose.reverseY?2u:0u;shadow.useAtlas=true;
    shadow.show=state.show;shadow.isInScreen=state.inScreen;
    layer.submitImage(shadow);refreshVisibility(state,world,camera);
}
VisibilityState originalPartVisibility(const TileImage* tile,const Part& part){
    VisibilityState state;state.visual=true;if(!tile)return state;
    // GobMachParts.setPartsInfo0x1c36c stores these unrotated bounds once;
    // unlike a generic QobImage, it subtracts raw socket0 from all four edges.
    const auto anchor=part.socket(0);
    const float halfWidth=tile->pixels.width/float(tile->columns)*.5f;
    const float halfHeight=tile->pixels.height/float(tile->rows)*.5f;
    state.bounds={-halfWidth-anchor.x,-halfHeight-anchor.y,halfWidth-anchor.x,halfHeight-anchor.y};return state;
}
void collectOriginalPart(VisualLayer& bodyLayer,VisualLayer& shadowLayer,const PreparedTiles& tiles,
                         const PartPose& pose,Vec2 center,const MapCamera* camera,
                         VisibilityState& body,VisibilityState& shadow,MachineKind kind){
    if(!pose.part||!body.show||body.removed)return;
    const auto* tile=tiles.lookup(image(*pose.part));if(!tile)return;
    VisualImage visual;visual.tile=tiles.visual(*tile);visual.screenPosition=camera?camera->toScreen(center):center;
    visual.angle=pose.angle;visual.reverse=pose.reverseY?2u:0u;visual.useAtlas=kind!=MachineKind::bipod;
    if(!visual.useAtlas)refreshVisibility(body,center,camera);
    visual.isInScreen=body.inScreen;bodyLayer.submitImage(visual);
    if(visual.useAtlas)refreshVisibility(body,center,camera);
    collectOriginalPartShadow(shadowLayer,tiles,pose,center,camera,shadow,kind);
}
void PartSprites::append(Frame& frame,std::span<const PartPose> poses,Vec2 position)const{
    if(!std::isfinite(position.x)||!std::isfinite(position.y))throw std::runtime_error("Nonfinite part placement");
    VisualLayer shadows;std::vector<const PartPose*> ordered;ordered.reserve(poses.size());
    for(const auto& pose:poses)ordered.push_back(&pose);
    std::stable_sort(ordered.begin(),ordered.end(),[](const auto* a,const auto* b){return a->mountIndex<b->mountIndex;});
    for(const auto* pose:ordered){VisibilityState state;
        collectOriginalPartShadow(shadows,tiles_,*pose,{position.x+pose->center.x,position.y+pose->center.y},nullptr,state);}
    Frame shadowFrame;shadows.drain().appendBatches(shadowFrame);
    for(auto& shadow:shadowFrame.sprites)shadow.visualLayer=0;
    // append() can assemble several actors over an existing terrain prefix.
    // Keep every actor's layer0 shadow before already appended part bodies.
    const auto body=std::find_if(frame.sprites.begin(),frame.sprites.end(),[](const auto& sprite){
        return sprite.visualLayer&&sprite.textureSource.variant==TextureVariant::rgba;});
    frame.sprites.insert(body,std::make_move_iterator(shadowFrame.sprites.begin()),std::make_move_iterator(shadowFrame.sprites.end()));
    for(const auto& pose:poses){const auto& p=*pose.part;const auto name=image(p);const auto* tile=tiles_.lookup(name);
        if(!tile)continue;
        const auto size=tile->pixels;const auto columns=std::max(1u,p.tileCount);
        if(std::uint32_t(size.width)%columns)throw std::runtime_error("Original tile grid does not divide image");
        frame.sprites.push_back({name,{position.x+pose.center.x,position.y+pose.center.y},{size.width/float(columns),size.height},pose.angle,0,columns,pose.reverseY});
        frame.sprites.back().visualLayer=pose.layer;
    }
}
}

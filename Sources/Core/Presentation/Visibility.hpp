#pragma once
#include "Core/Presentation/MapPreview.hpp"
namespace hm2 {
struct VisualBounds {float left=0,bottom=0,right=1,top=1;};
struct VisibilityState {
    VisualBounds bounds;
    bool show=true,visual=false,removed=false,inScreen=true;
};
// QobBase::isInCam0x37750 and tick0x36d98. A non-visual machine's cached
// inScreen is distinct from its current camera overlap and its parts' caches.
[[nodiscard]] inline bool inCamera(const VisibilityState& s,Vec2 world,const MapCamera* camera){
    if(!s.show)return false;if(!camera)return true;
    const auto p=camera->toScreen(world);
    if(p.x+s.bounds.right< -60)return false;
    if(p.x+s.bounds.left>camera->viewport.width+60)return false;
    if(p.y+s.bounds.top< -60)return false;
    return p.y+s.bounds.bottom<=camera->viewport.height+60;
}
inline void refreshVisibility(VisibilityState& s,Vec2 world,const MapCamera* camera){
    if(s.show&&s.visual&&!s.removed)s.inScreen=inCamera(s,world,camera);
}
}

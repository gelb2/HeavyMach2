#include "Core/Presentation/CameraPresentation.hpp"
#include "Core/Presentation/HUDLayout.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace hm2 {
namespace {
constexpr float idleScale=1,maximumReferenceFocus=1.6f,finiteMapSpan=1022;
float requestedFocus(const CameraFocus& focus){
    const auto target=focus.target?focus.target:focus.lastPath;
    const float dx=target?target->x-focus.player.x:0,dy=target?target->y-focus.player.y:0;
    if(!std::isfinite(dx)||!std::isfinite(dy))throw std::invalid_argument("Nonfinite camera focus");
    // Keep the original distance response, expressed relative to idle rather
    // than multiplying it by a screen-cover factor a second time.
    return std::clamp(2.f-std::max(std::abs(dx)/360.f,std::abs(dy)/240.f),1.f,maximumReferenceFocus)/maximumReferenceFocus;
}
}
void CameraPresentation::configure(DisplayConfiguration display){
    (void)display.content();
    display_=display;distance_={};state_={};
    if(!display.enabled)return;
    state_.policy=display.compact&&display.focusZoom?CameraPolicy::compactFocus:CameraPolicy::fixedSurface;
    state_.fillSurface=display.compact;
    state_.idleScale=idleScale;
    if(display.compact){
        const float cover=std::max(display.canvas.width,display.canvas.height)/finiteMapSpan;
        // Every visible point remains inside the original finite map. Even
        // extreme compact aspect ratios use one uniform scale, never pixels
        // stretched beyond the source image or a letterbox.
        state_.idleScale=std::max(idleScale,cover);state_.minimumScale=cover;state_.edgeOverscan=false;
    }
    project();
}
void CameraPresentation::project(){
    if(!display_.enabled)return;
    // Regular surfaces never enter the compact controller. Retain the same
    // scale1/division/offset operations as the pre-change iPad path.
    state_.worldScale=state_.policy==CameraPolicy::compactFocus?state_.idleScale*distance_.current:1.f;
    const DisplayLayout layout(display_);
    state_.worldToCanvas=layout.worldAtScale(state_.worldScale);
    state_.viewport=layout.worldViewportAtScale(state_.worldScale);
}
void CameraPresentation::advance(CameraFocus focus){
    if(state_.policy!=CameraPolicy::compactFocus)return;
    float minimum=state_.minimumScale;
    const float bottomDistance=focus.player.y+512;
    if(bottomDistance>0){
        // A finite map cannot pan below its bottom edge. Keep the player's
        // center above the default HUD at that edge while zooming out. User
        // placements remain intentional overlays and do not change the world.
        const DisplayLayout layout(display_);const float scale=state_.worldScale;
        const float left=(focus.player.x-512)*scale+display_.canvas.width,right=(focus.player.x+512)*scale;
        for(unsigned group=0;group<hud::count;++group){const auto b=hud::visualBounds({},group,layout);if(left<=b.x+b.width&&right>=b.x)minimum=std::max(minimum,std::min(state_.idleScale,(b.y+b.height+2)/bottomDistance));}
    }
    distance_.target=std::max(minimum/state_.idleScale,requestedFocus(focus));
    distance_.current=std::max(minimum/state_.idleScale,distance_.current+(distance_.target-distance_.current)/100.f);
    state_.currentZoom=distance_.current;state_.targetZoom=distance_.target;project();
}
}

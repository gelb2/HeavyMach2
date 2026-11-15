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
    state_.idleScale=display.compact?idleScale:std::max(display.canvas.width/768.f,display.canvas.height/1024.f);
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
    // The pre-iPhone HD app rendered a 768x1024 logical canvas fitted by the
    // renderer. Restore that uniform projection; physical points are not HD
    // world units. Keep the separate Phone focus controller unchanged.
    state_.worldScale=state_.policy==CameraPolicy::compactFocus?state_.idleScale*distance_.current:state_.idleScale;
    const DisplayLayout layout(display_);
    state_.worldToCanvas=display_.compact?layout.worldAtScale(state_.worldScale):DisplayTransform{state_.worldScale,{0,0}};
    state_.viewport=display_.compact?layout.worldViewportAtScale(state_.worldScale):Size{display_.canvas.width/state_.worldScale,display_.canvas.height/state_.worldScale};
    // The 13-inch surface differs from the source 3:4 canvas by less than
    // one source pixel. Keep the original HD camera/visibility arithmetic;
    // subpixel output clipping must not select a different gameplay policy.
    if(!display_.compact&&std::round(state_.viewport.width)==768&&std::round(state_.viewport.height)==1024){
        state_.policy=CameraPolicy::legacyHD;state_.viewport={768,1024};
        state_.worldToCanvas.offset={(display_.canvas.width-768*state_.worldScale)*.5f,(display_.canvas.height-1024*state_.worldScale)*.5f};
    }
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

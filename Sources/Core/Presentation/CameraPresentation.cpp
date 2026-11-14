#include "Core/Presentation/CameraPresentation.hpp"
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
        // An extremely wide/short surface uses the existing map edge sampler,
        // not a larger idle unit size. No new world geometry is created.
        state_.minimumScale=std::min(idleScale,cover);state_.edgeOverscan=cover>idleScale;
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
    distance_.target=std::max(state_.minimumScale/state_.idleScale,requestedFocus(focus));
    distance_.current+=(distance_.target-distance_.current)/100.f;
    state_.currentZoom=distance_.current;state_.targetZoom=distance_.target;project();
}
}

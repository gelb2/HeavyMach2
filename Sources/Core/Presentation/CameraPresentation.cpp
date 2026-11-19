#include "Core/Presentation/CameraPresentation.hpp"
#include "Core/Presentation/HUDLayout.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace hm2 {
namespace {
constexpr float idleScale=1,maximumReferenceFocus=1.6f,finiteMapSpan=1022;
constexpr float hudLeadTicks=28,hudEaseTicks=12,maxHudScaleStep=.008f;
float requestedFocus(const CameraFocus& focus){
    const auto target=focus.target?focus.target:focus.lastPath;
    const float dx=target?target->x-focus.player.x:0,dy=target?target->y-focus.player.y:0;
    if(!std::isfinite(dx)||!std::isfinite(dy))throw std::invalid_argument("Nonfinite camera focus");
    // Keep the original distance response, expressed relative to idle rather
    // than multiplying it by a screen-cover factor a second time.
    return std::clamp(2.f-std::max(std::abs(dx)/360.f,std::abs(dy)/240.f),1.f,maximumReferenceFocus)/maximumReferenceFocus;
}

float hudScale(const DisplayLayout& layout,Vec2 player,float floor,float ceiling,float reachScale,
               bool anticipate,float descent){
    const float bottomDistance=player.y+512;
    if(!(bottomDistance>0))return floor;
    const float left=(player.x-512)*reachScale+layout.configuration().canvas.width;
    const float right=(player.x+512)*reachScale;
    float scale=floor;
    for(unsigned group=0;group<hud::count;++group){
        const auto bounds=hud::visualBounds({},group,layout);
        const float top=bounds.y+bounds.height+2;
        if(anticipate){
            const float gap=std::max({0.f,left-(bounds.x+bounds.width),bounds.x-right});
            // Begin raising the target within one HUD height of overlap.
            // At overlap the full correction contributes to the target.
            const float weight=1-std::clamp(gap/bounds.height,0.f,1.f);
            // Lead a descending player. Standing near the bottom must still
            // allow the target distance to zoom out as far as HUD clearance permits.
            const float lead=std::min(bounds.height*.5f/reachScale,descent*hudLeadTicks);
            const float distanceAhead=bottomDistance-lead;
            const float required=distanceAhead>top/ceiling?top/distanceAhead:ceiling;
            scale=std::max(scale,std::lerp(floor,required,weight));
        }else if(left<=bounds.x+bounds.width&&right>=bounds.x){
            scale=std::max(scale,std::min(ceiling,top/bottomDistance));
        }
    }
    return scale;
}
}
void CameraPresentation::configure(DisplayConfiguration display){
    (void)display.content();
    display_=display;distance_={};previousPlayerY_.reset();state_={};
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
    // A finite map cannot pan below its bottom edge. Keep the default HUD
    // correction independent of user placements and of the previous phone zoom.
    const bool tablet=display_.device==DisplayDevice::tablet;
    const float descent=std::max(0.f,previousPlayerY_.value_or(focus.player.y)-focus.player.y);
    const float targetFloor=hudScale(DisplayLayout(display_),focus.player,state_.minimumScale,state_.idleScale,
                                     tablet?state_.worldScale:state_.idleScale,!tablet,descent);
    const float currentFloor=tablet?targetFloor:state_.minimumScale;
    distance_.target=std::max(targetFloor/state_.idleScale,requestedFocus(focus));
    float step=(distance_.target-distance_.current)/100.f;
    const float hudGap=targetFloor/state_.idleScale-distance_.current;
    // Move in sooner for the HUD while retaining the distance zoom's ease.
    if(!tablet&&hudGap>0)step=std::max(step,std::min(hudGap/hudEaseTicks,maxHudScaleStep/state_.idleScale));
    distance_.current=std::max(currentFloor/state_.idleScale,distance_.current+step);
    previousPlayerY_=focus.player.y;
    state_.currentZoom=distance_.current;state_.targetZoom=distance_.target;project();
}
}

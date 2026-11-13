#include "Core/Simulation/WorldNavigation.hpp"
#include <bit>
#include <cassert>
#include <cmath>
#include <numbers>
using namespace hm2;
void same(float a,float b,float tolerance=.0001f){assert(std::abs(a-b)<tolerance);}
int main(){
    BipodMotion walk;walk.setDestination({100,0});
    // Expected values derived independently from original ARM operations;
    // .local/evidence/.../player/derived-no-target-trace.json is not runtime.
    walk.tick();assert(walk.state().walkStep==3&&walk.state().currentFoot==1);same(walk.state().position.x,0);
    walk.tick();same(walk.state().stepLen,3.4482758045f);same(walk.state().moveLen,.5747126341f);same(walk.state().position.x,.5747126341f);same(walk.state().left.addX,-.5747126341f);same(walk.state().right.addX,2.8735632896f);same(walk.state().bodyAddY,-.3448275924f);
    walk.tick();same(walk.state().position.x,1.5292639732f);
    for(int i=4;i<=21;++i)walk.tick();assert(walk.state().walkStep==1);same(walk.state().position.x,17.9356803894f);
    walk.tick();assert(walk.state().currentFoot==0&&walk.state().walkStep==3);same(walk.state().position.x,14.9464006424f);same(walk.state().stepLen,-17.9356803894f);same(walk.state().bodyAddY,-2);
    for(int i=23;i<=40;++i)walk.tick();same(walk.state().position.x,33.06689453125f);same(walk.state().stepLen,18.7419853210f);same(walk.state().bodyAddY,1.8741985559f);
    // A new goal must retain in-progress gait, not restart or snap the player.
    const auto before=walk.state();walk.setDestination({200,100});same(walk.state().stepLen,before.stepLen);same(walk.state().moveLen,before.moveLen);assert(walk.state().currentFoot==before.currentFoot);
    // Interpolation first, then .01 snap: .011*(6/7)<.01.
    BipodState foot;foot.state=1;foot.walkStep=3;foot.currentFoot=0;foot.destination={100,0};foot.left.angle=.011f;BipodMotion snap(foot);snap.tick();same(snap.state().left.angle,0,.000001f);
    // Reaching the ±2 box changes state but leaves residual movement and Y.
    auto stopped=before;stopped.destination=stopped.position;BipodMotion stop(stopped);stop.tick();assert(stop.state().state==0);same(stop.state().bodyAddY,before.bodyAddY);assert(stop.state().speed!=0);
    const auto coast=stop.state();stop.tick();same(stop.state().bodyAddY,coast.bodyAddY);same(stop.state().bodyScaleState,coast.bodyScaleState);
    // Target aiming does not start walking; nearby weapons converge separately.
    BipodMotion aim;aim.tick(nullptr,Vec2{100,0});same(aim.state().position.x,0);same(aim.state().leftWeaponAngle,-.125f);same(aim.state().rightWeaponAngle,.125f);assert(aim.state().weaponReady);
    BipodMotion north;north.tick(nullptr,Vec2{0,200});same(north.state().bodyAngle,std::numbers::pi_v<float>/20);assert(!north.state().weaponReady);same(north.state().leftWeaponAngle,north.state().rightWeaponAngle);
    BipodState hidden;hidden.inScreen=false;BipodMotion offscreen(hidden);offscreen.setDestination({100,0});offscreen.tick();same(offscreen.state().position.x,0);assert(offscreen.state().walkStep==1);
    // Input camera inverse, HUD strip, ownership, drag/end thresholds, capacity.
    WorldNavigation nav;MapCamera camera{{},{},{480,320}};BipodMotion actor;
    assert(!nav.input(PointerPhase::begin,{300,99},1,camera,actor));assert(nav.count()==0);
    assert(nav.input(PointerPhase::begin,{340,160},1,camera,actor));same(actor.state().destination.x,100);same(actor.state().destination.y,0);
    assert(!nav.input(PointerPhase::begin,{400,160},2,camera,actor));
    nav.input(PointerPhase::move,{400,160},1,camera,actor);assert(nav.count()==1); // exactly60 does not append.
    nav.input(PointerPhase::move,{401,160},1,camera,actor);assert(nav.count()==2);
    nav.input(PointerPhase::end,{431,160},1,camera,actor);assert(nav.count()==2&&nav.pointer()==0); // exactly30 does not append.
    nav.input(PointerPhase::begin,{400,160},1,camera,actor);assert(nav.count()==2);same(nav.point(1).x,161); // Near later waypoint trims at stored point.
    for(int i=0;i<20;++i)nav.input(PointerPhase::move,{float(500+i*100),160},1,camera,actor);assert(nav.count()==12);same(nav.point(11).x,2160);
    nav.input(PointerPhase::cancel,{},1,camera,actor);assert(nav.pointer()==0);
    nav.input(PointerPhase::begin,{340,160},1,camera,actor);const auto oldCount=nav.count();nav.input(PointerPhase::end,{800,160},2,camera,actor);assert(nav.pointer()==0&&nav.count()==oldCount);
    WorldNavigation empty;MapCamera c{{},{},{768,1024}};processCamera(c,{100,-100},empty);same(c.center.x,2);same(c.center.y,-2);same(c.backgroundWorld.x,1.88f);
    c.center={500,-500};processCamera(c,{500,-500},empty);same(c.center.x,128);same(c.center.y,-94);
    // Original HD framing remains identical with configured map fitting.
    for(const Vec2 start:{Vec2{0,0},Vec2{500,-500},Vec2{-600,600}})for(const Vec2 player:{Vec2{-275,-412},Vec2{100,-100},Vec2{500,500}}){MapCamera original,configured;original.center=configured.center=start;configured.fitWorld=true;for(unsigned i=0;i<20;++i){processCamera(original,player,empty);processCamera(configured,player,empty);assert(std::bit_cast<unsigned>(original.center.x)==std::bit_cast<unsigned>(configured.center.x)&&std::bit_cast<unsigned>(original.center.y)==std::bit_cast<unsigned>(configured.center.y));}}
    // A smaller view includes offset in its post-ease visible-focus limits.
    MapCamera small;small.viewport={970,500};small.fitWorld=small.keepFocusVisible=true;small.offset={20,-15};small.inputMinimumY=62.5f;small.focusMinimumY=86.25f;small.lowerOverscan=58.75f;small.focusRadius=25;constrainCamera(small,{-275,-412});const auto onScreen=small.toScreen({-275,-412});assert(onScreen.x>=25&&onScreen.x<=small.viewport.width-25&&onScreen.y>=small.focusMinimumY+25&&onScreen.y<=small.viewport.height-25);
    // Whole-map fit and an infeasible focus interval never invert std::clamp.
    small.viewport={1200,1400};constrainCamera(small,{5000,5000});assert(std::isfinite(small.center.x)&&std::isfinite(small.center.y));
    // Mixed movement/aim/idle states run together without sharing their state.
    BipodMotion moving,idle,targeted;moving.setDestination({500,0});
    for(int i=0;i<300;++i){moving.tick();idle.tick();targeted.tick(nullptr,Vec2{0,200});}
    assert(moving.state().position.x>100);same(idle.state().position.x,0);same(targeted.state().position.x,0);assert(targeted.state().weaponReady);
}

#include "Core/Simulation/WeaponController.hpp"
#include <cmath>
#include <stdexcept>

namespace hm2 {
namespace {
float unit(WeaponHost& host){
    const auto n=host.random();
    if(n>0x7fffffffu)throw std::runtime_error("World random value exceeds original Darwin domain");
    return float(n)*0x1p-31f;
}
Vec2 endpoint(const WeaponInfo& w,const WeaponEmission& shot,const WeaponTarget* target,WeaponHost& host){
    if(target){
        const Vec2 n{std::sin(shot.angle),-std::cos(shot.angle)};
        const Vec2 v{target->position.x-shot.muzzle.x,target->position.y-shot.muzzle.y};
        const float cross=n.x*v.y-n.y*v.x;
        const float dot=n.x*v.x+n.y*v.y;
        const float a=(cross*.1f)*w.aimRate,twoA=a+a;
        const float lateral=(dot*.5f)*w.aimRate;
        const float baseX=target->position.x-lateral*n.x;
        const float jitterX=unit(host)*twoA-a;
        const float baseY=target->position.y-lateral*n.y;
        const float jitterY=unit(host)*twoA-a;
        return {baseX+jitterX,baseY+jitterY};
    }
    const float jitter=unit(host)*.1f-.05f;
    const float range=float(w.shotRange);
    const float distance=range*.8f+(unit(host)*range)*.2f;
    const float angle=shot.angle+jitter;
    return {shot.muzzle.x+std::cos(angle)*distance,shot.muzzle.y+std::sin(angle)*distance};
}
}
void WeaponController::reload(double time){
    if(!std::isfinite(time)||!std::isfinite(float(time)))throw std::runtime_error("Invalid weapon reload time");
    state_.fireLeft=0;state_.fireTime=float(time);
}
void WeaponController::tick(const WeaponInfo* w,const WeaponTick& input,WeaponHost& host){
    if(!w||!input.pose.part)return;
    const auto* target=host.target();
    if((!target||target->hp<1)&&!input.isFire)return;
    if(!std::isfinite(input.time))throw std::runtime_error("Invalid weapon tick time");
    const float elapsed=float(input.time-double(state_.fireTime));
    if(state_.fireLeft<1&&elapsed>=w->reloadDelay)state_.fireLeft=w->shotCount;
    bool prepare=state_.fireLeft>0&&elapsed>=w->shotDelay&&input.weaponReady;
    if(prepare&&target){
        const Vec2 difference{input.machinePosition.x-target->position.x,input.machinePosition.y-target->position.y};
        const float squared=difference.x*difference.x+difference.y*difference.y;
        const int range=w->shotRange;
        if(squared>=float(range*range)){
            if(input.isPlayer)host.outOfRange(input.left);
            prepare=false;
        }
    }
    if(prepare){
        if(!input.currentMuzzle)throw std::runtime_error("Missing original part muzzle state");
        WeaponEmission shot;
        const auto local=Models::muzzlePosition(input.pose,*input.currentMuzzle);
        shot.muzzle={input.machinePosition.x+local.x,input.machinePosition.y+local.y};
        shot.angle=input.pose.angle;
        if(++*input.currentMuzzle>=input.pose.part->sockets.size())*input.currentMuzzle=1;
        shot.destination=endpoint(*w,shot,target,host);
        while(state_.fireLeft>0&&input.time>=double(float(state_.fireTime+w->shotDelay))){
            --state_.fireLeft;
            const float delay20=w->shotDelay*.2f;
            const float delta=unit(host)*(delay20+delay20)+w->shotDelay*-.2f;
            state_.fireTime=float(input.time+double(delta));
            host.emit(*w,shot);
        }
        state_.fireBound=-5;
        host.fireSound(*w,{(input.machinePosition.x-input.playerPosition.x)*.01f,(input.machinePosition.y-input.playerPosition.y)*.01f});
    }
    state_.fireBound+=(0.f-state_.fireBound)/10.f;
    if(input.parentAddX&&state_.fireBound<*input.parentAddX)*input.parentAddX=state_.fireBound*.5f;
}
}

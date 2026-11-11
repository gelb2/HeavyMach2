#include "Core/Simulation/Dash.hpp"
#include <cmath>

namespace hm2 {
namespace {
Vec2 normalized(Vec2 value){
    const float length=std::sqrt(value.x*value.x+value.y*value.y);
    if(length<1e-5f)return {};
    return {value.x/length,value.y/length};
}
float easyOut(float value){
    if(value==0.f)return value;
    if(.01f<std::abs(value))return value+(0.f-value)/8.f;
    return 0.f;
}
}
bool DashGesture::groundBegin(Vec2 target,Vec2 player,double time,float special,DashStartHost& host){
    if(tapTime_!=0.f&&time-double(tapTime_)<.5&&special>=100.f){
        host.setDashVelocity(normalized({target.x-player.x,target.y-player.y}));
        host.clearMovePointer();host.setSpecial(special-100.f);host.sound(14);
        // Successful source branch returns without changing tapDashTime.
        return true;
    }
    tapTime_=float(time);return false;
}
void DashGesture::pathDrag(bool ownsPointer,unsigned count){if(ownsPointer&&count)tapTime_=0;}
bool DashMotion::tick(float renderTime,const DashTick& tick,DashMotionHost& host){
    if(!tick.show||tick.paused||tick.state<0||tick.state>1||(tick.machineType!=2&&!tick.inScreen))return false;
    if(velocity_.x==0.f&&velocity_.y==0.f)return false;
    const Vec2 before=host.position(),delta{velocity_.x*10.f,velocity_.y*10.f};
    const float x=lastEffectPosition_.x-before.x,y=lastEffectPosition_.y-before.y;
    if(800.f<x*x+y*y){
        lastEffectPosition_=before; // Write happens even when the pool is full.
        if(auto* particle=host.allocateParticle(renderTime)){
            auto* tile=host.preparedTile("Efx_Dash.png");if(tile){tile->columns=1;tile->rows=1;}
            auto& p=particle->state();p.life=.2f;p.tile=tile;p.tileNumber=0;p.blendType=2;
            p.angle=float(std::atan2(double(delta.y),double(delta.x)));p.scale={.5f,.5f};p.scaleVelocity=.05f;
            p.position=before;particle->start(renderTime);
        }
    }
    host.addPosition(delta);velocity_.x=easyOut(velocity_.x);velocity_.y=easyOut(velocity_.y);return true;
}
}

#include "Core/Simulation/VehicleMotion.hpp"
#include <bit>
#include <cmath>
#include <numbers>

namespace hm2 {
namespace {
constexpr float tau=std::bit_cast<float>(0x40c90fdau);
float wrapDoubleComparison(float current,float wanted){
    const float delta=wanted-current;
    if(double(delta)>std::numbers::pi)current+=tau;
    else if(double(delta)<-std::numbers::pi)current-=tau;
    return current;
}
float wrapFloatComparison(float current,float wanted){
    const float delta=wanted-current;
    if(delta>std::numbers::pi_v<float>)current+=tau;
    else if(delta<-std::numbers::pi_v<float>)current-=tau;
    return current;
}
}
void VehicleMotion::tick(WorldRandom& random,const TerrainEdge* wall){
    auto& s=state_;if(s.state!=1)return;
    const Vec2 delta{s.destination.x-s.position.x,s.destination.y-s.position.y};
    if(std::abs(delta.x)<=1&&std::abs(delta.y)<=1){s.state=0;s.speed=0;return;}
    float wanted;
    if(wall&&wall->normal.x*delta.x+wall->normal.y*delta.y>0){
        const float direction=float(std::atan2(double(wall->dir.y),double(wall->dir.x)));
        wanted=wall->dir.x*delta.x+wall->dir.y*delta.y<0
            ?float(double(direction)+(std::numbers::pi+double(.1f))):direction-.1f;
    }else wanted=float(std::atan2(double(delta.y),double(delta.x)));
    s.angle=wrapDoubleComparison(s.angle,wanted);
    s.angle+=(wanted-s.angle)/s.rotationSpeed;
    if(delta.x*delta.x+delta.y*delta.y>900)s.speed+=(s.moveSpeed-s.speed)/100;
    else s.speed+=(.5f-s.speed)/60;
    s.velocity={std::cos(s.angle)*s.speed,std::sin(s.angle)*s.speed};
    s.position.x+=s.velocity.x;s.position.y+=s.velocity.y;
    if(random.random()%5==0){
        const float dx=randomUnit(random)-.5f,dy=randomUnit(random)-.5f;
        s.position.x+=dx;s.position.y+=dy;
        s.scaleState=1+(randomUnit(random)*.02f-.01f);
    }
}
std::optional<bool> EnemyWeaponAim::tick(double time,Vec2 position,float machineAngle,
        std::optional<Vec2> target,WorldRandom& random,bool machine,bool part,int hp){
    if(!machine||!part||hp<1)return std::nullopt;
    if(target){
        const float wanted=float(std::atan2(double(target->y-position.y),double(target->x-position.x)));
        state_.angle=wrapFloatComparison(state_.angle,wanted);
        state_.angle+=(wanted-state_.angle)/10;
        return std::abs(state_.angle-wanted)<.1f;
    }
    state_.angle=wrapFloatComparison(state_.angle,machineAngle);
    const float phase=float((time-double(state_.startTime))*.5);
    const float oscillation=std::cos(phase);
    const float amplitude=randomUnit(random)*.1f+.1f;
    const float wanted=machineAngle+oscillation*amplitude;
    state_.angle+=(wanted-state_.angle)/10;
    // Idle does not write the owning machine's weaponReady field.
    return std::nullopt;
}
}

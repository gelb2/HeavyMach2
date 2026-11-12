#include "Core/Simulation/ProjectileMotion.hpp"
#include <bit>
#include <cmath>
#include <stdexcept>

namespace hm2 {
namespace {float distanceSquared(Vec2 a,Vec2 b){const float x=a.x-b.x,y=a.y-b.y;return x*x+y*y;}}
ProjectileMotion::ProjectileMotion(const WeaponInfo& info,Vec2 source,Vec2 destination,double time,std::uint32_t random,bool hostile):kind_(info.kind){
    if(info.kind==WeaponKind::gun||info.kind==WeaponKind::sniper)throw std::runtime_error("Direct-damage weapon must not create a GobBullet");
    if(random>0x7fffffffu||!std::isfinite(time))throw std::runtime_error("Invalid original projectile start state");
    state_.position=source;state_.destination=destination;state_.toPlayer=hostile;state_.startTime=float(time);
    state_.shotLengthSquared=state_.previousLengthSquared=distanceSquared(destination,source);
    state_.angle=float(std::atan2(double(destination.y-source.y),double(destination.x-source.x)));
    const float u=float(random)*0x1p-31f;
    const float speed=info.speed+((u*info.speed)*.2f)-info.speed*.1f;
    const Vec2 direction{std::cos(state_.angle),std::sin(state_.angle)};
    if(info.kind==WeaponKind::missile){
        state_.acceleration={(direction.x*speed)*.05f,(direction.y*speed)*.05f};
        state_.velocity={(direction.x*speed)*.3f,(direction.y*speed)*.3f};
    }else state_.velocity={direction.x*speed,direction.y*speed};
}
bool ProjectileMotion::tick(bool paused){
    if(paused||!state_.show||state_.removed)return false;
    if(state_.collideCheck)throw std::runtime_error("Point-collision GobBullet needs its world query consumer");
    if(state_.gravity!=0)state_.velocity.y-=state_.gravity;
    if(kind_==WeaponKind::missile){
        state_.gravity=0;state_.velocity.x+=state_.acceleration.x;state_.velocity.y+=state_.acceleration.y;
    }else if(kind_==WeaponKind::railgun)state_.angle+=.1f;
    else state_.angle=float(std::atan2(double(state_.velocity.y),double(state_.velocity.x)));
    constexpr float increment=std::bit_cast<float>(0x3c87fcb9u);
    state_.velocityTick={state_.velocity.x/increment,state_.velocity.y/increment};
    state_.position.x+=state_.velocityTick.x;state_.position.y+=state_.velocityTick.y;
    const float squared=distanceSquared(state_.destination,state_.position);
    const float stepSquared=state_.velocityTick.x*state_.velocityTick.x+state_.velocityTick.y*state_.velocityTick.y;
    const bool collided=state_.previousLengthSquared<squared||squared<stepSquared;
    state_.previousLengthSquared=squared;
    return collided;
}
}

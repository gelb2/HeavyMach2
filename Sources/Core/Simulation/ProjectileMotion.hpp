#pragma once
#include "Core/Data/Weapons.hpp"
#include "Core/Types.hpp"

namespace hm2 {
struct ProjectileState {
    Vec2 position,destination,velocity,acceleration,velocityTick;
    float angle=0,gravity=0,shotLengthSquared=0,previousLengthSquared=0,startTime=0;
    bool show=true,removed=false,collideCheck=false,toPlayer=false;
};
// GobBullet's movement consumer, separate from actual world damage, sound and
// effect consumers. Default controller-created bullets retain collideCheck=false.
class ProjectileMotion {
    WeaponKind kind_;
    ProjectileState state_;
public:
    ProjectileMotion(const WeaponInfo&,Vec2 source,Vec2 destination,double time,std::uint32_t worldRandom,bool hostile=false);
    ProjectileMotion(const WeaponInfo& info,ProjectileState state):kind_(info.kind),state_(state){}
    // Returns the original endpoint collision event. The world must run the
    // setCollide attack/effects/removal path; this is not a successful hit flag.
    bool tick(bool paused=false);
    void remove(){state_.removed=true;}
    [[nodiscard]] const ProjectileState& state()const{return state_;}
};
}

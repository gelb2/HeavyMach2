#pragma once
#include "Core/Simulation/TerrainCollision.hpp"
#include "Core/Simulation/WorldRandom.hpp"
#include <optional>

namespace hm2 {
struct VehicleState {
    Vec2 position,destination,velocity;
    float angle=0,speed=0,moveSpeed=.3f,rotationSpeed=30,scaleState=1;
    int state=0;
};
// GobHvMach::processMove_Vehicle0xeb08. Visibility, pause and object collision
// belong to the owning world's common processMove, not this movement function.
class VehicleMotion {
    VehicleState state_;
public:
    explicit VehicleMotion(VehicleState state={}):state_(state){}
    void setDestination(Vec2 destination){state_.destination=destination;state_.state=1;}
    void applyContact(Vec2 position){state_.position=position;}
    void tick(WorldRandom&,const TerrainEdge* currentWall=nullptr);
    [[nodiscard]] const VehicleState& state()const{return state_;}
};
struct EnemyWeaponAimState {float angle=0,startTime=0;};
// PartsController_WeaponAI init0x1e2e4/tick0x1e354. Each weapon part has its own
// start time/angle and shares the world's RNG. The common fire controller runs
// after this method, including early guard cases in the original superclass.
class EnemyWeaponAim {
    EnemyWeaponAimState state_;
public:
    explicit EnemyWeaponAim(EnemyWeaponAimState state={}):state_(state){}
    std::optional<bool> tick(double time,Vec2 position,float machineAngle,
                             std::optional<Vec2> livingTarget,WorldRandom&,
                             bool machinePresent=true,bool partPresent=true,int hp=1);
    [[nodiscard]] const auto& state()const{return state_;}
};
}

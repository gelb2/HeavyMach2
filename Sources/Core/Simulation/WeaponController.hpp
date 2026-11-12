#pragma once
#include "Core/Data/Models.hpp"
#include "Core/Data/Weapons.hpp"
#include "Core/Simulation/WorldRandom.hpp"
#include <cstdint>

namespace hm2 {
struct WeaponTarget {
    std::uint64_t id=0;
    Vec2 position;
    int hp=0;
};
struct WeaponEmission {
    Vec2 muzzle,destination;
    float angle=0;
};
// Calls use the owning world's RNG and real effect/attack consumers. Allocation
// and particle reset may consume RNG within emit(); no per-weapon RNG exists.
struct WeaponHost:WorldRandom {
    virtual ~WeaponHost()=default;
    virtual const WeaponTarget* target()const=0;
    virtual void emit(const WeaponInfo&,const WeaponEmission&)=0;
    virtual void outOfRange(bool left)=0;
    virtual void fireSound(const WeaponInfo&,Vec2 relativePosition)=0;
};
struct WeaponTick {
    double time=0;
    Vec2 machinePosition,playerPosition;
    PartPose pose;
    unsigned* currentMuzzle=nullptr;
    float* parentAddX=nullptr;
    bool weaponReady=false,isFire=false,isPlayer=false,left=false;
};
struct WeaponControllerState {int fireLeft=0;float fireTime=0,fireBound=0;};
// PartsController_Weapon::setReload0x1cee0 and tick0x1cf38. The common control
// flow emits attacks; each weapon kind's real damage/projectile/VFX consumer is
// supplied by the world. This class does not manufacture successful hits.
class WeaponController {
    WeaponControllerState state_;
public:
    explicit WeaponController(double time=0){reload(time);}
    explicit WeaponController(WeaponControllerState state):state_(state){}
    void reload(double time);
    void tick(const WeaponInfo*,const WeaponTick&,WeaponHost&);
    [[nodiscard]] const WeaponControllerState& state()const{return state_;}
};
}

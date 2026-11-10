#pragma once
#include "Core/Data/Machines.hpp"
#include "Core/Simulation/VehicleMotion.hpp"
#include "Core/Simulation/WeaponController.hpp"
#include <optional>
#include <span>

namespace hm2 {
class EnemyVehicle;
struct EnemyVehicleEmission {
    const EnemyVehicle* owner=nullptr;
    Vec2 machinePosition,partCenter;
    std::size_t mountIndex=0;
    bool hostile=true;
};
struct EnemyVehicleHost:WorldRandom {
    virtual const WeaponTarget* player()const=0;
    // Called after terrain correction and before common target following.
    // The world applies original enemy-list/player contact order and may move
    // either body; applyContact() updates this actor without changing its wall.
    virtual void objectContacts(EnemyVehicle&)=0;
    virtual void emit(const WeaponInfo&,const WeaponEmission&,const EnemyVehicleEmission&)=0;
    virtual void fireSound(const WeaponInfo&,Vec2 relativePosition)=0;
};
struct EnemyVehicleTick {double time=0;bool paused=false,inScreen=true;};
struct EnemyVehicleState {
    int hp=0;
    bool show=true,firstAttack=false,targetPlayer=false,moveToTarget=false,weaponReady=false,attackable=false;
    float createTime=0,stopDelay=0,attackCheckTime=0,targetCheckTime=0,lastAttackTime=0,attackDelay=1,liveTime=12;
};
struct EnemyVehicleWeapon {
    std::size_t mountIndex=0;
    const WeaponInfo* info=nullptr;
    EnemyWeaponAim aim;
    WeaponController controller{WeaponControllerState{}};
    unsigned currentMuzzle=1;
    bool removed=false;
};
// GobHvM_Enemy's verified vehicle path. Machine/weapon metadata and model graph
// remain common catalogues, not per-unit C++ assembly definitions.
class EnemyVehicle {
    const Models& models_;
    const MachineInfo& info_;
    VehicleMotion motion_;
    EnemyVehicleState state_;
    TerrainContact contact_;
    std::optional<Vec2> spawner_;
    std::vector<PartMount> mounts_;
    std::vector<bool> activeParts_;
    std::vector<EnemyVehicleWeapon> weapons_;
    std::vector<PartPose> poses_,atlasPoses_;
    void vehicleAI(double,EnemyVehicleHost&);
    void attackAI(double,EnemyVehicleHost&);
    void followTarget(double,EnemyVehicleHost&);
    void tickParts(double,EnemyVehicleHost&);
public:
    EnemyVehicle(const Models&,const Weapons&,const MachineInfo&,Vec2 position,double time,
                 WorldRandom&,std::optional<Vec2> spawnerPosition=std::nullopt);
    void tick(EnemyVehicleTick,const TerrainCollision&,EnemyVehicleHost&);
    void setDestination(Vec2 value){motion_.setDestination(value);}
    void applyContact(Vec2 value){motion_.applyContact(value);}
    void setMachineState(int);
    // Enemy.onCrash: retain insertion-index0 as the layer1 corpse and remove
    // every later part, including its child controller. Does not clear target
    // or change HP; the original damage caller may retarget after onCrash.
    void removePartsForCorpse();
    [[nodiscard]] EnemyVehicleState& state(){return state_;}
    [[nodiscard]] const EnemyVehicleState& state()const{return state_;}
    [[nodiscard]] const MachineInfo& info()const{return info_;}
    [[nodiscard]] const VehicleState& motion()const{return motion_.state();}
    [[nodiscard]] const TerrainContact& contact()const{return contact_;}
    [[nodiscard]] std::span<const PartMount> mounts()const{return mounts_;}
    [[nodiscard]] bool partActive(std::size_t index)const{return activeParts_.at(index);}
    [[nodiscard]] std::span<const EnemyVehicleWeapon> weapons()const{return weapons_;}
    // Post-child part state: center was refreshed before the weapon's aim/fire.
    [[nodiscard]] std::span<const PartPose> poses()const{return poses_;}
    // Original atlas preparation precedes the part's child controller tick.
    [[nodiscard]] std::span<const PartPose> atlasPoses()const{return atlasPoses_;}
};
}

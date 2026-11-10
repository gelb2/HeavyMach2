#pragma once
#include "Core/Simulation/EnemyVehicle.hpp"
#include <memory>

namespace hm2 {
class EnemyActor;
using EnemyActorState=EnemyVehicleState;
using EnemyActorWeapon=EnemyVehicleWeapon;
using EnemyActorTick=EnemyVehicleTick;
struct EnemyActorEmission {
    const EnemyActor* owner=nullptr;
    Vec2 machinePosition,partCenter;
    std::size_t mountIndex=0;
    bool hostile=true;
};
struct EnemyActorHost:WorldRandom {
    virtual const WeaponTarget* player()const=0;
    virtual void objectContacts(EnemyActor&)=0;
    virtual void emit(const WeaponInfo&,const WeaponEmission&,const EnemyActorEmission&)=0;
    virtual void fireSound(const WeaponInfo&,Vec2 relativePosition)=0;
};
// The three runtime kinds actually produced by HD MapInfo spawners. Existing
// vehicle behavior is delegated; non-spawned bipod/train paths are not guessed.
// World ordering, damage/drop/reward and delayed corpse destruction stay with
// the host, just as for EnemyVehicle.
class EnemyActor {
    const Models& models_;
    const MachineInfo& info_;
    bool boss_=false;
    std::unique_ptr<EnemyVehicle> vehicle_;
    EnemyActorState state_;
    VehicleState motion_;
    TerrainContact contact_;
    std::vector<PartMount> mounts_;
    std::vector<bool> activeParts_;
    std::vector<EnemyActorWeapon> weapons_;
    std::vector<PartPose> poses_,atlasPoses_;
    void aircraftAI(double,WorldRandom&);
    void moveAircraft();
    void attackAI(double,EnemyActorHost&);
    void followTarget(double,EnemyActorHost&);
    void tickParts(double,EnemyActorHost&);
public:
    static bool supports(MachineKind kind){return kind==MachineKind::vehicle||kind==MachineKind::turret||kind==MachineKind::aircraft;}
    EnemyActor(const Models&,const Weapons&,const MachineInfo&,Vec2,double time,
               WorldRandom&,std::optional<Vec2> spawner=std::nullopt);
    ~EnemyActor();
    EnemyActor(const EnemyActor&)=delete;
    EnemyActor& operator=(const EnemyActor&)=delete;
    void tick(EnemyActorTick,const TerrainCollision&,EnemyActorHost&);
    void setDestination(Vec2);
    void applyContact(Vec2);
    void setMachineState(int);
    void removePartsForCorpse();
    // Invoke AFTER the host's complete super-onDamage/onCrash continuation.
    // Aircraft retarget without enabling moveToTarget; turrets/vehicles do both.
    void retargetAfterDamage();
    void setBoss(bool value){boss_=value;} // EnemySpawner supplies its boss flag.
    [[nodiscard]] bool isBoss()const{return boss_;}
    [[nodiscard]] EnemyActorState& state();
    [[nodiscard]] const EnemyActorState& state()const;
    [[nodiscard]] const VehicleState& motion()const;
    [[nodiscard]] const MachineInfo& info()const{return info_;}
    [[nodiscard]] const TerrainContact& contact()const;
    [[nodiscard]] std::span<const PartMount> mounts()const;
    [[nodiscard]] bool partActive(std::size_t)const;
    [[nodiscard]] std::span<const EnemyActorWeapon> weapons()const;
    [[nodiscard]] std::span<const PartPose> poses()const;
    [[nodiscard]] std::span<const PartPose> atlasPoses()const;
};
}

#pragma once
#include "Core/Data/Machines.hpp"
#include "Core/Simulation/VehicleMotion.hpp"
#include "Core/Simulation/WeaponController.hpp"
#include <span>
#include <vector>

namespace hm2 {
class Bot;
struct BotItemTarget {std::uint64_t id=0;Vec2 position;};
struct BotEmission {
    const Bot* owner=nullptr;
    Vec2 machinePosition,partCenter;
    std::size_t mountIndex=0;
    bool hostile=false,playerShooter=false;
};
struct BotHost:WorldRandom {
    virtual const WeaponTarget* player()const=0;
    virtual const WeaponTarget* playerTarget()const=0;
    virtual const WeaponTarget* target(std::uint64_t id)const=0;
    // Original listItem child order, including children awaiting their later
    // listItem removal pass. Item IDs are stable identities, not vector addresses.
    virtual std::span<const BotItemTarget> items()const=0;
    virtual void emit(const WeaponInfo&,const WeaponEmission&,const BotEmission&)=0;
    virtual void fireSound(const WeaponInfo&,Vec2 relativePosition)=0;
    virtual void writeSlotHP(unsigned slot,int hp)=0; // Working slot memory only.
    virtual void clearSlotCode(unsigned slot)=0; // First byte of code, no refund.
    // QobHvMach.setTarget(nil) notifies the old actual target onTarget(false).
    // It does not clear Player's independent selection of that same target.
    virtual void unselectTarget(std::uint64_t id)=0;
    // Must execute the real common HvMach crash SFX21/particle/Player-target
    // continuation. This actor separately unselects its own target afterwards.
    // Enemy corpse/drop/reward behavior is NOT this callback.
    virtual void baseCrash(const Bot&)=0;
    // Common onDamage continuation: shared random()%3 + SFX15..17 and the
    // original relative-position calculation, after onCrash if HP reached0.
    virtual void hitSound(const Bot&)=0;
    // Bot.init creates gauge children before makeMach adds part children.
    // Called after movement/follow, before any part/controller callback.
    virtual void beforeParts(const Bot&){}
};
struct BotState {
    int hp=256;
    unsigned slotIndex=0;
    bool show=true,removed=false,moveToTarget=false,weaponReady=false,attackable=false;
    std::uint64_t target=0,pickItem=0;
    float createTime=0,stopDelay=0,jobCheckTime=0,hpRefreshTime=0,targetCheckTime=0;
    // Source fields with initialization but no observed runtime consumers.
    float liveTime=12,lastAttackTime=0,attackDelay=1;
};
struct BotGauge {float scaleX=.5f,x=-1;}; // BG scale.5,(0,20); FG scaleY.5,Y1.
struct BotWeapon {
    std::size_t mountIndex=0;const WeaponInfo* info=nullptr;
    EnemyWeaponAim aim;WeaponController controller{WeaponControllerState{}};
    unsigned currentMuzzle=1;
};
struct BotMetrics {std::uint64_t ticks=0,wanderAttempts=0,wanderAccepted=0,itemSearches=0,regeneratedHP=0,emissions=0,damageCalls=0,deaths=0;};
struct BotTick {double time=0;bool paused=false;};
// Original GobHvM_Bot. Models/WeaponInfo remain common decoded catalogues;
// this actor contains no unit IDs, custom image names or replacement assembly.
class Bot {
    const Models& models_;const MachineInfo& info_;
    BotState state_;VehicleState motion_;BotGauge gauge_;BotMetrics metrics_;
    std::vector<PartMount> mounts_;std::vector<BotWeapon> weapons_;
    std::vector<PartPose> poses_,atlasPoses_;
    void aircraftAI(double,BotHost&);
    void attackJob(BotHost&);
    void pickJob(double,BotHost&);
    void regenerate(double,BotHost&);
    void moveAircraft();
    void followTarget(double,BotHost&);
    void tickParts(double,BotHost&);
    void crash(BotHost&);
public:
    // Caller performs the source spawn XY RNG2 first. This constructor consumes
    // Bot.init stopDelay RNG1 then makeMachFromName direction RNG1. Slot index
    // and nonzero saved HP are assigned AFTER construction, as in setStage.
    Bot(const Models&,const Weapons&,const MachineInfo&,Vec2,double,WorldRandom&);
    void tick(BotTick,BotHost&);
    void setSlotIndex(unsigned);
    void setHP(int value){state_.hp=value;} // Original raw setter, no gauge/write.
    void setDestination(Vec2);
    void setShow(bool show){state_.show=show;}
    void clearTargetIf(std::uint64_t,BotHost&); // Enemy.onCrash before loot/rewards.
    void pickedItem(std::uint64_t); // FieldItem's actual pickItem: notification.
    void refreshHPGauge(BotHost&); // Writes actual slot HP, then visual ratio.
    // Shared QuickItems already writes the Bot slot before its gauge callback.
    // This adapter phase avoids duplicating that original memory write.
    void refreshGaugeVisual();
    void onDamage(int,BotHost&);
    [[nodiscard]] const BotState& state()const{return state_;}
    [[nodiscard]] const VehicleState& motion()const{return motion_;}
    [[nodiscard]] const BotGauge& gauge()const{return gauge_;}
    [[nodiscard]] const MachineInfo& info()const{return info_;}
    [[nodiscard]] const BotMetrics& metrics()const{return metrics_;}
    [[nodiscard]] std::span<const PartMount> mounts()const{return mounts_;}
    [[nodiscard]] std::span<const BotWeapon> weapons()const{return weapons_;}
    [[nodiscard]] std::span<const PartPose> poses()const{return poses_;}
    [[nodiscard]] std::span<const PartPose> atlasPoses()const{return atlasPoses_;}
};
}

#pragma once
#include "Core/Simulation/Particles.hpp"

namespace hm2 {
// Called after EasyMove's actual ground/path begin branch, never by enemy
// selection, Player-radius capture, item mode, or an invented HUD button.
struct DashStartHost {
    virtual ~DashStartHost()=default;
    virtual void setDashVelocity(Vec2)=0;
    virtual void clearMovePointer()=0;
    virtual void setSpecial(float)=0;
    virtual void sound(unsigned originalID)=0;
};
class DashGesture {
    float tapTime_=0;
public:
    // World.onTap_EasyMove0x12698: caller has already committed the path,
    // setMoveToTarget(false), state1, and applied the original input gates.
    // Those gates include unpaused/live Player, normal EasyMove dispatch,
    // screenY>=100, no move pointer, outside Player's distance-squared<900
    // capture and no live
    // enemy selected. ActionMove and quick-item modes do not call this API.
    bool groundBegin(Vec2 worldTarget,Vec2 player,double worldTime,float special,DashStartHost&);
    // The owned path-drag branch clears the timestamp even before testing
    // whether the drag is long enough to append a new waypoint.
    void pathDrag(bool ownsPointer,unsigned activePathCount);
    [[nodiscard]] float tapTime()const{return tapTime_;}
};
struct DashTick {
    bool show=true,paused=false,inScreen=true;
    int state=0;
    unsigned machineType=0; // Original0 bipod,1 vehicle,2 aircraft,3 turret,4 train.
};
struct DashMotionHost {
    virtual ~DashMotionHost()=default;
    virtual Vec2 position()const=0; // Position AFTER the ordinary machine move.
    virtual Particle* allocateParticle(float renderTime)=0; // Existing pool/RNG.
    virtual TileImage* preparedTile(std::string_view originalName)=0;
    virtual TileImage* prepareOriginalTile(std::string_view name){return preparedTile(name);}
    virtual void addPosition(Vec2)=0; // Original addPos, BEFORE terrain correction.
};
class DashMotion {
    Vec2 velocity_{},lastEffectPosition_{};
public:
    // Original setDashVel0xc508 writes velocity only; it does not reset the
    // previous effect position, change destination, or grant invulnerability.
    void setVelocity(Vec2 value){velocity_=value;}
    // Original processMove0xd6d8: ordinary kind move -> this consumer -> common
    // checkObjectCollide. No separate timer, cooldown or per-frame SP cost.
    bool tick(float renderTime,const DashTick&,DashMotionHost&);
    [[nodiscard]] Vec2 velocity()const{return velocity_;}
    [[nodiscard]] Vec2 lastEffectPosition()const{return lastEffectPosition_;}
};
}

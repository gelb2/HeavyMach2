#pragma once
#include "Core/Data/Weapons.hpp"
#include "Core/Presentation/PreparedTiles.hpp"
#include "Core/Presentation/Visibility.hpp"
#include "Core/Simulation/Particles.hpp"

namespace hm2 {
struct AirStrikeBullet {
    const WeaponInfo* weapon=nullptr;
    float angle=0;
    Vec2 source,destination;
    bool toPlayer=false,drop=true;
    int impactDamage=0;
};
struct AirStrikeHost {
    virtual ~AirStrikeHost()=default;
    // Prepared-only fixture lookup; live creation uses the explicit original
    // shared-manager request below, never a draw-time fallback.
    virtual TileImage* preparedTile(std::string_view)=0;
    virtual TileImage* prepareOriginalTile(std::string_view name){return preparedTile(name);}
    // Existing world pool allocate/reset, including its shared-world RNG.
    virtual Particle* allocateParticle(float renderTime)=0;
    virtual void sound(unsigned originalSfx)=0;
    // Must run common GobBullet init -> toPlayer(false) -> setFire -> drop(true)
    // -> addChild in this synchronous call, including that consumer's own RNG.
    // This is not PartsController WeaponEffects.emit (which adds a flash).
    virtual void createBullet(const AirStrikeBullet&)=0;
};
struct AirStrikeState {
    Vec2 position,velocity,target;
    float angle=0,scale=1.3f,startTime=0,fireDelay=0;
    int attackCount=0;
    bool removed=false;
};
// GobAirStrike0x5758/0x58b4/0x5c08. Inventory/quick selection belong to the
// caller. The shared Weapons cache and host-owned tiles outlive this actor.
class AirStrike {
    const WeaponInfo* weapon_;
    const TileImage* plane_;
    AirStrikeHost& host_;
    WorldRandom& random_;
    AirStrikeState state_;
    VisibilityState visibility_;
    std::uint64_t bulletRequests_=0;
    int impactDamage_;
public:
    AirStrike(const WeaponInfo*,float fireDelay,Vec2 target,Vec2 player,
              double worldTime,float renderTime,AirStrikeHost&,WorldRandom&,int impactDamage=0);
    // World.tick0x110d8 still ticks children while paused (worldTime freezes).
    // Preserve this actor's lack of a local pause gate; movement and an already
    // open firing branch continue. Removed actors are no longer ticked.
    void tick(double worldTime,const MapCamera* camera=nullptr);
    void collect(VisualLayer&,const MapCamera&,const PreparedTiles&);
    [[nodiscard]] const AirStrikeState& state()const{return state_;}
    [[nodiscard]] std::uint64_t bulletRequests()const{return bulletRequests_;}
};
}

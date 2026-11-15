#pragma once
#include "Core/Simulation/Particles.hpp"
#include "Core/Simulation/ProjectileMotion.hpp"
#include "Core/Simulation/Trail.hpp"
#include "Core/Simulation/WeaponController.hpp"
#include <memory>
#include <string_view>

namespace hm2 {
class ProjectileVisual;
struct WeaponEffectResources {
    TileImage* fire=nullptr;        // Efx_Fire.png
    TileImage* damage=nullptr;      // Efx_Damage.png
    TileImage* shell=nullptr;       // BulletShell.png
    TileImage* gunTrail=nullptr;    // Efx_Trail04.png
    TileImage* sniperTrail=nullptr; // Efx_Trail02.png
};
struct WeaponEffectInput {
    double worldTime=0;
    float renderTime=0;
    Vec2 machinePosition,partCenter,playerPosition;
    bool left=false,hostile=false,playerShooter=false;
    int damageBonus=0;
};
struct WeaponProjectile {
    // The world's immutable Weapons catalogue must outlive active projectiles.
    const WeaponInfo* info;
    ProjectileMotion motion;
    const TileImage* tile=nullptr;
    std::shared_ptr<Trail> trail;
    float effectDelay=.04f;
    int layer=5;
    std::shared_ptr<ProjectileVisual> visual{};
};
enum class WeaponParticleRole {flash,hitEffect,shell};
struct WeaponEffectsHost {
    virtual ~WeaponEffectsHost()=default;
    virtual const WeaponTarget* target()const=0;
    virtual void damage(const WeaponInfo&,std::uint64_t target,int amount)=0;
    // WeaponInfo-owned bullet/effect images were prepared globally at startup.
    // nullptr preserves the original nil tile behavior, including RNG/TTL.
    virtual TileImage* preparedTile(std::string_view)=0;
    // Hardcoded Gun/Sniper trail/shell requests occur at their original VFX
    // creation sites. A live host uses the shared filename cache (first miss
    // only); a fixture may preserve its supplied resources, including nil.
    virtual TileImage* prepareOriginalTile(std::string_view,TileImage* suppliedPrepared){return suppliedPrepared;}
    virtual void fireSound(const WeaponInfo&,Vec2 relativePosition)=0;
    // Transfers the actual initialized motion and shared trail to the world.
    // The world owns subsequent movement, setCollide damage/removal and draw.
    virtual void attachProjectile(WeaponProjectile)=0;
    // Observation hooks at actual particle start/trail attachment. They must
    // not alter the world/RNG; logical damage is exclusively damage() above.
    virtual void particleStarted(WeaponParticleRole,const Particle&){}
    virtual void trailAttached(const Trail&){}
};
// The original nine-kind Gun/Sniper direct-damage / seven GobBullet consumers
// inside PartsController_Weapon0x1cf38 and GobBullet::setFire0x69e4.
// Common controller sound/recoil remain in WeaponController, after emit().
class WeaponEffects {
    ParticlePool& particles_;
    WeaponEffectResources resources_;
    std::vector<std::shared_ptr<Trail>> trails_;
    void flash(const WeaponEmission&,const WeaponEffectInput&,WorldRandom&,WeaponEffectsHost&);
    std::shared_ptr<Trail> attachTrail(const TileImage*,Vec2,TrailConfig,float wind,float diffuse,
                                     double,WorldRandom&,WeaponEffectsHost&,bool endpoint,Vec2 destination);
public:
    WeaponEffects(ParticlePool&,WeaponEffectResources);
    void emit(const WeaponInfo&,const WeaponEmission&,const WeaponEffectInput&,WorldRandom&,WeaponEffectsHost&);
    // Common GobBullet setFire path. Controller emit invokes it after flash;
    // AirStrike invokes it without flash, then original setDrop before attach.
    void createProjectile(const WeaponInfo&,const WeaponEmission&,const WeaponEffectInput&,WorldRandom&,WeaponEffectsHost&,bool drop=false);
    void tickTrails(double worldTime,bool paused,WorldRandom&);
    void pruneRemovedTrails(); // Ownership cleanup only; no tick or RNG.
    void appendTrails(Frame&,const MapCamera&)const;
    [[nodiscard]] std::span<const std::shared_ptr<Trail>> trails()const{return trails_;}
};
}

#pragma once
#include "Core/Data/Weapons.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include "Core/Simulation/Particles.hpp"
#include "Core/Simulation/ProjectileMotion.hpp"
#include "Core/Simulation/Trail.hpp"
#include <memory>

namespace hm2 {
class PreparedTiles;
struct ProjectileVisualState {
    Vec2 bulletScale{1,1};
    float bulletAngle=0,effectAngle=0,effectRegenTime=0,effectDelay=.04f;
    unsigned parentTileNumber=0;
    bool effectVisible=true,drop=false;
};
struct ProjectileChildVisibility {
    // Original QobImage uses the previous-tick isInScreen field before its
    // QobBase parent refreshes it. The scene supplies these separate child gates.
    bool effect=false,bullet=false;
};
// GobBullet's visual child tick/atlas and parent draw consumers. Movement,
// collision/damage/removal and separate world-owned trail ticks stay with the
// caller. In particular draw() must not be folded into the simulation tick.
class ProjectileVisual {
    const WeaponInfo* info_;
    const TileImage* bullet_,*effect_;
    std::shared_ptr<Trail> trail_;
    ProjectileVisualState state_;
    bool hasEffectChild_=false;
public:
    ProjectileVisual(const WeaponInfo&,const TileImage* bullet,const TileImage* effect,
                     std::shared_ptr<Trail>,bool hostile=false);
    [[nodiscard]] const ProjectileVisualState& state()const{return state_;}
    [[nodiscard]] const TileImage* bulletTile()const{return bullet_;}
    [[nodiscard]] const TileImage* effectTile()const{return effect_;}
    [[nodiscard]] bool hasEffectChild()const{return hasEffectChild_;}
    [[nodiscard]] const std::shared_ptr<Trail>& trail()const{return trail_;}
    void setDrop(bool value){state_.drop=value;}
    void setParentTileNumber(unsigned value){state_.parentTileNumber=value;}
    void tick(const ProjectileState&,bool paused=false);
    // Consume the common prepared cache's original identity, split, origin,
    // Retina and pixel format; no per-projectile resource or identity path.
    void collect(VisualLayer&,const MapCamera&,const ProjectileState&,ProjectileChildVisibility,const PreparedTiles&)const;
    // Execute at the original parent object's non-atlas draw slot, including
    // during pause. May consume the same world RNG or mutate a shared trail.
    // Returns the actual started particle, or nullptr (no regen / full pool).
    Particle* draw(const ProjectileState&,double worldTime,float renderTime,ParticlePool&,WorldRandom&);
};
}

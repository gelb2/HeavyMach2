#include "Core/Simulation/ProjectileVisual.hpp"
#include "Core/Presentation/PreparedTiles.hpp"
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace hm2 {
namespace {
int type(const WeaponInfo& info){return static_cast<int>(info.effect);}
}
ProjectileVisual::ProjectileVisual(const WeaponInfo& info,const TileImage* bullet,const TileImage* effect,
                                 std::shared_ptr<Trail> trail,bool hostile)
    :info_(&info),bullet_(bullet),effect_(effect),trail_(std::move(trail)){
    state_.effectDelay=hostile?.06f:.04f;
    hasEffectChild_=type(info)==1||type(info)==2;
}
void ProjectileVisual::tick(const ProjectileState& motion,bool paused){
    if(paused||!motion.show)return;
    if(!motion.removed&&state_.drop&&bullet_){
        const float dx=motion.destination.x-motion.position.x,dy=motion.destination.y-motion.position.y;
        const float ratio=(dx*dx+dy*dy)/motion.shotLengthSquared;
        state_.bulletScale={ratio+.3f,ratio*.7f+.6f};
    }
    if(bullet_)state_.bulletAngle=motion.angle;
    if(hasEffectChild_){
        state_.effectAngle=motion.angle;
        if(type(*info_)==2)state_.effectVisible=!state_.effectVisible;
    }
}
void ProjectileVisual::collect(VisualLayer& layer,const MapCamera& camera,const ProjectileState& motion,
                               ProjectileChildVisibility visible,const PreparedTiles& tiles)const{
    if(!motion.show||motion.removed)return;
    // Original addChild insertion order is effect first, bullet second.
    if(hasEffectChild_&&state_.effectVisible&&effect_){
        VisualImage image;image.tile=tiles.visual(*effect_);image.blendType=2;image.useAtlas=true;
        image.screenPosition=camera.toScreen(motion.position);image.angle=state_.effectAngle;
        image.isInScreen=visible.effect;layer.submitImage(image);
    }
    if(bullet_){
        VisualImage image;image.tile=tiles.visual(*bullet_);image.useAtlas=true;
        image.screenPosition=camera.toScreen(motion.position);image.angle=state_.bulletAngle;
        image.scale=state_.bulletScale;image.isInScreen=visible.bullet;layer.submitImage(image);
    }
}
Particle* ProjectileVisual::draw(const ProjectileState& motion,double worldTime,float renderTime,
                                 ParticlePool& pool,WorldRandom& random){
    if(!motion.show||motion.removed)return nullptr;
    if(!std::isfinite(worldTime)||!std::isfinite(float(worldTime))||!std::isfinite(renderTime))
        throw std::runtime_error("Invalid original projectile effect clock");
    const int effectType=type(*info_);
    if(effectType==3){
        // The float addition is performed BEFORE comparison with double time;
        // equality emits nothing. Failed pool allocation still advances time.
        if(!(worldTime>double(state_.effectRegenTime+state_.effectDelay)))return nullptr;
        state_.effectRegenTime=float(worldTime);
        auto* particle=pool.allocate(renderTime,random);
        if(!particle)return nullptr;
        auto& p=particle->state();const bool hostile=motion.toPlayer;
        p.life=hostile?.5f:.6f;p.tile=effect_;p.tileNumber=int(state_.parentTileNumber);
        p.position=motion.position;
        const float scale=randomUnit(random)*(hostile?.2f:.4f)+(hostile?.1f:.2f);
        p.scale={scale,scale};p.scaleVelocity=randomUnit(random)*(hostile?.5f:1.5f)+.5f;
        const float angle=randomUnit(random)*std::numbers::pi_v<float>;p.angle=angle+angle;
        p.rotationVelocity=randomUnit(random)*(hostile?3.f:5.f)-(hostile?1.5f:2.5f);
        particle->start(renderTime);return particle;
    }
    if(effectType>2&&effectType<8&&trail_)trail_->add(motion.position,worldTime,random);
    return nullptr;
}
}

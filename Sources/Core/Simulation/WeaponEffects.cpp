#include "Core/Simulation/WeaponEffects.hpp"
#include "Core/Simulation/ProjectileVisual.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace hm2 {
namespace {
int directDamage(const WeaponInfo& info,const WeaponEffectInput& input){
    std::int32_t damage=info.damage;
    if(input.playerShooter){
        // Original ARM mul/add wrap at 32 bits; signed division truncates.
        const auto product=std::bit_cast<std::int32_t>(std::uint32_t(input.damageBonus)*std::uint32_t(damage));
        damage=std::bit_cast<std::int32_t>(std::uint32_t(damage)+std::uint32_t(product/100));
    }
    return damage;
}
}
WeaponEffects::WeaponEffects(ParticlePool& pool,WeaponEffectResources resources):particles_(pool),resources_(resources){
    // GobWorld::init prepares these shared splits before active firing.
    if(resources_.fire){resources_.fire->columns=4;resources_.fire->rows=4;}
    if(resources_.damage){resources_.damage->columns=6;resources_.damage->rows=1;}
}
void WeaponEffects::flash(const WeaponEmission& shot,const WeaponEffectInput& input,WorldRandom& random,WeaponEffectsHost& host){
    if(auto* particle=particles_.allocate(input.renderTime,random)){
        auto& s=particle->state();s.life=.1f;s.tile=resources_.fire;
        s.tileNumber=int(random.random()%12);s.position=shot.muzzle;
        const float rotation=randomUnit(random)*std::numbers::pi_v<float>;
        s.angle=rotation+rotation;particle->start(input.renderTime);
        host.particleStarted(WeaponParticleRole::flash,*particle);
    }
}
std::shared_ptr<Trail> WeaponEffects::attachTrail(const TileImage* tile,Vec2 position,TrailConfig config,float wind,float diffuse,
                                                double time,WorldRandom& random,WeaponEffectsHost& host,bool endpoint,Vec2 destination){
    auto trail=std::make_shared<Trail>(tile,position,config);
    trail->state().blendType=2;trail->state().wind=wind;trail->state().diffuse=diffuse;
    trail->add(position,time,random);if(endpoint)trail->add(destination,time,random);
    trails_.push_back(trail);host.trailAttached(*trail);return trail;
}
void WeaponEffects::createProjectile(const WeaponInfo& info,const WeaponEmission& shot,const WeaponEffectInput& input,WorldRandom& random,WeaponEffectsHost& host,bool drop){
    if(info.kind==WeaponKind::gun||info.kind==WeaponKind::sniper||static_cast<int>(info.kind)<0||static_cast<int>(info.kind)>8)
        throw std::runtime_error("Original direct-damage weapon cannot create GobBullet");
    if(!std::isfinite(input.worldTime)||!std::isfinite(float(input.worldTime)))throw std::runtime_error("Invalid original projectile clock");
    ProjectileMotion motion(info,shot.muzzle,shot.destination,input.worldTime,random.random(),input.hostile);
    const int effect=static_cast<int>(info.effect);
    const TileImage* effectTile=info.effectImage.empty()?nullptr:host.preparedTile(info.effectImage);
    std::shared_ptr<Trail> trail;
    if(effect>=4&&effect<=7){
        TrailConfig config;float wind=0,diffuse=.05f;
        switch(effect){
            case 4:config={4,16,64,.2f,.5f};wind=20;break;
            case 5:config={4,16,64,.3f,.3f};break;
            case 6:config={32,64,64,.2f,.3f};wind=40;diffuse=.1f;break;
            case 7:config={1,64,16,0,.05f};break;
        }
        trail=attachTrail(effectTile,shot.muzzle,config,wind,diffuse,input.worldTime,random,host,false,{});
    }
    const TileImage* image=info.bulletImage.empty()?nullptr:host.preparedTile(info.bulletImage);
    auto visual=std::make_shared<ProjectileVisual>(info,image,effectTile,trail,input.hostile);
    host.fireSound(info,{(shot.muzzle.x-input.playerPosition.x)/100.f,(shot.muzzle.y-input.playerPosition.y)/100.f});
    if(drop)visual->setDrop(true);
    host.attachProjectile({&info,std::move(motion),image,std::move(trail),input.hostile?.06f:.04f,5,std::move(visual),input.impactDamage});
}
void WeaponEffects::emit(const WeaponInfo& info,const WeaponEmission& shot,const WeaponEffectInput& input,WorldRandom& random,WeaponEffectsHost& host){
    const bool sniper=info.kind==WeaponKind::sniper;
    if(static_cast<int>(info.kind)<0||static_cast<int>(info.kind)>8)
        throw std::runtime_error("Unverified weapon emission consumer");
    if(!std::isfinite(input.worldTime)||!std::isfinite(float(input.worldTime))||!std::isfinite(input.renderTime))
        throw std::runtime_error("Invalid original weapon effect clock");
    flash(shot,input,random,host);
    if(info.kind!=WeaponKind::gun&&!sniper){
        createProjectile(info,shot,input,random,host);
        return;
    }
    if(auto* hit=particles_.allocate(input.renderTime,random)){
        auto& s=hit->state();s.blendType=2;s.life=.3f;s.tile=resources_.damage;s.tileNumber=0;s.position=shot.destination;
        hit->animate(6);hit->start(input.renderTime);host.particleStarted(WeaponParticleRole::hitEffect,*hit);
        auto* trailTile=host.prepareOriginalTile(sniper?"Efx_Trail02.png":"Efx_Trail04.png",sniper?resources_.sniperTrail:resources_.gunTrail);
        attachTrail(trailTile,shot.muzzle,
                    {sniper?4.f:1.f,64,16,0,sniper?1.f:.2f},0,sniper?.02f:0,
                    input.worldTime,random,host,true,shot.destination);
        if(auto* shell=particles_.allocate(input.renderTime,random)){
            const float directionUnit=randomUnit(random);
            const float jitter=(directionUnit+directionUnit)-1.f;
            const double side=input.left?std::numbers::pi/2:-std::numbers::pi/2;
            const float angle=float(double(shot.angle)+side+double(jitter));
            const float speedUnit=randomUnit(random);const float speed=(speedUnit+speedUnit)+1.5f;
            auto* shellTile=host.prepareOriginalTile("BulletShell.png",resources_.shell);
            if(shellTile){shellTile->columns=2;shellTile->rows=1;}
            const float lifeUnit=randomUnit(random);const float factor=sniper?.7f:.5f;
            auto& c=shell->state();c.life=lifeUnit*factor+factor;c.tile=shellTile;c.tileNumber=0;
            c.position={input.machinePosition.x+input.partCenter.x,input.machinePosition.y+input.partCenter.y};
            c.scale={sniper?.7f:.6f,sniper?.7f:.6f};c.scaleVelocity=-.004f;c.rotationVelocity=.4f;
            c.velocity={std::cos(angle)*speed,std::sin(angle)*speed};c.velocityAdd=-.05f;
            shell->start(input.renderTime);host.particleStarted(WeaponParticleRole::shell,*shell);
        }
    }
    // This is a fresh lookup after VFX, just as the original callback site.
    // A missing/dead target was already handled by the controller's entry gate;
    // this site only checks pointer presence, not HP or the visual endpoint.
    if(const auto* target=host.target())host.damage(info,target->id,directDamage(info,input));
}
void WeaponEffects::tickTrails(double time,bool paused,WorldRandom& random){
    // QobBase checks removeThis before ticking a child: newly removed trails
    // remain owned until the next pass. Projectile payloads may retain them.
    for(auto it=trails_.begin();it!=trails_.end();){
        if((*it)->state().removed)it=trails_.erase(it);
        else{(*it)->tick(time,paused,random);++it;}
    }
}
void WeaponEffects::pruneRemovedTrails(){
    std::erase_if(trails_,[](const auto& trail){return trail->state().removed;});
}
void WeaponEffects::appendTrails(Frame& frame,const MapCamera& camera)const{
    for(const auto& trail:trails_)trail->append(frame,camera);
}
}

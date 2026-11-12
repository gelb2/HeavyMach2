#pragma once
#include "Core/Data/Weapons.hpp"
#include "Core/Types.hpp"
#include <cstddef>

namespace hm2 {
struct ImpactTarget {std::uint64_t id=0;Vec2 position;int hp=0,radius=0;bool inScreen=true;};
struct ProjectileImpactHost {
    virtual ~ProjectileImpactHost()=default;
    virtual std::size_t enemyCount()const=0;
    virtual ImpactTarget enemy(std::size_t)const=0;
    virtual ImpactTarget player()const=0;
    virtual std::size_t botCount()const=0;
    virtual ImpactTarget bot(std::size_t)const=0;
    virtual std::uint64_t selectedEnemy()const=0;
    virtual int damageBonus()const=0;
    virtual void damage(std::uint64_t,int amount)=0;
    virtual void finishTrail(Vec2)=0;
    virtual void nuclearFlash(Vec2)=0;
    virtual void explosionSound(Vec2)=0;
    virtual void explosionParticle(const WeaponInfo&,Vec2)=0;
    virtual void removeProjectile()=0;
};
// GobBullet::setCollide0x71d0; QVector::length0x3f138 uses float sqrt.
// Callback order is observable: a damage call can synchronously kill/drop/level
// up before the next target. Never precompute a batch of successful hits.
void impactProjectile(const WeaponInfo&,Vec2 position,bool toPlayer,
                      ProjectileImpactHost&);
}

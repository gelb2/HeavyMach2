#include "Core/Simulation/ProjectileImpact.hpp"
#include <algorithm>
#include <cmath>

namespace hm2 {
namespace {
Vec2 delta(Vec2 source,Vec2 target){return {std::abs(source.x-target.x),std::abs(source.y-target.y)};}
float length(Vec2 p){return std::sqrt(p.x*p.x+p.y*p.y);}
bool inSquare(Vec2 d,int range){return d.x<float(range)&&d.y<float(range);}
}
void impactProjectile(const WeaponInfo& w,Vec2 position,bool hostile,ProjectileImpactHost& host,int impactDamage){
    const int baseDamage=impactDamage>0?impactDamage:w.damage;
    if(w.effect==WeaponEffect::laser||w.effect==WeaponEffect::trail)host.finishTrail(position);
    if(w.damageRange==0){
        // Original zero-range branch iterates enemies regardless of toPlayer;
        // it also omits isInScreen. Preserve this observed rule explicitly.
        for(std::size_t i=0;i<host.enemyCount();++i){
            const auto t=host.enemy(i);if(!t.id||t.hp<1)continue;
            float radius=float(t.radius);if(host.selectedEnemy()==t.id)radius+=radius;
            const auto d=delta(position,t.position);
            if(d.x*d.x+d.y*d.y<radius*radius)host.damage(t.id,baseDamage);
        }
    }else{
        if(w.kind==WeaponKind::nuclear)host.nuclearFlash(position);
        host.explosionSound(position);
        if(!hostile){
            for(std::size_t i=0;i<host.enemyCount();++i){
                const auto t=host.enemy(i);if(!t.id||t.hp<1||!t.inScreen)continue;
                const auto d=delta(position,t.position);if(!inSquare(d,w.damageRange))continue;
                float weight=1-length(d)/float(w.damageRange);if(weight<=0)continue;
                weight=std::min(1.f,weight+.5f);float damage=float(baseDamage)*weight;
                // Earlier onDamage may synchronously level up/change stats.
                // The original reads the global value for each target.
                const int bonus=host.damageBonus();
                if(bonus>0)damage+=(float(bonus)*damage)/100.f;
                host.damage(t.id,int(damage));
            }
        }else{
            const auto player=host.player();const auto d=delta(position,player.position);
            if(inSquare(d,w.damageRange))host.damage(player.id,int(float(baseDamage)*(1-length(d)/float(w.damageRange))));
            for(std::size_t i=0;i<host.botCount();++i){
                const auto t=host.bot(i);if(!t.id||t.hp<1||!t.inScreen)continue;
                const auto b=delta(position,t.position);if(inSquare(b,w.damageRange))host.damage(t.id,int(float(baseDamage)*(1-length(b)/float(w.damageRange))));
            }
        }
    }
    if(!w.explodeImage.empty())host.explosionParticle(w,position);
    host.removeProjectile();
}
}

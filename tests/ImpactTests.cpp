#include "Core/Simulation/ProjectileImpact.hpp"
#include <cassert>
#include <string>
#include <vector>
using namespace hm2;
struct Host:ProjectileImpactHost {
    std::vector<ImpactTarget> enemies,bots;ImpactTarget me{1,{0,0},256,25};
    std::vector<std::pair<std::uint64_t,int>> hits;std::vector<std::string> events;std::uint64_t selected=0;int bonus=0;bool changeBonusOnDamage=false;
    std::size_t enemyCount()const override{return enemies.size();}ImpactTarget enemy(std::size_t n)const override{return enemies.at(n);}
    ImpactTarget player()const override{return me;}std::size_t botCount()const override{return bots.size();}ImpactTarget bot(std::size_t n)const override{return bots.at(n);}
    std::uint64_t selectedEnemy()const override{return selected;}
    int damageBonus()const override{return bonus;}
    void damage(std::uint64_t id,int amount)override{hits.push_back({id,amount});events.push_back("damage");if(changeBonusOnDamage)bonus=100;}
    void finishTrail(Vec2)override{events.push_back("trail");}void nuclearFlash(Vec2)override{events.push_back("nuclear");}
    void explosionSound(Vec2)override{events.push_back("sound");}void explosionParticle(const WeaponInfo&,Vec2)override{events.push_back("particle");}void removeProjectile()override{events.push_back("remove");}
};
int main(){
    WeaponInfo w;w.kind=WeaponKind::missile;w.effect=WeaponEffect::trail;w.damage=26;w.damageRange=60;w.explodeImage="OriginalExplosion.png";
    Host h;h.enemies={{2,{},200,30},{3,{30,0},200,30},{4,{59,0},200,30},{5,{60,0},200,30},{6,{59,59},200,30},{7,{},200,30,false},{8,{},0,30}};
    h.bonus=10;impactProjectile(w,{},false,h);assert((h.hits==std::vector<std::pair<std::uint64_t,int>>{{2,28},{3,28},{4,14}}));
    assert((h.events==std::vector<std::string>{"trail","sound","damage","damage","damage","particle","remove"}));
    Host hostile;hostile.me.position={59,59};hostile.bots={{9,{30,0},100,20},{10,{},0,20},{11,{},100,20,false}};
    hostile.bonus=999;impactProjectile(w,{},true,hostile);assert(hostile.hits.size()==2&&hostile.hits[0].first==1&&hostile.hits[0].second==-10&&hostile.hits[1].second==13);
    Host changing;changing.enemies={{2,{},200,30},{3,{},200,30}};changing.changeBonusOnDamage=true;impactProjectile(w,{},false,changing);
    assert(changing.hits[0].second==26&&changing.hits[1].second==52);
    Host zero;zero.selected=2;zero.enemies={{2,{59,0},100,30,false},{3,{30,0},100,30},{4,{29,0},100,30}};
    w.damageRange=0;w.effect=WeaponEffect::none;zero.bonus=999;impactProjectile(w,{},true,zero);
    assert((zero.hits==std::vector<std::pair<std::uint64_t,int>>{{2,26},{4,26}}));assert(zero.events.front()=="damage");
    Host nuclear;w.kind=WeaponKind::nuclear;w.damageRange=60;impactProjectile(w,{},false,nuclear);
    assert((nuclear.events==std::vector<std::string>{"nuclear","sound","particle","remove"}));
}

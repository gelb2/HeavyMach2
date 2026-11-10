#include "Core/Simulation/WeaponEffects.hpp"
#include "Core/Simulation/ProjectileVisual.hpp"
#include <cassert>
#include <cmath>
#include <map>
#include <numbers>
#include <string>

using namespace hm2;
namespace {
void near(float a,float b){assert(std::abs(a-b)<.00001f);}
struct Host:WeaponHost,WeaponEffectsHost {
    std::map<std::string,TileImage,std::less<>> images;
    WeaponTarget victim{17,{200,40},1000};bool present=true,damageRandom=false,nilImages=false,projectileRandom=false;
    WeaponEffects* effects=nullptr;WeaponEffectInput input;
    unsigned reads=0;std::vector<std::uint32_t> values;
    std::vector<std::string> events;std::vector<int> damages;std::vector<Vec2> soundPositions;
    std::vector<WeaponProjectile> projectiles;
    Host(){
        for(const auto* name:{"Efx_Fire.png","Efx_Damage.png","BulletShell.png","Efx_Trail04.png","Efx_Trail02.png","Efx_Trail01.png","Blt_Missile01.png",
             "Blt_Bullet01.png","Blt_Canon01.png","Efx_Flame.png","Blt_Shotgun01.png","Efx_Trail03.png","Efx_Rail01.png","Blt_Railgun01.png","Blt_Nuclear01.png"})
            images.emplace(name,TileImage{name,{64,32},1,1,{}});
        images.at("Efx_Rail01.png").columns=4;
        input.worldTime=2.5;input.renderTime=17.5f;input.left=true;input.partCenter={12,21};input.playerShooter=true;
    }
    const WeaponTarget* target()const override{return present?&victim:nullptr;}
    std::uint32_t random()override{
        ++reads;events.push_back("R"+std::to_string(reads));return values.empty()?0x40000000u:values.at(reads-1);
    }
    void emit(const WeaponInfo& info,const WeaponEmission& shot)override{effects->emit(info,shot,input,*this,*this);}
    void outOfRange(bool)override{events.push_back("range");}
    void fireSound(const WeaponInfo&,Vec2 relative)override{events.push_back("audio");soundPositions.push_back(relative);}
    void damage(const WeaponInfo&,std::uint64_t id,int amount)override{
        assert(id==victim.id);events.push_back("damage");damages.push_back(amount);victim.hp-=amount;if(damageRandom)(void)random();
    }
    TileImage* preparedTile(std::string_view name)override{
        if(nilImages)return nullptr;const auto it=images.find(name);return it==images.end()?nullptr:&it->second;
    }
    void attachProjectile(WeaponProjectile p)override{events.push_back("projectile");projectiles.push_back(std::move(p));if(projectileRandom)(void)random();}
    void particleStarted(WeaponParticleRole role,const Particle&)override{
        events.push_back(role==WeaponParticleRole::flash?"flash":role==WeaponParticleRole::hitEffect?"hit":"shell");
    }
    void trailAttached(const Trail&)override{events.push_back("trail");}
    void clear(){reads=0;events.clear();damages.clear();soundPositions.clear();}
};
struct Fixture {
    Host host;ParticlePool pool;WeaponEffects effects;
    static WeaponEffectResources resources(Host& h){return {h.preparedTile("Efx_Fire.png"),h.preparedTile("Efx_Damage.png"),h.preparedTile("BulletShell.png"),h.preparedTile("Efx_Trail04.png"),h.preparedTile("Efx_Trail02.png")};}
    Fixture():pool(0,host),effects(pool,resources(host)){host.effects=&effects;host.clear();}
    void freeSlots(unsigned free){
        for(unsigned i=0;i<64-free;++i){auto* p=pool.allocate(0,host);assert(p);p->start(0);}host.clear();
    }
};
WeaponInfo gun(){WeaponInfo w;w.kind=WeaponKind::gun;w.damage=7;w.shotCount=10;w.shotDelay=.16f;w.reloadDelay=2.5f;w.shotRange=400;w.aimRate=1;w.fireSound="Wpn_Gun01.wav";return w;}
WeaponInfo missile(){auto w=gun();w.kind=WeaponKind::missile;w.damage=26;w.shotCount=2;w.shotDelay=.5f;w.reloadDelay=2;w.shotRange=600;w.speed=.2f;w.effect=WeaponEffect::trail;w.effectImage="Efx_Trail01.png";w.bulletImage="Blt_Missile01.png";w.fireSound="Wpn_Missile01.wav";return w;}
WeaponTick controllerInput(Part& part,unsigned& muzzle){
    WeaponTick in;in.time=2.5;in.pose={&part,{12,21},0,3,0,false};in.currentMuzzle=&muzzle;
    in.weaponReady=true;in.isPlayer=true;in.left=true;return in;
}
}
int main(){
    WeaponEmission shot{{38,24.5f},{163,24.5f},0};const auto w=gun();
    Fixture full;full.freeSlots(3);full.effects.emit(w,shot,full.host.input,full.host,full.host);
    assert((full.host.events==std::vector<std::string>{"R1","R2","R3","flash","R4","hit","trail","R5","R6","R7","R8","shell","damage"}));
    assert(full.host.damages==std::vector<int>{7}&&full.host.soundPositions.empty()&&full.host.projectiles.empty());
    const auto& flash=full.pool.particles()[61].state();const auto& hit=full.pool.particles()[62].state();const auto& shell=full.pool.particles()[63].state();
    near(flash.life,.1f);near(flash.angle,std::numbers::pi_v<float>);assert(flash.tileNumber==4&&flash.tile->columns==4&&flash.tile->rows==4);
    near(flash.startTime,17.5f);assert(hit.animationFrames==6&&hit.animate&&!hit.fadeOut&&hit.tile->columns==6);
    near(hit.position.x,163);near(hit.life,.3f);near(shell.scale.x,.6f);near(shell.life,.75f);
    near(shell.position.x,12);near(shell.position.y,21);near(shell.velocity.y,2.5f);near(shell.scaleVelocity,-.004f);
    assert(shell.tile->columns==2&&shell.tile->rows==1&&full.effects.trails().size()==1);
    const auto& trail=*full.effects.trails()[0];assert(trail.state().tile->name=="Efx_Trail04.png"&&trail.lastPosition()==2);
    near(trail.config().width,1);near(trail.config().fadeTime,.2f);near(trail.state().wind,0);near(trail.state().diffuse,0);
    Frame frame;MapCamera camera;full.effects.appendTrails(frame,camera);assert(frame.sprites.size()==1&&frame.sprites[0].strip.size()==6);
    full.effects.tickTrails(2.5,false,full.host); // Delay equality preserves alpha.
    assert(!trail.state().removed);full.effects.tickTrails(2.75,false,full.host);assert(trail.state().removed&&full.effects.trails().size()==1);
    full.effects.tickTrails(2.75,false,full.host);assert(full.effects.trails().empty());
    // End-to-end real controller -> emission consumer: saturation preserves
    // controller RNG, damage and common sound even with no visual capacity.
    const unsigned expected[]={3,6,7,11};
    for(unsigned free=0;free<4;++free){
        Fixture f;f.freeSlots(free);Part part{"Fixture",4,1,{{-9,-3},{17,.5f}}};unsigned muzzle=1;
        auto in=controllerInput(part,muzzle);WeaponController controller;controller.tick(&w,in,f.host);
        assert(f.host.reads==expected[free]&&f.host.damages==std::vector<int>{7}&&f.host.events.back()=="audio");
        near(f.host.soundPositions[0].x,0);assert(f.host.projectiles.empty());
        assert(f.effects.trails().size()==(free>=2?1:0));
    }
    // Damage callbacks can consume the same world stream before common audio.
    Fixture callback;callback.freeSlots(0);callback.host.damageRandom=true;
    Part part{"Fixture",4,1,{{-9,-3},{17,.5f}}};unsigned muzzle=1;auto in=controllerInput(part,muzzle);
    WeaponController controller;controller.tick(&w,in,callback.host);
    assert((callback.host.events==std::vector<std::string>{"R1","R2","R3","damage","R4","audio"}));
    // Direct damage checks target pointer after VFX, without endpoint hit tests.
    Fixture bonus;bonus.freeSlots(0);bonus.host.input.damageBonus=50;bonus.effects.emit(w,shot,bonus.host.input,bonus.host,bonus.host);assert(bonus.host.damages.back()==10);
    bonus.host.input.damageBonus=-50;bonus.effects.emit(w,shot,bonus.host.input,bonus.host,bonus.host);assert(bonus.host.damages.back()==4);
    bonus.host.input.playerShooter=false;bonus.effects.emit(w,shot,bonus.host.input,bonus.host,bonus.host);assert(bonus.host.damages.back()==7);
    bonus.host.present=false;const auto count=bonus.host.damages.size();bonus.effects.emit(w,shot,bonus.host.input,bonus.host,bonus.host);assert(bonus.host.damages.size()==count);
    // Sniper has its own hardcoded trail/casing, despite the info's Laser effect.
    Fixture sniper;sniper.freeSlots(3);auto s=w;s.kind=WeaponKind::sniper;s.damage=33;s.effect=WeaponEffect::laser;sniper.host.input.left=false;
    sniper.effects.emit(s,shot,sniper.host.input,sniper.host,sniper.host);
    assert(sniper.host.reads==8&&sniper.host.projectiles.empty()&&sniper.host.damages.back()==33);
    const auto& casing=sniper.pool.particles()[63].state();near(casing.scale.x,.7f);near(casing.life,.5f*.7f+.7f);near(casing.velocity.y,-2.5f);
    near(sniper.effects.trails()[0]->config().width,4);near(sniper.effects.trails()[0]->config().fadeTime,1);near(sniper.effects.trails()[0]->state().diffuse,.02f);
    assert(sniper.effects.trails()[0]->state().tile->name=="Efx_Trail02.png");
    // ML creates real motion/trail; its setFire sound precedes world attachment,
    // then the controller requests its separate common sound.
    const auto ml=missile();
    for(unsigned free=0;free<2;++free){
        Fixture f;f.freeSlots(free);Part mlPart{"Fixture",4,1,{{0,0},{20,-4}}};unsigned cur=1;
        auto mlInput=controllerInput(mlPart,cur);WeaponController c;c.tick(&ml,mlInput,f.host);
        assert(f.host.reads==(free?7:4)&&f.host.projectiles.size()==1&&f.host.damages.empty()&&f.host.soundPositions.size()==2);
        assert(f.host.events[f.host.events.size()-3]=="audio"&&f.host.events[f.host.events.size()-2]=="projectile"&&f.host.events.back()=="audio");
        auto& p=f.host.projectiles[0];assert(p.info==&ml&&p.trail==f.effects.trails()[0]&&p.tile->name=="Blt_Missile01.png"&&p.layer==5&&p.visual);
        assert(!p.motion.state().collideCheck&&!p.motion.state().toPlayer);near(p.effectDelay,.04f);near(p.motion.state().startTime,2.5f);
        near(p.trail->state().wind,20);near(p.trail->state().diffuse,.05f);near(p.trail->config().length,16);assert(p.trail->lastPosition()==1);
        near(f.host.soundPositions[0].x,p.motion.state().position.x/100.f);near(f.host.soundPositions[1].x,0);
        const auto before=p.motion.state().position;assert(!p.motion.tick());assert(p.motion.state().position.x!=before.x);
    }
    // Same fixed stream, full flash pool changes which random controls speed.
    for(unsigned free=0;free<2;++free){
        Fixture f;f.freeSlots(free);f.host.values={0x20000000u,0x60000000u,0x40000000u,0x10000000u,0x30000000u,0x50000000u,0x70000000u};
        Part p{"Fixture",4,1,{{0,0},{0,0}}};unsigned cur=1;auto input=controllerInput(p,cur);WeaponController c;c.tick(&ml,input,f.host);
        const auto v=f.host.projectiles[0].motion.state().velocity;near(std::sqrt(v.x*v.x+v.y*v.y),free?.215f*.3f:.185f*.3f);
    }
    // Nil prepared textures do not turn an emission into a silent ammo/RNG skip.
    Host nil;nil.nilImages=true;ParticlePool nilPool(0,nil);WeaponEffects nilEffects(nilPool,{});nil.clear();nilEffects.emit(w,shot,nil.input,nil,nil);
    assert(nil.reads==8&&nil.damages.back()==7&&nilEffects.trails().size()==1);
    // The world scheduler owns trail tick/removal order. Cleanup must not tick
    // another time or consume a visual RNG value while pruning shared owners.
    Fixture prune;prune.freeSlots(2);prune.effects.emit(w,shot,prune.host.input,prune.host,prune.host);
    auto retained=prune.effects.trails()[0];retained->state().removed=true;
    prune.host.clear();const auto countBefore=retained->lastPosition();prune.effects.pruneRemovedTrails();
    assert(prune.effects.trails().empty()&&prune.host.reads==0&&prune.host.events.empty()&&retained->lastPosition()==countBefore);
    // The original nine-kind controller has two direct-damage branches and
    // seven branches creating exactly one GobBullet per emission, not pellets.
    for(const int kind:{1,2,3,4,5,7,8})for(unsigned free=0;free<2;++free){
        Fixture f;f.freeSlots(free);auto info=w;info.kind=static_cast<WeaponKind>(kind);info.speed=.2f;
        switch(kind){
            case 1:info.effect=WeaponEffect::shotgun;info.effectImage="Efx_Trail04.png";info.bulletImage="Blt_Shotgun01.png";break;
            case 2:info.effect=WeaponEffect::laser;info.effectImage="Efx_Trail03.png";break;
            case 3:info.effect=WeaponEffect::rail;info.effectImage="Efx_Rail01.png";info.bulletImage="Blt_Railgun01.png";break;
            case 4:info.bulletImage="Blt_Canon01.png";break;
            case 5:info.effect=WeaponEffect::trail;info.effectImage="Efx_Trail01.png";info.bulletImage="Blt_Missile01.png";break;
            case 7:info.bulletImage="Blt_Nuclear01.png";break;
            case 8:info.bulletImage="Blt_Bullet01.png";break;
        }
        f.effects.emit(info,shot,f.host.input,f.host,f.host);
        assert(f.host.projectiles.size()==1&&f.host.damages.empty()&&f.host.soundPositions.size()==1);
        assert(f.host.reads==(free?4u:1u));
        assert(f.host.events[f.host.events.size()-2]=="audio"&&f.host.events.back()=="projectile");
        auto& p=f.host.projectiles[0];assert(p.visual&&p.visual->bulletTile()==p.tile&&p.visual->trail()==p.trail);
        assert(!p.motion.state().collideCheck&&!p.motion.state().toPlayer);
        if(kind==1||kind==2||kind==3||kind==5){
            assert(p.trail&&f.effects.trails().size()==1&&p.trail->lastPosition()==1);
            const auto c=p.trail->config();
            if(kind==1){near(c.width,1);near(c.length,64);assert(c.blocks==16);near(c.fadeDelay,0);near(c.fadeTime,.05f);near(p.trail->state().wind,0);}
            if(kind==2){near(c.width,4);near(c.length,16);assert(c.blocks==64);near(c.fadeDelay,.3f);near(c.fadeTime,.3f);near(p.trail->state().wind,0);}
            if(kind==3){near(c.width,32);near(c.length,64);assert(c.blocks==64);near(c.fadeDelay,.2f);near(c.fadeTime,.3f);near(p.trail->state().wind,40);near(p.trail->state().diffuse,.1f);}
        }else assert(!p.trail&&f.effects.trails().empty());
    }
    // Starting Stage01_01 enemy weapon consumers: Bullet8 none/Bullet01 and
    // Bullet8 Flame2/Canon01, both actual hostile motions, not direct Gun hits.
    for(bool flame:{false,true}){
        Fixture f;f.freeSlots(0);f.host.input.hostile=true;f.host.input.playerShooter=false;
        auto enemy=w;enemy.kind=WeaponKind::bullet;enemy.speed=.02f;
        enemy.bulletImage=flame?"Blt_Canon01.png":"Blt_Bullet01.png";
        if(flame){enemy.effect=WeaponEffect::flame;enemy.effectImage="Efx_Flame.png";}
        f.effects.emit(enemy,shot,f.host.input,f.host,f.host);
        assert(f.host.reads==1&&f.host.projectiles.size()==1&&f.host.damages.empty());
        auto& p=f.host.projectiles[0];assert(p.motion.state().toPlayer&&p.visual&&p.visual->hasEffectChild()==flame);
        assert(p.tile->name==enemy.bulletImage&&p.visual->state().effectDelay==.06f&&p.effectDelay==.06f);
        assert(p.visual->effectTile()==(flame?f.host.preparedTile("Efx_Flame.png"):nullptr));
        if(flame){p.visual->tick(p.motion.state());assert(!p.visual->state().effectVisible);
            p.visual->tick(p.motion.state(),true);assert(!p.visual->state().effectVisible);
            p.visual->tick(p.motion.state());assert(p.visual->state().effectVisible);}
    }
    Fixture shotgun;shotgun.freeSlots(0);auto sh=w;sh.kind=WeaponKind::shotgun;sh.shotCount=4;sh.shotDelay=0;sh.reloadDelay=1;
    sh.speed=.3f;sh.effect=WeaponEffect::shotgun;sh.effectImage="Efx_Trail04.png";sh.bulletImage="Blt_Shotgun01.png";
    Part shPart{"Fixture",4,1,{{0,0},{0,0}}};unsigned shMuzzle=1;auto shInput=controllerInput(shPart,shMuzzle);
    WeaponController shController;shController.tick(&sh,shInput,shotgun.host);
    // Four common ammo-loop emissions (one each), not four extra pellets per
    // emission: endpoint RNG2 + (fireTime RNG1 + speed RNG1)*4.
    assert(shotgun.host.projectiles.size()==4&&shotgun.host.reads==10&&shotgun.host.soundPositions.size()==5);
    assert(shController.state().fireLeft==0&&shotgun.host.damages.empty());
    // Common GobBullet setFire is identical after the controller's flash;
    // the direct AirStrike path consumes no flash/reset RNG or particles.
    Fixture controllerBullet,directBullet;auto common=missile();
    controllerBullet.effects.emit(common,shot,controllerBullet.host.input,controllerBullet.host,controllerBullet.host);
    directBullet.effects.createProjectile(common,shot,directBullet.host.input,directBullet.host,directBullet.host,true);
    assert(controllerBullet.pool.allocations()==1&&directBullet.pool.allocations()==0);
    assert(controllerBullet.host.reads==4&&directBullet.host.reads==1);
    assert((directBullet.host.events==std::vector<std::string>{"R1","trail","audio","projectile"}));
    const auto& normal=controllerBullet.host.projectiles.at(0);const auto& drop=directBullet.host.projectiles.at(0);
    assert(!normal.visual->state().drop&&drop.visual->state().drop);
    assert(normal.motion.state().position.x==drop.motion.state().position.x&&normal.motion.state().position.y==drop.motion.state().position.y);
    assert(normal.motion.state().velocity.x==drop.motion.state().velocity.x&&normal.motion.state().velocity.y==drop.motion.state().velocity.y);
    assert(normal.motion.state().acceleration.x==drop.motion.state().acceleration.x&&normal.motion.state().acceleration.y==drop.motion.state().acceleration.y);
    assert(normal.visual->effectTile()->name==drop.visual->effectTile()->name&&normal.trail->lastPosition()==drop.trail->lastPosition());
    assert(controllerBullet.host.soundPositions[0].x==directBullet.host.soundPositions[0].x&&controllerBullet.host.soundPositions[0].y==directBullet.host.soundPositions[0].y);
    directBullet.host.clear();directBullet.host.projectileRandom=true;
    directBullet.effects.createProjectile(common,shot,directBullet.host.input,directBullet.host,directBullet.host,true);
    assert((directBullet.host.events==std::vector<std::string>{"R1","trail","audio","projectile","R2"}));
    assert(directBullet.host.projectiles.back().visual->state().drop&&directBullet.pool.allocations()==0);
    // An out-of-domain kind fails before consuming any world state.
    Fixture unsupported;auto unknown=w;unknown.kind=static_cast<WeaponKind>(9);bool rejected=false;
    try{unsupported.effects.emit(unknown,shot,unsupported.host.input,unsupported.host,unsupported.host);}catch(...){rejected=true;}
    assert(rejected&&unsupported.host.reads==0&&unsupported.host.events.empty());
}

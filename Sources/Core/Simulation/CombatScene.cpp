#include "Core/Simulation/CombatScene.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include "Core/Data/Weapons.hpp"
#include "Core/Presentation/PreparedTiles.hpp"
#include "Core/Presentation/Visibility.hpp"
#include "Core/Simulation/EnemySpawner.hpp"
#include "Core/Simulation/EnemyVehicle.hpp"
#include "Core/Simulation/ObjectCollision.hpp"
#include "Core/Simulation/ProjectileImpact.hpp"
#include "Core/Simulation/ProjectileVisual.hpp"
#include "Core/Simulation/WeaponEffects.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace hm2 {
namespace {
float distance2(Vec2 a,Vec2 b){const float x=a.x-b.x,y=a.y-b.y;return x*x+y*y;}
Vec2 add(Vec2 a,Vec2 b){return {a.x+b.x,a.y+b.y};}
std::string partImage(const Part& p){return p.tileCount?p.name+".png":"PartsDummy.png";}
VisibilityState imageVisibility(const TileImage* tile){
    VisibilityState out;out.visual=true;
    if(tile){const float w=tile->pixels.width/float(tile->columns),h=tile->pixels.height/float(tile->rows);out.bounds={-w*.5f,-h*.5f,w*.5f,h*.5f};}
    return out;
}
}
struct CombatScene::Impl:EnemyVehicleHost,EnemySpawnerHost {
    struct Enemy {
        std::uint64_t id=0;std::size_t spawner=0;EnemyVehicle actor;
        VisibilityState root;
        std::vector<VisibilityState> parts;
        float crashTime=0,targetScale=1,targetAlpha=1,targetAngle=0;bool removed=false,removePending=false,targetShow=false;
        VisibilityState targetVisibility;
        Enemy(std::uint64_t n,std::size_t s,const Models& m,const Weapons& w,const MachineInfo& i,Vec2 p,double t,WorldRandom& r)
        :id(n),spawner(s),actor(m,w,i,p,t,r,p),parts(actor.mounts().size()){}
    };
    struct PlayerWeapon {std::size_t mount=0;const WeaponInfo* info=nullptr;WeaponController controller;unsigned muzzle=1;bool left=false;};
    struct Projectile {
        WeaponProjectile value;VisibilityState root,effect,bullet;
        explicit Projectile(WeaponProjectile p):value(std::move(p)),effect(imageVisibility(value.visual?value.visual->effectTile():nullptr)),bullet(imageVisibility(value.tile)){
            root.visual=true;
        }
    };
    struct Dynamic {
        std::shared_ptr<Trail> trail;
        std::shared_ptr<Projectile> projectile;
        VisibilityState visibility;
        bool drawVisible=false;
    };
    CombatSceneHooks& hooks;
    CombatMetrics metrics;
    DarwinWorldRandom ownRandom;
    WorldRandom* externalRandom=nullptr;
    MapPreview map;
    Models models;
    Machines machines;
    PlayerEquipment equipment;
    Weapons weapons;
    PreparedTiles tiles{"world"};
    TerrainCollision terrain;
    GameClock clock;
    MapCamera camera;
    BipodMotion playerMotion;
    TerrainContact playerContact;
    int hp=256;
    bool saveAllowed=false;
    WorldNavigation navigation;
    std::vector<PartMount> playerMounts;
    std::vector<std::size_t> playerAnimationSlots;
    std::array<std::string,4> equipmentCodes;
    std::vector<PlayerWeapon> playerWeapons;
    std::vector<VisibilityState> playerParts;
    VisibilityState playerRoot;
    std::array<VisibilityState,64> particleVisible;
    ParticlePool pool;
    std::unique_ptr<WeaponEffects> effects;
    std::array<VisualLayer,8> layers;
    std::array<VisualLayerFrame,8> prepared;
    std::vector<EnemySpawner> spawners;
    std::vector<std::unique_ptr<Enemy>> actors;
    std::vector<Enemy*> targetDrawNodes;
    std::vector<Dynamic> dynamic;
    std::vector<std::uint64_t> selection;
    std::uint64_t nextID=2,targetID=0;
    std::uintptr_t targetPointer=0;
    float hpRefreshTime=0;
    bool paused=false,active=true,leftFire=false,rightFire=false,moveToTarget=false;
    float targetCheckTime=0;
    mutable WeaponTarget targetScratch,playerScratch;
    std::size_t spawning=0;

    Impl(ResourceAccess& r,CombatSceneHooks& h,const CombatSetup& setup)
    :hooks(h),ownRandom(setup.randomSeed),externalRandom(setup.random),map(r,setup.map),
     models(Models::parse(Chunk::parse(r.read("PartsInfo.mcb")),Chunk::parse(r.read("MachInfo.mcb")))),
     machines(Machines::parse(DataTable::parse(r.read("MachList.tble")))),
     equipment(PlayerEquipment::parse(DataTable::parse(r.read("ItemInfo.tble")))),
     weapons(Weapons::parse(DataTable::parse(r.read("WeaponInfo.tble")))),terrain(map.map()),clock(setup.initialRenderTime),hp(setup.playerHP),saveAllowed(setup.isSave),pool(setup.initialRenderTime,static_cast<EnemyVehicleHost&>(*this)){
        const auto gate=std::find_if(map.map().gates.begin(),map.map().gates.end(),[&](const auto& g){return g.number==setup.gate;});
        if(gate==map.map().gates.end())throw std::runtime_error("Original map gate is missing");
        for(std::size_t i=0;i<4;++i)equipmentCodes[i]=setup.equipment[i];
        Vec2 playerPosition=gate->position;float playerDirection=0;
        if(setup.offsetInsideGate){
            float nearest=std::abs(-512.f-gate->position.x);
            if(nearest>std::abs(512.f-gate->position.x)){playerDirection=std::numbers::pi_v<float>;nearest=std::abs(512.f-gate->position.x);}
            if(nearest>std::abs(-512.f-gate->position.y)){playerDirection=std::numbers::pi_v<float>/2;nearest=std::abs(-512.f-gate->position.y);}
            if(nearest>std::abs(512.f-gate->position.y))playerDirection=-std::numbers::pi_v<float>/2;
            playerPosition.x+=std::cos(playerDirection)*50.f;playerPosition.y+=std::sin(playerDirection)*50.f;
        }
        playerMotion=BipodMotion(playerPosition,playerDirection,equipment.item(setup.equipment[1]).parameter3);
        playerContact.position=playerPosition;
        playerMounts=equipment.mounts(models,setup.equipment);playerParts.resize(playerMounts.size());
        for(std::size_t i=0;i<playerMounts.size();++i)playerAnimationSlots.push_back(i);
        tiles.prepareWorldEffects(r);
        if(auto* t=tiles.prepare(r,"Target.png")){t->columns=2;t->rows=1;}
        for(const auto* name:{"BulletShell.png","Efx_Trail04.png","Efx_Trail02.png","Efx_Nuclear.png"})tiles.prepare(r,name);
        for(const auto& [name,w]:weapons.all()){
            metrics.playerWeaponEmissions.emplace(name,0);metrics.enemyWeaponEmissions.emplace(name,0);
            if(!w.bulletImage.empty())tiles.prepare(r,w.bulletImage);
            if(!w.effectImage.empty())if(auto* t=tiles.prepare(r,w.effectImage)){t->columns=w.effectColumns;t->rows=1;}
            if(!w.explodeImage.empty())tiles.prepare(r,w.explodeImage);
        }
        // These are original persistent world resource splits, including images
        // also mentioned by weapon rows. Restore them after generic prewarming.
        tiles.prepareWorldEffects(r);
        // Explicit preparation: field equipment changes only use this cache.
        // Traverse the common PartsInfo catalogue, not a list of spare units.
        for(const auto& [name,part]:models.parts()){
            (void)name;if(auto* tile=tiles.prepare(r,partImage(part))){tile->columns=std::max(1u,part.tileCount);tile->rows=1;}
        }
        hooks.prepareTiles(r,tiles);
        prepareParts(r,playerMounts,playerParts);
        for(std::size_t i=0;i<playerMounts.size();++i){
            const auto& m=playerMounts[i];const auto& p=models.part(m.part);
            if(p.type==4){
                const auto w=weapons.all().find(p.name);
                if(w!=weapons.all().end())playerWeapons.push_back({i,&w->second,WeaponController(0),1,m.parentSocket==1});
            }
        }
        for(const auto& s:map.map().spawners){
            const auto& info=machines.machine(s.model);
            if(info.kind!=MachineKind::vehicle)throw std::runtime_error("CombatScene currently restores vehicle map enemies only");
            spawners.emplace_back(s,info.attack!=0);
            const auto mounts=models.modelMounts(s.model);std::vector<VisibilityState> unused(mounts.size());prepareParts(r,mounts,unused);
        }
        effects=std::make_unique<WeaponEffects>(pool,WeaponEffectResources{
            tiles.lookup("Efx_Fire.png"),tiles.lookup("Efx_Damage.png"),tiles.lookup("BulletShell.png"),tiles.lookup("Efx_Trail04.png"),tiles.lookup("Efx_Trail02.png")});
        for(auto& visible:particleVisible)visible.visual=true;
        camera.center={gate->position.x+140,gate->position.y};
        processCamera(camera,playerMotion.state().position,navigation);
    }
    WorldRandom& rng(){return static_cast<EnemyVehicleHost&>(*this);}
    std::uint32_t random()override{++metrics.randomCalls;return externalRandom?externalRandom->random():ownRandom.random();}
    void reseed(std::uint32_t seed)override{if(externalRandom)externalRandom->reseed(seed);else ownRandom.reseed(seed);}
    void prepareParts(ResourceAccess& r,std::span<const PartMount> mounts,std::vector<VisibilityState>& visibility){
        for(std::size_t i=0;i<mounts.size();++i){const auto& p=models.part(mounts[i].part);auto* t=tiles.prepare(r,partImage(p));
            if(t){t->columns=std::max(1u,p.tileCount);t->rows=1;}
            visibility[i]=imageVisibility(t);
        }
    }
    Enemy* enemy(std::uint64_t id){for(auto& e:actors)if(e->id==id&&!e->removed)return e.get();return nullptr;}
    const Enemy* enemy(std::uint64_t id)const{for(const auto& e:actors)if(e->id==id&&!e->removed)return e.get();return nullptr;}
    Enemy* selected(){return enemy(targetID);}
    const Enemy* selected()const{return enemy(targetID);}
    const WeaponTarget* selectedTarget()const{
        const auto* e=selected();if(!e)return nullptr;
        targetScratch={e->id,e->actor.motion().position,e->actor.state().hp};return &targetScratch;
    }
    const WeaponTarget* player()const override{playerScratch={1,playerMotion.state().position,hp};return &playerScratch;}
    bool inCamera(Vec2 position)const override{return hm2::inCamera(VisibilityState{},position,&camera);}
    bool spawn(const MapSpawner& s,bool)override{
        auto e=std::make_unique<Enemy>(nextID++,spawning,models,weapons,machines.machine(s.model),s.position,clock.worldTime(),rng());
        for(std::size_t i=0;i<e->parts.size();++i)e->parts[i]=imageVisibility(tiles.lookup(partImage(models.part(e->actor.mounts()[i].part))));
        e->targetVisibility=imageVisibility(tiles.lookup("Target.png"));
        actors.push_back(std::move(e));return true;
    }
    void soundID(unsigned id,Vec2 p){++metrics.soundRequests;hooks.soundID(id,p);}
    void fireSound(const WeaponInfo& w,Vec2 p)override{++metrics.soundRequests;hooks.sound(w.fireSound,p);}
    Vec2 damageSoundPosition(Vec2 p)const{
        // 0xf3d0, not an absolute distance. Negative dx produces NaN in the
        // original sqrt, and the audio adapter still receives that request.
        const float dx=p.x-playerMotion.state().position.x;
        return {float(std::sqrt(double(dx))/10.0),0};
    }
    void baseCrash(Vec2 position){
        const float dx=position.x-playerMotion.state().position.x;
        const float dy=position.y-playerMotion.state().position.y;
        soundID(21,{float(std::sqrt(double(dx))*.1),float(std::sqrt(double(dy))*.1)});
        if(auto* particle=pool.allocate(clock.renderTime(),rng())){
            auto& s=particle->state();s.blendType=0;s.life=1;s.tile=tiles.lookup("Efx_ExplodeNPC.png");s.position=position;s.scale={1,1};
            particle->animate(16);particle->start(clock.renderTime());
        }
    }
    void tickCorpse(Enemy& enemy){
        // GobHvM_Enemy::processCrash0xa998. Pool allocation/reset is inside the
        // probability branch; all later smoke RNG depends on a free slot.
        if(random()%20==0){
            if(auto* p=pool.allocate(clock.renderTime(),rng())){
                auto& s=p->state();s.blendType=0;s.life=1;s.tile=tiles.lookup("Efx_Smoke.png");
                const float scale=randomUnit(rng())*.4f+.4f;s.scale={scale,scale};
                s.scaleVelocity=randomUnit(rng())*.005f+.001f;
                s.angle=randomUnit(rng())*std::bit_cast<float>(0x40c90fdau);
                s.rotationVelocity=randomUnit(rng())*.02f;
                s.velocity.x=randomUnit(rng())*-.1f-.5f;s.velocity.y=randomUnit(rng())*.1f+.2f;
                const float radius=float(enemy.actor.info().radius);
                const float x=randomUnit(rng())*((radius*.3f)+(radius*.3f))+radius*-.3f;
                const float y=randomUnit(rng())*((radius*.3f)+(radius*.3f))+radius*-.3f;
                s.position=add(enemy.actor.motion().position,{x,y});p->animate(16);p->start(clock.renderTime());
            }
        }
        if(double(enemy.crashTime)<clock.worldTime()){
            // This final explosion allocates before requesting its sound, while
            // base onCrash requests the sound before its first allocation.
            if(auto* p=pool.allocate(clock.renderTime(),rng())){
                auto& s=p->state();s.blendType=0;s.life=1;s.tile=tiles.lookup("Efx_ExplodeNPC.png");
                s.position=enemy.actor.motion().position;s.scale={1,1};p->animate(16);p->start(clock.renderTime());
            }
            const auto pos=enemy.actor.motion().position,player=playerMotion.state().position;
            soundID(21,{float(std::sqrt(double(pos.x-player.x))*.1),float(std::sqrt(double(pos.y-player.y))*.1)});
            enemy.removePending=true;
        }
    }
    void onTarget(Enemy& actor,bool show){
        if(show){actor.targetScale=3;actor.targetAlpha=1;}
        actor.targetShow=show;
    }
    void setTarget(std::uint64_t id){
        if(auto* old=enemy(targetID))onTarget(*old,false);
        targetID=id;if(auto* next=enemy(id))onTarget(*next,true);
    }
    void clearPlayerTarget(){
        setTarget(0);moveToTarget=false;
        auto state=playerMotion.state();state.state=0;playerMotion=BipodMotion(state);
    }
    void damage(std::uint64_t id,int amount){
        ++metrics.damageCalls;
        if(id==1){
            amount=hooks.playerDamage(amount,rng());
            hp=std::bit_cast<std::int32_t>(std::uint32_t(hp)-std::uint32_t(amount));
            if(hp<1){hp=0;baseCrash(playerMotion.state().position);clearPlayerTarget();auto s=playerMotion.state();s.state=2;playerMotion=BipodMotion(s);hooks.playerDied();}
            soundID(15+random()%3,damageSoundPosition(playerMotion.state().position));hooks.playerHPChanged(hp);return;
        }
        auto* e=enemy(id);if(!e)return;
        auto& state=e->actor.state();state.hp=std::bit_cast<std::int32_t>(std::uint32_t(state.hp)-std::uint32_t(amount));
        if(state.hp<1){
            state.hp=0;++metrics.kills;baseCrash(e->actor.motion().position);
            const bool wasFirst=!selection.empty()&&selection.front()==id;
            if(targetID==id){clearPlayerTarget();if(navigation.count())playerMotion.setDestination(navigation.point(0));}
            state.targetPlayer=false;state.moveToTarget=false;e->actor.setMachineState(2);
            hooks.dropEnemyLoot(e->actor.info(),e->actor.motion().position,clock.worldTime(),clock.renderTime(),rng());
            e->actor.removePartsForCorpse();
            spawners[e->spawner].enemyDead(clock.worldTime(),rng());
            std::erase(selection,id);
            if(wasFirst&&!selection.empty())setTarget(selection.front());
            hooks.rewardEnemyKill(e->actor.info(),e->actor.motion().position,clock.worldTime(),clock.renderTime(),rng());
            const float u=randomUnit(rng());e->crashTime=float((clock.worldTime()+3.0)+double(u+u));
        }
        soundID(15+random()%3,damageSoundPosition(e->actor.motion().position));
        // Enemy::onDamage resumes after super/onCrash and may retarget even
        // after its parts have been removed. Controllers are removed separately.
        state.targetPlayer=true;state.moveToTarget=true;
    }
    struct EffectsHost:WeaponEffectsHost {
        Impl& scene;bool hostile;std::uint64_t owner;
        EffectsHost(Impl& s,bool h,std::uint64_t o):scene(s),hostile(h),owner(o){}
        const WeaponTarget* target()const override{return hostile?scene.player():scene.selectedTarget();}
        void damage(const WeaponInfo&,std::uint64_t id,int amount)override{scene.damage(id,amount);}
        TileImage* preparedTile(std::string_view name)override{return scene.tiles.lookup(name);}
        void fireSound(const WeaponInfo& info,Vec2 p)override{scene.fireSound(info,p);}
        void attachProjectile(WeaponProjectile p)override{++scene.metrics.projectilesCreated;scene.dynamic.push_back({{},std::make_shared<Projectile>(std::move(p)),{},false});}
        void trailAttached(const Trail&)override{auto trail=scene.effects->trails().back();scene.dynamic.push_back({trail,{},imageVisibility(trail->state().tile),false});}
    };
    void emitWeapon(const WeaponInfo& w,const WeaponEmission& shot,WeaponEffectInput input,std::uint64_t owner){
        ++metrics.emissions;if(input.hostile)++metrics.enemyEmissions;else ++metrics.playerEmissions;
        ++(input.hostile?metrics.enemyWeaponEmissions:metrics.playerWeaponEmissions).at(w.part);
        EffectsHost host(*this,input.hostile,owner);effects->emit(w,shot,input,rng(),host);
    }
    void emit(const WeaponInfo& w,const WeaponEmission& shot,const EnemyVehicleEmission& input)override{
        WeaponEffectInput effect;effect.worldTime=clock.worldTime();effect.renderTime=clock.renderTime();
        effect.machinePosition=input.machinePosition;effect.partCenter=input.partCenter;effect.playerPosition=playerMotion.state().position;effect.hostile=true;
        emitWeapon(w,shot,effect,0);
    }
    void objectContacts(EnemyVehicle& actor)override{
        VisibilityState root;root.show=actor.state().show;
        if(!hm2::inCamera(root,actor.motion().position,&camera))return;
        ObjectBody self{actor.motion().position,actor.info().radius,actor.info().kind};
        for(auto& e:actors){
            if(&e->actor==&actor)break;
            if(e->removed||!hm2::inCamera(e->root,e->actor.motion().position,&camera))continue;
            ObjectBody other{e->actor.motion().position,e->actor.info().radius,e->actor.info().kind};
            if(resolveObjectCollision(self,other))e->actor.applyContact(other.position);
        }
        ObjectBody other{playerMotion.state().position,machines.machine("Player").radius,MachineKind::bipod};
        if(resolveObjectCollision(self,other))playerMotion.applyContact(other.position);
        actor.applyContact(self.position);
    }
    void collectPart(const PartPose& pose,Vec2 actor,VisibilityState& visibility){
        auto* tile=tiles.lookup(partImage(*pose.part));if(!tile)return;
        VisualImage image;image.tile=tiles.visual(*tile);image.screenPosition=camera.toScreen(add(actor,pose.center));
        image.angle=pose.angle;image.reverse=pose.reverseY?2:0;image.useAtlas=true;image.isInScreen=visibility.inScreen;
        layers.at(pose.layer).submitImage(image);
        refreshVisibility(visibility,add(actor,pose.center),&camera);
    }
    void advanceTarget(Enemy& enemy){
        if(enemy.targetShow&&!paused){
            const float alphaDifference=enemy.targetAlpha-.5f;
            enemy.targetAlpha=std::abs(alphaDifference)<=.1f?.5f:enemy.targetAlpha+(.5f-enemy.targetAlpha)/30.f;
            const float scaleDifference=enemy.targetScale-1.f;
            enemy.targetScale=std::abs(scaleDifference)<=.01f?1.f:enemy.targetScale+(1.f-enemy.targetScale)/3.f;
            enemy.targetAngle=std::sin(float(clock.worldTime()+clock.worldTime()));
        }
    }
    void registerTarget(Enemy& enemy,bool showAtChildTick){
        if(!showAtChildTick)return;
        refreshVisibility(enemy.targetVisibility,enemy.actor.motion().position,&camera);
        if(enemy.targetVisibility.inScreen)targetDrawNodes.push_back(&enemy);
    }
    void drawTargets(Frame& frame){
        auto* tile=tiles.lookup("Target.png");if(!tile)return;
        // These are registered non-atlas object pointers, not cached atlas
        // quads. A same-frame setTarget promotion can reset scale/alpha after
        // the enemy child tick, and its direct draw sees those final fields.
        for(const auto* enemy:targetDrawNodes){
            VisualImage image;image.tile=tiles.visual(*tile);image.tileNo=1;image.blendType=2;
            image.screenPosition=camera.toScreen(enemy->actor.motion().position);image.scale={enemy->targetScale,enemy->targetScale};
            image.angle=enemy->targetAngle;image.alpha=enemy->targetAlpha;layers[7].submitImage(image);
        }
        layers[7].drain().appendBatches(frame);
    }
    void tickPool(){
        auto& particles=pool.particles();
        for(std::size_t i=0;i<particles.size();++i){
            auto& particle=particles[i];particle.tick(clock.renderTime(),camera.viewport.height*.5f);
            auto& s=particle.state();if(!s.show)continue;
            if(s.tile){
                particleVisible[i].bounds=imageVisibility(s.tile).bounds;
                VisualImage image;image.tile=tiles.visual(*s.tile);image.screenPosition=camera.toScreen(s.position);
                image.scale={s.scale.x*s.scaleScale.x,s.scale.y*s.scaleScale.y};image.angle=s.angle;image.alpha=s.alpha;image.tileNo=unsigned(s.tileNumber);
                image.blendType=unsigned(s.blendType);image.useAtlas=s.useAtlas;image.isInScreen=particleVisible[i].inScreen;
                layers[5].submitImage(image);s.tileNumber=int(image.tileNo);
            }
            refreshVisibility(particleVisible[i],s.position,&camera);
        }
        metrics.particleAllocations=pool.allocations();
    }
    struct PlayerHost:WeaponHost {
        Impl& scene;PlayerWeapon& weapon;Vec2 center;
        PlayerHost(Impl& s,PlayerWeapon& w,Vec2 c):scene(s),weapon(w),center(c){}
        std::uint32_t random()override{return scene.random();}
        const WeaponTarget* target()const override{return scene.selectedTarget();}
        void emit(const WeaponInfo& info,const WeaponEmission& shot)override{
            WeaponEffectInput input;input.worldTime=scene.clock.worldTime();input.renderTime=scene.clock.renderTime();
            input.machinePosition=scene.playerMotion.state().position;input.partCenter=center;input.playerPosition=input.machinePosition;
            input.left=weapon.left;input.playerShooter=true;input.damageBonus=scene.hooks.damageBonus();scene.emitWeapon(info,shot,input,1);
        }
        void outOfRange(bool left)override{scene.hooks.outOfRange(left);}
        void fireSound(const WeaponInfo& info,Vec2 p)override{scene.fireSound(info,p);}
    };
    void equipWeapon(EquipmentCodes requested,unsigned slot){
        if(slot!=2&&slot!=3)throw std::invalid_argument("CombatScene weapon slot must be2or3");
        // setParts0xb7b4 returns before removing anything when getPartsInfo is
        // nil. An empty inventory entry does not synthesize an unequip branch.
        if(requested[slot].empty())return;
        auto codes=equipmentCodes;codes[slot]=requested[slot];
        EquipmentCodes beforeCodes,afterCodes;
        for(std::size_t i=0;i<4;++i){beforeCodes[i]=equipmentCodes[i];afterCodes[i]=codes[i];}
        const auto before=equipment.mounts(models,beforeCodes);
        const auto after=equipment.mounts(models,afterCodes);
        const auto canonicalWeapon=[&](const std::vector<PartMount>& mounts)->std::optional<std::size_t>{
            for(std::size_t i=0;i<mounts.size();++i){const auto& mount=mounts[i];
                if(mount.parent>=0&&mount.parentSocket==slot-1&&mounts[std::size_t(mount.parent)].parentSocket==3)return i;
            }
            return std::nullopt;
        };
        const auto oldCanonical=canonicalWeapon(before),newCanonical=canonicalWeapon(after);
        if(!newCanonical)throw std::runtime_error("New weapon has no common equipment mount");
        auto fresh=after[*newCanonical];
        const auto* tile=tiles.lookup(partImage(models.part(fresh.part))); // Prepared only.
        const auto newInfo=weapons.all().find(fresh.part);
        std::optional<std::size_t> removed;
        if(oldCanonical){
            const auto i=std::find(playerAnimationSlots.begin(),playerAnimationSlots.end(),*oldCanonical);
            if(i==playerAnimationSlots.end())throw std::runtime_error("Old weapon lost its animation mapping");
            removed=std::size_t(i-playerAnimationSlots.begin());
        }
        // Match surviving canonical graph roles. Insertion indices can change
        // when a formerly empty slot is filled; image names alone cannot match
        // two equal weapons or the two equal feet.
        std::vector<std::size_t> animationSlots;animationSlots.reserve(after.size());
        for(std::size_t i=0;i<playerMounts.size();++i){
            if(removed&&i==*removed)continue;
            const auto& old=before.at(playerAnimationSlots[i]);
            const auto sameRole=[&](const PartMount& next){
                if(old.part!=next.part||old.parentSocket!=next.parentSocket||old.reverseY!=next.reverseY)return false;
                if(old.parent<0||next.parent<0)return old.parent==next.parent;
                return before[std::size_t(old.parent)].part==after[std::size_t(next.parent)].part;
            };
            const auto next=std::find_if(after.begin(),after.end(),sameRole);
            if(next==after.end())throw std::runtime_error("Surviving Player part lost its canonical role");
            animationSlots.push_back(std::size_t(next-after.begin()));
        }
        const auto parent=std::find(animationSlots.begin(),animationSlots.end(),std::size_t(fresh.parent));
        if(parent==animationSlots.end())throw std::runtime_error("New weapon has no active body socket");
        fresh.parent=int(parent-animationSlots.begin());
        // Everything above validates before changing the active graph. Remove
        // only this part/controller; retain other ammo/clock/recoil/muzzle state.
        if(removed){
            for(const auto& mount:playerMounts)if(mount.parent==int(*removed))throw std::runtime_error("Weapon replacement requires a leaf mount");
            std::erase_if(playerWeapons,[&](const auto& weapon){return weapon.mount==*removed;});
            playerMounts.erase(playerMounts.begin()+std::ptrdiff_t(*removed));
            playerParts.erase(playerParts.begin()+std::ptrdiff_t(*removed));
            for(auto& mount:playerMounts)if(mount.parent>int(*removed))--mount.parent;
            for(auto& weapon:playerWeapons)if(weapon.mount>*removed)--weapon.mount;
        }
        const auto index=playerMounts.size();playerMounts.push_back(std::move(fresh));
        playerParts.push_back(imageVisibility(tile));
        animationSlots.push_back(*newCanonical);playerAnimationSlots=std::move(animationSlots);
        equipmentCodes=std::move(codes);
        if(newInfo!=weapons.all().end())playerWeapons.push_back({index,&newInfo->second,WeaponController(clock.worldTime()),1,slot==2});
    }
    void tickPlayer(){
        if(hp>0&&hp<hooks.playerMaximumHP()&&double(hpRefreshTime)+.3<clock.worldTime()){
            ++hp;hpRefreshTime=float(clock.worldTime());
        }
        hooks.playerProgressTick();
        if(!paused&&(playerMotion.state().state==0||playerMotion.state().state==1)&&playerRoot.inScreen){
            const auto* target=selectedTarget();std::optional<Vec2> aim;if(target)aim=target->position;
            const int wall=playerContact.current;playerMotion.tick(wall<0?nullptr:&terrain.edges().at(std::size_t(wall)),aim);
            playerContact=terrain.resolve(playerMotion.state().position,playerContact.previous);playerMotion.applyContact(playerContact.position);
            if(target&&double(targetCheckTime+.5f)<clock.worldTime()){
                if(moveToTarget)playerMotion.setDestination(distance2(playerMotion.state().position,target->position)<40000.f?playerMotion.state().position:target->position);
                targetCheckTime=float(clock.worldTime());
            }
        }
        // Bipod owns angles and foot/body offsets. Preserve controller recoil
        // as a separate addition rather than replacing it with the gait offset.
        EquipmentCodes codes;for(std::size_t i=0;i<4;++i)codes[i]=equipmentCodes[i];
        const auto animated=equipment.mounts(models,codes,playerMotion.angles(),playerMotion.offsets());
        for(std::size_t i=0;i<playerMounts.size();++i){
            auto& m=playerMounts[i];const auto& animation=animated.at(playerAnimationSlots.at(i));m.angle=animation.angle;
            if(models.part(m.part).type==2)m.addPosition=animation.addPosition;
            else m.addPosition.y=animation.addPosition.y;
            const auto poses=models.mountedPose(playerMounts,playerMotion.state().angle);
            const auto p=std::find_if(poses.begin(),poses.end(),[&](const auto& pose){return pose.mountIndex==i;});
            if(p==poses.end())throw std::runtime_error("Player mounting lost insertion index");
            collectPart(*p,playerMotion.state().position,playerParts[i]);
            auto w=std::find_if(playerWeapons.begin(),playerWeapons.end(),[&](const auto& weapon){return weapon.mount==i;});
            if(w==playerWeapons.end())continue;
            WeaponTick tick;tick.time=clock.worldTime();tick.machinePosition=playerMotion.state().position;tick.playerPosition=tick.machinePosition;
            tick.pose=*p;tick.currentMuzzle=&w->muzzle;tick.weaponReady=playerMotion.state().weaponReady;
            tick.isFire=w->left?leftFire:rightFire;tick.isPlayer=true;tick.left=w->left;
            if(m.parent>=0)tick.parentAddX=&playerMounts[std::size_t(m.parent)].addPosition.x;
            PlayerHost host(*this,*w,p->center);const auto* t=host.target();const bool enters=(t&&t->hp>0)||tick.isFire;
            w->controller.tick(w->info,tick,host);if(enters)m.addPosition.x=w->controller.state().fireBound;
        }
    }
    struct ImpactHost:ProjectileImpactHost {
        Impl& scene;Projectile& projectile;
        ImpactHost(Impl& s,Projectile& p):scene(s),projectile(p){}
        std::size_t enemyCount()const override{return scene.actors.size();}
        ImpactTarget enemy(std::size_t i)const override{const auto& e=*scene.actors.at(i);return {e.removed?0:e.id,e.actor.motion().position,e.actor.state().hp,e.actor.info().radius,e.root.inScreen};}
        ImpactTarget player()const override{return {1,scene.playerMotion.state().position,scene.hp,scene.machines.machine("Player").radius,scene.playerRoot.inScreen};}
        std::size_t botCount()const override{return 0;}
        ImpactTarget bot(std::size_t)const override{throw std::out_of_range("No restored bots in this scene");}
        std::uint64_t selectedEnemy()const override{return scene.targetID;}
        int damageBonus()const override{return scene.hooks.damageBonus();}
        void damage(std::uint64_t id,int amount)override{scene.damage(id,amount);}
        void finishTrail(Vec2 position)override{if(projectile.value.trail)projectile.value.trail->add(position,scene.clock.worldTime(),scene.rng());}
        void nuclearFlash(Vec2 position)override{
            // 0x71d0, CFString0x5e0fc -> Efx_Nuclear.png.
            if(auto* p=scene.pool.allocate(scene.clock.renderTime(),scene.rng())){
                auto& s=p->state();s.blendType=2;s.life=.25f;s.tile=scene.tiles.lookup("Efx_Nuclear.png");
                s.position=position;s.scale={.1f,.1f};s.scaleVelocity=.25f;p->start(scene.clock.renderTime());
            }
        }
        void explosionSound(Vec2 position)override{const auto player=scene.playerMotion.state().position;scene.soundID(20,{(position.x-player.x)/100.f,0});}
        void explosionParticle(const WeaponInfo& info,Vec2 position)override{
            if(auto* p=scene.pool.allocate(scene.clock.renderTime(),scene.rng())){
                auto& s=p->state();s.blendType=0;s.tile=scene.tiles.lookup(info.explodeImage);s.position=position;s.scale={info.explodeScale,info.explodeScale};
                if(s.tile){const auto count=s.tile->columns*s.tile->rows;s.life=float(count)*std::bit_cast<float>(0x3d0b4396u);p->animate(int(count));}
                p->start(scene.clock.renderTime());
            }
        }
        void removeProjectile()override{++scene.metrics.projectilesRemoved;projectile.value.motion.remove();}
    };
    void tickDynamic(){
        for(std::size_t i=0;i<dynamic.size();){
            // Copy shared ownership before callbacks append/reallocate this list.
            auto trail=dynamic[i].trail;auto projectile=dynamic[i].projectile;
            if((trail&&trail->state().removed)||(projectile&&projectile->value.motion.state().removed)){
                dynamic.erase(dynamic.begin()+std::ptrdiff_t(i));continue;
            }
            if(trail){trail->tick(clock.worldTime(),paused,rng());refreshVisibility(dynamic[i].visibility,trail->state().position,&camera);dynamic[i].drawVisible=trail->state().show&&dynamic[i].visibility.inScreen;}
            else if(projectile){
                ++metrics.projectileTicks;
                if(projectile->value.motion.tick(paused)){
                    ImpactHost host(*this,*projectile);impactProjectile(*projectile->value.info,projectile->value.motion.state().position,projectile->value.motion.state().toPlayer,host);
                }
                // Parent removeThis suppresses only its own registration. Its
                // QobBase still ticks visual children on this terminal pass;
                // they are retired with the parent at the next list visit.
                auto childState=projectile->value.motion.state();childState.removed=false;
                projectile->value.visual->tick(childState,paused);
                projectile->value.visual->collect(layers[5],camera,childState,{projectile->effect.inScreen,projectile->bullet.inScreen},tiles);
                const auto pos=projectile->value.motion.state().position;
                refreshVisibility(projectile->effect,pos,&camera);refreshVisibility(projectile->bullet,pos,&camera);refreshVisibility(projectile->root,pos,&camera);
                dynamic[i].drawVisible=projectile->root.inScreen&&!projectile->value.motion.state().removed;
            }
            ++i;
        }
        effects->pruneRemovedTrails();
    }
    void tick(){
        if(!clock.step(paused,active))return;
        for(auto& layer:layers)(void)layer.drain();
        targetDrawNodes.clear();
        if(!paused){
            if(auto* target=selected();target&&!hm2::inCamera(target->root,target->actor.motion().position,&camera))clearPlayerTarget();
            navigation.process(playerMotion);
        }
        std::optional<Vec2> target;if(const auto* e=selected())target=e->actor.motion().position;
        processCamera(camera,playerMotion.state().position,navigation,target);
        // listEnemy is visited before map spawners append new actors to it.
        const auto priorEnemies=actors.size();
        for(std::size_t i=0;i<priorEnemies;++i){auto& e=*actors[i];if(e.removed)continue;if(e.removePending){e.removed=true;continue;}
            ++metrics.enemyTicks;if(!paused&&e.actor.motion().state==2)tickCorpse(e);
            advanceTarget(e);const bool targetShowAtChildTick=e.targetShow;
            e.actor.tick({clock.worldTime(),paused,e.root.inScreen},terrain,*this);
            registerTarget(e,targetShowAtChildTick);
            auto poses=std::vector<PartPose>(e.actor.atlasPoses().begin(),e.actor.atlasPoses().end());
            std::sort(poses.begin(),poses.end(),[](const auto& a,const auto& b){return a.mountIndex<b.mountIndex;});
            for(const auto& pose:poses)collectPart(pose,e.actor.motion().position,e.parts.at(pose.mountIndex));
        }
        // listBot is currently empty; listItem remains an explicit progression
        // hook at its original objectBase position, before pool/spawners/Player.
        hooks.tickItems(clock.worldTime(),clock.renderTime(),playerMotion.state().position,rng());
        hooks.collectItems(layers[1],camera,tiles);
        tickPool();
        for(spawning=0;spawning<spawners.size();++spawning)spawners[spawning].tick(clock.worldTime(),playerMotion.state().position,*this);
        tickPlayer();tickDynamic();
        for(std::size_t i=0;i<layers.size();++i)prepared[i]=layers[i].drain();
        metrics.particleAllocations=pool.allocations();
    }
    bool select(std::uint64_t id,bool append){
        auto* e=enemy(id);if(!e||e->actor.state().hp<1)return false;
        if(append){if(selection.empty())return false;if(std::find(selection.begin(),selection.end(),id)!=selection.end())return true;if(selection.size()<16){selection.push_back(id);onTarget(*e,true);}return true;}
        moveToTarget=targetID==id;for(auto old:selection)if(auto* prior=enemy(old))onTarget(*prior,false);selection.assign(1,id);setTarget(id);
        if(moveToTarget)navigation=WorldNavigation{};return true;
    }
    bool input(PointerPhase phase,Vec2 screen,std::uintptr_t id){
        if(paused||!active||hp<1)return false;
        if(!id||!std::isfinite(screen.x)||!std::isfinite(screen.y)||screen.y<100)return false;
        if(phase==PointerPhase::cancel){const bool was=targetPointer==id;if(was)targetPointer=0;return navigation.input(phase,screen,id,camera,playerMotion)||was;}
        const auto world=camera.toWorld(screen);std::uint64_t found=0;float best=10000;
        for(const auto& e:actors)if(!e->removed&&e->actor.state().hp>0){const auto d=distance2(world,e->actor.motion().position);const float radius=float(e->actor.info().radius);if(d<radius*radius&&d<best){best=d;found=e->id;}}
        if(phase==PointerPhase::begin){
            if(navigation.pointer())return true;
            if(distance2(world,playerMotion.state().position)<900)return navigation.input(phase,screen,id,camera,playerMotion);
            if(found){targetPointer=id;return select(found,false);}
            moveToTarget=false;
        }
        if(phase==PointerPhase::move&&targetPointer==id){if(found)select(found,true);return true;}
        if(phase==PointerPhase::end){const bool hadTargetPointer=targetPointer!=0;targetPointer=0;const bool moved=navigation.input(phase,screen,id,camera,playerMotion);return hadTargetPointer||moved;}
        return navigation.input(phase,screen,id,camera,playerMotion);
    }
    Frame draw(){
        ++metrics.drawCalls;Frame frame=map.compose(camera);
        for(std::size_t layer=0;layer<prepared.size();++layer){
            if(layer==5){
                for(auto& node:dynamic){
                    if(!node.drawVisible)continue;
                    if(node.trail)node.trail->append(frame,camera);
                    else if(node.projectile)node.projectile->value.visual->draw(node.projectile->value.motion.state(),clock.worldTime(),clock.renderTime(),pool,rng());
                }
            }
            if(layer==7)drawTargets(frame);
            prepared[layer].appendBatches(frame);
        }
        metrics.particleAllocations=pool.allocations();return frame;
    }
};
CombatScene::CombatScene(ResourceAccess& r,CombatSceneHooks& h,CombatSetup setup):impl_(std::make_unique<Impl>(r,h,setup)){}
CombatScene::~CombatScene()=default;
void CombatScene::tick(){impl_->tick();}
bool CombatScene::input(PointerPhase p,Vec2 v,std::uintptr_t id){return impl_->input(p,v,id);}
bool CombatScene::selectEnemy(std::uint64_t id,bool append){return impl_->select(id,append);}
void CombatScene::setFiring(bool left,bool right){impl_->leftFire=left;impl_->rightFire=right;}
void CombatScene::equipWeapon(EquipmentCodes codes,unsigned slot){impl_->equipWeapon(codes,slot);}
void CombatScene::setPlayerHP(int hp){impl_->hp=hp;}
void CombatScene::setIsSave(bool value){impl_->saveAllowed=value;}
bool CombatScene::isSave()const{return impl_->saveAllowed;}
void CombatScene::setPaused(bool p){impl_->paused=p;}
void CombatScene::setActive(bool a){impl_->active=a;if(!a){impl_->navigation.cancel();impl_->targetPointer=0;impl_->leftFire=impl_->rightFire=false;}}
const GameClock& CombatScene::clock()const{return impl_->clock;}
const BipodState& CombatScene::player()const{return impl_->playerMotion.state();}
int CombatScene::playerHP()const{return impl_->hp;}
const MapCamera& CombatScene::camera()const{return impl_->camera;}
const TerrainCollision& CombatScene::terrain()const{return impl_->terrain;}
const CombatMetrics& CombatScene::metrics()const{return impl_->metrics;}
std::vector<CombatEnemySnapshot> CombatScene::enemies()const{
    std::vector<CombatEnemySnapshot> out;
    for(const auto& e:impl_->actors)if(!e->removed){
        std::size_t parts=0;for(std::size_t i=0;i<e->actor.mounts().size();++i)if(e->actor.partActive(i))++parts;
        out.push_back({e->id,e->actor.info().name,e->actor.motion().position,e->actor.state().hp,e->actor.state().targetPlayer,e->actor.motion().state==2,parts});
    }
    return out;
}
std::vector<CombatPlayerWeaponSnapshot> CombatScene::playerWeapons()const{
    std::vector<CombatPlayerWeaponSnapshot> result;
    for(const auto& weapon:impl_->playerWeapons){const auto& mount=impl_->playerMounts.at(weapon.mount);const auto& state=weapon.controller.state();
        result.push_back({weapon.left?2u:3u,weapon.muzzle,mount.parentSocket,weapon.mount,mount.part,mount.reverseY,mount.addPosition,state.fireLeft,state.fireTime,state.fireBound});
    }
    return result;
}
std::uint64_t CombatScene::selectedEnemy()const{return impl_->targetID;}
float CombatScene::selectedEnemyHpRate()const{
    const auto* enemy=impl_->selected();
    return enemy&&enemy->actor.info().hp>0?float(enemy->actor.state().hp)/float(enemy->actor.info().hp):0.f;
}
std::size_t CombatScene::spawnerCount()const{return impl_->spawners.size();}
std::size_t CombatScene::activeProjectiles()const{std::size_t n=0;for(const auto& d:impl_->dynamic)if(d.projectile&&!d.projectile->value.motion.state().removed)++n;return n;}
std::size_t CombatScene::visibleParticles()const{std::size_t n=0;for(const auto& p:impl_->pool.particles())if(p.state().show)++n;return n;}
Frame CombatScene::preparationFrame()const{auto frame=impl_->map.compose(impl_->camera);impl_->tiles.appendPreparation(frame);return frame;}
Frame CombatScene::drawFrame(){return impl_->draw();}
}

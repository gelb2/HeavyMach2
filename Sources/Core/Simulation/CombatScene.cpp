#include "Core/Presentation/PartSprites.hpp"
#include "Core/Simulation/CombatScene.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include "Core/Data/Weapons.hpp"
#include "Core/Presentation/PreparedTiles.hpp"
#include "Core/Presentation/SharedAssetPreparation.hpp"
#include "Core/Presentation/Visibility.hpp"
#include "Core/Simulation/EnemySpawner.hpp"
#include "Core/Simulation/EnemyActors.hpp"
#include "Core/Simulation/ObjectCollision.hpp"
#include "Core/Simulation/ProjectileImpact.hpp"
#include "Core/Simulation/ProjectileVisual.hpp"
#include "Core/Simulation/WeaponEffects.hpp"
#include "Core/Simulation/AirStrike.hpp"
#include "Core/Simulation/Dash.hpp"
#include "Core/Presentation/MovementPathVisual.hpp"
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
struct CombatScene::Impl:BotHost,EnemyActorHost,EnemySpawnerHost,AirStrikeHost,DashStartHost,DashMotionHost,WorldNavigationEvents {
    struct Enemy {
        std::uint64_t id=0;std::optional<std::size_t> spawner;EnemyActor actor;
        VisibilityState root;
        std::vector<VisibilityState> parts,shadows;
        float crashTime=0,targetScale=1,targetAlpha=1,targetAngle=0;bool removed=false,removePending=false,targetShow=false;
        VisibilityState targetVisibility;
        // addEnemyMach sets layer 1. addMissionObjects overrides it to 5.
        Enemy(std::uint64_t n,std::optional<std::size_t> s,const Models& m,const Weapons& w,const MachineInfo& i,Vec2 p,double t,WorldRandom& r)
        :id(n),spawner(s),actor(m,w,i,p,t,r,s?std::optional<Vec2>(p):std::nullopt,s?1u:5u),parts(actor.mounts().size()),shadows(actor.mounts().size()){}
    };
    struct FriendlyBot {
        std::uint64_t id;Bot actor;VisibilityState root;std::vector<VisibilityState> parts,shadows;std::array<VisibilityState,2> gauge;std::uint64_t itemCaptures=0,hpRemoved=0,enemyHPRemoved=0;
        FriendlyBot(std::uint64_t n,const Models& m,const Weapons& w,const MachineInfo& i,Vec2 p,double t,WorldRandom& r):id(n),actor(m,w,i,p,t,r),parts(actor.mounts().size()),shadows(actor.mounts().size()){}
    };
    struct PlayerWeapon {std::size_t mount=0;const WeaponInfo* info=nullptr;WeaponController controller;unsigned muzzle=1;bool left=false;};
    struct Projectile {
        WeaponProjectile value;VisibilityState root,effect,bullet;std::uint64_t shooter=0;
        explicit Projectile(WeaponProjectile p,std::uint64_t owner=0):value(std::move(p)),effect(imageVisibility(value.visual?value.visual->effectTile():nullptr)),bullet(imageVisibility(value.tile)){
            root.visual=true;shooter=owner;
        }
    };
    struct Dynamic {
        std::shared_ptr<Trail> trail;
        std::shared_ptr<Projectile> projectile;
        VisibilityState visibility;
        bool drawVisible=false;
        std::shared_ptr<AirStrike> airStrike;
    };
    CombatSceneHooks& hooks;
    CombatMetrics metrics;
    DisplayConfiguration display;CameraPresentation presentation;
    void updateDisplay(std::optional<Vec2> target,bool advance){
        if(!display.enabled){camera.viewport={768,1024};camera.inputMinimumY=100;camera.fitWorld=camera.keepFocusVisible=camera.fillSurface=false;camera.lowerOverscan=94;return;}
        if(advance)presentation.advance({playerMotion.state().position,target,navigation.count()?std::optional<Vec2>(navigation.point(navigation.count()-1)):std::nullopt});
        const auto& view=presentation.snapshot();const DisplayLayout layout(display);
        camera.fillSurface=view.fillSurface;camera.viewport=view.viewport;
        const float uiRatio=layout.ui(DisplayRegion::bottom).scale/view.worldScale;
        // Compact surfaces have movable HUD groups. The UI dispatcher owns
        // actual button hits; a fixed bottom strip would block exposed world
        // points even after a group has moved. Retain the HD floor on regular.
        camera.inputMinimumY=display.compact?0.f:100*uiRatio;camera.lowerOverscan=view.fillSurface?0.f:94*uiRatio;
        camera.fitWorld=true;camera.keepFocusVisible=camera.viewport.width<768||camera.viewport.height<1024;
        camera.focusMinimumY=138*uiRatio+(display.compact?layout.ui(DisplayRegion::bottom).offset.y/view.worldScale:0);camera.focusRadius=float(machines.machine("Player").radius);
        camera.focusLeft=display.compact?display.insets.left/view.worldScale:0;camera.focusRight=display.compact?display.insets.right/view.worldScale:0;
    }
    ResourceAccess& resources;
    std::unique_ptr<PreparedTiles> ownedSharedAssets;
    PreparedTiles* sharedAssets=nullptr;
    DarwinWorldRandom ownRandom;
    WorldRandom* externalRandom=nullptr;
    MapPreview map;
    Models models;
    Machines machines;
    PlayerEquipment equipment;
    Weapons weapons;
    PreparedTiles tiles;
    TerrainCollision terrain;
    GameClock clock;
    MapCamera camera;
    BipodMotion playerMotion;
    DashGesture dashGesture;DashMotion dashMotion;
    TerrainContact playerContact;
    int hp=256;
    bool saveAllowed=false;
    int gameState=0;
    float stateChangeTime=0;
    MachineKind playerKind=MachineKind::bipod;
    WorldNavigation navigation;
    MovementPathVisual movementVisual;
    std::unique_ptr<WorldGates> gates;
    std::vector<PartMount> playerMounts;
    std::vector<std::size_t> playerAnimationSlots;
    std::array<std::string,4> equipmentCodes;
    std::vector<PlayerWeapon> playerWeapons;
    std::vector<VisibilityState> playerParts,playerShadows;
    VisibilityState playerRoot;
    std::array<VisibilityState,2> playerHPVisible;
    float playerHPScale=1,playerHPX=-1;
    struct PickupNotice {
        std::optional<ui::Text> label;
        float start=0,height=50;
        Vec2 scale{1,1};
        bool visible=false;
    } pickupNotice;
    std::array<VisibilityState,64> particleVisible;
    ParticlePool pool;
    std::unique_ptr<WeaponEffects> effects;
    std::array<VisualLayer,8> layers;
    std::array<VisualLayerFrame,8> prepared;
    std::vector<EnemySpawner> spawners;
    std::vector<std::unique_ptr<Enemy>> actors;
    std::vector<Enemy*> targetDrawNodes;
    std::vector<std::shared_ptr<FriendlyBot>> botActors;
    mutable std::vector<BotItemTarget> botItemView;
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
    :hooks(h),resources(r),ownedSharedAssets(setup.sharedAssets?nullptr:std::make_unique<PreparedTiles>("standalone-shared")),
     sharedAssets(setup.sharedAssets?setup.sharedAssets:ownedSharedAssets.get()),ownRandom(setup.randomSeed),externalRandom(setup.random),map(r,setup.map),
     models(Models::parse(Chunk::parse(r.read("PartsInfo.mcb")),Chunk::parse(r.read("MachInfo.mcb")))),
     machines(Machines::parse(DataTable::parse(r.read("MachList.tble")))),
     equipment(PlayerEquipment::parse(DataTable::parse(r.read("ItemInfo.tble")))),
     weapons(Weapons::parse(DataTable::parse(r.read("WeaponInfo.tble")))),tiles("world",sharedAssets),terrain(map.map()),clock(setup.initialRenderTime,setup.initialWorldTime),hp(setup.playerHP),saveAllowed(setup.isSave),pool(setup.initialRenderTime,static_cast<EnemyActorHost&>(*this)){
        if(ownedSharedAssets)prepareOriginalWeaponAssets(r,*sharedAssets,weapons);
        stateChangeTime=setup.initialRenderTime;
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
        playerMounts=equipment.mounts(models,setup.equipment);playerParts.resize(playerMounts.size());playerShadows.resize(playerMounts.size());
        playerKind=machines.machine("Player").kind;
        for(std::size_t i=0;i<playerMounts.size();++i)playerAnimationSlots.push_back(i);
        sharedAssets->prepareWorldEffects(r);
        movementVisual.prepare(r,tiles);navigation.observe(this);
        if(auto* t=tiles.prepare(r,"Target.png")){t->columns=2;t->rows=1;}
        for(const auto& [name,w]:weapons.all()){
            (void)w;
            metrics.playerWeaponEmissions.emplace(name,0);metrics.enemyWeaponEmissions.emplace(name,0);
        }
        // Player.init0xbcdc uses the shared manager for the field gauge and
        // pickup notice. Other VFX request their own shared tile when created.
        if(auto* tile=sharedAssets->prepare(r,"HPGauge.png")){tile->columns=1;tile->rows=2;}
        sharedAssets->prepare(r,"PickItem.png");
        for(auto& visible:playerHPVisible)visible=imageVisibility(tiles.lookup("HPGauge.png"));
        hooks.prepareTiles(r,tiles);
        prepareParts(r,playerMounts,playerParts,playerShadows);
        for(std::size_t i=0;i<playerMounts.size();++i){
            const auto& m=playerMounts[i];const auto& p=models.part(m.part);
            if(p.type==4){
                const auto w=weapons.all().find(p.name);
                if(w!=weapons.all().end())playerWeapons.push_back({i,&w->second,WeaponController(0),1,m.parentSocket==1});
            }
        }
        for(const auto kind:{MachineKind::vehicle,MachineKind::turret,MachineKind::aircraft}){metrics.enemySpawnsByKind.emplace(kind,0);metrics.enemyEmissionsByKind.emplace(kind,0);metrics.enemyDamageCallsByKind.emplace(kind,0);metrics.enemyKillsByKind.emplace(kind,0);metrics.enemyRemovalsByKind.emplace(kind,0);}
        for(const auto& s:map.map().spawners){
            const auto& info=machines.machine(s.model);
            if(!EnemyActor::supports(info.kind))throw std::runtime_error("Original map enemy kind has no restored consumer");
            spawners.emplace_back(s,info.attack!=0);
        }
        WorldGateNames gateNames(r);gates=std::make_unique<WorldGates>(r,tiles,map.map().gates,setup.openEventGate,gateNames,[this](const ui::TextRasterRequest& request){return hooks.prepareWorldText(request);});
        // setStage calls addMissionObjects after openMap and before Player/BOT.
        for(const auto& object:setup.missionObjects){
            if(object.kind==MissionFieldObject::Kind::item){
                hooks.placeMissionItem(object.code,object.itemPosition,rng());
                continue;
            }
            const float x=randomUnit(rng())*600.f-300.f;
            const float y=randomUnit(rng())*600.f-300.f;
            auto& enemy=addEnemy(machines.machine(object.code),{x,y},std::nullopt,false);
            enemy.actor.setMovementRates(1.5f,50.f);
        }
        playerMotion=BipodMotion(playerPosition,playerDirection,equipment.item(setup.equipment[1]).parameter3);
        playerContact.position=playerPosition;
        effects=std::make_unique<WeaponEffects>(pool,WeaponEffectResources{
            tiles.lookup("Efx_Fire.png"),tiles.lookup("Efx_Damage.png"),nullptr,nullptr,nullptr});
        for(auto& visible:particleVisible)visible.visual=true;
        camera.center={gate->position.x+140,gate->position.y};
        processCamera(camera,playerMotion.state().position,navigation);
        // setStage: XY RNG2, Bot.init RNG1, makeMach direction RNG1, then slot/HP.
        for(unsigned slot=0;slot<setup.botSlots.size();++slot){const auto& saved=setup.botSlots[slot];if(saved.machine.empty())continue;
            const auto found=machines.all().find(saved.machine);if(found==machines.all().end())continue;
            const float x=playerPosition.x+(randomUnit(rng())*100.f-50.f),y=playerPosition.y+(randomUnit(rng())*100.f-50.f);
            auto b=std::make_shared<FriendlyBot>(nextID++,models,weapons,found->second,Vec2{x,y},clock.worldTime(),rng());
            b->actor.setSlotIndex(slot);prepareParts(r,b->actor.mounts(),b->parts,b->shadows);
            if(auto* gauge=sharedAssets->prepare(r,"HPGauge.png")){gauge->columns=1;gauge->rows=2;}
            for(auto& v:b->gauge)v=imageVisibility(tiles.lookup("HPGauge.png"));
            if(saved.savedHP){b->actor.setHP(saved.savedHP);b->actor.refreshHPGauge(*this);}
            botActors.push_back(std::move(b));++metrics.botSpawns;
        }
    }
    WorldRandom& rng(){return static_cast<EnemyActorHost&>(*this);}
    std::uint32_t random()override{++metrics.randomCalls;return externalRandom?externalRandom->random():ownRandom.random();}
    void reseed(std::uint32_t seed)override{if(externalRandom)externalRandom->reseed(seed);else ownRandom.reseed(seed);}
    void prepareParts(ResourceAccess& r,std::span<const PartMount> mounts,std::vector<VisibilityState>& visibility,std::vector<VisibilityState>& shadows){
        for(std::size_t i=0;i<mounts.size();++i){const auto& p=models.part(mounts[i].part);auto* t=tiles.prepare(r,partImage(p));
            if(t){t->columns=std::max(1u,p.tileCount);t->rows=1;}
            visibility[i]=originalPartVisibility(t,p);shadows[i]=imageVisibility(p.tileCount?tiles.prepareShadow(r,partImage(p)):nullptr);
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
    const WeaponTarget* playerTarget()const override{return selectedTarget();}
    const WeaponTarget* target(std::uint64_t id)const override{const auto* e=enemy(id);if(!e)return nullptr;targetScratch={id,e->actor.motion().position,e->actor.state().hp};return &targetScratch;}
    std::span<const BotItemTarget> items()const override{botItemView=hooks.botItems();return botItemView;}
    FriendlyBot* bot(std::uint64_t id){for(auto& b:botActors)if(b->id==id)return b.get();return nullptr;}
    void writeSlotHP(unsigned slot,int value)override{hooks.botHPChanged(slot,value);}
    void clearSlotCode(unsigned slot)override{hooks.botDestroyed(slot);++metrics.botDeaths;}
    void unselectTarget(std::uint64_t id)override{if(auto* e=enemy(id))onTarget(*e,false);}
    void baseCrash(const Bot& actor)override{baseCrash(actor.motion().position);}
    void hitSound(const Bot& actor)override{soundID(15+random()%3,damageSoundPosition(actor.motion().position));}
    void beforeParts(const Bot& actor)override{
        FriendlyBot* b=nullptr;for(auto& candidate:botActors)if(&candidate->actor==&actor){b=candidate.get();break;}if(!b)throw std::logic_error("Missing original BOT child owner");
        const auto root=actor.motion().position;refreshVisibility(b->root,root,&camera);const auto* tile=tiles.lookup("HPGauge.png");if(!tile)return;
        for(unsigned child=0;child<2;++child){const Vec2 position{root.x+(child?actor.gauge().x:0.f),root.y+(child?21.f:20.f)};
            VisualImage image;image.tile=tiles.visual(*tile);image.tileNo=child?0:1;image.useAtlas=true;image.scale={child?actor.gauge().scaleX:.5f,.5f};
            image.screenPosition=camera.toScreen(position);image.isInScreen=b->gauge[child].inScreen;layers[5].submitImage(image);refreshVisibility(b->gauge[child],position,&camera);}
    }
    bool inCamera(Vec2 position)const override{return hm2::inCamera(VisibilityState{},position,&camera);}
    Enemy& addEnemy(const MachineInfo& info,Vec2 position,std::optional<std::size_t> source,bool boss){
        auto e=std::make_unique<Enemy>(nextID++,source,models,weapons,info,position,clock.worldTime(),rng());
        e->actor.setBoss(boss);++metrics.enemySpawnsByKind.at(e->actor.info().kind);
        prepareParts(resources,e->actor.mounts(),e->parts,e->shadows);
        e->targetVisibility=imageVisibility(tiles.lookup("Target.png"));
        actors.push_back(std::move(e));
        return *actors.back();
    }
    bool spawn(const MapSpawner& s,bool boss)override{
        addEnemy(machines.machine(s.model),s.position,spawning,boss);
        return true;
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
        targetID=id;if(auto* next=enemy(id)){onTarget(*next,true);hooks.enemyTargetChanged(next->actor.info().name);}
    }
    void clearPlayerTarget(){
        setTarget(0);moveToTarget=false;
        auto state=playerMotion.state();state.state=0;playerMotion=BipodMotion(state);
    }
    MovementPathState pathState(const WorldNavigation& path)const{
        MovementPathState out;out.start=path.startIndex();out.count=path.count();
        for(unsigned i=0;i<out.count;++i)out.points[(out.start+i)%12]=path.point(i);
        return out;
    }
    void acceptedMovePath(const WorldNavigation& path,Vec2 player,bool reset)override{movementVisual.acceptedSetMovePath(pathState(path),player,reset);}
    void clearedMovePath(unsigned start,unsigned count)override{movementVisual.clearMovePath(start,count);}
    void damage(std::uint64_t id,int amount){
        ++metrics.damageCalls;
        if(id==1){
            amount=hooks.playerDamage(amount,rng());
            hp=std::bit_cast<std::int32_t>(std::uint32_t(hp)-std::uint32_t(amount));
            if(hp<1){hp=0;baseCrash(playerMotion.state().position);clearPlayerTarget();auto s=playerMotion.state();s.state=2;playerMotion=BipodMotion(s);hooks.playerDied();}
            soundID(15+random()%3,damageSoundPosition(playerMotion.state().position));hooks.playerHPChanged(hp);refreshPlayerHPGauge();return;
        }
        if(auto* b=bot(id)){++metrics.botDamageCalls;const auto before=b->actor.state().hp;b->actor.onDamage(amount,*this);const auto removed=std::uint64_t(std::max(std::int64_t(0),std::int64_t(before)-b->actor.state().hp));metrics.botHPRemoved+=removed;b->hpRemoved+=removed;return;}
        auto* e=enemy(id);if(!e)return;
        ++metrics.enemyDamageCallsByKind.at(e->actor.info().kind);
        auto& state=e->actor.state();state.hp=std::bit_cast<std::int32_t>(std::uint32_t(state.hp)-std::uint32_t(amount));
        if(state.hp<1){
            state.hp=0;++metrics.kills;++metrics.enemyKillsByKind.at(e->actor.info().kind);baseCrash(e->actor.motion().position);
            const bool wasFirst=!selection.empty()&&selection.front()==id;
            if(targetID==id){clearPlayerTarget();if(navigation.count())playerMotion.setDestination(navigation.point(0));}
            state.targetPlayer=false;state.moveToTarget=false;e->actor.setMachineState(2);
            for(auto& b:botActors)b->actor.clearTargetIf(id,*this);
            hooks.dropEnemyLoot(e->actor.info(),e->actor.motion().position,clock.worldTime(),clock.renderTime(),rng());
            e->actor.removePartsForCorpse();
            if(e->spawner)spawners[*e->spawner].enemyDead(clock.worldTime(),rng());
            std::erase(selection,id);
            if(wasFirst&&!selection.empty())setTarget(selection.front());
            hooks.rewardEnemyKill(e->actor.info(),e->actor.motion().position,clock.worldTime(),clock.renderTime(),rng());
            const float u=randomUnit(rng());e->crashTime=float((clock.worldTime()+3.0)+double(u+u));
        }
        soundID(15+random()%3,damageSoundPosition(e->actor.motion().position));
        // Enemy::onDamage resumes after super/onCrash and may retarget even
        // after its parts have been removed. Controllers are removed separately.
        e->actor.retargetAfterDamage();
    }
    struct EffectsHost:WeaponEffectsHost {
        Impl& scene;bool hostile;std::uint64_t owner;
        EffectsHost(Impl& s,bool h,std::uint64_t o):scene(s),hostile(h),owner(o){}
        const WeaponTarget* target()const override{if(hostile)return scene.player();if(auto* b=scene.bot(owner))return scene.target(b->actor.state().target);return scene.selectedTarget();}
        void damage(const WeaponInfo&,std::uint64_t id,int amount)override{const auto* victim=scene.bot(owner)?scene.enemy(id):nullptr;const int before=victim?victim->actor.state().hp:0;scene.damage(id,amount);if(victim&&victim->actor.state().hp<before){const auto removed=std::uint64_t(before-victim->actor.state().hp);scene.metrics.botEnemyHPRemoved+=removed;if(auto* bot=scene.bot(owner))bot->enemyHPRemoved+=removed;}}
        TileImage* preparedTile(std::string_view name)override{return scene.tiles.lookup(name);}
        TileImage* prepareOriginalTile(std::string_view name,TileImage*)override{return scene.sharedAssets->prepare(scene.resources,name);}
        void fireSound(const WeaponInfo& info,Vec2 p)override{scene.fireSound(info,p);}
        void attachProjectile(WeaponProjectile p)override{++scene.metrics.projectilesCreated;scene.dynamic.push_back({{},std::make_shared<Projectile>(std::move(p),owner),{},false,{}});}
        void trailAttached(const Trail&)override{auto trail=scene.effects->trails().back();scene.dynamic.push_back({trail,{},imageVisibility(trail->state().tile),false,{}});}
    };
    TileImage* preparedTile(std::string_view name)override{return tiles.lookup(name);}
    TileImage* prepareOriginalTile(std::string_view name)override{return sharedAssets->prepare(resources,name);}
    Particle* allocateParticle(float time)override{auto* p=pool.allocate(time,rng());metrics.particleAllocations=pool.allocations();return p;}
    void sound(unsigned id)override{soundID(id,{});}
    void createBullet(const AirStrikeBullet& shot)override{
        if(!shot.weapon)throw std::logic_error("AirStrike emitted without its original weapon");
        WeaponEffectInput input;input.worldTime=clock.worldTime();input.renderTime=clock.renderTime();input.playerPosition=playerMotion.state().position;input.hostile=shot.toPlayer;
        EffectsHost host(*this,input.hostile,0);
        // Synchronous original setFire consumes speed/trail RNG and sound;
        // setDrop(true) follows it, then addChild. No controller flash/bonus.
        effects->createProjectile(*shot.weapon,{shot.source,shot.destination,shot.angle},input,rng(),host,shot.drop);
        ++metrics.airStrikeProjectiles;
    }
    void startAirStrike(const WeaponInfo* weapon,float delay,Vec2 target){
        auto actor=std::make_shared<AirStrike>(weapon,delay,target,playerMotion.state().position,clock.worldTime(),clock.renderTime(),*this,rng());
        dynamic.push_back({{},{},{},false,std::move(actor)});++metrics.airStrikeRequests;
    }
    void setDashVelocity(Vec2 value)override{dashMotion.setVelocity(value);++metrics.dashStarts;}
    void clearMovePointer()override{navigation.cancel();}
    void setSpecial(float value)override{hooks.setPlayerSpecial(value);}
    Vec2 position()const override{return playerMotion.state().position;}
    void addPosition(Vec2 delta)override{const auto p=playerMotion.state().position;playerMotion.applyContact({p.x+delta.x,p.y+delta.y});}
    void emitWeapon(const WeaponInfo& w,const WeaponEmission& shot,WeaponEffectInput input,std::uint64_t owner){
        ++metrics.emissions;if(input.hostile)++metrics.enemyEmissions;else ++metrics.playerEmissions;
        ++(input.hostile?metrics.enemyWeaponEmissions:metrics.playerWeaponEmissions).at(w.part);
        EffectsHost host(*this,input.hostile,owner);effects->emit(w,shot,input,rng(),host);
    }
    void emit(const WeaponInfo& w,const WeaponEmission& shot,const EnemyActorEmission& input)override{
        ++metrics.enemyEmissionsByKind.at(input.owner->info().kind);
        WeaponEffectInput effect;effect.worldTime=clock.worldTime();effect.renderTime=clock.renderTime();
        effect.machinePosition=input.machinePosition;effect.partCenter=input.partCenter;effect.playerPosition=playerMotion.state().position;effect.hostile=true;
        emitWeapon(w,shot,effect,0);
    }
    void emit(const WeaponInfo& w,const WeaponEmission& shot,const BotEmission& input)override{
        ++metrics.emissions;++metrics.botEmissions;WeaponEffectInput effect;
        effect.worldTime=clock.worldTime();effect.renderTime=clock.renderTime();effect.machinePosition=input.machinePosition;effect.partCenter=input.partCenter;
        effect.playerPosition=playerMotion.state().position;effect.hostile=false;effect.playerShooter=false;
        std::uint64_t owner=0;for(const auto& b:botActors)if(&b->actor==input.owner){owner=b->id;break;}
        EffectsHost host(*this,false,owner);effects->emit(w,shot,effect,rng(),host);
    }
    void objectContacts(EnemyActor& actor)override{
        VisibilityState root;root.show=actor.state().show;
        if(!hm2::inCamera(root,actor.motion().position,&camera))return;
        ObjectBody self{actor.motion().position,actor.info().radius,actor.info().kind};
        for(auto& e:actors){
            if(&e->actor==&actor)break;
            if(e->removed||e->actor.info().kind==MachineKind::aircraft||!hm2::inCamera(e->root,e->actor.motion().position,&camera))continue;
            ObjectBody other{e->actor.motion().position,e->actor.info().radius,e->actor.info().kind};
            if(resolveObjectCollision(self,other))e->actor.applyContact(other.position);
        }
        ObjectBody other{playerMotion.state().position,machines.machine("Player").radius,MachineKind::bipod};
        if(resolveObjectCollision(self,other))playerMotion.applyContact(other.position);
        actor.applyContact(self.position);
    }
    void collectPart(const PartPose& pose,Vec2 actor,VisibilityState& visibility,VisibilityState& shadow,MachineKind kind){
        // GobMachParts.setHvMach0x1c780 enables atlas only for machType!=0.
        collectOriginalPart(layers.at(pose.layer),layers[0],tiles,pose,add(actor,pose.center),&camera,visibility,shadow,kind);
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
        const auto& freshPart=models.part(fresh.part);
        auto* tile=tiles.prepare(resources,partImage(freshPart));
        if(tile){tile->columns=std::max(1u,freshPart.tileCount);tile->rows=1;}
        const auto* shadow=freshPart.tileCount?tiles.prepareShadow(resources,partImage(freshPart)):nullptr;
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
            playerShadows.erase(playerShadows.begin()+std::ptrdiff_t(*removed));
            for(auto& mount:playerMounts)if(mount.parent>int(*removed))--mount.parent;
            for(auto& weapon:playerWeapons)if(weapon.mount>*removed)--weapon.mount;
        }
        const auto index=playerMounts.size();playerMounts.push_back(std::move(fresh));
        playerParts.push_back(originalPartVisibility(tile,models.part(playerMounts.back().part)));
        playerShadows.push_back(imageVisibility(shadow));
        animationSlots.push_back(*newCanonical);playerAnimationSlots=std::move(animationSlots);
        equipmentCodes=std::move(codes);
        if(newInfo!=weapons.all().end())playerWeapons.push_back({index,&newInfo->second,WeaponController(clock.worldTime()),1,slot==2});
    }
    void refreshPlayerHPGauge(){
        playerHPScale=float(hp)/float(hooks.playerMaximumHP());playerHPX=playerHPScale*21.f-21.f;
    }
    void collectPlayerHPGauge(){
        const auto* tile=tiles.lookup("HPGauge.png");if(!tile)return;
        const auto root=playerMotion.state().position;
        for(unsigned i=0;i<2;++i){const Vec2 position{root.x+(i?playerHPX:0.f),root.y+(i?31.f:30.f)};
            VisualImage image;image.tile=tiles.visual(*tile);image.tileNo=i?0:1;image.useAtlas=true;image.blendType=0;
            image.screenPosition=camera.toScreen(position);image.scale={i?playerHPScale:1.f,1.f};image.isInScreen=playerHPVisible[i].inScreen;
            layers[5].submitImage(image);refreshVisibility(playerHPVisible[i],position,&camera);
        }
    }
    void tickPickupNotice(){
        auto& notice=pickupNotice;
        if(notice.start==0)return;
        const float age=float(clock.worldTime()-double(notice.start));
        if(age<=.6f){
            const float angle=float(double(age/.6f)*std::numbers::pi*3.0);
            notice.height=50.f+std::abs(std::sin(angle)*((.6f-age)*20.f));
        }else{
            notice.height=50.f;
            if(age>2.5f){
                notice.scale.x+=.1f;
                notice.scale.y-=.1f;
                if(notice.scale.y<=0){notice.visible=false;notice.start=0;}
            }
        }
    }
    void collectPickupNotice(){
        auto& notice=pickupNotice;
        if(!notice.visible)return;
        const auto* tile=tiles.lookup("PickItem.png");if(!tile)return;
        const auto root=playerMotion.state().position;
        const Vec2 world{root.x,root.y+notice.height};
        if(!hm2::inCamera(imageVisibility(tile),world,&camera))return;
        VisualImage background;
        background.tile=tiles.visual(*tile);
        background.screenPosition=camera.toScreen(world);
        background.scale=notice.scale;
        auto& layer=layers[3];
        layer.submitImage(background);
        if(notice.label){
            notice.label->position=background.screenPosition;
            notice.label->scale.y=notice.scale.y;
            Frame frame;
            notice.label->append(frame);
            for(auto& sprite:frame.sprites)layer.submitNonAtlas(std::move(sprite));
        }
    }
    void tickPlayer(){
        tickPickupNotice();
        if(hp>0&&hp<hooks.playerMaximumHP()&&double(hpRefreshTime)+.3<clock.worldTime()){
            ++hp;hpRefreshTime=float(clock.worldTime());refreshPlayerHPGauge();
        }
        hooks.playerProgressTick();
        // Player HP/SP processing precedes GobHvMach.tick. That superclass
        // skips its child pass when hidden, and processCrash while paused.
        if(!playerRoot.show)return;
        if(!paused&&playerMotion.state().state==2){
            playerMotion.setState(3); // Player.onDestroy0xb024; no corpse/drop.
            if(gameState!=3){
                gameState=3;stateChangeTime=hooks.globalRenderTime(clock.renderTime());
                hooks.playerDestroyed();
            }
        }
        if(!paused&&(playerMotion.state().state==0||playerMotion.state().state==1)&&playerRoot.inScreen){
            const auto* target=selectedTarget();std::optional<Vec2> aim;if(target)aim=target->position;
            const int wall=playerContact.current;playerMotion.tick(wall<0?nullptr:&terrain.edges().at(std::size_t(wall)),aim);
            const auto allocations=pool.allocations();const auto randomCalls=metrics.randomCalls;
            if(dashMotion.tick(clock.renderTime(),{true,paused,playerRoot.inScreen,playerMotion.state().state,0},*this))++metrics.dashMoveTicks;
            metrics.dashParticleAllocations+=pool.allocations()-allocations;metrics.dashRandomCalls+=metrics.randomCalls-randomCalls;
            // The common collision pass also runs when Dash returned false.
            playerContact=terrain.resolve(playerMotion.state().position,playerContact.previous);playerMotion.applyContact(playerContact.position);
            if(target&&double(targetCheckTime+.5f)<clock.worldTime()){
                if(moveToTarget)playerMotion.setDestination(distance2(playerMotion.state().position,target->position)<40000.f?playerMotion.state().position:target->position);
                targetCheckTime=float(clock.worldTime());
            }
        }
        collectPlayerHPGauge();
        collectPickupNotice();
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
            collectPart(*p,playerMotion.state().position,playerParts[i],playerShadows[i],playerKind);
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
        std::size_t botCount()const override{return scene.botActors.size();}
        ImpactTarget bot(std::size_t i)const override{const auto& b=*scene.botActors.at(i);return {b.id,b.actor.motion().position,b.actor.state().hp,b.actor.info().radius,b.root.inScreen};}
        std::uint64_t selectedEnemy()const override{return scene.targetID;}
        int damageBonus()const override{return scene.hooks.damageBonus();}
        void damage(std::uint64_t id,int amount)override{
            const bool airStrike=projectile.value.visual->state().drop;
            const auto* victim=(airStrike||scene.bot(projectile.shooter))&&id!=1?scene.enemy(id):nullptr;const int before=victim?victim->actor.state().hp:0;
            if(airStrike)++scene.metrics.airStrikeDamageCalls;scene.damage(id,amount);
            if(victim&&victim->actor.state().hp<before){const auto removed=std::uint64_t(before-victim->actor.state().hp);
                if(airStrike)scene.metrics.airStrikeEnemyHPRemoved+=removed;
                if(auto* owner=scene.bot(projectile.shooter)){owner->enemyHPRemoved+=removed;scene.metrics.botEnemyHPRemoved+=removed;}
            }
        }
        void finishTrail(Vec2 position)override{if(projectile.value.trail)projectile.value.trail->add(position,scene.clock.worldTime(),scene.rng());}
        void nuclearFlash(Vec2 position)override{
            // 0x71d0, CFString0x5e0fc -> Efx_Nuclear.png.
            if(auto* p=scene.pool.allocate(scene.clock.renderTime(),scene.rng())){
                auto& s=p->state();s.blendType=2;s.life=.25f;s.tile=scene.sharedAssets->prepare(scene.resources,"Efx_Nuclear.png");
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
            auto trail=dynamic[i].trail;auto projectile=dynamic[i].projectile;auto airStrike=dynamic[i].airStrike;
            if((trail&&trail->state().removed)||(projectile&&projectile->value.motion.state().removed)||(airStrike&&airStrike->state().removed)){
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
            else if(airStrike){
                // World.tick calls QobBase during pause. AirStrike itself has
                // no pause gate, unlike GobBullet/Trail; frozen world time only.
                ++metrics.airStrikeTicks;airStrike->tick(clock.worldTime(),&camera);airStrike->collect(layers[7],camera,tiles);
            }
            ++i;
        }
        effects->pruneRemovedTrails();
    }
    void tick(){
        if(!clock.step(paused,active))return;
        for(auto& layer:layers)(void)layer.drain();
        targetDrawNodes.clear();
        if(!paused&&gameState==0){
            const auto before=pathState(navigation);
            const auto* currentTarget=selectedTarget();movementVisual.processTarget(playerMotion.state().position,currentTarget?std::optional<Vec2>{currentTarget->position}:std::nullopt);
            if(auto* target=selected();target&&!hm2::inCamera(target->root,target->actor.motion().position,&camera))clearPlayerTarget();
            navigation.process(playerMotion);
            movementVisual.processMyMach(playerMotion.state().position,clock.worldTime(),before,pathState(navigation));
        }
        std::optional<Vec2> target;if(const auto* e=selected())target=e->actor.motion().position;
        updateDisplay(target,true);
        processCamera(camera,playerMotion.state().position,navigation,target);
        movementVisual.appendWorldLayers(layers[1],layers[7],&camera);
        // listEnemy is visited before map spawners append new actors to it.
        const auto priorEnemies=actors.size();
        for(std::size_t i=0;i<priorEnemies;++i){auto& e=*actors[i];if(e.removed)continue;if(e.removePending){e.removed=true;++metrics.enemyRemovalsByKind.at(e.actor.info().kind);continue;}
            ++metrics.enemyTicks;if(!paused&&e.actor.motion().state==2)tickCorpse(e);
            advanceTarget(e);const bool targetShowAtChildTick=e.targetShow;
            e.actor.tick({clock.worldTime(),paused,e.root.inScreen},terrain,*this);
            registerTarget(e,targetShowAtChildTick);
            auto poses=std::vector<PartPose>(e.actor.atlasPoses().begin(),e.actor.atlasPoses().end());
            std::sort(poses.begin(),poses.end(),[](const auto& a,const auto& b){return a.mountIndex<b.mountIndex;});
            for(const auto& pose:poses)collectPart(pose,e.actor.motion().position,e.parts.at(pose.mountIndex),e.shadows.at(pose.mountIndex),e.actor.info().kind);
        }
        // Original listBot precedes listItem, including its later prune pass.
        for(std::size_t i=0;i<botActors.size();){auto b=botActors[i];
            if(b->actor.state().removed){botActors.erase(botActors.begin()+std::ptrdiff_t(i));++metrics.botRemovals;continue;}
            ++metrics.botTicks;b->actor.tick({clock.worldTime(),paused},*this);
            for(const auto& pose:b->actor.atlasPoses())collectPart(pose,b->actor.motion().position,b->parts.at(pose.mountIndex),b->shadows.at(pose.mountIndex),b->actor.info().kind);
            ++i;
        }
        hooks.tickItems(clock.worldTime(),clock.renderTime(),playerMotion.state().position,rng());
        hooks.collectItems(layers[0],layers[1],camera,tiles);
        tickPool();
        for(spawning=0;spawning<spawners.size();++spawning)spawners[spawning].tick(clock.worldTime(),playerMotion.state().position,*this);
        gates->tick(playerMotion.state().position,[this]{return paused;},[this](const MapGate& gate){hooks.linkGate(gate);},&camera);gates->collect(layers[1],&camera);
        tickPlayer();tickDynamic();
        for(std::size_t i=0;i<layers.size();++i)prepared[i]=layers[i].drain();
        metrics.particleAllocations=pool.allocations();
    }
    bool select(std::uint64_t id,bool append,bool userSelection=false){
        auto* e=enemy(id);if(!e||e->actor.state().hp<1)return false;
        if(append){if(selection.empty())return false;if(std::find(selection.begin(),selection.end(),id)!=selection.end())return true;if(selection.size()<16){selection.push_back(id);onTarget(*e,true);}return true;}
        moveToTarget=targetID==id;for(auto old:selection)if(auto* prior=enemy(old))onTarget(*prior,false);selection.assign(1,id);
        if(moveToTarget)navigation.clearPath();
        if(userSelection)movementVisual.setTargetPathShown(true);
        setTarget(id);return true;
    }
    bool input(PointerPhase phase,Vec2 screen,std::uintptr_t id){
        if(phase==PointerPhase::cancel){const bool was=targetPointer==id;if(was)targetPointer=0;return navigation.input(phase,screen,id,camera,playerMotion)||was;}
        if(paused||!active||hp<1)return false;
        if(!id||!std::isfinite(screen.x)||!std::isfinite(screen.y)||screen.y<camera.inputMinimumY)return false;
        // Original EasyMove0x12698, scoped here. Do not reassociate the
        // float operations or globally change the other camera consumers.
        const Vec2 base{camera.center.x-camera.viewport.width*.5f,camera.center.y-camera.viewport.height*.5f};
        const Vec2 shifted{base.x+camera.offset.x,base.y+camera.offset.y};
        const Vec2 world{shifted.x+screen.x,shifted.y+screen.y};std::uint64_t found=0;float best=10000;
        for(const auto& e:actors)if(!e->removed&&e->actor.state().hp>0){const auto d=distance2(world,e->actor.motion().position);const float radius=float(e->actor.info().radius);if(d<radius*radius&&d<best){best=d;found=e->id;}}
        if(phase==PointerPhase::begin){
            if(navigation.pointer())return true;
            if(distance2(world,playerMotion.state().position)<900)return navigation.inputWorld(phase,world,id,playerMotion);
            if(found){targetPointer=id;return select(found,false,true);}
            moveToTarget=false;playerMotion.setState(1);
            const bool handled=navigation.inputWorld(phase,world,id,playerMotion);
            dashGesture.groundBegin(world,playerMotion.state().position,clock.worldTime(),hooks.playerSpecial(),*this);
            return handled;
        }
        if(phase==PointerPhase::move){dashGesture.pathDrag(navigation.pointer()==id,navigation.count());}
        if(phase==PointerPhase::move&&targetPointer==id){if(found)select(found,true);return true;}
        if(phase==PointerPhase::end){const bool hadTargetPointer=targetPointer!=0;targetPointer=0;const bool moved=navigation.inputWorld(phase,world,id,playerMotion);return hadTargetPointer||moved;}
        return navigation.inputWorld(phase,world,id,playerMotion);
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
ui::PreparedScreenText CombatSceneHooks::prepareWorldText(const ui::TextRasterRequest&){throw std::logic_error("Missing original world text host");}
void CombatSceneHooks::setPlayerSpecial(float){throw std::logic_error("Missing actual Player SP consumer");}
void CombatSceneHooks::placeMissionItem(std::string_view,Vec2,WorldRandom&){throw std::logic_error("Missing mission item consumer");}
CombatScene::CombatScene(ResourceAccess& r,CombatSceneHooks& h,CombatSetup setup):impl_(std::make_unique<Impl>(r,h,setup)){}
CombatScene::~CombatScene()=default;
void CombatScene::tick(){impl_->tick();}
bool CombatScene::input(PointerPhase p,Vec2 v,std::uintptr_t id){return impl_->input(p,v,id);}
bool CombatScene::selectEnemy(std::uint64_t id,bool append){return impl_->select(id,append);}
void CombatScene::setFiring(bool left,bool right){impl_->leftFire=left;impl_->rightFire=right;}
void CombatScene::equipWeapon(EquipmentCodes codes,unsigned slot){impl_->equipWeapon(codes,slot);}
void CombatScene::refreshPlayerHPGauge(){impl_->refreshPlayerHPGauge();}
void CombatScene::showLevelUpEffect(){
    auto& p=*impl_;++p.metrics.levelUpEffectRequests;
    auto* effect=allocateParticle();if(!effect)return;
    auto* tile=p.sharedAssets->prepare(p.resources,"Efx_LevelUp.png");if(tile){tile->columns=1;tile->rows=1;}
    auto& s=effect->state();s.tile=tile;s.tileNumber=0;s.life=2.f;s.blendType=2;s.scale={.5f,.5f};s.scaleVelocity=.006f;s.position=p.playerMotion.state().position;
    effect->start(p.clock.renderTime());++p.metrics.levelUpEffectsCreated;
}
void CombatScene::beginPickupNotice(){
    auto& notice=impl_->pickupNotice;
    notice.scale={1,1};
    notice.visible=true;
    notice.label.reset();
}
void CombatScene::setPickupNoticeText(std::string_view text,double worldTime){
    ui::TextRasterRequest request{"Player.pickup",std::string(text),"Arial",{120,14},12,ui::TextAlign::center,false,0};
    auto prepared=impl_->hooks.prepareWorldText(request);
    if(!prepared.bitmap||!(prepared.bitmap->request==request))throw std::runtime_error("Pickup text preparation changed the original request");
    impl_->pickupNotice.label.emplace(std::move(prepared.image),std::move(prepared.bitmap));
    impl_->pickupNotice.start=float(worldTime);
}
Particle* CombatScene::allocateParticle(){auto* particle=impl_->pool.allocate(impl_->clock.renderTime(),impl_->rng());impl_->metrics.particleAllocations=impl_->pool.allocations();return particle;}
const TileImage* CombatScene::preparedTile(std::string_view name)const{return impl_->tiles.lookup(name);}
const TileImage* CombatScene::prepareRepairEffect(std::string_view name){return impl_->sharedAssets->prepare(impl_->resources,name);}
bool CombatScene::isPaused()const{return impl_->paused;}
void CombatScene::startAirStrike(const WeaponInfo* weapon,float delay,Vec2 target){impl_->startAirStrike(weapon,delay,target);}
std::size_t CombatScene::activeAirStrikes()const{std::size_t count=0;for(const auto& node:impl_->dynamic)if(node.airStrike&&!node.airStrike->state().removed)++count;return count;}
void CombatScene::setPlayerHP(int hp){impl_->hp=hp;}
void CombatScene::setIsSave(bool value){impl_->saveAllowed=value;}
bool CombatScene::isSave()const{return impl_->saveAllowed;}
void CombatScene::setPaused(bool p){impl_->paused=p;}
void CombatScene::setGameState(int value){impl_->gameState=value;impl_->stateChangeTime=impl_->hooks.globalRenderTime(impl_->clock.renderTime());}
int CombatScene::gameState()const{return impl_->gameState;}
float CombatScene::gameStateChangeTime()const{return impl_->stateChangeTime;}
void CombatScene::setActive(bool a){impl_->active=a;if(!a){impl_->navigation.cancel();impl_->targetPointer=0;impl_->leftFire=impl_->rightFire=false;}}
const GameClock& CombatScene::clock()const{return impl_->clock;}
const BipodState& CombatScene::player()const{return impl_->playerMotion.state();}
int CombatScene::playerHP()const{return impl_->hp;}
const MapCamera& CombatScene::camera()const{return impl_->camera;}
const TerrainCollision& CombatScene::terrain()const{return impl_->terrain;}
const CombatMetrics& CombatScene::metrics()const{return impl_->metrics;}
std::span<const WorldGateSnapshot> CombatScene::gates()const{return impl_->gates->snapshots();}
void CombatScene::collectRadarMarks(std::vector<ui::RadarMark>& marks)const{
    const auto& p=*impl_;
    // DlgRader.draw0x23d28 visits Player, listBot and actual listGate in order.
    marks.clear();
    marks.push_back({p.playerMotion.state().position,0});
    for(const auto& bot:p.botActors)marks.push_back({bot->actor.motion().position,1});
    for(const auto& gate:p.gates->snapshots())marks.push_back({gate.definition.position,2});
}
CombatDashSnapshot CombatScene::dashState()const{return {impl_->dashMotion.velocity(),impl_->dashMotion.lastEffectPosition(),impl_->dashGesture.tapTime(),impl_->hooks.playerSpecial(),impl_->navigation.pointer(),impl_->navigation.count(),impl_->playerContact.current};}
std::vector<CombatEnemySnapshot> CombatScene::enemies()const{
    std::vector<CombatEnemySnapshot> out;
    for(const auto& e:impl_->actors)if(!e->removed){
        std::size_t parts=0;for(std::size_t i=0;i<e->actor.mounts().size();++i)if(e->actor.partActive(i))++parts;
        out.push_back({e->id,e->actor.info().name,e->actor.motion().position,e->actor.state().hp,e->actor.state().targetPlayer,e->actor.motion().state==2,parts,e->actor.info().kind,e->actor.isBoss(),e->actor.state().moveToTarget,e->crashTime});
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
Frame CombatScene::preparationFrame()const{auto frame=impl_->map.compose(impl_->camera);if(impl_->ownedSharedAssets)impl_->sharedAssets->appendPreparation(frame);impl_->tiles.appendPreparation(frame);auto gates=impl_->gates->preparationFrame();frame.sprites.insert(frame.sprites.end(),std::make_move_iterator(gates.sprites.begin()),std::make_move_iterator(gates.sprites.end()));return frame;}
std::uint64_t CombatScene::resourceRevision()const{return impl_->tiles.revision();}
Frame CombatScene::globalPreparationFrame()const{Frame frame;impl_->sharedAssets->appendPreparation(frame);return frame;}
Frame CombatScene::drawFrame(){return impl_->draw();}
}

namespace hm2 {
std::vector<CombatBotSnapshot> CombatScene::bots()const{
    std::vector<CombatBotSnapshot> result;for(const auto& b:impl_->botActors){const auto& a=b->actor;result.push_back({b->id,a.info().name,a.state().slotIndex,a.motion().position,a.state().hp,a.info().hp,a.info().referenceType,a.state().removed,a.state().target,a.state().pickItem,a.mounts().empty()?0:a.mounts().back().angle,a.gauge(),a.metrics(),b->itemCaptures,b->hpRemoved,b->enemyHPRemoved});}return result;
}
void CombatScene::setBotHP(std::uint64_t id,int value){auto* b=impl_->bot(id);if(!b)throw std::logic_error("Missing original BOT repair actor");b->actor.setHP(value);}
void CombatScene::refreshBotHPGauge(std::uint64_t id){auto* b=impl_->bot(id);if(!b)throw std::logic_error("Missing original BOT repair gauge");b->actor.refreshGaugeVisual();}
void CombatScene::restoreBotsHP(){for(auto& b:impl_->botActors){b->actor.setHP(b->actor.info().hp);b->actor.refreshHPGauge(*impl_);}}
void CombatScene::botPickedItem(std::uint64_t bot,std::uint64_t item){if(auto* b=impl_->bot(bot)){b->actor.pickedItem(item);++impl_->metrics.botItemCaptures;++b->itemCaptures;}}
}

namespace hm2 {
void CombatScene::setDisplay(DisplayConfiguration display){impl_->navigation.cancel();impl_->targetPointer=0;impl_->leftFire=impl_->rightFire=false;impl_->display=display;impl_->presentation.configure(display);impl_->updateDisplay({},false);constrainCamera(impl_->camera,impl_->playerMotion.state().position);}
const CameraPresentationState& CombatScene::cameraPresentation()const{return impl_->presentation.snapshot();}
}

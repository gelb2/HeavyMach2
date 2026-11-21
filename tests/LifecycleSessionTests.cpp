#include "PreparedTextStub.hpp"
#include "Core/Session/GameSession.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include "Core/Progression/Progression.hpp"
#include "Core/Data/Models.hpp"
#include "Core/Data/Weapons.hpp"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace hm2;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;
    Bytes read(std::string_view name)override{std::ifstream in(root/std::string(name),std::ios::binary);assert(in);return {std::istreambuf_iterator<char>(in),{}};}
    bool exists(std::string_view name)const override{return std::filesystem::exists(root/std::string(name));}
};
struct Host:SessionHost {
    unsigned saves=0,sounds=0;
    ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& r)override{return test::preparedTextStub(r);}
    void sound(std::string_view,Vec2)override{++sounds;}void soundID(unsigned,Vec2)override{++sounds;}
    void alert(std::string_view)override{}void requestName(unsigned)override{}void requestDelete(unsigned)override{}
    bool writeSave(std::span<const std::uint8_t>,SaveReason)override{++saves;return true;}
    std::uint32_t absoluteTimeLow()override{return 1;}std::uint32_t unixTime()override{return 1794268800;}
};
void press(GameSession& game,unsigned id){game.action({ui::ScreenId::field,ui::Panel::root,{ui::ButtonEventType::pop,{0,id,0},1,{}}});}
void check(Files& files,bool phone){
    Host host;DarwinWorldRandom random(7);SaveData save;save.initializeSlot(0,"LIFE","kr",1794268800);
    GameSession game(files,host,random,save,"kr");game.setDisplay(DisplayConfiguration::surface(phone?Size{912,420}:Size{1024,1366},phone?DisplayInsets{68,20,68,0}:DisplayInsets{0,25,0,0}));
    assert(game.selectSlot(0));(void)game.state();
    for(unsigned i=0;game.screenTransition().phase!=8&&i<200;++i){game.tick();(void)game.state();}
    assert(game.screenTransition().phase==8);
    assert(game.enterField("Stage01_01",0));(void)game.state();
    game.tick();
    for(unsigned i=0;game.help()&&i<6;++i){game.inputOverlay(PointerPhase::begin,{200,200},300+i);game.inputOverlay(PointerPhase::end,{200,200},300+i);}
    assert(!game.help());
    auto* scene=game.battle();assert(scene&&!scene->isPaused());
    const auto writes=host.saves;
    for(unsigned cycle=0;cycle<10;++cycle){
        game.tick();const auto time=scene->clock().worldTime();const auto position=scene->player().position;const auto calls=random.calls();
        game.setActive(false);game.setActive(false);
        for(unsigned i=0;i<10;++i)game.tick();
        assert(scene->clock().worldTime()==time&&random.calls()==calls);
        game.setActive(true);game.setActive(true);
        if(!game.state().systemVisible||!scene->isPaused())std::cerr<<"LIFECYCLE cycle="<<cycle<<" screen="<<unsigned(game.state().screen)<<" phase="<<game.screenTransition().phase<<" hp="<<scene->playerHP()<<" help="<<bool(game.help())<<" inventory="<<game.state().inventoryVisible<<" system="<<game.state().systemVisible<<" paused="<<scene->isPaused()<<std::endl;
        assert(game.state().systemVisible&&scene->isPaused());
        for(unsigned i=0;i<5;++i)game.tick();
        assert(scene->clock().worldTime()==time&&scene->player().position.x==position.x&&scene->player().position.y==position.y);
        assert(!game.inputWorld(PointerPhase::begin,{400,300},90));
        press(game,1021);assert(!game.state().systemVisible&&!scene->isPaused());
        game.tick();assert(scene->clock().worldTime()>time);
    }
    assert(host.saves==writes);
    // An existing paused inventory must stay paused and accessible.
    press(game,1025);assert(game.state().inventoryVisible&&scene->isPaused());
    game.setActive(false);game.setActive(true);assert(game.state().inventoryVisible&&scene->isPaused());
    press(game,1025);assert(!scene->isPaused());
    std::cout<<"HM2_LIFECYCLE "<<(phone?"iphone":"ipad")<<" interruptions10 frozenWorld/menu/manualResume/idempotent/noSave/existingDialog PASS\n";
}
void settleDoors(GameSession& game){
    unsigned ticks=0;
    while(game.screenTransition().phase!=8){game.tick();(void)game.state();game.drawWorld();assert(++ticks<300);}
}
bool preparedContains(GameSession& game,std::string_view image){
    const auto frame=game.preparationFrame();
    return std::any_of(frame.sprites.begin(),frame.sprites.end(),[&](const auto& sprite){return sprite.image==image;});
}
void mapLoadingInterruption(Files& files,bool phone,int interruptStep){
    // Same original saved-stage/gate fixture as actualFieldGateAndMapLoading.
    // Enter through title/slots and use the real gate movement input.
    const auto models=Models::parse(Chunk::parse(files.read("PartsInfo.mcb")),Chunk::parse(files.read("MachInfo.mcb")));
    const auto weapons=Weapons::parse(DataTable::parse(files.read("WeaponInfo.tble")));
    Host host;DarwinWorldRandom random(1);SaveData save;save.initializeSlot(0,"LIFE-GATE","us",1794268800);
    progression::SlotView initial(save.slot(0));initial.string(0x2d,16,"Stage01_02");initial.u32(0x68,0);initial.hp(180);
    initial.u32(0x64c,50);initial.u32(0x1b60,1);initial.string(0x8c+8*6,6,"10011");initial.byte(0x53c+8,1);
    GameSession game(files,host,random,save,"us");
    game.setDisplay(DisplayConfiguration::surface(phone?Size{912,420}:Size{1024,1366},phone?DisplayInsets{68,20,68,0}:DisplayInsets{0,25,0,0}));
    game.action({ui::ScreenId::title,ui::Panel::root,{},true,true});settleDoors(game);
    assert(game.state().screen==ui::ScreenId::slots);
    game.action({ui::ScreenId::slots,ui::Panel::root,{ui::ButtonEventType::pop,{0,1001,0},1,{}}});settleDoors(game);
    assert(game.state().screen==ui::ScreenId::field&&game.battle()&&game.help());
    for(unsigned i=0;game.help()&&i<6;++i){game.inputOverlay(PointerPhase::begin,{200,200},400+i);game.inputOverlay(PointerPhase::end,{200,200},400+i);}
    assert(!game.help()&&!game.battle()->isPaused());
    const auto all=game.battle()->gates();const auto found=std::find_if(all.begin(),all.end(),[](const auto& gate){return gate.definition.number==0;});
    assert(found!=all.end()&&found->definition.linkMap=="Stage01_01"&&!found->definition.eventOnly);
    const auto gate=found->definition;auto* oldScene=game.battle();const auto* oldTile=oldScene->preparedTile("LinkGate.png");assert(oldTile);
    const auto point=game.worldCanvasPoint(oldScene->camera().toScreen(gate.position));
    assert(game.inputWorld(PointerPhase::begin,point,490));assert(game.inputWorld(PointerPhase::end,point,490));
    unsigned movement=0;while(!game.mapLoading()){game.tick();game.drawWorld();assert(++movement<300);}
    assert(game.battle()==oldScene&&oldScene->isPaused());
    const auto oldTime=oldScene->clock().worldTime();const auto oldResourceRevision=game.resourceRevision();
    const auto writesBefore=host.saves;
    bool retainedAtObjects=false,releasedAtTiles=false;
    auto inspectOwnership=[&]{
        const int step=game.mapLoading()->state().step;
        if(step==2){assert(!game.battle());assert(oldScene->preparedTile("LinkGate.png")==oldTile);assert(oldScene->clock().worldTime()==oldTime);assert(game.resourceRevision()==oldResourceRevision);assert(preparedContains(game,"LinkGate.png"));retainedAtObjects=true;}
        if(step==4){assert(!game.battle());assert(game.resourceRevision()==oldResourceRevision+1);assert(!preparedContains(game,"LinkGate.png"));releasedAtTiles=true;}
    };
    unsigned loading=0;
    while(game.mapLoading()->state().step!=interruptStep){inspectOwnership();game.tick();game.drawWorld();assert(++loading<300);}
    inspectOwnership();
    if(interruptStep<5)assert(!game.battle());else assert(game.battle());
    const auto renderTime=game.renderTime();const auto calls=random.calls();const auto reseeds=random.reseeds();const auto writes=host.saves;
    const auto interruptedTime=game.battle()?game.battle()->clock().worldTime():oldTime;
    game.setActive(false);game.setActive(false);
    for(unsigned i=0;i<10;++i)game.tick();
    assert(game.mapLoading()->state().step==interruptStep&&game.renderTime()==renderTime);
    assert(random.calls()==calls&&random.reseeds()==reseeds&&host.saves==writes);
    game.setActive(true);game.setActive(true);
    if(game.battle())assert(game.battle()->isPaused());
    const auto idleWeapons=[&](CombatScene& current){std::uint64_t count=0;for(const auto& enemy:current.enemies())if(enemy.hp>0&&!enemy.targetingPlayer&&!enemy.corpse)for(const auto& mount:models.modelMounts(enemy.name)){const auto& part=models.part(mount.part);if(part.type==4&&weapons.all().contains(part.name))++count;}return count;};
    std::uint64_t pausedNewCalls=0,pausedNewSpawns=0,pausedNewIdle=0;bool observedNew=false;
    std::map<std::uint64_t,Vec2> pausedPositions;
    while(game.mapLoading()){
        inspectOwnership();
        if(auto* current=game.battle();current&&game.mapLoading()->state().step>=5){
            assert(current->isPaused()&&current->clock().worldTime()==interruptedTime);
            std::uint64_t spawns=0;for(const auto& [kind,count]:current->metrics().enemySpawnsByKind){(void)kind;spawns+=count;}
            if(observedNew&&(spawns!=pausedNewSpawns||random.calls()-pausedNewCalls!=3*(spawns-pausedNewSpawns)+pausedNewIdle))std::cerr<<"PAUSED_INIT step="<<game.mapLoading()->state().step<<" worldTime="<<current->clock().worldTime()<<" rngDelta="<<random.calls()-pausedNewCalls<<" spawnDelta="<<spawns-pausedNewSpawns<<" emissions="<<current->metrics().emissions<<" damage="<<current->metrics().damageCalls<<std::endl;
            // Original paused child pass retains first-spawn construction
            // (3 RNG per enemy) and idle weapon sway (1 RNG per existing
            // untargeted weapon). It must not advance movement/fire/damage.
            if(observedNew)assert(random.calls()-pausedNewCalls==3*(spawns-pausedNewSpawns)+pausedNewIdle);
            observedNew=true;pausedNewCalls=random.calls();pausedNewSpawns=spawns;pausedNewIdle=idleWeapons(*current);
            for(const auto& enemy:current->enemies()){if(auto i=pausedPositions.find(enemy.id);i!=pausedPositions.end())assert(enemy.position.x==i->second.x&&enemy.position.y==i->second.y);pausedPositions[enemy.id]=enemy.position;}
            assert(current->metrics().emissions==0&&current->metrics().damageCalls==0);
        }
        game.tick();game.drawWorld();assert(++loading<600);
    }
    auto* current=game.battle();assert(current&&current->isPaused()&&game.state().systemVisible);
    assert(current->clock().worldTime()==interruptedTime&&retainedAtObjects&&releasedAtTiles);
    assert(host.saves==writesBefore+1); // Original stage4 save, no interruption save.
    const auto finalCalls=random.calls(),finalReseeds=random.reseeds();
    for(unsigned i=0;i<10;++i){game.tick();assert(current->clock().worldTime()==interruptedTime);}
    assert(random.calls()==finalCalls+10*idleWeapons(*current)&&random.reseeds()==finalReseeds&&host.saves==writesBefore+1);
    press(game,1021);assert(!game.state().systemVisible&&!current->isPaused());game.tick();assert(current->clock().worldTime()>interruptedTime);
    std::cout<<"HM2_LIFECYCLE_GATE "<<(phone?"iphone":"ipad")<<" interruptedStep="<<interruptStep<<" realGate/nullSceneOrFade/pendingPause/inactiveRNGFrozen/pausedOriginalInitAndIdleRNG/worldFrozen/emissions0/damage0/explicitResume/oldTilesPhase2-retained-phase4-released PASS\n";
}
}
int main(int argc,char** argv){if(argc<2)return 77;Files files;files.root=argv[1];check(files,true);check(files,false);for(bool phone:{true,false})for(int step:{2,4,5})mapLoadingInterruption(files,phone,step);}

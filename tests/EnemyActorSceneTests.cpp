#include "PreparedTextStub.hpp"
#include "Core/Simulation/CombatScene.hpp"
#include "Core/Session/GameSession.hpp"
#include "Core/Data/Weapons.hpp"
#include "Core/Progression/Progression.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
using namespace hm2;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;std::map<std::string,Bytes,std::less<>> cache;std::uint64_t reads=0;mutable std::uint64_t probes=0;
    explicit Files(const char* p):root(p){}
    Bytes read(std::string_view name)override{++reads;const auto found=cache.find(name);if(found!=cache.end())return found->second;std::ifstream f(root/std::string(name),std::ios::binary);if(!f)throw std::runtime_error("Missing original Scene asset "+std::string(name));Bytes data{std::istreambuf_iterator<char>(f),{}};cache.emplace(std::string(name),data);return data;}
    bool exists(std::string_view name)const override{++probes;return cache.find(name)!=cache.end()||std::filesystem::exists(root/std::string(name));}
};
struct Hooks:CombatSceneHooks {
    ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& r)override{return test::preparedTextStub(r);}
    void sound(std::string_view,Vec2)override{}void soundID(unsigned,Vec2)override{}
    void dropEnemyLoot(const MachineInfo&,Vec2,double,float,WorldRandom&)override{}
    void rewardEnemyKill(const MachineInfo&,Vec2,double,float,WorldRandom&)override{}
    int playerDamage(int value,WorldRandom&)override{return value;}int damageBonus()const override{return 0;}
};
void allFields(Files& files,const Maps& maps,const Machines& machines){
    unsigned fields=0,definitions=0,actualSpawns=0;std::map<MachineKind,unsigned> kinds;
    for(const auto& [name,map]:maps.all()){for(const auto& s:map.spawners){++definitions;++kinds[machines.machine(s.model).kind];}if(!name.starts_with("Stage")){assert(map.spawners.empty());continue;}
        assert(!map.gates.empty());CombatSetup setup;setup.map=name;setup.gate=map.gates.front().number;setup.offsetInsideGate=true;setup.randomSeed=37;Hooks hooks;CombatScene scene(files,hooks,setup);const auto reads=files.reads,probes=files.probes;assert(scene.spawnerCount()==map.spawners.size());
        const auto position=scene.player().position;std::map<MachineKind,unsigned> expected;
        for(const auto& s:map.spawners)if(std::abs(s.position.x-position.x)<1024&&std::abs(s.position.y-position.y)<1024)++expected[machines.machine(s.model).kind];
        scene.tick();const auto spawned=scene.enemies();for(const auto& e:spawned){assert(e.kind==machines.machine(e.name).kind&&e.boss==e.name.starts_with("Boss"));assert(e.hp==machines.machine(e.name).hp&&!e.corpse);}
        for(const auto kind:{MachineKind::vehicle,MachineKind::turret,MachineKind::aircraft})assert(scene.metrics().enemySpawnsByKind.at(kind)==expected[kind]);
        assert(scene.metrics().enemyTicks==0&&scene.metrics().randomCalls==64+3*spawned.size());actualSpawns+=unsigned(spawned.size());
        for(unsigned i=0;i<3;++i){scene.tick();assert(!scene.drawFrame().sprites.empty());}
        assert(files.reads==reads&&files.probes==probes);++fields;
        std::cout<<"FIELD "<<name<<" spawnerDefinitions="<<map.spawners.size()<<" actualFirstPass="<<spawned.size()<<" warmIO=0\n";
    }
    assert(fields==38&&definitions==695&&kinds[MachineKind::vehicle]==385&&kinds[MachineKind::turret]==307&&kinds[MachineKind::aircraft]==3&&kinds[MachineKind::bipod]==0&&kinds[MachineKind::train]==0);
    std::cout<<"FIELDS PASS actualEntries="<<fields<<" mapDefinitions=43 spawnerDefinitions="<<definitions<<" actualSpawns="<<actualSpawns<<" vehicle385/turret307/aircraft3\n";
}
struct Host:SessionHost {
    ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& r)override{return test::preparedTextStub(r);}
    std::vector<std::string> sounds,alerts;unsigned writes=0;
    void sound(std::string_view n,Vec2)override{sounds.emplace_back(n);}void soundID(unsigned id,Vec2)override{sounds.push_back("sfx:"+std::to_string(id));}
    void alert(std::string_view n)override{alerts.emplace_back(n);}void requestName(unsigned)override{}void requestDelete(unsigned)override{}
    bool writeSave(std::span<const std::uint8_t>,SaveReason)override{++writes;return true;}
    std::uint32_t absoluteTimeLow()override{return 731;}std::uint32_t unixTime()override{return 1794268800;}
};
std::string bestItem(const DataTable& table,std::string_view type,std::string_view param){std::string code;float best=-1;for(std::size_t row=0;row<table.rowCount();++row)if(table.value(row,"Type")==type&&table.real(row,param)>best){best=table.real(row,param);code=table.value(row,"Code").value();}assert(!code.empty());return code;}
std::string bestWeapon(const DataTable& table,const Weapons& weapons,bool missile){std::string code;float best=-1;for(std::size_t row=0;row<table.rowCount();++row){if(table.value(row,"Type")!="PtWpn")continue;const auto& weapon=weapons.weapon(table.value(row,"strParam").value());if(missile?weapon.kind!=WeaponKind::missile:(weapon.kind!=WeaponKind::gun&&weapon.kind!=WeaponKind::sniper))continue;const float score=float(weapon.damage)*weapon.shotCount/std::max(.01f,weapon.reloadDelay);if(score>best){best=score;code=table.value(row,"Code").value();}}assert(!code.empty());return code;}
SaveData highLevel(const DataTable& table,const Weapons& weapons){
    SaveData saved;saved.initializeSlot(0,"EnemyActorScene","us",1794268800);progression::SlotView slot(saved.slot(0));slot.level(99);slot.experience(progression::Experience::bounds(99).previous);
    const std::string equipped[]={bestItem(table,"PtBody","Param1"),bestItem(table,"PtBtm","Param3"),bestWeapon(table,weapons,false),bestWeapon(table,weapons,true),bestItem(table,"PtArmor","Param1")};
    for(unsigned i=0;i<5;++i){slot.string(0x8c+i*6,6,equipped[i]);slot.byte(0x53c+i,1);slot.u32(0x604+i*4,i);}slot.u32(0x604+5*4,std::uint32_t(-1));
    slot.string(0x8c+8*6,6,"01006");slot.byte(0x53c+8,200);slot.string(0x61c,6,"01006");slot.string(0x8c+9*6,6,"02007");slot.byte(0x53c+9,200);slot.string(0x61c+6,6,"02007");
    std::cout<<"START validLv99 equipment=";for(const auto& c:equipped)std::cout<<c<<",";std::cout<<" realAidKit01006=200 realNuclear02007=200\n";return saved;
}
void quick(GameSession& game,unsigned slot,Vec2 world){game.action({ui::ScreenId::field,ui::Panel::root,{ui::ButtonEventType::push,{0,6200+slot,0},1,{}}});const auto screen=game.battle()->camera().toScreen(world);assert(game.inputWorld(PointerPhase::begin,screen,100+slot));(void)game.inputWorld(PointerPhase::end,screen,100+slot);}
void heal(GameSession& game){if(game.battle()->playerHP()>0&&game.state().hpRate<.65f)quick(game,0,game.battle()->player().position);}
void battle(Files& files,const DataTable& items,const Weapons& weapons,std::string_view map,bool aircraft){
    Host host;DarwinWorldRandom random(731);GameSession game(files,host,random,highLevel(items,weapons));assert(game.selectSlot(0));game.enterBase();assert(game.enterField(map,0));(void)game.state();const auto reads=files.reads,probes=files.probes;const auto writes=host.writes;
    const auto xpAt=[&]{auto s=game.workingSlot();return std::uint32_t(s[0x70])|(std::uint32_t(s[0x71])<<8)|(std::uint32_t(s[0x72])<<16)|(std::uint32_t(s[0x73])<<24);};const auto xpBefore=xpAt();
    game.tick();game.drawWorld();auto* scene=game.battle();const MachineKind required=aircraft?MachineKind::aircraft:MachineKind::turret;
    auto findLiving=[&](MachineKind kind)->CombatEnemySnapshot{for(const auto& e:scene->enemies())if(e.kind==kind&&e.hp>0)return e;assert(false);return {};};
    const auto target=findLiving(required);if(aircraft){assert(target.boss);quick(game,1,target.position);}else{
        // Walk to unoccupied ground near the real turret, without selecting it
        // yet, so its original attack AI can acquire/fire before Player kills it.
        const auto point=scene->camera().toScreen({target.position.x+65,target.position.y});assert(game.inputWorld(PointerPhase::begin,point,220));(void)game.inputWorld(PointerPhase::end,point,220);
    }
    bool enemyFired=false,retargetObserved=false;unsigned frames=1;std::uint64_t playerHPLost=0;
    // The original default Picker can deliver a drop before corpse removal.
    // Observe creation throughout the battle, independently of its lifetime.
    std::size_t maximumDrops=game.fieldItems().size();
    for(;frames<3000&&!enemyFired;++frames){heal(game);const int hp=scene->playerHP();game.tick();game.drawWorld();if(hp>scene->playerHP())playerHPLost+=std::uint64_t(hp-scene->playerHP());assert(scene->playerHP()>0);
        maximumDrops=std::max(maximumDrops,game.fieldItems().size());
        for(const auto& e:scene->enemies())if(e.id==target.id&&e.targetingPlayer){if(aircraft)assert(!e.moveToTarget);retargetObserved=true;}
        enemyFired=scene->metrics().enemyEmissionsByKind.at(required)>0;
    }
    std::cout<<"BATTLE acquire "<<map<<" frames="<<frames<<" enemyFired="<<enemyFired<<" playerHP="<<scene->playerHP()<<" target="<<target.name<<" kind="<<int(required)<<"\n";assert(enemyFired&&retargetObserved);
    assert(scene->selectEnemy(target.id)&&scene->selectEnemy(target.id));bool corpse=false,removed=false;std::uint64_t removedID=target.id;
    for(;frames<9000&&!removed;++frames){heal(game);const int hp=scene->playerHP();game.tick();const auto frame=game.drawWorld();assert(!frame.sprites.empty());if(hp>scene->playerHP())playerHPLost+=std::uint64_t(hp-scene->playerHP());assert(scene->playerHP()>0);
        maximumDrops=std::max(maximumDrops,game.fieldItems().size());
        bool found=false;for(const auto& e:scene->enemies())if(e.id==removedID){found=true;if(e.hp==0){corpse=true;assert(e.corpse&&e.activeParts==1&&e.targetingPlayer);if(aircraft)assert(!e.moveToTarget);else assert(e.moveToTarget);}}
        if(corpse&&!found)removed=true;
    }
    const auto& m=scene->metrics();std::cout<<"BATTLE final "<<map<<" frames="<<frames<<" enemyEmissions="<<m.enemyEmissionsByKind.at(required)<<" damage="<<m.enemyDamageCallsByKind.at(required)<<" kills="<<m.enemyKillsByKind.at(required)<<" removed="<<m.enemyRemovalsByKind.at(required)<<" bullets="<<m.projectilesCreated<<" playerHPLost="<<playerHPLost<<" corpse="<<corpse<<" xp="<<xpAt()<<" drops="<<game.fieldItems().size()<<std::endl;
    std::cout<<"BATTLE drops maxObserved="<<maximumDrops<<" botCaptures="<<m.botItemCaptures<<"\n";
    assert(corpse&&removed&&m.enemyDamageCallsByKind.at(required)>0&&m.enemyKillsByKind.at(required)>0&&m.enemyRemovalsByKind.at(required)>0&&m.projectilesCreated>0&&m.playerEmissions>0&&xpAt()>xpBefore&&maximumDrops>0);
    std::cout<<"BATTLE warm "<<map<<" readDelta="<<files.reads-reads<<" existsDelta="<<files.probes-probes<<" saveDelta="<<host.writes-writes<<std::endl;assert(files.reads==reads&&files.probes==probes&&host.writes==writes);
}
}
int main(int argc,char** argv){if(argc<2)return 77;Files files(argv[1]);const auto maps=Maps::parse(Chunk::parse(files.read("MapInfo.mcb")));const auto machines=Machines::parse(DataTable::parse(files.read("MachList.tble")));allFields(files,maps,machines);const auto items=DataTable::parse(files.read("ItemInfo.tble"));const auto weapons=Weapons::parse(DataTable::parse(files.read("WeaponInfo.tble")));battle(files,items,weapons,"Stage01_02",false);battle(files,items,weapons,"Stage02_07",true);battle(files,items,weapons,"Stage03_04",true);std::cout<<"HM2_ENEMY_ACTOR_SCENE PASS 38actualfields/original695definitions commonvehicle-turret-aircraft realemit/projectile/damage/corpse/removal/reward/drop warmIO0 noFrameSave\n";}

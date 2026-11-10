#include "Core/Progression/Progression.hpp"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
using namespace hm2;using namespace hm2::progression;
namespace {
std::vector<std::uint8_t> read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("Missing original table: "+p.string());return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
struct Random:WorldRandom {std::vector<std::uint32_t> values;std::size_t calls=0;Random(std::initializer_list<std::uint32_t> v={}):values(v){}std::uint32_t random()override{const auto value=calls<values.size()?values[calls]:0;++calls;return value;}};
struct Log:Events {
    std::vector<std::string> calls;SlotView* slot=nullptr;unsigned levels=0,alerts=0,quick=0;bool lastPickup=true;
    void changed(Change c)override{calls.push_back("dirty"+std::to_string(int(c)));}
    void sound(unsigned id)override{calls.push_back("sound"+std::to_string(id));}
    void levelUp()override{++levels;calls.push_back("level");if(slot)slot->hp(slot->maxHP());}
    void experienceGauge(float)override{calls.push_back("gauge");}
    void missionUpdated(const MissionDefinition&,const MissionState& s)override{calls.push_back(s.requireComplete?"missionReady":"missionProgress");}
    void missionReward(const MissionDefinition&,const Action& a)override{calls.push_back("reward"+std::to_string(a.type));}
    void pickupBegin(const Item&)override{calls.push_back("pickupBegin");}
    void pickupText(const Item&,int,bool success,double)override{calls.push_back("pickupText");lastPickup=success;}
    void quickItemChanged(const Item&)override{++quick;calls.push_back("quick");}
    void alert(std::string_view)override{++alerts;calls.push_back("alert");}
};
std::array<std::uint8_t,SlotView::size> slotBytes(){
    std::array<std::uint8_t,SlotView::size> bytes{};bytes.fill(0xa5);SlotView s(bytes);s.level(0);s.experience(0);s.credit(500);s.hp(256);s.maxHP(256);s.experienceGauge(0);s.u32(0x88,3);s.u16(0x658,0);s.u16(0x65a,0);
    for(unsigned i=0;i<200;++i){s.byte(0x8c+i*6,0);s.byte(0x53c+i,0);}for(unsigned i=0;i<6;++i)s.u32(0x604+i*4,std::uint32_t(-1));return bytes;
}
void experience(){
    assert(originalSoundName(3)=="Game_GetItem.wav"&&originalSoundName(7)=="Game_LevelUp.wav"&&originalSoundName(11)=="SFX_MissionComplete.wav"&&originalSoundName(22).empty());
    const auto b0=Experience::bounds(0),b1=Experience::bounds(1),b40=Experience::bounds(40),b41=Experience::bounds(41),b99=Experience::bounds(99);
    assert(b0.previous==0&&b0.next==300);assert(b1.previous==300&&b1.next==747);
    assert(b40.previous==7193178&&b40.next==8829964);assert(b41.previous==8829964&&b41.next==10829964);assert(b99.previous==620729964&&b99.next==640129964);
    auto bytes=slotBytes();SlotView s(bytes);Log log;log.slot=&s;Experience exp(s,log);
    assert(!exp.add(299,7)&&s.level()==0&&s.experience()==299);assert(exp.add(1,7)&&s.level()==1&&s.maxHP()==276&&s.hp()==276&&s.experienceGauge()==0);assert(log.levels==1);
    log.calls.clear();assert(exp.add(1000000,7)&&s.level()==2&&s.experienceGauge()==1);assert((log.calls==std::vector<std::string>{"level","sound7","gauge","dirty0"}));
    assert(exp.add(0,1)&&s.level()==3&&log.levels==2); // No while; another call can advance once.
    s.level(99);s.experience(640129964);assert(!exp.add(1,7)&&s.level()==99&&s.experienceGauge()==1);
    assert(bytes[0x2310]==0xa5);std::cout<<"EXP thresholds/equality/one-level-per-call/screen99 boundaries PASS\n";
}
void dropsAndPickup(const Definitions& defs,const Machines& machines){
    const auto& machine=machines.machine("E_Tank_01");ItemSpawner spawner(defs);assert(machine.experience==20&&machine.coin==3&&machine.coinRate==100);
    Random first{0,0,0,0,0};std::vector<FieldItem> drops;spawner.drop(machine,{0,0},first,[&](auto item){drops.push_back(item);});assert(first.calls==5&&drops.size()==1&&drops[0].item->code=="00001"&&drops[0].value==3&&drops[0].velocity.x==3&&drops[0].velocity.y==0);
    Random second{0,0,0,0,1,1300,0,0};std::vector<FieldItem> extra;spawner.drop(machine,{0,0},second,[&](auto item){extra.push_back(item);});assert(second.calls==8&&extra.size()==2&&extra[1].item->code=="05025"&&extra[1].rootUsesAtlas);
    Random upper{0,0x7fffffff,0,0,0};std::vector<FieldItem> rounded;spawner.drop(machine,{0,0},upper,[&](auto item){rounded.push_back(item);});assert(rounded.size()==1&&rounded[0].value==4); // int31->float can round U to1.
    for(const auto& [n,id]:std::vector<std::pair<unsigned,std::string>>{{0,"05001"},{999,"05001"},{1000,"05002"},{1299,"05002"},{1300,"05025"},{2299,"05025"},{2300,"01001"},{2399,"01001"}}){Random r{n};assert(spawner.spawnItem("Tank_L01",r)->code==id&&r.calls==1);}
    Random noGroup;assert(spawner.spawnCount("NULL",noGroup)==0&&noGroup.calls==0);Random fixed{123};assert(spawner.spawnCount("Tank_L03",fixed)==1&&fixed.calls==1);
    auto bytes=slotBytes();SlotView slot(bytes);Log log;Progression p(slot,defs,log);p.restore();assert(slot.credit()==500);
    auto f=drops.front();f.velocity={};Random ticks;
    f.tick(double(.2f),{0,0},{},ticks,nullptr,[&](const auto& item,int value){p.pickup(item,value,double(.2f));});assert(!f.removed&&slot.credit()==500&&ticks.calls==2);
    f.tick(.21,{30,0},{},ticks,nullptr,[&](const auto& item,int value){p.pickup(item,value,.21);});assert(!f.removed&&f.checkTime==float(.21)&&ticks.calls==4);
    // Check interval updated on a failed distance check; strict .2 gate persists.
    f.tick(.22,{0,0},{},ticks,nullptr,[&](const auto& item,int value){p.pickup(item,value,.22);});assert(!f.removed&&slot.credit()==500);
    f.tick(.42,{29.999f,29.999f},{},ticks,nullptr,[&](const auto& item,int value){p.pickup(item,value,.42);});assert(f.removed&&slot.credit()==503&&ticks.calls==8);
    const auto count=ticks.calls;f.tick(1,{0,0},{},ticks,nullptr,[&](const auto&,int){assert(false);});assert(ticks.calls==count);
    assert((log.calls==std::vector<std::string>{"pickupBegin","dirty1","pickupText","sound3"}));
    std::cout<<"Tank_L01 weighted boundaries/shared RNG and field rectangular pickup PASS\n";
}
void fieldMotionAndBots(const Definitions& defs){
    const auto* coin=defs.findItem("00001");assert(coin);Random random;auto f=FieldItem::create(*coin,3,{0,0},random);assert(random.calls==2);
    f.tick(.01,{100,100},{},random,nullptr,[](const auto&,int){assert(false);});assert(std::abs(f.velocity.x-18.f/7)<.000001f&&f.position.x==f.velocity.x&&random.calls==4);
    f.position={};f.velocity={};std::array<PickupBot,2> bots{{{1,{0,0}},{2,{20,0}}}};unsigned claims=0;
    f.tick(.3,{100,100},bots,random,nullptr,[](const auto&,int){assert(false);},[&](auto){++claims;});assert(!f.pickerBot&&claims==0);
    bots[1].position={19.999f,19.999f};f.tick(.6,{100,100},bots,random,nullptr,[](const auto&,int){assert(false);},[&](auto i){assert(i==1);++claims;});assert(f.pickerBot&&claims==1);
    f.tick(.616,{100,100},bots,random,nullptr,[](const auto&,int){assert(false);});assert(f.velocity.x==.5f&&f.velocity.y==.5f&&f.position.x==.5f&&f.position.y==.5f&&f.speed==.001f);assert(f.scale>=.49f&&f.scale<=.51f&&f.alpha==.2f);
    Map map;map.collisionPolygons={{{0,0},{100,0}}};TerrainCollision terrain(map);auto collision=FieldItem::create(*coin,3,{50,5},random);collision.velocity={.7f,0};
    collision.tick(.01,{1000,1000},{},random,&terrain,[](const auto&,int){assert(false);});assert(std::abs(collision.position.x-50.6f)<.00001f&&collision.position.y==16); // Reverse edge order would end at y4.
    std::cout<<"Field inertia/div7/picker type2/strict20/homing/visual RNG PASS\n";
}
void missionAndReward(const Definitions& defs){
    auto bytes=slotBytes();SlotView slot(bytes);slot.string(0x8c+5*6,6,"01001");slot.byte(0x53c+5,5);Log log;log.slot=&slot;Progression p(slot,defs,log);p.restore();
    assert(defs.missions().size()==60&&defs.items().size()==255);assert(p.missions.available("M001010","Base001",0));assert(!p.missions.available("M001020","Base001",0));
    assert(p.acceptMission("M001010","Base001"));assert(!p.acceptMission("M001010","Base001"));assert(!p.claimMission("M001010","Base001"));
    p.killed("E_Tank_02",0,"Stage01_01");assert(p.missions.state("M001010")->did[0]==0);
    for(int i=0;i<5;++i)p.killed("E_Tank_01",20,"unrelated-map"); // kill's Place argument is unused in HD.
    const auto* state=p.missions.state("M001010");assert(state->did[0]==5&&state->requireComplete&&!state->complete);assert(slot.experience()==100&&slot.credit()==500&&slot.level()==0);
    assert(std::count(log.calls.begin(),log.calls.end(),"missionProgress")==4&&std::count(log.calls.begin(),log.calls.end(),"missionReady")==1);
    p.killed("E_Tank_01",20,"Stage01_01");assert(state->did[0]==5&&slot.experience()==120);assert(!p.claimMission("M001010","Base002"));
    log.calls.clear();assert(p.claimMission("M001010","Base001"));assert(slot.credit()==600&&slot.experience()==320&&slot.level()==1&&slot.hp()==276);assert(p.inventory.count(*defs.findItem("01001"))==8);
    const auto find=[&](const char* name){return std::find(log.calls.begin(),log.calls.end(),name)-log.calls.begin();};assert(find("reward0")<find("reward1")&&find("reward1")<find("reward2"));assert(find("level")<find("sound7")&&find("sound7")<find("gauge"));
    assert(state->complete&&state->accepted&&p.missions.acceptedCount()==0&&p.missions.available("M001020","Base001",1));assert(!p.claimMission("M001010","Base001"));
    p.prepareSave();assert(slot.u16(0x658)==60&&slot.u16(0x65a)==1&&bytes[0x2310]==0xa5);
    auto restoredBytes=bytes;SlotView restoredSlot(restoredBytes);Log restoredLog;Progression restored(restoredSlot,defs,restoredLog);restored.restore();assert(restored.missions.state("M001010")->complete&&restored.missions.acceptedCount()==0);assert(!restored.claimMission("M001010","Base001"));assert(restoredSlot.credit()==600&&restoredSlot.experience()==320&&restored.inventory.count(*defs.findItem("01001"))==8);
    // Original restore uses LDRSB for the ten char progress bytes.
    auto signedBytes=slotBytes();SlotView signedSlot(signedBytes);signedSlot.u16(0x658,1);signedSlot.string(0x65c,8,"M001010");signedSlot.byte(0x664,1);signedSlot.byte(0x665,200);signedSlot.byte(0x670,0);
    Log signedLog;Progression signedProgress(signedSlot,defs,signedLog);signedProgress.restore();assert(signedProgress.missions.state("M001010")->did[0]==-56&&!signedProgress.missions.state("M001010")->requireComplete);
    std::cout<<"Mission kill/filter/cap/base claim/action order/original slot round-trip PASS\n";
}
void inventoryFailures(const Definitions& defs){
    const auto* part=defs.findItem("10011");const auto* kit=defs.findItem("01001");assert(part&&!part->countable&&kit&&kit->countable);
    auto bytes=slotBytes();SlotView slot(bytes);slot.u32(0x88,1);for(int i=0;i<23;++i){slot.string(0x8c+i*6,6,part->code);slot.byte(0x53c+i,1);}Log log;Progression p(slot,defs,log);p.restore();
    assert(p.inventory.emptySlots()==2);assert(!p.pickup(*part,3,.5));assert(p.inventory.count(*part)==25&&log.alerts==1&&!log.lastPickup&&p.inventory.emptySlots()==0);assert(log.calls.back()=="sound3");
    Random create;FieldItem f=FieldItem::create(*kit,1,{0,0},create);
    f.velocity={};Random ticks;f.tick(1,{0,0},{},ticks,nullptr,[&](const auto& item,int value){assert(!p.pickup(item,value,1));});assert(f.removed&&log.alerts==2);
    auto stackBytes=slotBytes();SlotView stackSlot(stackBytes);stackSlot.string(0x8c,6,kit->code);stackSlot.byte(0x53c,200);Log stackLog;Progression stacks(stackSlot,defs,stackLog);stacks.restore();assert(stacks.pickup(*kit,100,1));assert(stacks.inventory.count(*kit)==300&&stackSlot.byte(0x53c)==200&&stackLog.quick==0);
    stacks.inventory.quickSlot(0,kit);assert(stacks.pickup(*kit,1,1.1)&&stackLog.quick==1);stacks.prepareSave();assert(stacks.inventory.count(*kit)==255&&stackSlot.byte(0x53c)==255&&stackSlot.string(0x61c,6)==kit->code);
    std::cout<<"Inventory partial failure/pickup removal/deferred255 save cap PASS\n";
}
}
int main(int argc,char** argv){
    experience();if(argc<2)return 0;const std::filesystem::path root=argv[1];auto table=[&](std::string_view name){return DataTable::parse(read(root/name));};
    const auto defs=Definitions::parse(table("ItemInfo.tble"),table("ItemRegen.tble"),table("MissionList.tble"),table,"us");
    // Verify the literal runtime path, not Ghidra's sanitized CFString symbol.
    assert(defs.findItem("01001")->iconImage()=="Icon01001.png");
    for(const auto& [code,item]:defs.items()){(void)code;assert(std::filesystem::is_regular_file(root/item.iconImage()));}
    const auto machines=Machines::parse(table("MachList.tble"));dropsAndPickup(defs,machines);fieldMotionAndBots(defs);missionAndReward(defs);inventoryFailures(defs);
    std::cout<<"HM2_PROGRESSION originalItems="<<defs.items().size()<<" missions="<<defs.missions().size()<<" PASS (memory only; no persistence writes)\n";
}

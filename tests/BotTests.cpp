#include "Core/Simulation/Bot.hpp"
#include "Core/Simulation/GameClock.hpp"
#include "Core/Simulation/WeaponEffects.hpp"
#include "Core/Progression/Progression.hpp"
#include "Core/Presentation/PreparedTiles.hpp"
#include "Core/Data/SaveData.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>

using namespace hm2;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;unsigned reads=0;mutable unsigned probes=0;
    explicit Files(const char* p):root(p){}
    Bytes read(std::string_view name)override{++reads;std::ifstream f(root/std::string(name),std::ios::binary);if(!f)throw std::runtime_error("Missing original BOT resource "+std::string(name));return {std::istreambuf_iterator<char>(f),{}};}
    bool exists(std::string_view name)const override{++probes;return std::filesystem::exists(root/std::string(name));}
};
struct Catalog {
    Models models;Machines machines;Weapons weapons;DataTable items;
    explicit Catalog(Files& f):models(Models::parse(Chunk::parse(f.read("PartsInfo.mcb")),Chunk::parse(f.read("MachInfo.mcb")))),machines(Machines::parse(DataTable::parse(f.read("MachList.tble")))),weapons(Weapons::parse(DataTable::parse(f.read("WeaponInfo.tble")))),items(DataTable::parse(f.read("ItemInfo.tble"))){}
};
struct Host:BotHost,WeaponEffectsHost {
    PreparedTiles& tiles;std::uint64_t calls=0;std::deque<std::uint32_t> sequence;
    std::vector<std::string> events;std::vector<unsigned> sfx;std::vector<int> damageAmounts;
    WeaponTarget playerValue{1,{0,0},100000};std::map<std::uint64_t,WeaponTarget> targets{{9,{9,{100,0},100000}}};std::uint64_t selected=0,emittingTarget=0;
    std::vector<BotItemTarget> itemList;std::array<std::uint8_t,SaveData::slotSize> bytes{};progression::SlotView slot{bytes};
    ParticlePool pool;WeaponEffects effects;double time=0;float renderTime=0;
    Host(PreparedTiles& t):tiles(t),pool(0,*this),effects(pool,{t.lookup("Efx_Fire.png"),t.lookup("Efx_Damage.png"),t.lookup("BulletShell.png"),t.lookup("Efx_Trail04.png"),t.lookup("Efx_Trail02.png")}){events.clear();}
    std::uint32_t random()override{++calls;events.push_back("R");if(sequence.empty())return 0x40000000u;const auto value=sequence.front();sequence.pop_front();return value;}
    const WeaponTarget* player()const override{return &playerValue;}
    const WeaponTarget* target(std::uint64_t id)const override{const auto found=targets.find(id);return found==targets.end()?nullptr:&found->second;}
    const WeaponTarget* playerTarget()const override{return selected?target(selected):nullptr;}
    const WeaponTarget* target()const override{return target(emittingTarget);}
    std::span<const BotItemTarget> items()const override{return itemList;}
    void emit(const WeaponInfo& info,const WeaponEmission& shot,const BotEmission& context)override{
        assert(context.owner&&!context.hostile&&!context.playerShooter);emittingTarget=context.owner->state().target;events.push_back("emit");
        WeaponEffectInput input;input.worldTime=time;input.renderTime=renderTime;input.machinePosition=context.machinePosition;input.partCenter=context.partCenter;input.playerPosition=playerValue.position;
        input.hostile=context.hostile;input.playerShooter=context.playerShooter;input.damageBonus=77;effects.emit(info,shot,input,*this,*this);
    }
    void fireSound(const WeaponInfo& info,Vec2)override{assert(info.fireSound=="Wpn_Gun01.wav");events.push_back("fireSound");}
    TileImage* preparedTile(std::string_view name)override{return tiles.lookup(name);}
    void damage(const WeaponInfo&,std::uint64_t id,int amount)override{auto& target=targets.at(id);damageAmounts.push_back(amount);target.hp=std::max(0,target.hp-amount);}
    void attachProjectile(WeaponProjectile)override{throw std::logic_error("Actual six registered BOTs have no projectile weapon");}
    void writeSlotHP(unsigned i,int hp)override{slot.u32(0x64c+i*4,std::uint32_t(hp));events.push_back("hp:"+std::to_string(i)+":"+std::to_string(hp));}
    void clearSlotCode(unsigned i)override{slot.byte(0x63a+i*6,0);events.push_back("clear:"+std::to_string(i));}
    void unselectTarget(std::uint64_t id)override{events.push_back("unselect:"+std::to_string(id));}
    void baseCrash(const Bot& actor)override{
        events.push_back("base21");sfx.push_back(21);events.push_back("pool");
        if(auto* p=pool.allocate(renderTime,*this)){auto& s=p->state();s.blendType=0;s.life=1;s.tile=tiles.lookup("Efx_ExplodeNPC.png");s.position=actor.motion().position;s.scale={1,1};p->animate(16);p->start(renderTime);}
    }
    void hitSound(const Bot&)override{sfx.push_back(15+random()%3);events.push_back("hit");}
    void begin(double t){time=t;renderTime=float(t);events.clear();}
};
void close(float a,float b){assert(std::abs(a-b)<.00001f);}
void spawnAndModels(const Catalog& data,PreparedTiles& tiles,Files& files){
    unsigned inventoryBots=0,machineBots=0;
    for(std::size_t row=0;row<data.items.rowCount();++row)if(data.items.value(row,"Type")=="Bot"){
        ++inventoryBots;const auto name=data.items.value(row,"strParam").value();const auto& info=data.machines.machine(name);Host host(tiles);
        const auto before=host.calls;const float x=10+(randomUnit(host)*100.f-50.f),y=20+(randomUnit(host)*100.f-50.f);
        Bot actor(data.models,data.weapons,info,{x,y},7.25,host);assert(host.calls==before+4&&actor.state().hp==info.hp&&actor.motion().moveSpeed==2&&actor.motion().rotationSpeed==15);
        assert(actor.state().createTime==7.25f&&actor.state().stopDelay==8.25f&&actor.state().liveTime==12&&actor.state().lastAttackTime==0&&actor.state().attackDelay==1);
        assert(actor.motion().position.x==10&&actor.motion().position.y==20&&actor.motion().state==0&&actor.gauge().scaleX==.5f&&actor.gauge().x==-1);
        assert(actor.mounts().size()==(info.referenceType==0?3:2)&&actor.weapons().size()==(info.referenceType==0?1:0));
        unsigned rotors=0;for(const auto& m:actor.mounts()){const auto& part=data.models.part(m.part);assert(m.layer==(m.parent<0?5u:6u));assert(files.exists(part.tileCount?part.name+".png":"PartsDummy.png"));if(part.type==7)++rotors;}
        assert(rotors==1&&actor.poses().size()==actor.mounts().size());
        for(const auto& w:actor.weapons())assert(w.currentMuzzle==1&&w.aim.state().startTime==0&&w.controller.state().fireTime==0&&w.controller.state().fireLeft==0);
        for(unsigned slot=0;slot<3;++slot){actor.setSlotIndex(slot);actor.setHP(info.hp/2);actor.refreshHPGauge(host);assert(host.slot.u32(0x64c+slot*4)==unsigned(info.hp/2));assert(actor.gauge().scaleX==.25f&&actor.gauge().x==-5.25f);}
    }
    for(const auto& [id,info]:data.machines.all())if(id.starts_with("Bot_")){++machineBots;Host host(tiles);Bot actor(data.models,data.weapons,info,{},0,host);assert(actor.info().kind==MachineKind::aircraft);}
    assert(inventoryBots==6&&machineBots==11);
}
void wander(const Catalog& data,PreparedTiles& tiles){
    Host host(tiles);host.sequence={0,0};Bot actor(data.models,data.weapons,data.machines.machine("Bot_D01"),{},0,host);const auto initial=host.calls;
    host.playerValue.position={50,0};host.begin(1);actor.tick({1,false},host);
    assert(host.calls==initial+2&&actor.metrics().wanderAttempts==1&&actor.metrics().wanderAccepted==0&&actor.motion().state==0);
    host.playerValue.position.x=std::nextafter(50.f,100.f);host.begin(1);actor.tick({1,false},host);
    assert(host.calls==initial+5&&actor.metrics().wanderAccepted==1&&actor.motion().state==1);close(actor.motion().speed,.02f);
    assert(actor.state().stopDelay==float(1.+double(.5f*.3f)+double(.3f))&&actor.state().jobCheckTime==0);
    const auto attempts=actor.metrics().wanderAttempts;const auto calls=host.calls;const double deadline=actor.state().stopDelay;host.begin(deadline);actor.tick({deadline,false},host);assert(actor.metrics().wanderAttempts==attempts&&host.calls==calls);
    host.begin(std::nextafter(deadline,100.));actor.tick({host.time,false},host);assert(actor.metrics().wanderAttempts==attempts+1);
    host.begin(1000);actor.tick({1000,false},host);assert(!actor.state().removed&&actor.metrics().deaths==0); // liveTime12 has no runtime expiry consumer.
}
void picker(const Catalog& data,PreparedTiles& tiles,const progression::Definitions& definitions){
    Host host(tiles);host.sequence={0,0};Bot actor(data.models,data.weapons,data.machines.machine("Bot_P01"),{},0,host);
    host.itemList={{11,{100,0}},{22,{-100,0}},{33,{400,0}}};const auto before=host.calls;host.begin(0);actor.tick({0,false},host);
    assert(actor.state().pickItem==11&&actor.motion().destination.x==100&&actor.metrics().itemSearches==1&&host.calls==before+4);
    assert(actor.state().jobCheckTime==1.5f&&actor.state().stopDelay==1);actor.pickedItem(22);assert(actor.state().pickItem==11&&actor.state().jobCheckTime==1.5f);
    // Actual common FieldItem performs the strict20 BOT capture and later
    // strict30 Player arrival. Bot's notification itself does not award loot.
    host.playerValue.position={-150,0};const auto* coin=definitions.findItem("00001");assert(coin);auto item=progression::FieldItem::create(*coin,3,actor.motion().position,host);
    const progression::PickupBot bots[]={{2,actor.motion().position}};unsigned arrived=0,picked=0;
    item.tick(1,host.playerValue.position,bots,host,nullptr,[&](const auto&,int){++arrived;},[&](std::size_t index){assert(index==0);++picked;actor.pickedItem(11);});
    assert(item.pickerBot&&picked==1&&arrived==0&&actor.state().pickItem==0&&actor.state().jobCheckTime==0);
    for(unsigned i=1;i<=1200&&!item.removed;++i)item.tick(1+double(GameClock::increment)*i,host.playerValue.position,bots,host,nullptr,[&](const auto& value,int count){assert(&value==coin&&count==3);++arrived;});
    assert(item.removed&&arrived==1);
    // A candidate at distance exactly400 is excluded, without clearing the
    // previous target on a later no-result search.
    Host exact(tiles);exact.sequence={0,0};Bot boundary(data.models,data.weapons,data.machines.machine("Bot_P01"),{},0,exact);exact.itemList={{44,{400,0}}};exact.begin(0);boundary.tick({0,false},exact);assert(boundary.state().pickItem==0&&boundary.metrics().itemSearches==1);
}
void damageAndRegen(const Catalog& data,PreparedTiles& tiles){
    Host host(tiles);Bot actor(data.models,data.weapons,data.machines.machine("Bot_P01"),{},0,host);actor.setSlotIndex(2);actor.setHP(100);actor.refreshHPGauge(host);assert(host.slot.u32(0x654)==100);
    host.begin(5);actor.tick({5,false},host);assert(actor.state().hp==100&&actor.metrics().regeneratedHP==0);
    host.begin(std::nextafter(5.,6.));actor.tick({host.time,false},host);assert(actor.state().hp==101&&actor.state().hpRefreshTime==5&&host.slot.u32(0x654)==101);
    host.begin(10);actor.tick({10,false},host);assert(actor.state().hp==101);host.begin(std::nextafter(10.,11.));actor.tick({host.time,false},host);assert(actor.state().hp==102);
    const auto rotor=actor.mounts().back().angle;const auto pos=actor.motion().position;host.begin(100);actor.tick({100,true},host);assert(actor.state().hp==102&&actor.motion().position.x==pos.x&&actor.mounts().back().angle!=rotor);
    actor.setHP(actor.info().hp);actor.refreshHPGauge(host);assert(host.slot.u32(0x654)==unsigned(actor.info().hp)&&actor.gauge().scaleX==.5f&&actor.gauge().x==0);
    actor.setSlotIndex(1);host.slot.string(0x640,6,"04201");host.begin(100);const auto before=host.calls;actor.onDamage(25,host);
    assert(actor.state().hp==175&&host.calls==before+1&&host.slot.u32(0x650)==175&&host.slot.string(0x640,6)=="04201");
    host.events.clear();const auto deathCalls=host.calls;actor.onDamage(200,host);
    assert((host.events==std::vector<std::string>{"base21","pool","R","clear:1","hp:1:0","R","hit","hp:1:0"}));
    assert(host.calls==deathCalls+2&&actor.state().hp==0&&actor.state().removed&&actor.motion().state==2&&actor.metrics().deaths==1&&host.slot.string(0x640,6).empty()&&host.slot.byte(0x641)=='4'&&host.slot.u32(0x650)==0);
    const auto retired=host.calls;actor.tick({200,false},host);assert(host.calls==retired);
    Host full(tiles);for(auto& particle:full.pool.particles())particle.state().show=true;Bot saturated(data.models,data.weapons,data.machines.machine("Bot_P01"),{},0,full);saturated.setHP(1);const auto rng=full.calls;saturated.onDamage(1,full);assert(saturated.state().removed&&full.calls==rng+1&&full.pool.allocations()==0);
}
void weaponsAndPause(const Catalog& data,PreparedTiles& tiles){
    const char* ids[]={"Bot_A01","Bot_A02","Bot_A03"};const int damages[]={8,16,24};
    for(unsigned i=0;i<3;++i){Host host(tiles);Bot actor(data.models,data.weapons,data.machines.machine(ids[i]),{},0,host);host.selected=9;
        for(unsigned frame=1;frame<=400;++frame){host.begin(double(GameClock::increment)*frame);actor.tick({host.time,false},host);host.pool.tick(host.renderTime);host.effects.tickTrails(host.time,false,host);}
        assert(actor.metrics().emissions>0&&host.damageAmounts.size()==actor.metrics().emissions&&std::all_of(host.damageAmounts.begin(),host.damageAmounts.end(),[&](int value){return value==damages[i];}));
        assert(host.targets.at(9).hp==100000-int(host.damageAmounts.size())*damages[i]&&actor.state().target==9&&actor.state().moveToTarget);
        host.events.clear();actor.clearTargetIf(10,host);assert(actor.state().target==9&&host.events.empty());actor.clearTargetIf(9,host);assert(actor.state().target==0&&!actor.state().moveToTarget&&host.selected==9&&(host.events==std::vector<std::string>{"unselect:9"}));
        std::cout<<"HM2_BOT_ATTACK "<<ids[i]<<" emits="<<actor.metrics().emissions<<" actualCommonDamage="<<100000-host.targets.at(9).hp<<" friendly/noPlayerBonus PASS\n";
    }
    Host idle(tiles);Bot actor(data.models,data.weapons,data.machines.machine("Bot_A01"),{},0,idle);const auto p=actor.motion().position;const auto calls=idle.calls;
    const auto rotor=actor.mounts().back().angle;idle.begin(0);actor.tick({0,true},idle);assert(idle.calls==calls+1&&actor.mounts().back().angle!=rotor&&actor.motion().position.x==p.x&&actor.metrics().wanderAttempts==0&&actor.metrics().emissions==0);
    Host dying(tiles);Bot attacker(data.models,data.weapons,data.machines.machine("Bot_A01"),{},0,dying);dying.selected=9;dying.begin(0);attacker.tick({0,false},dying);assert(attacker.state().target==9);
    dying.events.clear();attacker.onDamage(attacker.state().hp,dying);
    assert((dying.events==std::vector<std::string>{"base21","pool","R","unselect:9","clear:0","hp:0:0","R","hit","hp:0:0"}));
    assert(attacker.state().target==0&&!attacker.state().moveToTarget&&dying.selected==9);
}
}
int main(int argc,char** argv){
    if(argc<2)return 77;Files files(argv[1]);Catalog data(files);auto read=[&](std::string_view name){return DataTable::parse(files.read(name));};
    const auto definitions=progression::Definitions::parse(data.items,read("ItemRegen.tble"),read("MissionList.tble"),read,"us");
    PreparedTiles tiles("bot-tests");tiles.prepareWorldEffects(files);for(const auto* name:{"BulletShell.png","Efx_Trail04.png","Efx_Trail02.png","HPGauge.png"})tiles.prepare(files,name);
    spawnAndModels(data,tiles,files);const auto reads=files.reads,probes=files.probes;wander(data,tiles);picker(data,tiles,definitions);damageAndRegen(data,tiles);weaponsAndPause(data,tiles);assert(files.reads==reads&&files.probes==probes);
    SaveData original;original.initializeSlot(0,"BOT","us",1794268800);progression::SlotView slot(original.slot(0));assert(slot.string(0x63a,6)=="04201");
    std::cout<<"HM2_BOT originalItems6/Mach11 MCB/WeaponAI/Rotor/gauge/slotHP RNG4spawn; follow/strict2500/400/ties; actualFieldItem relay; regen5/damage/death/fullPool/retirement/pause warmIO0 PASS; live Scene/QuickRepair/GPU/shadow/save integration pending\n";
}

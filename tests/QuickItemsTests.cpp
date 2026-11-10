#include "Core/Progression/QuickItems.hpp"
#include "Core/Data/SaveData.hpp"
#include "Core/Presentation/PreparedTiles.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace hm2;
using namespace hm2::progression;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;unsigned reads=0;mutable unsigned probes=0;
    explicit Files(const char* p):root(p){}
    Bytes read(std::string_view name)override{++reads;std::ifstream f(root/std::string(name),std::ios::binary);if(!f)throw std::runtime_error("Missing original quick-item test resource "+std::string(name));return {std::istreambuf_iterator<char>(f),{}};}
    bool exists(std::string_view name)const override{++probes;return std::filesystem::exists(root/std::string(name));}
};
struct Host:QuickItemsHost {
    Inventory& inventory;PreparedTiles& tiles;DarwinWorldRandom random{731};ParticlePool pool{0,random};
    std::optional<RepairTarget> actor=RepairTarget{0,true,{0,0},100,350};std::vector<RepairTarget> botActors;
    std::vector<std::string> events;std::vector<AirStrikeRequest> attacks;
    Host(Inventory& i,PreparedTiles& t):inventory(i),tiles(t){}
    std::optional<RepairTarget> player()const override{return actor;}
    std::vector<RepairTarget> bots()const override{return botActors;}
    void setHP(const RepairTarget& target,int hp)override{
        events.push_back("HP");if(target.player){assert(actor&&actor->id==target.id);actor->hp=hp;}
        else{const auto found=std::find_if(botActors.begin(),botActors.end(),[&](const auto& b){return b.id==target.id;});assert(found!=botActors.end());found->hp=hp;}
    }
    void refreshActorHP(const RepairTarget&)override{events.push_back("actorGauge");}
    void refreshPlayerHP()override{events.push_back("playerGauge");}
    Particle* allocateParticle(float time)override{events.push_back("particle");return pool.allocate(time,random);}
    const TileImage* preparedTile(std::string_view name)override{events.push_back("tile:"+std::string(name));return tiles.lookup(name);}
    void selectQuick(int n)override{events.push_back("select:"+std::to_string(n));}
    void message(std::string_view key,int bg,bool autoHide)override{assert(bg==0&&!autoHide);events.push_back("message:"+std::string(key));}
    void hideMessage()override{events.push_back("hide");}
    void refreshQuickItem(const Item& item,int count)override{assert(count==inventory.count(item));events.push_back("count:"+std::to_string(count));}
    void sound(unsigned id)override{events.push_back("sfx:"+std::to_string(id));}
    void startAirStrike(const AirStrikeRequest& value)override{
        // Boundary-only test: this records the requested real actor. It does
        // not claim to execute its missiles, damage, plane or marker effects.
        assert(value.item&&inventory.count(*value.item)>0);attacks.push_back(value);events.push_back("airstrike");
    }
    void resetParticles(){for(auto& p:pool.particles())p.state().show=false;}
};
void expect(const std::vector<std::string>& actual,std::initializer_list<const char*> expected){
    assert(actual.size()==expected.size());auto i=actual.begin();for(const auto* text:expected)assert(*i++==text);
}
}
int main(int argc,char** argv){
    if(argc<2)return 77;Files files(argv[1]);
    auto read=[&](std::string_view name){return DataTable::parse(files.read(name));};
    const auto defs=Definitions::parse(read("ItemInfo.tble"),read("ItemRegen.tble"),read("MissionList.tble"),read,"us");
    const auto weapons=Weapons::parse(read("WeaponInfo.tble"));
    SaveData data;data.initializeSlot(0,"MACH","us",1794268800);SlotView slot(data.slot(0));slot.maxHP(300);slot.hp(90);
    Inventory inventory(defs);inventory.importSlot(slot);
    PreparedTiles tiles("quick-items-test");for(const auto& [id,item]:defs.items())if(item.type==1)tiles.prepare(files,item.iconImage());
    Host host(inventory,tiles);QuickItems quick(inventory,slot,weapons,host);MapCamera camera;
    inventory.countChanged=[&](const Item&){host.events.push_back("subtract");};
    const auto* small=defs.findItem("01001");assert(small&&small->type==1&&small->param1==120&&small->param2==0);
    assert(host.random.calls()==64);const auto warmReads=files.reads,warmProbes=files.probes;
    assert(quick.press(0,5,false)&&quick.inUseItem()==small&&quick.tapState()==1&&quick.selectedQuick()==0);
    expect(host.events,{"select:0","sfx:0","message:TapToRepairMach"});assert(inventory.count(*small)==5&&host.random.calls()==64);
    host.events.clear();assert(quick.input(PointerPhase::end,camera.toScreen({0,0}),camera,false,1,1));assert(quick.inUseItem()==small&&host.events.empty());
    host.events.clear();
    assert(quick.input(PointerPhase::begin,camera.toScreen({0,0}),camera,false,2,2));
    assert(host.actor->hp==220&&slot.hp()==220&&inventory.count(*small)==4&&host.random.calls()==65&&!quick.inUseItem()&&quick.selectedQuick()==-1&&quick.tapState()==0);
    expect(host.events,{"particle","tile:Icon01001.png","HP","actorGauge","playerGauge","subtract","count:4","sfx:13","select:-1","hide"});
    const auto& particle=host.pool.particles()[0].state();assert(particle.show&&particle.tile&&particle.tile->name=="Icon01001.png"&&particle.tileNumber==0&&particle.life==1&&particle.scale.x==.5f&&particle.scale.y==.5f&&particle.scaleVelocity==.01f&&particle.startTime==2&&particle.blendType==2&&particle.useAtlas&&particle.fadeOut);

    // Player has source priority inside the strict radius, even when a BOT is
    // closer. The actual six AidKit parameters determine HP and quantity.
    const int amounts[]={120,240,500,105,175,350};
    for(unsigned i=0;i<6;++i){const auto code="0100"+std::to_string(i+1);const auto* kit=defs.findItem(code);assert(kit);
        assert(inventory.setItem(20,kit,2));inventory.quickSlot(0,kit);host.actor=RepairTarget{0,true,{29,0},100,350};host.botActors={{1,false,{0,0},0,40,0}};slot.hp(90);host.resetParticles();host.events.clear();
        assert(quick.press(0,2,false));const auto rng=host.random.calls();const int beforeCount=inventory.count(*kit);assert(quick.input(PointerPhase::begin,camera.toScreen({0,0}),camera,false,10+float(i),10+i));
        assert(host.actor->hp==std::min(350,100+amounts[i])&&slot.hp()==host.actor->hp&&host.botActors[0].hp==0&&host.random.calls()==rng+1);
        assert(inventory.count(*kit)==beforeCount-1); // Original subItem consumes the actual Item identity, across stacks.
    }

    // Full HP still consumes the kit. A full particle pool skips only the
    // visual/reset RNG, and never suppresses the original HP/quantity action.
    inventory.quickSlot(0,small);assert(inventory.setItem(20,small,2));host.actor=RepairTarget{0,true,{0,0},350,350};host.botActors.clear();
    for(auto& p:host.pool.particles())p.state().show=true;host.events.clear();quick.press(0,2,false);const auto fullRandom=host.random.calls();const int fullCount=inventory.count(*small);
    quick.input(PointerPhase::begin,camera.toScreen({0,0}),camera,false,20,20);assert(host.actor->hp==350&&slot.hp()==350&&inventory.count(*small)==fullCount-1&&host.random.calls()==fullRandom);

    // Exact30 is outside; no-target tap disarms without repair, subtraction,
    // sound13 or RNG. Non-begin/cancel retains the armed mode, like the source.
    host.resetParticles();host.actor=RepairTarget{0,true,{0,0},100,350};host.events.clear();quick.press(0,1,false);host.events.clear();
    const int noTargetCount=inventory.count(*small);const auto noTargetRandom=host.random.calls();
    assert(quick.input(PointerPhase::cancel,camera.toScreen({30,0}),camera,false,21,21)&&quick.inUseItem()==small&&host.events.empty());
    quick.input(PointerPhase::begin,camera.toScreen({30,0}),camera,false,21,21);assert(host.actor->hp==100&&inventory.count(*small)==noTargetCount&&host.random.calls()==noTargetRandom);
    expect(host.events,{"select:-1","hide"});

    // BOT ties preserve child order; even a0HP BOT receives the source repair.
    // BOT repair does not copy that actor's HP into the Player slot.
    host.actor=RepairTarget{0,true,{100,0},100,350};host.botActors={{7,false,{-10,0},0,80,0},{8,false,{10,0},0,80,1}};slot.hp(123);host.resetParticles();host.events.clear();quick.press(0,1,false);
    quick.input(PointerPhase::begin,camera.toScreen({0,0}),camera,false,22,22);assert(host.botActors[0].hp==80&&host.botActors[1].hp==0&&host.actor->hp==100&&slot.hp()==123&&slot.u32(0x64c)==80);
    assert(std::find(host.events.begin(),host.events.end(),"playerGauge")==host.events.end());

    // Paused quick presses do nothing; dead/missing Player blocks the world
    // event, including repair of another BOT. Pressing the same item cancels.
    host.actor=RepairTarget{0,true,{0,0},100,350};host.events.clear();assert(!quick.press(0,1,true)&&host.events.empty());quick.press(0,1,false);host.events.clear();
    const int pausedCount=inventory.count(*small);const auto pausedRng=host.random.calls();assert(!quick.input(PointerPhase::begin,camera.toScreen({0,0}),camera,true,23,23)&&quick.inUseItem()==small&&inventory.count(*small)==pausedCount&&host.random.calls()==pausedRng&&host.events.empty());quick.clear();
    host.actor->hp=0;quick.press(0,1,false);const auto deadItem=quick.inUseItem();host.events.clear();
    assert(!quick.input(PointerPhase::begin,camera.toScreen({0,0}),camera,false,23,23)&&quick.inUseItem()==deadItem&&host.events.empty());
    host.actor->hp=100;host.events.clear();quick.press(0,1,false);assert(!quick.inUseItem());expect(host.events,{"select:0","sfx:0","hide","select:-1"});
    quick.press(0,1,false);host.events.clear();assert(quick.press(4,0,false)&&quick.selectedQuick()==-1&&quick.inUseItem()==small);expect(host.events,{"select:-1"});quick.clear();

    // A zero-count quick selection is rejected even if the source pointer is
    // registered. Registration remains intact after its last item is consumed.
    const auto* percentage=defs.findItem("01006");inventory.quickSlot(0,percentage);const int percentCount=inventory.count(*percentage);if(percentCount)inventory.subtract(*percentage,std::int16_t(percentCount));host.events.clear();quick.press(0,0,false);assert(!quick.inUseItem()&&inventory.quickSlots()[0]==percentage);expect(host.events,{"select:-1"});

    // REA ARM float order: ((center-half)+offset)+screen. The shared
    // MapCamera's algebraic ordering rounds this concrete boundary inside30.
    // Original distanceSquared is exactly900 and therefore refuses repair.
    inventory.quickSlot(0,small);host.actor=RepairTarget{0,true,{-88.642364502f,0},100,350};host.botActors.clear();host.resetParticles();slot.hp(100);
    camera.center={276.96005249f,0};camera.offset={-5.967381477f,0};quick.press(0,1,false);host.events.clear();
    const auto boundaryRNG=host.random.calls();const int boundaryCount=inventory.count(*small);
    quick.input(PointerPhase::begin,{54.364967346f,512},camera,false,24,24);
    assert(host.actor->hp==100&&slot.hp()==100&&inventory.count(*small)==boundaryCount&&host.random.calls()==boundaryRNG);
    expect(host.events,{"select:-1","hide"});

    // Source airstrike boundary: arm on Push, consume only after an actual
    // start request, keep original key/delay/count/coordinates, then disarm.
    // This host does NOT execute GobAirStrike, so no attack completion is claimed.
    camera.center={5,9};camera.offset={12,4};host.actor=RepairTarget{0,true,{0,0},100,350};
    constexpr const char* attackCodes[]={"02004","02005","02007"};constexpr float delays[]={1.5f,1.f,2.f};constexpr const char* attackKeys[]={"AS_BOMB_01","AS_MISSILE_01","AS_NUCLEAR_01"};
    for(unsigned i=0;i<3;++i){const auto* item=defs.findItem(attackCodes[i]);assert(item&&item->param2==delays[i]);assert(inventory.setItem(21,item,2));inventory.quickSlot(1,item);host.events.clear();
        assert(quick.press(1,2,false)&&quick.tapState()==2);const int prior=inventory.count(*item);host.events.clear();assert(quick.input(PointerPhase::begin,{400,600},camera,false,30+float(i),40+i));
        const auto& request=host.attacks.back();assert(request.item==item&&request.weapon==&weapons.weapon(attackKeys[i])&&request.weaponKey==attackKeys[i]&&request.fireDelay==delays[i]&&request.attackCount==request.weapon->shotCount&&request.position.x==33&&request.position.y==101&&request.worldTime==40+i);
        assert(inventory.count(*item)==prior-1&&!quick.inUseItem());assert(host.events.front()=="airstrike"&&host.events[1]=="subtract"&&host.events[2].starts_with("count:")&&host.events[3]=="select:-1"&&host.events[4]=="hide");
    }
    assert(files.reads==warmReads&&files.probes==warmProbes); // No asset/DB/save path on use.
    std::cout<<"QUICK_ITEMS sixOriginalAidKits HP/quantity/particle/RNG/order/target/boundary/paused gates PASS; airstrike request boundary only, actual Scene integration pending\n";
}

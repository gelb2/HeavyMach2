#include "PreparedTextStub.hpp"
#include "Core/Session/GameSession.hpp"
#include "Core/Progression/Progression.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numbers>

using namespace hm2;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;unsigned reads=0;mutable unsigned probes=0;
    explicit Files(const char* path):root(path){}
    Bytes read(std::string_view name)override{++reads;std::ifstream file(root/std::string(name),std::ios::binary);if(!file)throw std::runtime_error("Missing original GateSession asset "+std::string(name));return {std::istreambuf_iterator<char>(file),{}};}
    bool exists(std::string_view name)const override{++probes;return std::filesystem::exists(root/std::string(name));}
};
struct Host:SessionHost {
    Bytes stored;std::vector<SaveReason> writes;std::vector<std::string> sounds,alerts;
    std::vector<ui::TextRasterRequest> textRequests;
    ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& r)override{textRequests.push_back(r);return test::preparedTextStub(r);}
    void sound(std::string_view name,Vec2)override{sounds.emplace_back(name);}
    void soundID(unsigned,Vec2)override{}
    void alert(std::string_view key)override{alerts.emplace_back(key);}
    void requestName(unsigned)override{assert(false);}
    void requestDelete(unsigned)override{assert(false);}
    bool writeSave(std::span<const std::uint8_t> bytes,SaveReason reason)override{assert(bytes.size()==SaveData::byteCount);stored.assign(bytes.begin(),bytes.end());writes.push_back(reason);return true;}
    std::uint32_t absoluteTimeLow()override{return 301;}
    std::uint32_t unixTime()override{return 1794268800;}
};
std::uint32_t word(std::span<const std::uint8_t> b,unsigned p){return std::uint32_t(b[p])|(std::uint32_t(b[p+1])<<8)|(std::uint32_t(b[p+2])<<16)|(std::uint32_t(b[p+3])<<24);}
std::string string(std::span<const std::uint8_t> b,unsigned p,unsigned capacity){const auto start=b.begin()+p,end=start+capacity;return {start,std::find(start,end,0)};}
const MapGate& findGate(const Map& map,int number){const auto i=std::find_if(map.gates.begin(),map.gates.end(),[&](const auto& g){return g.number==number;});assert(i!=map.gates.end());return *i;}
float distance(Vec2 a,Vec2 b){const float x=a.x-b.x,y=a.y-b.y;return std::sqrt(x*x+y*y);}
void expectSpawn(const CombatScene& scene,const MapGate& gate){
    // Independent cardinal-side oracle: strict ties prefer left,right,bottom,top.
    // It does not call the product's spawn or mutate the actor.
    const float distances[]={std::abs(gate.position.x+512.f),std::abs(gate.position.x-512.f),std::abs(gate.position.y+512.f),std::abs(gate.position.y-512.f)};
    unsigned side=0;for(unsigned i=1;i<4;++i)if(distances[i]<distances[side])side=i;
    constexpr Vec2 offsets[]={{50,0},{-50,0},{0,50},{0,-50}};
    const auto& p=scene.player();assert(std::abs(p.position.x-(gate.position.x+offsets[side].x))<.0002f&&std::abs(p.position.y-(gate.position.y+offsets[side].y))<.0002f);
    const float angles[]={0,std::numbers::pi_v<float>,std::numbers::pi_v<float>/2,-std::numbers::pi_v<float>/2};assert(std::abs(p.angle-angles[side])<.0002f);
}
const progression::MissionDefinition& definition(const progression::Definitions& defs,std::string_view id){const auto i=std::find_if(defs.missions().begin(),defs.missions().end(),[&](const auto& d){return d.id==id;});assert(i!=defs.missions().end());return *i;}
SaveData fixture(std::string_view map,int gate,const progression::MissionDefinition* mission=nullptr,int missionState=0){
    SaveData save;save.initializeSlot(0,"GATEFIX","us",1794268800);progression::SlotView slot(save.slot(0));slot.string(0x2d,16,map);slot.u32(0x68,std::uint32_t(gate));
    // Focused original-save fixture: Help was already seen. Keep original
    // default equipment, HP and registered Picker; no actor/result injection.
    slot.u32(0x1b5c,5);
    if(missionState){
        // Saved acceptance intentionally bypasses level/previous-mission UI
        // availability. Restore uses the real Missions.importSlot procedure.
        // State2 means requirements already met, but reward remains unclaimed.
        assert(mission);slot.u16(0x658,1);slot.string(0x65c,8,mission->id);slot.byte(0x664,1);
        for(unsigned n=0;n<mission->requirements.size();++n){const auto& r=mission->requirements[n];assert(r.type==2&&r.count>0&&r.count<128);slot.byte(0x665+n,missionState==2?std::uint8_t(r.count):0);}
        slot.byte(0x66f,missionState==2);slot.byte(0x670,0);
    }
    return save;
}
void requestWalk(GameSession& game,Vec2 destination){
    assert(game.battle()&&!game.battle()->isPaused()&&!game.help());
    const auto logical=game.battle()->camera().toScreen(destination);assert(logical.y>=100);
    // The actual begin/end input creates the real WorldNavigation destination.
    // No repeated double taps, Player setters, damage or link callbacks here.
    assert(game.inputWorld(PointerPhase::begin,logical,41));assert(game.inputWorld(PointerPhase::end,logical,41));
}
unsigned walkUntilTransition(GameSession& game,const MapGate& gate,Vec2 start){
    requestWalk(game,gate.position);unsigned ticks=0;
    while(!game.mapLoading()&&game.screenTransition().phase==8&&ticks<600){
        game.tick();(void)game.drawWorld();++ticks;
    }
    if(!game.mapLoading()&&game.screenTransition().phase==8){const auto& p=game.battle()->player().position;std::cerr<<"GATE_ROUTE_FAILURE target="<<gate.linkMap<<" gate="<<gate.number<<" start="<<start.x<<","<<start.y<<" current="<<p.x<<","<<p.y<<" hp="<<game.battle()->playerHP()<<" ticks="<<ticks<<"\n";}
    assert(ticks<600&&game.battle()&&game.battle()->isPaused());
    const auto actual=game.battle()->player().position;assert(distance(actual,start)>20.f&&std::abs(actual.x-gate.position.x)<30.f&&std::abs(actual.y-gate.position.y)<30.f);
    assert(string(game.workingSlot(),0x2d,16)==gate.linkMap&&word(game.workingSlot(),0x68)==std::uint32_t(gate.linkGate));
    return ticks;
}
void waitForNewStage(GameSession& game){
    assert(game.mapLoading());unsigned ticks=0;
    while(game.mapLoading()->state().step<5&&ticks<200){game.tick();(void)game.drawWorld();++ticks;}
    assert(ticks<200&&game.mapLoading()&&game.mapLoading()->state().step==5&&game.battle()&&!game.battle()->isPaused());
}
void labelsAndPlace(Files& files,const Maps& maps,const WorldGateNames& names){
    Host host;DarwinWorldRandom random(9);GameSession game(files,host,random,fixture("Stage01_01",1));assert(game.selectSlot(0)&&game.enterField("Stage01_01",1));assert(game.battle()->gates().size()==3);
    const auto& raw=maps.map("Stage01_01").gates;for(std::size_t i=0;i<raw.size();++i){const auto& state=game.battle()->gates()[i];assert(state.definition.number==raw[i].number&&state.definition.linkMap==raw[i].linkMap&&state.destinationName==*names.find(raw[i].linkMap));}
    const auto& marks=game.state().radarMarks;
    const auto player=game.battle()->player().position;
    const auto bots=game.battle()->bots();
    assert(marks.size()==1+bots.size()+raw.size()&&marks[0].tile==0);
    assert(marks[0].worldPosition.x==player.x&&marks[0].worldPosition.y==player.y);
    for(std::size_t i=0;i<bots.size();++i){
        const auto& mark=marks[1+i];
        assert(mark.tile==1&&mark.worldPosition.x==bots[i].position.x&&mark.worldPosition.y==bots[i].position.y);
    }
    for(std::size_t i=0;i<raw.size();++i){
        const auto& mark=marks[1+bots.size()+i];
        assert(mark.tile==2&&mark.worldPosition.x==raw[i].position.x&&mark.worldPosition.y==raw[i].position.y);
    }
    assert(host.textRequests.size()==6);for(const auto& r:host.textRequests)assert(r.fontName=="Trebuchet-BoldItalic"&&r.fontSize==13&&r.dimensions.width==64&&r.dimensions.height==48&&r.alignment==ui::TextAlign::center);
    assert(host.textRequests[0].text=="RuinField Base"&&host.textRequests[2].text=="wetland"&&host.textRequests[4].text=="wide planes 01");
    game.tick();const auto frame=game.drawWorld();assert(std::any_of(frame.sprites.begin(),frame.sprites.end(),[](const auto& s){return s.image=="LinkGate.png"&&!s.atlasBatch;}));assert(std::any_of(frame.sprites.begin(),frame.sprites.end(),[](const auto& s){return s.image.starts_with("cpu-text:GobLinkGate:");}));
    Host distinctHost;DarwinWorldRandom distinctRandom(11);GameSession distinct(files,distinctHost,distinctRandom,fixture("Stage02_09",0));assert(distinct.selectSlot(0)&&distinct.enterField("Stage02_09",0));assert(string(distinct.workingSlot(),0x3d,24)=="under beach 01");
    assert(distinct.state().fieldPlaceName=="under beach 01"&&distinct.state().fieldPlaceRevision==1);
    assert(std::any_of(distinctHost.textRequests.begin(),distinctHost.textRequests.end(),[](const auto& r){return r.text=="under city 05";})); // Gate's MapList name intentionally differs from target MapInfo.
    std::cout<<"GATE_SESSION actualScene3/rawOrder/MapListLabelProducer/commonText64x48/realFrame/slotMapInfoDescription PASS\n";
}
void normalFieldRoute(Files& files,const Maps& maps){
    Host host;DarwinWorldRandom random(3);GameSession game(files,host,random,fixture("Stage01_01",1));assert(game.selectSlot(0)&&game.enterField("Stage01_01",1));const auto& source=findGate(maps.map("Stage01_01"),1);assert(source.linkMap=="Stage01_02"&&!source.eventOnly);
    const auto before=host.writes.size();const auto diskBefore=host.stored;const auto start=game.battle()->player().position;expectSpawn(*game.battle(),source);
    const auto movedTicks=walkUntilTransition(game,source,start);assert(game.mapLoading()&&game.battle()->isSave()&&host.writes.size()==before&&host.stored==diskBefore);assert(std::count(host.sounds.begin(),host.sounds.end(),"Title_TitleIn.wav")==1);
    const double pausedClock=game.battle()->clock().worldTime();waitForNewStage(game);assert(game.battle()->clock().worldTime()==pausedClock);const auto& target=findGate(maps.map(source.linkMap),source.linkGate);expectSpawn(*game.battle(),target);assert(game.battle()->isSave()&&host.writes.size()==before+1&&host.writes.back()==SaveReason::stageTransition);assert(string(game.workingSlot(),0x3d,24)==maps.map(source.linkMap).description);
    DarwinWorldRandom check(99);const auto disk=SaveData::decode(host.stored,check,false);assert(string(disk.slot(0),0x2d,16)==source.linkMap&&word(disk.slot(0),0x68)==std::uint32_t(source.linkGate));
    game.tick();(void)game.drawWorld();assert(game.battle()->clock().worldTime()>pausedClock);
    assert(game.metrics().unsupportedActions==0);std::cout<<"GATE_SESSION actualWorldInput normal Stage01_01->Stage01_02 tick="<<movedTicks<<"/deferredMapLoading/destinationGate50offset/isSave1/oneStageSave/persistentClockThenResume PASS\n";
}
void baseRoute(Files& files,const Maps& maps){
    Host host;DarwinWorldRandom random(4);GameSession game(files,host,random,fixture("Stage01_01",0));assert(game.selectSlot(0)&&game.enterField("Stage01_01",0));const auto& source=findGate(maps.map("Stage01_01"),0);const auto before=host.writes.size();const auto start=game.battle()->player().position;
    const auto movedTicks=walkUntilTransition(game,source,start);assert(!game.mapLoading()&&game.battle()->isSave()&&game.screenTransition().phase==0&&game.screenTransition().requestedScreen==3&&host.writes.size()==before);
    unsigned ticks=0;while(game.state().screen!=ui::ScreenId::base&&ticks<200){game.tick();(void)game.drawWorld();++ticks;}assert(ticks<200&&!game.battle()&&host.writes.size()==before+1&&host.writes.back()==SaveReason::enterBase&&string(game.workingSlot(),0x2d,16)=="Base001");
    assert(game.screenTransition().phase==5&&std::count(host.sounds.begin(),host.sounds.end(),"Loading_Close.wav")==2&&std::count(host.sounds.begin(),host.sounds.end(),"Loading_Open.wav")==1);assert(game.metrics().unsupportedActions==0);
    std::cout<<"GATE_SESSION actualWorldInput Stage01_01->Base001 tick="<<movedTicks<<"/immediatePause/deferredDoor3/enterBaseSave PASS\n";
}
void eventMatrix(Files& files,const Maps& maps,const progression::Definitions& defs){
    struct Event {const char* from;const char* to;const char* mission;};
    constexpr Event events[]={{"Stage01_02","Stage01_08","M001070"},{"Stage01_07","Stage01_09","M001120"},{"Stage02_06","Stage02_07","M001170"},{"Stage04_11","Stage04_12","M001230"}};
    for(const auto& event:events){const auto& map=maps.map(event.from);const auto gateIt=std::find_if(map.gates.begin(),map.gates.end(),[&](const auto& g){return g.eventOnly&&g.linkMap==event.to;});assert(gateIt!=map.gates.end());const auto& source=*gateIt;const auto& mission=definition(defs,event.mission);assert(std::any_of(mission.acceptActions.begin(),mission.acceptActions.end(),[&](const auto& a){return a.type==7&&a.place==event.from;}));
        for(int state=0;state<3;++state){
            // Accepted-incomplete starts at the real open event gate's normal
            // inward50 spawn so a short actual navigation can exercise it.
            // Closed cases start at a real non-event gate; never spawn through
            // a gate that the original load filter does not create.
            const auto regular=std::find_if(map.gates.begin(),map.gates.end(),[](const auto& g){return !g.eventOnly;});assert(regular!=map.gates.end());const int entry=state==1?source.number:regular->number;
            Host host;DarwinWorldRandom random(7);GameSession game(files,host,random,fixture(event.from,entry,&mission,state));assert(game.selectSlot(0)&&game.enterField(event.from,entry));
            const auto& accepted=game.state().acceptedMissions;assert(accepted.size()==std::size_t(state!=0));if(state)assert(accepted[0].dataId==event.mission&&accepted[0].complete==(state==2));
            const auto gates=game.battle()->gates();const bool created=std::any_of(gates.begin(),gates.end(),[&](const auto& g){return g.definition.eventOnly&&g.definition.number==source.number&&g.definition.linkMap==event.to;});assert(created==(state==1)&&gates.size()==map.gates.size()-std::size_t(state!=1));
            const auto& marks=game.state().radarMarks;
            assert(std::count_if(marks.begin(),marks.end(),[](const auto& mark){return mark.tile==2;})==gates.size());
            if(state==1){const auto before=host.writes.size();const auto stored=host.stored;const auto start=game.battle()->player().position;expectSpawn(*game.battle(),source);const auto movedTicks=walkUntilTransition(game,source,start);assert(game.mapLoading()&&!game.battle()->isSave()&&host.writes.size()==before);
                const double pausedClock=game.battle()->clock().worldTime();waitForNewStage(game);assert(game.battle()->clock().worldTime()==pausedClock);expectSpawn(*game.battle(),findGate(maps.map(event.to),source.linkGate));assert(!game.battle()->isSave()&&host.writes.size()==before&&host.stored==stored&&string(game.workingSlot(),0x2d,16)==event.to&&std::count(host.sounds.begin(),host.sounds.end(),"Title_TitleIn.wav")==1);
                game.tick();(void)game.drawWorld();assert(game.battle()->clock().worldTime()>pausedClock&&host.writes.size()==before);
                std::cout<<"GATE_SESSION actualEventRoute "<<event.mission<<" "<<event.from<<"->"<<event.to<<" tick="<<movedTicks<<"/isSave0/noStageWrite/persistentClockThenResume PASS\n";
            }
            assert(game.metrics().unsupportedActions==0);
        }
    }
    std::cout<<"GATE_SESSION event4x3 actualMissionRestore/none_acceptedIncomplete_requirementsMet/loadFilter12states PASS; savedAcceptance/seenHelp are explicit fixture conditions\n";
}
}
int main(int argc,char** argv){
    if(argc<2)return 77;Files files(argv[1]);const auto maps=Maps::parse(Chunk::parse(files.read("MapInfo.mcb")));const WorldGateNames names(files);const auto table=[&](std::string_view file){return DataTable::parse(files.read(file));};
    const auto defs=progression::Definitions::parse(table("ItemInfo.tble"),table("ItemRegen.tble"),table("MissionList.tble"),table,"kr");
    labelsAndPlace(files,maps,names);normalFieldRoute(files,maps);baseRoute(files,maps);eventMatrix(files,maps,defs);
}

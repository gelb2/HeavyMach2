#include "Core/Validation/ConcurrentValidation.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
using namespace hm2;
namespace {
struct Files final:ResourceAccess {
    std::filesystem::path root;std::uint64_t reads=0;mutable std::uint64_t probes=0;
    explicit Files(const char* path):root(path){}
    Bytes read(std::string_view name)override{++reads;std::ifstream in(root/std::string(name),std::ios::binary);if(!in)throw std::runtime_error("Missing original validation resource "+std::string(name));return {std::istreambuf_iterator<char>(in),{}};}
    bool exists(std::string_view name)const override{++probes;return std::filesystem::exists(root/std::string(name));}
};
struct Host final:validation::ValidationHost {
    validation::ConcurrentValidation* runtime=nullptr;
    std::map<unsigned,Bytes> saved;std::map<unsigned,std::uint64_t> sounds,saves,presses,uiDraws,restarts;
    void sound(unsigned i,std::string_view,Vec2)override{++sounds[i];}
    void soundID(unsigned i,unsigned,Vec2)override{++sounds[i];}
    bool writeSave(unsigned i,std::span<const std::uint8_t> bytes,SaveReason)override{saved[i]={bytes.begin(),bytes.end()};++saves[i];return true;}
    std::uint32_t absoluteTimeLow(unsigned i)override{return 97+i;}
    std::uint32_t unixTime()override{return 1794268800;}
    bool pressButton(unsigned i,unsigned id,int data=0)override{
        assert(runtime);++presses[i];auto& session=runtime->session(i);const auto& state=session.state();std::string item;
        if(id>=5100&&id<5105&&id-5100<state.availableMissions.size())item=state.availableMissions[id-5100].dataId;
        if(id>=2200&&id<2224&&id-2200<state.shopItems.size())item=state.shopItems[id-2200].dataId;
        if(id==5004&&state.selectedMission>=0&&std::size_t(state.selectedMission)<state.availableMissions.size())item=state.availableMissions[std::size_t(state.selectedMission)].dataId;
        // CPU exercises the original GameSession action consumer. Native hosts
        // additionally execute Screens pointer begin/end at the real key center.
        for(const auto phase:{ui::ButtonEventType::push,ui::ButtonEventType::pop})session.action({state.screen,ui::Panel::root,{phase,{0,id,data,item},1,{}}});return true;
    }
    void sessionRestarted(unsigned i,GameSession&)override{++restarts[i];}
    void appendScenarioUI(unsigned i,GameSession&,Frame& frame)override{
        ++uiDraws[i];if(i)return;
        Sprite probe;probe.image="validation-compositor-probe";probe.center={10,20};probe.size={20,40};probe.clip=ClipRect{1,2,30,50};
        probe.strip.push_back({{4,7},{.125f,.25f},{.25f,.5f,.75f,1}});
        auto batch=std::make_shared<AtlasBatch>();batch->image=probe.image;AtlasQuad quad{};
        for(auto& vertex:quad.vertices){vertex.position={6,9};vertex.uv={.33f,.77f};vertex.color={1,3,5,7};}batch->quads.push_back(quad);batch->indices={0,1,2,2,1,3};probe.atlasBatch=batch;frame.sprites.push_back(std::move(probe));
    }
};
void compositor(validation::ConcurrentValidation& runtime,Host& host){
    auto first=runtime.drawFrame();const auto draws=runtime.metrics().drawFrames;const auto drawsHost=host.uiDraws;
    const auto again=runtime.drawFrame();const auto resized=runtime.drawFrame({3840,2160});const auto portrait=runtime.drawFrame({1080,1920});assert(portrait.sprites.size()==first.sprites.size());
    assert(runtime.metrics().drawFrames==draws&&host.uiDraws==drawsHost&&again.sprites.size()==first.sprites.size());
    const auto find=[](const Frame& f)->const Sprite&{const auto i=std::find_if(f.sprites.begin(),f.sprites.end(),[](const auto& s){return s.image=="validation-compositor-probe";});assert(i!=f.sprites.end());return *i;};
    const auto& a=find(first);const auto& b=find(resized);assert(a.clip&&b.clip&&a.strip.size()==1&&a.atlasBatch&&a.atlasBatch->quads.size()==1);
    struct Transform {float scale,x,y;};
    const auto transform=[&](float width,float height){const unsigned columns=unsigned(std::ceil(std::sqrt(float(runtime.scenarioCount())*width/height*(1024.f/768.f))));const unsigned rows=unsigned((runtime.scenarioCount()+columns-1)/columns);const float pw=width/float(columns),ph=height/float(rows);const float scale=std::min(std::max(1.f,pw-4)/768,std::max(1.f,ph-4)/1024);return Transform{scale,(pw-768*scale)*.5f,height-ph+(ph-1024*scale)*.5f};};
    const auto small=transform(1920,1080),large=transform(3840,2160);const float ratio=large.scale/small.scale;
    const auto near=[](float a,float b){return std::abs(a-b)<.001f;};
    const auto x=[&](float a){return large.x+(a-small.x)*ratio;};const auto y=[&](float a){return large.y+(a-small.y)*ratio;};
    assert(near(b.center.x,x(a.center.x))&&near(b.center.y,y(a.center.y))&&near(b.size.width,a.size.width*ratio)&&near(b.clip->x,x(a.clip->x)));
    assert(near(b.strip[0].position.x,x(a.strip[0].position.x))&&near(b.strip[0].position.y,y(a.strip[0].position.y))&&b.strip[0].uv.x==.125f&&b.strip[0].color.b==.75f);
    const auto& av=a.atlasBatch->quads[0].vertices[0];const auto& bv=b.atlasBatch->quads[0].vertices[0];assert(near(bv.position.x,x(av.position.x))&&near(bv.position.y,y(av.position.y))&&bv.uv.x==.33f&&bv.uv.y==.77f&&bv.color==ColorBytes({1,3,5,7}));assert(b.atlasBatch->indices==a.atlasBatch->indices);
}
}
int main(int argc,char** argv){
    if(argc<2)return 77;const auto begin=std::chrono::steady_clock::now();Files files(argv[1]);Host host;validation::ConcurrentValidation runtime(files,host);host.runtime=&runtime;
    assert(runtime.scenarioCount()==59&&runtime.metrics().combatScenarios==55);std::set<std::string> itemCodes;
    const auto originalItems=DataTable::parse(runtime.cachedResources().read("ItemInfo.tble"));std::vector<std::string> originalWeaponCodes;for(std::size_t row=0;row<originalItems.rowCount();++row)if(originalItems.value(row,"Type")=="PtWpn")originalWeaponCodes.emplace_back(originalItems.value(row,"Code").value());assert(originalWeaponCodes.size()==110);
    for(std::size_t i=0;i<55;++i){const auto& descriptor=runtime.scenarios()[i];assert(descriptor.role==validation::ScenarioRole::combat&&descriptor.weapons.size()==2);for(unsigned side=0;side<2;++side){const auto& code=descriptor.weapons[side];assert(code==originalWeaponCodes[i*2+side]);itemCodes.insert(code);assert(runtime.session(unsigned(i)).state().equipment[side+2].dataId==code);}assert(runtime.session(unsigned(i)).battle());}
    assert(itemCodes.size()==110);const auto warmReads=files.reads,warmProbes=files.probes;
    compositor(runtime,host);
    const unsigned frames=argc>2?unsigned(std::stoul(argv[2])):1200;
    for(unsigned i=0;i<frames;++i){runtime.tick();const auto frame=runtime.drawFrame();assert(!frame.sprites.empty());if(i==30)compositor(runtime,host);}
    const auto metrics=runtime.metrics();std::uint64_t emissions=0,damage=0,kills=0;unsigned progressed=0,complete=0,levelUps=0;std::set<std::string> fired;
    for(std::size_t i=0;i<runtime.scenarioCount();++i){const auto& o=runtime.observations()[i];assert(o.ticks==frames&&o.firstFrame==1&&o.lastFrame==frames);assert(o.draws==frames+1&&o.buttonFailures==0);emissions+=o.playerEmissions;damage+=o.damage;kills+=o.kills;if(o.playerEmissions)++progressed;if(o.missionComplete)++complete;if(i<55&&o.level>o.initialLevel)++levelUps;for(const auto& part:o.emittedWeaponParts)fired.insert(part);
        std::cout<<"SCENARIO "<<i<<" role="<<int(runtime.scenarios()[i].role)<<" ticks="<<o.ticks<<" interval="<<o.firstFrame<<":"<<o.lastFrame<<" emissions="<<o.playerEmissions<<" damage="<<o.damage<<" kills="<<o.kills<<" level="<<o.level<<" xp="<<o.experience<<" credit="<<o.credit<<" saves="<<o.saves<<" mission="<<o.missionComplete<<" weaponDefinitions="<<o.emittedWeaponParts.size()<<"\n";}
    assert(metrics.frames==frames&&metrics.commonFrameInterval&&metrics.activeWeaponDefinitions==fired.size()&&metrics.activeWeaponKinds>=5&&fired.size()>=5&&emissions>0&&damage>0&&kills>0);
    assert(metrics.menuActionFrames==9&&metrics.menuFramesWithCombatActivity==metrics.menuActionFrames);
    assert(metrics.maxConcurrentWeaponKinds>=5&&metrics.framesWithFiveWeaponKinds>0);
    assert(metrics.maxConcurrentWeaponDefinitions>=5&&metrics.maxConcurrentEmittingScenarios>=5&&metrics.framesWithFiveWeaponDefinitions>0);
    assert(runtime.frameActivity().size()==frames);for(const auto& activity:runtime.frameActivity()){assert(activity.tickedScenarios==59);std::cout<<"FRAME "<<activity.frame<<" ticked="<<activity.tickedScenarios<<" emittingScenarios="<<activity.emittingScenarios<<" weaponDefinitions="<<activity.emittedWeaponParts.size()<<" weaponKinds="<<activity.emittedWeaponKinds.size()<<" buttons="<<activity.buttons.size()<<" activeProjectiles="<<activity.activeProjectiles<<" combatProjectiles="<<activity.combatActiveProjectiles<<" combatEmit="<<activity.combatEmittingScenarios<<" combatDamage="<<activity.combatDamage<<" restart="<<activity.saveRestart<<" emissions="<<activity.emissions<<" damage="<<activity.damage<<" kills="<<activity.kills;for(const auto& part:activity.emittedWeaponParts)std::cout<<" "<<part;std::cout<<"\n";}
    for(std::size_t i=0;i<runtime.scenarioCount();++i)for(const auto& [part,count]:runtime.observations()[i].weaponEmissions)std::cout<<"WEAPON scenario="<<i<<" part="<<part<<" emissions="<<count<<"\n";
    assert(runtime.observations()[55].shopPurchased&&runtime.observations()[55].insufficientRejected);
    assert(runtime.observations()[56].missionAccepted&&runtime.observations()[56].missionComplete);
    assert(runtime.observations()[57].restarted&&host.restarts[57]==1);
    assert(runtime.observations()[0].level>=1&&runtime.observations()[0].experience>299);
    assert(files.reads==warmReads&&files.probes==warmProbes&&metrics.warmReadMisses==0&&metrics.warmExistsMisses==0);
    const auto& moving=runtime.observations()[58];
    std::cout<<"MOVEMENT distance="<<moving.distanceMoved<<" displacement="<<moving.maxDisplacement<<" movementFrames="<<moving.movementFrames<<" nav="<<moving.navigationRequests<<" rejected="<<moving.navigationRejected<<" blocked="<<moving.blockedFrames<<" drops="<<moving.maxObservedDrops<<" pickups="<<moving.pickupEvents<<" coins="<<moving.coinPickups<<" creditGain="<<moving.pickupCreditGain<<" inventoryGain="<<moving.inventoryGain<<" injury="<<moving.injuryPoints<<" movementOverlap="<<metrics.movementFramesWithCombatActivity<<" pickupOverlap="<<metrics.pickupFramesWithCombatActivity<<" movementFireOverlap="<<metrics.movementEmissionFramesWithCombatActivity<<"\n";
    for(const auto& route:moving.navigation)std::cout<<"ROUTE frame="<<route.frame<<" player="<<route.player.x<<","<<route.player.y<<" target="<<route.world.x<<","<<route.world.y<<" screen="<<route.screen.x<<","<<route.screen.y<<" code="<<route.lootCode<<" accepted="<<route.accepted<<" blocked="<<route.blocked<<"\n";
    for(const auto& item:moving.pickups)std::cout<<"PICKUP frame="<<item.frame<<" code="<<item.code<<" value="<<item.value<<" player="<<item.player.x<<","<<item.player.y<<" item="<<item.item.x<<","<<item.item.y<<" deltaCr="<<item.creditDelta<<"\n";
    assert(runtime.scenarios()[58].role==validation::ScenarioRole::movementLoot&&moving.navigationRequests>0&&moving.distanceMoved>30&&moving.maxDisplacement>30&&moving.playerEmissions>0&&moving.kills>0);
    assert(moving.coinPickups>0&&moving.pickupCreditGain>0&&moving.credit==moving.initialCredit+moving.pickupCreditGain&&moving.experience>moving.initialExperience);
    assert(metrics.movementFramesWithCombatActivity>0&&metrics.pickupFramesWithCombatActivity>0&&metrics.movementEmissionFramesWithCombatActivity>0);
    for(const auto& item:moving.pickups)assert(std::abs(item.player.x-item.item.x)<30&&std::abs(item.player.y-item.item.y)<30);
    const auto priorTicks=runtime.observations()[0].ticks;runtime.setActive(false);runtime.tick();assert(runtime.observations()[0].ticks==priorTicks&&runtime.metrics().frames==frames);runtime.setActive(true);
    const auto missing=runtime.missingFeatures();assert(std::find(missing.begin(),missing.end(),"quick-item actual use")!=missing.end());
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    std::cout<<"HM2_CONCURRENT PASS frames="<<frames<<" scenarios=59 combat=55 originalPlayerWeapons=110 firedDefinitions="<<fired.size()<<" activeWeaponKinds="<<metrics.activeWeaponKinds<<" maxSameFrameDefinitions="<<metrics.maxConcurrentWeaponDefinitions<<" maxSameFrameKinds="<<metrics.maxConcurrentWeaponKinds<<" maxSameFrameScenarios="<<metrics.maxConcurrentEmittingScenarios<<" framesFiveDefinitions="<<metrics.framesWithFiveWeaponDefinitions<<" framesFiveKinds="<<metrics.framesWithFiveWeaponKinds<<" menuFrames="<<metrics.menuActionFrames<<" menuWithCombat="<<metrics.menuFramesWithCombatActivity<<" emittingScenariosEver="<<progressed<<" emissions="<<emissions<<" damage="<<damage<<" kills="<<kills<<" completedMissions="<<complete<<" levelUps="<<levelUps<<" warmReads="<<metrics.warmReadMisses<<" warmExists="<<metrics.warmExistsMisses<<" seconds="<<seconds<<"\n";
}

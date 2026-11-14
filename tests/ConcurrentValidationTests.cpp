#include "PreparedTextStub.hpp"
#include "Core/Validation/ConcurrentValidation.hpp"
#include "Core/Validation/SessionFixtures.hpp"
#include "Core/Validation/HUDLayoutExercise.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
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
    // Optional Native-equivalent CPU UI host. Only glyph rasterization and
    // output devices are mocked; actual Screens/pointers/shared RNG remain.
    struct UIHost:ui::ScreensHost {
        Host& owner;unsigned index;
        UIHost(Host& value,unsigned i):owner(value),index(i){}
        std::optional<Bytes> loadHUDLayout(std::string_view profile)override{const auto key=std::pair{index,std::string(profile)};const auto i=owner.hudSettings.find(key);return i==owner.hudSettings.end()?std::nullopt:std::optional<Bytes>(i->second);}
        bool saveHUDLayout(std::string_view profile,std::span<const std::uint8_t> bytes)override{owner.hudSettings[{index,std::string(profile)}]={bytes.begin(),bytes.end()};return true;}
        void action(const ui::ScreenAction& a)override{if(owner.preparing)return;++owner.interfaces.at(index)->events[{a.button.key.id,a.button.key.intData}];owner.runtime->session(index).action(a);owner.refresh(index);}
        ui::PreparedScreenText prepareText(const ui::TextRasterRequest& r)override{return test::preparedTextStub(r);}
        void bgm(std::string_view)override{}
        void sound(std::string_view file)override{if(!owner.preparing)owner.sound(index,file,{});}
        void sfx(unsigned id)override{if(!owner.preparing)owner.soundID(index,id,{});}
    };
    struct UI {
        UIHost host;PreparedTiles tiles;std::unique_ptr<ui::Screens> screens;std::uint64_t revision=0;
        std::uint64_t ownerEpoch=0,observedOwner=0,observedGeneration=0,observedTiles=0,resourceEpoch=0;
        std::map<std::pair<unsigned,int>,std::uint64_t> events;
        UI(Host& owner,unsigned index):host(owner,index),tiles("validationUI"+std::to_string(index)){}
    };
    bool nativeUI=false,preparing=true;std::vector<std::unique_ptr<UI>> interfaces;
    std::map<std::pair<unsigned,std::string>,Bytes> hudSettings;
    std::map<unsigned,std::uint64_t> uiRandomCalls;
    static void prepare(ui::Screens& screens,PreparedTiles& tiles,float time){Frame frame;screens.append(frame,time);screens.appendPreparation(frame);}
    void refresh(unsigned index){
        auto& view=*interfaces.at(index);auto& game=runtime->session(index);const auto& state=game.state();
        if(!view.screens){++view.ownerEpoch;view.screens=std::make_unique<ui::Screens>(runtime->scenarioResources(index),view.tiles,view.host);view.screens->setDisplay(game.display());}
        if(view.revision!=game.presentationRevision()){view.screens->refresh(state,game.renderTime());view.revision=game.presentationRevision();prepare(*view.screens,view.tiles,game.renderTime());(void)game.preparationFrame();}
        else view.screens->updateHUD(state);
    }
    void prepareNativeUI(){
        assert(runtime&&nativeUI);interfaces.reserve(runtime->scenarioCount());for(unsigned i=0;i<runtime->scenarioCount();++i)interfaces.push_back(std::make_unique<UI>(*this,i));
        for(unsigned i=0;i<runtime->scenarioCount();++i){refresh(i);(void)runtime->session(i).preparationFrame();}
        // Same real builders and order as ConcurrentRuntime::prewarmMenus.
        UI temporary(*this,0);ui::Screens screens(runtime->cachedResources(),temporary.tiles,temporary.host);
        const auto base=runtime->session(0).state();auto state=base;
        for(unsigned id=1;id<=7;++id){state=base;state.screen=ui::ScreenId(id);screens.refresh(state);prepare(screens,temporary.tiles,0);}
        for(const auto* name:{"Base001","Base002","Base003","Base004","Base005","Base006"}){state=base;state.screen=ui::ScreenId::base;state.baseId=name;screens.refresh(state);prepare(screens,temporary.tiles,0);}
        state=base;state.screen=ui::ScreenId::base;state.selectedBaseButton=1;
        screens.refresh(state);prepare(screens,temporary.tiles,0);
        for(unsigned panel=0;panel<4;++panel){state=base;state.screen=ui::ScreenId::field;state.inventoryVisible=panel==0;state.missionVisible=panel==1;state.airliftVisible=panel==2;state.systemVisible=panel==3;screens.refresh(state);prepare(screens,temporary.tiles,0);}
        // Prepare the actual owned107 chooser inventory without executing
        // an equipment action or counting preparation as a passing scenario.
        for(unsigned side=0;side<2;++side){state=(*runtime).session(107).state();state.weaponChoiceType=2+side;state.weaponChoices.clear();for(const auto& item:state.inventory)if(item.originalType==10&&item.count>0&&!item.equipped){auto choice=item;choice.attackDelta=choice.attackPoint-(side?state.equipmentStats.attackRight:state.equipmentStats.attackLeft);state.weaponChoices.push_back(std::move(choice));}screens.refresh(state);prepare(screens,temporary.tiles,0);}
        state=base;state.screen=ui::ScreenId::shop;state.inventoryVisible=true;state.selectedShop=0;state.tradeVisible=true;state.tradeItemId="01001";state.tradeCount=1;state.tradePrice=10;screens.refresh(state);prepare(screens,temporary.tiles,0);
        for(unsigned i=0;i<runtime->scenarioCount();++i)if(runtime->scenarios()[i].role==validation::ScenarioRole::mission){state=runtime->session(i).state();state.screen=ui::ScreenId::command;state.selectedMission=0;if(!state.availableMissions.empty()){state.missionTitle=state.availableMissions[0].name;state.missionBody=state.availableMissions[0].description;}screens.refresh(state);prepare(screens,temporary.tiles,0);break;}
        MapLoading loading(runtime->cachedResources());(void)loading.preparationFrame();ui::Help help(runtime->cachedResources(),"kr",-1);(void)help.preparationFrame();
        for(const auto& popup:validation::completedMissionPreparation(runtime->cachedResources())){state=runtime->session(114).state();state.screen=ui::ScreenId::command;state.commandListVisible=false;state.completedMission=popup;screens.refresh(state);prepare(screens,temporary.tiles,0);}
        state=runtime->session(114).state();state.screen=ui::ScreenId::command;state.commandListVisible=true;screens.refresh(state);prepare(screens,temporary.tiles,0);
        preparing=false;
    }
    std::uint64_t scenarioUIResourceRevision(unsigned index)override{
        if(index>=interfaces.size()||!interfaces[index])return 0;auto& view=*interfaces[index];
        const auto generation=view.screens?view.screens->resourceGeneration():0,tiles=view.screens?view.screens->preparedResourceRevision():0;
        if(view.observedOwner!=view.ownerEpoch||view.observedGeneration!=generation||view.observedTiles!=tiles){++view.resourceEpoch;view.observedOwner=view.ownerEpoch;view.observedGeneration=generation;view.observedTiles=tiles;}
        return view.resourceEpoch;
    }
    void prepareHUDRegressionUI(){
        const unsigned index=119;
        if(interfaces.size()<runtime->scenarioCount())interfaces.resize(runtime->scenarioCount());
        if(!interfaces[index])interfaces[index]=std::make_unique<UI>(*this,index);
        refresh(index);
        UI temporary(*this,index);ui::Screens screens(runtime->cachedResources(),temporary.tiles,temporary.host);screens.setDisplay(runtime->session(index).display());
        auto state=runtime->session(index).state();state.systemVisible=true;screens.refresh(state);screens.beginHUDEdit();prepare(screens,temporary.tiles,0);
        preparing=false;
    }
    void exerciseHUDLayout(unsigned index,GameSession& game,validation::HUDLayoutObservation& observed,std::uint64_t frame)override{
        if(frame==340){interfaces.at(index)->screens.reset();interfaces.at(index)->revision=0;}
        refresh(index);validation::exerciseHUDLayoutFrame(*interfaces.at(index)->screens,game,observed,frame);
    }
    void tickScenarioUI(unsigned index,GameSession& game,WorldRandom& random)override{
        if(!nativeUI)return;refresh(index);
        struct Counted:WorldRandom{WorldRandom& source;std::uint64_t& count;Counted(WorldRandom& s,std::uint64_t& n):source(s),count(n){}std::uint32_t random()override{++count;return source.random();}} counted(random,uiRandomCalls[index]);
        interfaces.at(index)->screens->tick(game.renderTime(),counted);
    }

    std::map<unsigned,std::set<std::string>> pickupLabels;
    ui::PreparedScreenText prepareWorldText(unsigned index,const ui::TextRasterRequest& r)override{
        if(r.sourceId=="Player.pickup")pickupLabels[index].insert(r.text);
        return test::preparedTextStub(r);
    }
    validation::ConcurrentValidation* runtime=nullptr;
    std::map<unsigned,Bytes> saved;std::map<unsigned,std::uint64_t> sounds,saves,presses,uiDraws,restarts;
    void sound(unsigned i,std::string_view,Vec2)override{++sounds[i];}
    void soundID(unsigned i,unsigned,Vec2)override{++sounds[i];}
    bool writeSave(unsigned i,std::span<const std::uint8_t> bytes,SaveReason)override{saved[i]={bytes.begin(),bytes.end()};++saves[i];return true;}
    std::uint32_t absoluteTimeLow(unsigned i)override{return 97+i;}
    std::uint32_t unixTime()override{return 1794268800;}
    bool pressButton(unsigned i,unsigned id,int data=0)override{
        assert(runtime);++presses[i];
        if(nativeUI){refresh(i);auto& view=*interfaces.at(i);auto& game=runtime->session(i);
            for(const auto& key:view.screens->buttons())if(key.id==id&&key.intData==data){auto button=view.screens->button(key);if(!button||!button->visible||!button->active)continue;
                const auto eventKey=std::pair{id,data};const auto before=view.events[eventKey];view.screens->revealButton(key);const auto point=view.screens->buttonScreenPosition(key);const std::uintptr_t pointer=10000+i;
                if(!game.inputOverlay(PointerPhase::begin,point,pointer))view.screens->input(PointerPhase::begin,point,pointer,game.renderTime());
                if(!game.inputOverlay(PointerPhase::end,point,pointer))view.screens->input(PointerPhase::end,point,pointer,game.renderTime());return view.events[eventKey]>before;
            }return false;
        }
        auto& session=runtime->session(i);const auto& state=session.state();std::string item;
        if(id>=6200&&id<6205)item=state.quick[id-6200].dataId;
        if(id==1024)for(const auto& entry:state.inventory)if(entry.position==data)item=entry.dataId;
        if(id>=5100&&id<5105&&id-5100<state.availableMissions.size())item=state.availableMissions[id-5100].dataId;
        if(id>=2200&&id<2224&&id-2200<state.shopItems.size())item=state.shopItems[id-2200].dataId;
        if(id==5004&&state.selectedMission>=0&&std::size_t(state.selectedMission)<state.availableMissions.size())item=state.availableMissions[std::size_t(state.selectedMission)].dataId;
        if(id==5007)item=state.completedMission.dataId;
        // CPU exercises the original GameSession action consumer. Native hosts
        // additionally execute Screens pointer begin/end at the real key center.
        for(const auto phase:{ui::ButtonEventType::push,ui::ButtonEventType::pop})session.action({state.screen,ui::Panel::root,{phase,{0,id,data,item},1,{}}});return true;
    }
    void sessionRestarted(unsigned i,GameSession&)override{++restarts[i];if(nativeUI){interfaces.at(i)->screens.reset();interfaces.at(i)->revision=0;}}
    void appendScenarioUI(unsigned i,GameSession& game,Frame& frame)override{
        ++uiDraws[i];if(nativeUI){refresh(i);interfaces.at(i)->screens->append(frame,game.renderTime());}if(i)return;
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
    const auto source=runtime.scenarioFrame(0);const auto& original=find(source);assert(original.center.x==10&&original.center.y==20&&original.strip[0].position.x==4&&original.strip[0].position.y==7&&runtime.metrics().drawFrames==draws&&host.uiDraws==drawsHost);
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
void resourceAccounting(validation::ConcurrentValidation& runtime){
    const auto m=runtime.metrics();std::cout<<"RESOURCE_OPERATIONS preparationOps="<<m.resourcePreparationOperations<<" preparationReads="<<m.preparationReadRequests<<" preparationExists="<<m.preparationExistsRequests<<" steadyReads="<<m.steadyReadRequests<<" steadyExists="<<m.steadyExistsRequests<<" rawWarmReadMisses="<<m.warmReadMisses<<" rawWarmExistsMisses="<<m.warmExistsMisses<<" sourceBoundaryReads="<<m.preparationReadMisses<<" sourceBoundaryExists="<<m.preparationExistsMisses<<" sourceSteadyReads="<<m.steadyReadMisses<<" sourceSteadyExists="<<m.steadyExistsMisses<<std::endl;
    for(const auto& row:runtime.resourceActivity())if(!row.preparation||!row.sourceReadMisses.empty()||!row.sourceExistsMisses.empty()){
        std::cout<<"RESOURCE_OPERATION frame="<<row.frame<<" case="<<row.scenario<<" stage="<<row.stage<<" preparation="<<row.preparation<<" game="<<row.gameRevisionBefore<<":"<<row.gameRevisionAfter<<" ui="<<row.uiRevisionBefore<<":"<<row.uiRevisionAfter<<" replaced="<<row.gameOwnerChanged;for(const auto& name:row.reads)std::cout<<" read="<<name;for(const auto& name:row.exists)std::cout<<" exists="<<name;std::cout<<std::endl;
    }
    assert(m.resourcePreparationOperations>0&&m.preparationReadRequests>0&&m.preparationExistsRequests>0);
    assert(m.warmReadMisses==m.preparationReadMisses+m.steadyReadMisses&&m.warmExistsMisses==m.preparationExistsMisses+m.steadyExistsMisses);
    assert(m.steadyReadRequests==0&&m.steadyExistsRequests==0&&m.steadyReadMisses==0&&m.steadyExistsMisses==0);
    // Original1029 creates a new GuiHelp/ResMgr owner even when the QA byte
    // memo already contains its four pictures. Its live owner must advance
    // the same resource epoch used by GPU preparation, not just UI activity.
    const auto help=std::find_if(runtime.resourceActivity().begin(),runtime.resourceActivity().end(),[](const auto& r){return r.frame==182&&r.scenario==111&&r.stage=="schedule";});
    assert(help!=runtime.resourceActivity().end()&&help->preparation&&help->gameRevisionAfter>help->gameRevisionBefore);
    const std::set<std::string> helpNames{"HelpScr01_kr.png","HelpScr02_kr.png","HelpScr03_kr.png","HelpHelp.png"};
    assert(help->reads.size()==4&&help->exists.size()==4&&std::set<std::string>(help->reads.begin(),help->reads.end())==helpNames&&std::set<std::string>(help->exists.begin(),help->exists.end())==helpNames);
}
void recentRegressions(validation::ConcurrentValidation& runtime){
    std::uint64_t synchronizedFrame=0;
    for(unsigned i=121;i<validation::concurrentScenarioCount;++i){const auto& o=runtime.observations()[i].synchronizedWeapon;
        std::cout<<"SYNC_WEAPON case="<<i<<" pause="<<o.pausedFrame<<" resume="<<o.resumedFrame<<" emit="<<o.emittedFrame<<" counts="<<o.emissionsAtPause<<":"<<o.emissionsAtResume<<":"<<o.emissionsAfter<<std::endl;
        assert(o.pausedFrame&&o.resumedFrame>o.pausedFrame&&o.emittedFrame==o.resumedFrame&&o.emissionsAfter>o.emissionsAtResume&&o.emissionsAtPause==o.emissionsAtResume&&o.pausedWorldTime==o.resumedWorldTime);
        if(synchronizedFrame)assert(synchronizedFrame==o.resumedFrame);else synchronizedFrame=o.resumedFrame;
    }

    const auto metrics=runtime.metrics();assert(metrics.recentInputFramesWithCombatActivity>0&&metrics.hudFramesWithCombatActivity>0&&metrics.lifecycleFramesWithCombatActivity>0);
    for(unsigned index:{115u,116u}){const auto& o=runtime.observations()[index].recentInput;
        std::cout<<"PATH_REPLACE index="<<index<<" first="<<o.firstPathPoints<<" replacement="<<o.replacementPathPoints<<" dash="<<o.dashBefore<<":"<<o.dashAfter<<" SP="<<o.specialBefore<<":"<<o.specialAfter<<" inputs="<<o.inputs.size()<<std::endl;
        assert(o.firstPathPoints>=3&&o.replacementPathPoints>=(index==115?1u:3u)&&o.dashFrame==150&&o.dashAfter==o.dashBefore+1&&o.specialAfter==o.specialBefore-100&&o.savesBefore==o.savesAfter);
    }
    const auto& lifecycle=runtime.observations()[118].recentInput;
    assert(lifecycle.interruptedFrame==400&&lifecycle.reactivatedFrame==450&&lifecycle.resumedFrame==470&&lifecycle.inactivePaused&&lifecycle.inactiveMenu&&lifecycle.reactivatedPaused&&lifecycle.reactivatedMenu&&lifecycle.manuallyResumed);
    assert(lifecycle.worldTimeAtInterrupt==lifecycle.worldTimeAtReactivate&&lifecycle.worldTimeAtReactivate==lifecycle.worldTimeBeforeResume&&lifecycle.worldTimeAfterResume>lifecycle.worldTimeBeforeResume&&lifecycle.savesBefore==lifecycle.savesAfter);
    const auto& hud=runtime.observations()[119].hudLayout;std::cout<<"HUD_CONCURRENT menu="<<hud.menuOpened<<" editor="<<hud.editorOpened<<" dragged="<<hud.dragMoved<<" applied="<<hud.applied<<" reloaded="<<hud.reloaded<<" resumed="<<hud.resumed<<" actualInputs="<<hud.inputs<<std::endl;
    assert(hud.menuOpened&&hud.editorOpened&&hud.dragMoved&&hud.applied&&hud.reloaded&&hud.resumed&&hud.inputs>=7);
    const auto& samples=runtime.observations()[117].recentInput.zoom;assert(samples.size()==6&&samples[1].target&&samples[3].target);
    float lo=samples.front().zoom,hi=lo;for(const auto& sample:samples){lo=std::min(lo,sample.zoom);hi=std::max(hi,sample.zoom);assert(sample.output.width==samples.front().output.width&&sample.output.height==samples.front().output.height);std::cout<<"ZOOM_CONCURRENT frame="<<sample.frame<<" selected="<<sample.target<<" zoom="<<sample.zoom<<" world="<<sample.viewport.width<<":"<<sample.viewport.height<<" output="<<sample.output.width<<":"<<sample.output.height<<std::endl;}
    if(runtime.session(117).display().focusZoom)assert(hi-lo>.01f);else assert(lo==1&&hi==1);
    const auto& idle=runtime.observations()[120].recentInput.zoom;assert(idle.size()>=6);
    for(const auto& sample:idle){assert(sample.zoom==1&&sample.currentZoom==1&&sample.targetZoom==1&&!sample.target&&!sample.pathCount);const auto& config=runtime.session(120).display();const auto extent=config.enabled?(config.compact?ClipRect{0,0,config.canvas.width,config.canvas.height}:config.content()):ClipRect{0,0,768,1024};assert(sample.viewport.width==extent.width&&sample.viewport.height==extent.height);}
    std::cout<<"CAMERA_IDLE_CONCURRENT samples="<<idle.size()<<" scale="<<idle.back().zoom<<" viewport="<<idle.back().viewport.width<<":"<<idle.back().viewport.height<<std::endl;

}
void printCase38Weapons(Files& files){
    const auto items=DataTable::parse(files.read("ItemInfo.tble"));const auto weapons=Weapons::parse(DataTable::parse(files.read("WeaponInfo.tble")));const auto machines=Machines::parse(DataTable::parse(files.read("MachList.tble")));
    for(const auto* code:{"10016","10139","10011","10111"})for(std::size_t row=0;row<items.rowCount();++row)if(items.value(row,"Code")==code){const auto& w=weapons.weapon(items.value(row,"strParam").value());std::cout<<"CASE38_WEAPON code="<<code<<" part="<<w.part<<" kind="<<int(w.kind)<<" damage="<<w.damage<<" range="<<w.shotRange<<" shotCount="<<unsigned(w.shotCount)<<" shotDelay="<<w.shotDelay<<" reloadDelay="<<w.reloadDelay<<std::endl;}
    for(const auto* name:{"Player","E_Tank_01","E_Tank_02"}){const auto& m=machines.machine(name);std::cout<<"CASE38_MACHINE name="<<name<<" hp="<<m.hp<<" radius="<<m.radius<<std::endl;}
}
void printBotWeapons(Files& files){
    const auto models=Models::parse(Chunk::parse(files.read("PartsInfo.mcb")),Chunk::parse(files.read("MachInfo.mcb")));
    const auto weapons=Weapons::parse(DataTable::parse(files.read("WeaponInfo.tble")));
    const auto maps=Maps::parse(Chunk::parse(files.read("MapInfo.mcb")));
    std::set<std::string> unique;for(const auto& spawn:maps.map("Stage01_01").spawners)unique.insert(spawn.model);
    for(const auto& name:unique)for(const auto& mount:models.modelMounts(name)){const auto found=weapons.all().find(mount.part);if(found==weapons.all().end())continue;const auto& w=found->second;
        std::cout<<"BOT_THREAT_DATA model="<<name<<" part="<<mount.part<<" kind="<<int(w.kind)<<" damage="<<w.damage<<" damageRange="<<w.damageRange<<" shotRange="<<w.shotRange<<" shotCount="<<unsigned(w.shotCount)<<" shell="<<w.shell<<" bulletImage="<<w.bulletImage<<" effect="<<int(w.effect)<<std::endl;
    }
}
}
int main(int argc,char** argv){
    if(argc<2)return 77;const auto begin=std::chrono::steady_clock::now();Files files(argv[1]);if(argc>2&&std::string_view(argv[2])=="--bot-weapons"){printBotWeapons(files);return 0;}if(argc>2&&std::string_view(argv[2])=="--case38-weapons"){printCase38Weapons(files);return 0;}const bool displayTest=argc>2&&std::string_view(argv[2]).starts_with("--display-");Host host;host.nativeUI=displayTest||argc>2&&std::string_view(argv[2])=="--native-ui";validation::ConcurrentValidation runtime(files,host);host.runtime=&runtime;if(displayTest){const auto flag=std::string_view(argv[2]);runtime.setDisplay((flag=="--display-phone"||flag=="--display-phone-2100")?DisplayConfiguration::surface({852,393},{59,21,59,0}):(flag=="--display-phone-air-actual"||flag=="--display-phone-air-actual-2100")?DisplayConfiguration::surface({912,420},{68,20,68,0}):flag=="--display-phone-air"?DisplayConfiguration::surface({912,420},{62,21,62,0}):flag=="--display-phone-air-full"?DisplayConfiguration::surface({912,420}):flag=="--display-phone-portrait"?DisplayConfiguration::surface({420,912},{0,34,0,62}):DisplayConfiguration::surface({1024,1366},{0,20,0,24}));}if(host.nativeUI)host.prepareNativeUI();host.prepareHUDRegressionUI();
    if(displayTest){
        const bool extendedDiagnostic=std::string_view(argv[2])=="--display-phone-air-actual-2100"||std::string_view(argv[2])=="--display-phone-2100";
        const auto frames=extendedDiagnostic?2100u:validation::displayValidationFrames(runtime.session(0).display());
        for(unsigned i=0;i<frames;++i){runtime.tick();const auto frame=runtime.drawFrame();assert(!frame.sprites.empty());
            if(extendedDiagnostic&&(i+1==1800||i+1==2100))for(unsigned index=68;index<105;++index)if(runtime.scenarios()[index].map=="Stage03_04"){
                const auto& o=runtime.observations()[index];auto* battle=runtime.session(index).battle();
                std::cout<<std::setprecision(12)<<"WINDOW_DIAGNOSTIC frame="<<i+1<<" canvas="<<runtime.session(index).display().canvas.width<<":"<<runtime.session(index).display().canvas.height<<" map=Stage03_04 killFrame="<<o.firstKillFrames[2]<<" killWorld="<<o.firstKillWorldTimes[2]<<" scheduledCrash="<<o.firstCrashDeadlines[2]<<" worldNow="<<(battle?battle->clock().worldTime():-1)<<" removalFrame="<<o.firstRemovalFrames[2]<<" removalWorld="<<o.firstRemovalWorldTimes[2]<<" kills="<<o.enemyKills[2]<<" removed="<<o.enemyRemovals[2]<<std::endl;
            }
        }
        const auto m=runtime.metrics();unsigned failed=0;for(unsigned i=0;i<runtime.scenarioCount();++i){const auto& o=runtime.observations()[i];failed+=o.buttonFailures;std::cout<<"DISPLAY_CASE "<<i<<" ticks="<<o.ticks<<" buttonFailures="<<o.buttonFailures<<" emissions="<<o.playerEmissions<<" saves="<<o.saves<<std::endl;assert(o.ticks==frames);const auto frame=runtime.scenarioFrame(i);assert(frame.width==runtime.session(i).display().canvas.width);}
        for(unsigned i=68;i<105;++i)if(runtime.scenarios()[i].map=="Stage01_02"||runtime.scenarios()[i].map=="Stage02_07"||runtime.scenarios()[i].map=="Stage03_04"){const auto& o=runtime.observations()[i];const unsigned k=runtime.scenarios()[i].map=="Stage01_02"?1:2;std::cout<<"DISPLAY_FIELD map="<<runtime.scenarios()[i].map<<" emissions="<<o.enemyEmissions[k]<<" damage="<<o.enemyDamage[k]<<" kills="<<o.enemyKills[k]<<" removed="<<o.enemyRemovals[k]<<" killFrame="<<o.firstKillFrames[k]<<" removalFrame="<<o.firstRemovalFrames[k]<<std::endl;assert(o.enemyEmissions[k]>0&&o.enemyDamage[k]>0&&o.enemyKills[k]>0&&o.enemyRemovals[k]>0);}
        std::cout<<"DISPLAY_CONCURRENT frames="<<m.frames<<" scenarios="<<runtime.scenarioCount()<<" weapons="<<m.activeWeaponDefinitions<<" kinds="<<m.activeWeaponKinds<<" buttonFailures="<<failed<<" warmReads="<<m.warmReadMisses<<" warmExists="<<m.warmExistsMisses<<" commonInterval="<<m.commonFrameInterval<<" maxSameFrameKinds="<<m.maxConcurrentWeaponKinds<<std::endl;
        if(m.maxConcurrentWeaponKinds<5)for(const auto& activity:runtime.frameActivity())if(activity.emittedWeaponKinds.size()>=4||std::find(activity.emittedWeaponKinds.begin(),activity.emittedWeaponKinds.end(),WeaponKind::railgun)!=activity.emittedWeaponKinds.end()||std::find(activity.emittedWeaponKinds.begin(),activity.emittedWeaponKinds.end(),WeaponKind::sniper)!=activity.emittedWeaponKinds.end()){std::cout<<"DISPLAY_KIND_FRAME "<<activity.frame;for(auto kind:activity.emittedWeaponKinds)std::cout<<" "<<int(kind);std::cout<<" parts=";for(const auto& part:activity.emittedWeaponParts)std::cout<<part<<",";std::cout<<std::endl;}
        for(unsigned i=0;i<55;++i){const auto& o=runtime.observations()[i];
            for(const auto& g:o.combatGuide)std::cout<<"COMBAT_GUIDE case="<<i<<" frame="<<g.frame<<" pending="<<g.pendingPart<<" range="<<g.pendingRange<<" living="<<g.living<<" visible="<<g.visible<<" target="<<g.target<<" distance="<<g.distance<<" player="<<g.player.x<<":"<<g.player.y<<" targetPos="<<g.targetPosition.x<<":"<<g.targetPosition.y<<" viewport="<<g.viewport.x<<":"<<g.viewport.y<<" action="<<g.action<<" accepted="<<g.accepted<<" selected="<<g.selectedAfter<<" queuedInputs="<<g.queuedTargets.size()<<std::endl;
            if(o.emittedWeaponParts.size()<2){std::cout<<"DISPLAY_WEAPON_GAP case="<<i<<" equip=";for(const auto& code:runtime.scenarios()[i].weapons)std::cout<<code<<",";std::cout<<" fired=";for(const auto& part:o.emittedWeaponParts)std::cout<<part<<",";if(auto* scene=runtime.session(i).battle()){std::cout<<" hp="<<scene->playerHP()<<" selected="<<scene->selectedEnemy();for(const auto& w:scene->playerWeapons())std::cout<<" controller="<<w.part<<":"<<w.fireLeft<<":"<<w.fireTime; }std::cout<<std::endl;}}
        {auto& game=runtime.session(107);const auto& o=runtime.observations()[107];const auto* scene=game.battle();std::cout<<"PICKER_DIAGNOSTIC kills="<<o.kills<<" nav="<<o.navigationRequests<<" rejects="<<o.navigationRejected<<" distance="<<o.distanceMoved<<" player="<<scene->player().position.x<<","<<scene->player().position.y<<" blocked="<<scene->player().blocked<<" drops="<<game.fieldItems().size()<<std::endl;for(const auto& route:o.navigation)std::cout<<"PICKER_POINT "<<route.frame<<" from="<<route.player.x<<","<<route.player.y<<" to="<<route.world.x<<","<<route.world.y<<" why="<<route.lootCode<<std::endl;for(const auto& b:scene->bots())std::cout<<"PICKER_BOT slot="<<b.slot<<" pos="<<b.position.x<<","<<b.position.y<<" pick="<<b.pickItem<<" contacts="<<b.itemCaptures<<" searches="<<b.metrics.itemSearches<<std::endl;for(const auto& drop:game.fieldItems())std::cout<<"PICKER_DROP code="<<drop.code<<" pos="<<drop.position.x<<","<<drop.position.y<<" removed="<<drop.removed<<std::endl;}
        for(unsigned index:{106u,107u}){const auto& o=runtime.observations()[index];std::uint64_t incoming=0,contacts=0;for(const auto& b:o.bots){std::cout<<"DISPLAY_BOT case="<<index<<" slot="<<b.slot<<" emissions="<<b.emissions<<" damage="<<b.enemyHPRemoved<<" incoming="<<b.hpRemoved<<" contacts="<<b.itemCaptures<<" repair="<<b.repairAccepted<<std::endl;assert(b.spawned&&b.ticks&&b.rotorFrames&&b.repairAccepted&&b.repairAfter>b.repairBefore);if(index==106)assert(b.emissions&&b.enemyHPRemoved);else assert(b.itemCaptures);incoming+=b.hpRemoved;contacts+=b.itemCaptures;}assert(incoming&&o.botThreatEmissions&&o.distanceMoved>30);if(index==107){assert(contacts&&o.pickupEvents&&o.pickupCreditGain);for(const auto& swap:o.botWeaponSwaps){std::cout<<"DISPLAY_SWAP equip="<<swap.equipFrame<<" emission="<<swap.firstEmissionFrame<<std::endl;assert(swap.equipped&&swap.firstEmissionFrame>=swap.equipFrame&&swap.equipFrame);}}}
        const auto& death=runtime.observations()[112].gameOver;std::cout<<"DISPLAY_GAMEOVER death="<<death.deathFrame<<" request="<<death.requestFrame<<" base="<<death.baseFrame<<" enemyEmissions="<<death.enemyEmissions<<" damage="<<death.damageCalls<<std::endl;assert(death.deathFrame&&death.requestFrame&&death.baseFrame&&death.baseHP==256&&death.savedPayloadValid);
        const auto& moving=runtime.observations()[58];std::cout<<"DISPLAY_LOOT coins="<<moving.coinPickups<<" items="<<moving.itemPickups<<" inventoryGain="<<moving.inventoryGain<<std::endl;assert(moving.coinPickups>0&&moving.itemPickups>0&&moving.inventoryGain>0);
        resourceAccounting(runtime);recentRegressions(runtime);
        assert(failed==0&&m.commonFrameInterval&&m.activeWeaponDefinitions==110&&m.activeWeaponKinds==5&&m.maxConcurrentWeaponKinds>=5&&m.steadyReadRequests==0&&m.steadyExistsRequests==0&&m.steadyReadMisses==0&&m.steadyExistsMisses==0);return 0;
    }
    assert(runtime.scenarioCount()==validation::concurrentScenarioCount&&runtime.metrics().combatScenarios==55);std::set<std::string> itemCodes;
    const auto originalItems=DataTable::parse(runtime.cachedResources().read("ItemInfo.tble"));std::vector<std::string> originalWeaponCodes;for(std::size_t row=0;row<originalItems.rowCount();++row)if(originalItems.value(row,"Type")=="PtWpn")originalWeaponCodes.emplace_back(originalItems.value(row,"Code").value());assert(originalWeaponCodes.size()==110);
    for(std::size_t i=0;i<55;++i){const auto& descriptor=runtime.scenarios()[i];assert(descriptor.role==validation::ScenarioRole::combat&&descriptor.weapons.size()==2);for(unsigned side=0;side<2;++side){const auto& code=descriptor.weapons[side];assert(code==originalWeaponCodes[i*2+side]);itemCodes.insert(code);assert(runtime.session(unsigned(i)).state().equipment[side+2].dataId==code);}assert(runtime.session(unsigned(i)).battle());}
    assert(itemCodes.size()==110);const auto warmReads=files.reads,warmProbes=files.probes;
    compositor(runtime,host);
    const unsigned frames=host.nativeUI?(argc>3?unsigned(std::stoul(argv[3])):1200):(argc>2?unsigned(std::stoul(argv[2])):1200);
    std::size_t pickerRoutes=0;
    for(unsigned i=0;i<frames;++i){runtime.tick();const auto frame=runtime.drawFrame();assert(!frame.sprites.empty());if(i==30)compositor(runtime,host);
        if(host.nativeUI){const auto& observed=runtime.observations()[107];if(observed.navigation.size()!=pickerRoutes){pickerRoutes=observed.navigation.size();const auto& route=observed.navigation.back();const auto* scene=runtime.session(107).battle();const auto player=scene->player();std::cout<<"PICKER_ROUTE frame="<<i+1<<" threat="<<route.lootCode<<" player="<<player.position.x<<","<<player.position.y<<" state="<<player.state<<" blocked="<<player.blocked<<" ground="<<route.world.x<<","<<route.world.y<<" screen="<<route.screen.x<<","<<route.screen.y<<" accepted="<<route.accepted<<std::endl;auto enemies=scene->enemies();std::sort(enemies.begin(),enemies.end(),[&](const auto& a,const auto& b){const float ax=a.position.x-player.position.x,ay=a.position.y-player.position.y,bx=b.position.x-player.position.x,by=b.position.y-player.position.y;return ax*ax+ay*ay<bx*bx+by*by;});for(std::size_t j=0;j<std::min(std::size_t(3),enemies.size());++j){const auto& e=enemies[j];const float dx=e.position.x-player.position.x,dy=e.position.y-player.position.y;const auto screen=scene->camera().toScreen(e.position);std::cout<<"PICKER_NEAR frame="<<i+1<<" id="<<e.id<<" hp="<<e.hp<<" world="<<e.position.x<<","<<e.position.y<<" screen="<<screen.x<<","<<screen.y<<" distance="<<std::sqrt(dx*dx+dy*dy)<<" targetPlayer="<<e.targetingPlayer<<" chasing="<<e.moveToTarget<<std::endl;}}}
    }
    if(host.nativeUI){
        std::cout<<"NATIVE_UI_CPU_DIAGNOSTIC realScreens=1 realPointers=1 glyphStub=1 sharedUIRandom=1 frames="<<frames<<std::endl;
        for(unsigned index:{38u,58u,106u,107u}){const auto& o=runtime.observations()[index];std::cout<<"UI_PARITY scenario="<<index<<" uiRandomCalls="<<host.uiRandomCalls[index]<<" saves="<<o.saves<<" level="<<o.level<<" xp="<<o.experience<<" playerEmit="<<o.playerEmissions<<" kills="<<o.kills<<" nav="<<o.navigationRequests<<" rejects="<<o.navigationRejected<<" distance="<<o.distanceMoved<<" player="<<o.lastPosition.x<<","<<o.lastPosition.y<<" contacts="<<o.pickupEvents<<" creditGain="<<o.pickupCreditGain<<" inventoryGain="<<o.inventoryGain<<" threatEmit="<<o.botThreatEmissions<<std::endl;for(const auto& part:o.emittedWeaponParts)std::cout<<"UI_PARITY_WEAPON scenario="<<index<<" part="<<part<<std::endl;if(index==38&&runtime.session(index).battle())for(const auto& w:runtime.session(index).battle()->playerWeapons())std::cout<<"CASE38_CONTROLLER part="<<w.part<<" fireLeft="<<w.fireLeft<<" fireTime="<<w.fireTime<<" muzzle="<<w.currentMuzzle<<std::endl;}
    }
    // Print all appended results before any acceptance assertion so a failed
    // first BOT cannot hide the other five actual observations.
    for(unsigned index:{106u,107u}){const auto& v=runtime.observations()[index];std::cout<<"BOT_NAV_ACTUAL scenario="<<index<<" requests="<<v.navigationRequests<<" rejected="<<v.navigationRejected<<" distance="<<v.distanceMoved<<" maxDisplacement="<<v.maxDisplacement<<" threat="<<v.botThreatName<<" threatEmits="<<v.botThreatEmissions<<std::endl;}
    for(unsigned index:{106u,107u})for(const auto& b:runtime.observations()[index].bots)std::cout<<"BOT_ACTUAL scenario="<<index<<" code="<<b.code<<" machine="<<b.machine<<" ticks="<<b.ticks<<" removed="<<b.removed<<" lastSeen="<<b.lastObservedFrame<<" rotor="<<b.rotorFrames<<" emit="<<b.emissions<<" enemyHPRemoved="<<b.enemyHPRemoved<<" contacts="<<b.itemCaptures<<" damageCalls="<<b.damageCalls<<" HPRemoved="<<b.hpRemoved<<" repair="<<b.repairBefore<<":"<<b.repairAfter<<" repairFrame="<<b.repairFrame<<" repairCount="<<b.repairCountBefore<<":"<<b.repairCountAfter<<std::endl;
    for(const auto& swap:runtime.observations()[107].botWeaponSwaps)std::cout<<"BOT_WEAPON_SWAP code="<<swap.code<<" part="<<swap.part<<" raw="<<swap.inventoryPosition<<" choose="<<swap.chooserFrame<<" equip="<<swap.equipFrame<<" firstEmit="<<swap.firstEmissionFrame<<" count="<<swap.countBefore<<":"<<swap.countAfter<<" equipped="<<swap.equippedBefore<<":"<<swap.equippedAfter<<" result="<<swap.equipped<<" saves="<<swap.savesBefore<<":"<<swap.savesAfter<<std::endl;
    for(unsigned index=108;index<112;++index){const auto& h=runtime.observations()[index].help;std::cout<<"HELP_ACTUAL scenario="<<index<<" counter="<<h.initialCounter<<":"<<h.resultingCounter<<" firstPresent="<<h.initialPresent<<" layer="<<h.initialLayer<<" menu="<<h.menuOpened<<" inputs="<<h.inputs.size()<<" paused="<<h.pausedFrames<<" running="<<h.runningFrames<<" draws="<<h.drawFrames<<" guides="<<h.guideDrawFrames<<std::endl;}
    const auto metrics=runtime.metrics();std::uint64_t emissions=0,damage=0,kills=0;unsigned progressed=0,complete=0,levelUps=0;std::set<std::string> fired;
    for(std::size_t i=0;i<runtime.scenarioCount();++i){const auto& o=runtime.observations()[i];assert(o.ticks==frames&&o.firstFrame==1&&o.lastFrame==frames);assert(o.draws==frames+1&&o.buttonFailures==0);emissions+=o.playerEmissions;damage+=o.damage;kills+=o.kills;if(o.playerEmissions)++progressed;if(o.missionComplete)++complete;if(i<55&&o.level>o.initialLevel)++levelUps;for(const auto& part:o.emittedWeaponParts)fired.insert(part);
        if(i<55)for(const auto& code:runtime.scenarios()[i].weapons){std::string part;for(std::size_t row=0;row<originalItems.rowCount();++row)if(originalItems.value(row,"Code")==code){part=std::string(originalItems.value(row,"strParam").value());break;}assert(!part.empty()&&std::find(o.emittedWeaponParts.begin(),o.emittedWeaponParts.end(),part)!=o.emittedWeaponParts.end());}
        std::cout<<"SCENARIO "<<i<<" role="<<int(runtime.scenarios()[i].role)<<" ticks="<<o.ticks<<" interval="<<o.firstFrame<<":"<<o.lastFrame<<" emissions="<<o.playerEmissions<<" damage="<<o.damage<<" kills="<<o.kills<<" level="<<o.level<<" xp="<<o.experience<<" credit="<<o.credit<<" saves="<<o.saves<<" mission="<<o.missionComplete<<" weaponDefinitions="<<o.emittedWeaponParts.size()<<"\n";}
    assert(metrics.frames==frames&&metrics.commonFrameInterval&&metrics.activeWeaponDefinitions==fired.size()&&metrics.activeWeaponKinds==5&&fired.size()==110&&emissions>0&&damage>0&&kills>0);
    std::uint64_t originalMenuFrames=0,originalMenuOverlap=0;
    for(const auto& a:runtime.frameActivity())if(std::any_of(a.buttons.begin(),a.buttons.end(),[](const auto& b){return b.scenario<68;})){++originalMenuFrames;if(a.combatEmittingScenarios||a.combatActiveProjectiles||a.combatDamage)++originalMenuOverlap;}
    assert(originalMenuFrames==11&&originalMenuOverlap==11&&metrics.menuActionFrames>=originalMenuFrames);
    assert(metrics.maxConcurrentWeaponKinds>=5&&metrics.framesWithFiveWeaponKinds>0);
    assert(metrics.maxConcurrentWeaponDefinitions>=5&&metrics.maxConcurrentEmittingScenarios>=5&&metrics.framesWithFiveWeaponDefinitions>0);
    assert(runtime.frameActivity().size()==frames);for(const auto& activity:runtime.frameActivity()){assert(activity.tickedScenarios==validation::concurrentScenarioCount);std::cout<<"FRAME "<<activity.frame<<" ticked="<<activity.tickedScenarios<<" emittingScenarios="<<activity.emittingScenarios<<" weaponDefinitions="<<activity.emittedWeaponParts.size()<<" weaponKinds="<<activity.emittedWeaponKinds.size()<<" buttons="<<activity.buttons.size()<<" activeProjectiles="<<activity.activeProjectiles<<" combatProjectiles="<<activity.combatActiveProjectiles<<" combatEmit="<<activity.combatEmittingScenarios<<" combatDamage="<<activity.combatDamage<<" restart="<<activity.saveRestart<<" emissions="<<activity.emissions<<" damage="<<activity.damage<<" kills="<<activity.kills;for(const auto& part:activity.emittedWeaponParts)std::cout<<" "<<part;std::cout<<"\n";}
    for(std::size_t i=0;i<runtime.scenarioCount();++i)for(const auto& [part,count]:runtime.observations()[i].weaponEmissions)std::cout<<"WEAPON scenario="<<i<<" part="<<part<<" emissions="<<count<<"\n";
    assert(runtime.observations()[55].shopPurchased&&runtime.observations()[55].insufficientRejected);
    assert(runtime.observations()[56].missionAccepted&&runtime.observations()[56].missionComplete);
    assert(runtime.observations()[57].restarted&&host.restarts[57]==1);
    assert(runtime.observations()[0].level>=1&&runtime.observations()[0].experience>299);
    for(const auto& name:metrics.warmReadNames)std::cout<<"WARM_READ_MISS "<<name<<std::endl;
    for(const auto& name:metrics.warmExistsNames)std::cout<<"WARM_EXISTS_MISS "<<name<<std::endl;
    resourceAccounting(runtime);
    std::cout<<"RESOURCE_SOURCE_TOTALS reads="<<files.reads<<" before="<<warmReads<<" rawWarmMisses="<<metrics.warmReadMisses<<" exists="<<files.probes<<" before="<<warmProbes<<" rawWarmMisses="<<metrics.warmExistsMisses<<std::endl;
    assert(files.reads==warmReads+metrics.warmReadMisses&&files.probes==warmProbes+metrics.warmExistsMisses&&metrics.steadyReadRequests==0&&metrics.steadyExistsRequests==0&&metrics.steadyReadMisses==0&&metrics.steadyExistsMisses==0);
    const auto& moving=runtime.observations()[58];
    std::cout<<"MOVEMENT distance="<<moving.distanceMoved<<" displacement="<<moving.maxDisplacement<<" movementFrames="<<moving.movementFrames<<" nav="<<moving.navigationRequests<<" rejected="<<moving.navigationRejected<<" blocked="<<moving.blockedFrames<<" drops="<<moving.maxObservedDrops<<" pickups="<<moving.pickupEvents<<" coins="<<moving.coinPickups<<" creditGain="<<moving.pickupCreditGain<<" inventoryGain="<<moving.inventoryGain<<" injury="<<moving.injuryPoints<<" movementOverlap="<<metrics.movementFramesWithCombatActivity<<" pickupOverlap="<<metrics.pickupFramesWithCombatActivity<<" movementFireOverlap="<<metrics.movementEmissionFramesWithCombatActivity<<"\n";
    for(const auto& route:moving.navigation)std::cout<<"ROUTE frame="<<route.frame<<" player="<<route.player.x<<","<<route.player.y<<" target="<<route.world.x<<","<<route.world.y<<" screen="<<route.screen.x<<","<<route.screen.y<<" code="<<route.lootCode<<" accepted="<<route.accepted<<" blocked="<<route.blocked<<"\n";
    for(const auto& item:moving.pickups)std::cout<<"PICKUP frame="<<item.frame<<" code="<<item.code<<" value="<<item.value<<" player="<<item.player.x<<","<<item.player.y<<" item="<<item.item.x<<","<<item.item.y<<" deltaCr="<<item.creditDelta<<"\n";
    assert(runtime.scenarios()[58].role==validation::ScenarioRole::movementLoot&&moving.navigationRequests>0&&moving.distanceMoved>30&&moving.maxDisplacement>30&&moving.playerEmissions>0&&moving.kills>0);
    assert(moving.dashStarts==0);for(std::size_t i=1;i<moving.navigation.size();++i)assert(moving.navigation[i].frame-moving.navigation[i-1].frame>=31);
    assert(moving.coinPickups>0&&moving.pickupCreditGain>0&&moving.credit==moving.initialCredit+moving.pickupCreditGain&&moving.experience>moving.initialExperience);
    for(const auto& item:moving.pickups)if(item.type==0)
        assert(host.pickupLabels[58].contains(std::to_string(item.value)+" cr"));
    assert(metrics.movementFramesWithCombatActivity>0&&metrics.pickupFramesWithCombatActivity>0&&metrics.movementEmissionFramesWithCombatActivity>0);
    for(const auto& item:moving.pickups)assert(std::abs(item.player.x-item.item.x)<30&&std::abs(item.player.y-item.item.y)<30);
    assert(metrics.aidKitFramesWithCombatActivity>0);unsigned aidKitCases=0;
    for(std::size_t i=59;i<65;++i){const auto& o=runtime.observations()[i];std::size_t row=0;for(;row<originalItems.rowCount();++row)if(originalItems.value(row,"Code")==o.quickItemCode)break;assert(row<originalItems.rowCount());++aidKitCases;
        int amount=int(originalItems.real(row,"Param1"));if(originalItems.real(row,"Param2")==1.f)amount=o.quickMaximumHP*amount/100;
        assert(runtime.scenarios()[i].role==validation::ScenarioRole::aidKit&&o.quickUseFrame==241&&o.quickTapAccepted);
        assert(o.quickHPBefore>0&&o.quickHPBefore<o.quickMaximumHP&&o.quickHPAfter==std::min(o.quickMaximumHP,o.quickHPBefore+amount));
        assert(o.quickSlotHPAfter==o.quickHPAfter&&o.quickCountBefore==5&&o.quickCountAfter==4&&o.quickSelectionAfter==-1&&o.quickRepairSounds==1&&o.quickParticleAllocations==1);
        std::cout<<"AIDKIT code="<<o.quickItemCode<<" HP="<<o.quickHPBefore<<":"<<o.quickHPAfter<<" slotHP="<<o.quickSlotHPAfter<<" maxHP="<<o.quickMaximumHP<<" count="<<o.quickCountBefore<<":"<<o.quickCountAfter<<" SFX13="<<o.quickRepairSounds<<" particles="<<o.quickParticleAllocations<<"\n";
    }assert(aidKitCases==6);
    assert(metrics.airStrikeFramesWithCombatActivity>0);unsigned strikes=0;
    for(std::size_t i=65;i<68;++i){const auto& o=runtime.observations()[i];++strikes;
        assert(runtime.scenarios()[i].role==validation::ScenarioRole::airStrike&&o.strikeUseFrame==261&&o.strikeTapAccepted&&o.strikeTargetVisibleEnemy);
        assert(o.strikeCountBefore==5&&o.strikeCountAfter==4&&o.strikeRequests==1&&o.strikeProjectiles==(o.strikeItemCode=="02007"?1:10));
        assert(o.playerEmissions==0&&o.strikeEnemyHPRemoved>0&&o.strikeDamageCalls>0&&o.strikeTicks>0&&o.activeStrikes==0);
        std::cout<<"AIRSTRIKE code="<<o.strikeItemCode<<" bullets="<<o.strikeProjectiles<<" enemyHPRemoved="<<o.strikeEnemyHPRemoved<<" hits="<<o.strikeDamageCalls<<" playerEmissions="<<o.playerEmissions<<" count="<<o.strikeCountBefore<<":"<<o.strikeCountAfter<<"\n";
    }assert(strikes==3);
    std::set<std::string> fields{"Stage01_01"};std::array<std::uint64_t,3> first=runtime.observations()[0].firstSpawns;
    for(std::size_t i=68;i<105;++i){const auto& d=runtime.scenarios()[i];const auto& o=runtime.observations()[i];assert(d.role==validation::ScenarioRole::worldAudit&&fields.insert(d.map).second);
        for(unsigned k=0;k<3;++k)first[k]+=o.firstSpawns[k];
        std::cout<<"WORLD "<<d.map<<" first="<<o.firstSpawns[0]<<","<<o.firstSpawns[1]<<","<<o.firstSpawns[2]<<" emits="<<o.enemyEmissions[0]<<","<<o.enemyEmissions[1]<<","<<o.enemyEmissions[2]<<" kills="<<o.enemyKills[0]<<","<<o.enemyKills[1]<<","<<o.enemyKills[2]<<" removed="<<o.enemyRemovals[0]<<","<<o.enemyRemovals[1]<<","<<o.enemyRemovals[2]<<" quickUses="<<o.worldQuickUses<<std::endl;
        if(d.map=="Stage01_02"||d.map=="Stage02_07"||d.map=="Stage03_04"){const auto k=d.map=="Stage01_02"?1u:2u;assert(o.enemyEmissions[k]>0&&o.enemyDamage[k]>0&&o.enemyKills[k]>0&&o.enemyRemovals[k]>0);}
    }
    assert(fields.size()==38&&first[0]==385&&first[1]==307&&first[2]==3);
    const auto& dash=runtime.observations()[105];assert(runtime.scenarios()[105].role==validation::ScenarioRole::dash);
    std::cout<<"DASH starts="<<dash.dashStarts<<" ticks="<<dash.dashMoveTicks<<" SP="<<dash.dashSPBefore<<":"<<dash.dashSPAfter<<" sounds="<<dash.dashSounds<<" particles="<<dash.dashParticles<<" random="<<dash.dashRandomCalls<<" displacement="<<dash.dashMaxDisplacement<<std::endl;
    assert(dash.dashInputAccepted&&dash.dashInputFrame==110&&dash.dashStarts==1&&dash.dashSPBefore-dash.dashSPAfter==100&&dash.dashSounds==1&&dash.dashMoveTicks>0&&dash.dashParticles>0&&dash.dashRandomCalls==dash.dashParticles&&dash.dashMaxDisplacement>30);
    // Appended conditions retain independent original saves and never alter
    // normal initializeSlot's Picker. BOT contact != final Player reward.
    assert(runtime.scenarios()[106].role==validation::ScenarioRole::botAttack&&runtime.scenarios()[107].role==validation::ScenarioRole::botPicker);
    for(unsigned index:{106u,107u}){const auto& o=runtime.observations()[index];std::uint64_t incoming=0,contacts=0;
        for(unsigned slot=0;slot<3;++slot){const auto& bot=o.bots[slot];
            std::cout<<"BOT scenario="<<index<<" code="<<bot.code<<" machine="<<bot.machine<<" ticks="<<bot.ticks<<" rotor="<<bot.rotorFrames<<" emit="<<bot.emissions<<" enemyHPRemoved="<<bot.enemyHPRemoved<<" contacts="<<bot.itemCaptures<<" damageCalls="<<bot.damageCalls<<" HPRemoved="<<bot.hpRemoved<<" repair="<<bot.repairBefore<<":"<<bot.repairAfter<<" repairFrame="<<bot.repairFrame<<std::endl;
            assert(bot.spawned&&bot.slot==slot&&bot.id==2+slot&&bot.ticks>0&&bot.rotorFrames>0&&bot.repairFrame>=180&&bot.repairAccepted);
            assert(bot.repairAfter>bot.repairBefore&&bot.repairCountAfter==bot.repairCountBefore-1&&bot.repairParticles==1&&bot.repairSounds==1);
            if(index==106)assert(bot.code==std::string("0400")+char('1'+slot)&&bot.emissions>0&&bot.enemyHPRemoved>0);
            else assert(bot.code==std::string("0420")+char('1'+slot)&&bot.itemCaptures>0);
            incoming+=bot.hpRemoved;contacts+=bot.itemCaptures;
        }
        assert(incoming>0&&o.navigationRequests>0&&o.distanceMoved>30&&o.botThreatEmissions>0);
        assert(o.dashStarts==0);for(std::size_t n=1;n<o.navigation.size();++n)assert(o.navigation[n].frame-o.navigation[n-1].frame>=31);
        if(index==106)assert(o.playerEmissions==0);
        else assert(contacts>0&&o.pickupEvents>0&&o.coinPickups>0&&o.pickupCreditGain>0&&o.credit==o.initialCredit+o.pickupCreditGain);
    }
    for(unsigned side=0;side<2;++side){const auto& swap=runtime.observations()[107].botWeaponSwaps[side];assert(swap.inventoryPosition==10+side&&swap.chooserFrame>0&&swap.chooserShown&&swap.equipFrame>=swap.chooserFrame+31&&swap.equipped&&swap.firstEmissionFrame>=swap.equipFrame&&swap.firstEmissionFrame<=frames);assert(swap.ownedBefore&&swap.countBefore==1&&!swap.equippedBefore&&swap.ownedAfter&&swap.countAfter==1&&swap.equippedAfter&&swap.oldCountAfter==1&&!swap.oldEquippedAfter);assert(!swap.pausedBefore&&!swap.pausedAfter&&swap.savesBefore==swap.savesAfter);assert(runtime.session(107).state().equipment[2+side].dataId==swap.code);if(side)assert(swap.chooserFrame>runtime.observations()[107].botWeaponSwaps[0].equipFrame);}
    assert(metrics.botFramesWithCombatActivity>0&&metrics.helpFramesWithCombatActivity>0);
    for(unsigned index=108;index<112;++index){const auto& o=runtime.observations()[index].help;assert(runtime.scenarios()[index].role==validation::ScenarioRole::help);
        std::cout<<"HELP scenario="<<index<<" counter="<<o.initialCounter<<":"<<o.resultingCounter<<" firstPresent="<<o.initialPresent<<" initialLayer="<<o.initialLayer<<" menu="<<o.menuOpened<<" inputEnds="<<o.inputs.size()<<" paused="<<o.pausedFrames<<" running="<<o.runningFrames<<" actualDraws="<<o.drawFrames<<" guideDraws="<<o.guideDrawFrames<<std::endl;
        assert(o.runningFrames>0&&!o.initialPaused);
        if(index<110){assert(o.initialCounter==(index==108?0:4)&&o.resultingCounter==o.initialCounter+5&&o.initialPresent&&o.initialPage==2&&o.initialLayer==9&&!o.initialGuide&&!o.initialChangePages);assert(o.inputs.size()==1&&o.inputs[0].accepted&&o.inputs[0].closed&&!o.inputs[0].paused&&o.closedFrame==200&&o.drawFrames>0&&o.guideDrawFrames==0&&o.lastImage=="HelpScr03_kr.png");}
        if(index==110){assert(o.initialCounter==5&&o.resultingCounter==5&&!o.initialPresent&&o.inputs.size()==1&&!o.inputs[0].accepted&&!o.inputs[0].closed&&o.drawFrames==0&&o.guideDrawFrames==0);}
        if(index==111){assert(!o.initialPresent&&o.menuOpened&&o.menuPaused&&o.menuGuide&&o.menuPage==2&&o.menuLayer==13&&o.inputs.size()==4&&o.closedFrame==230);
            assert(o.inputs[0].accepted&&o.inputs[0].beforeGuide&&!o.inputs[0].afterGuide&&!o.inputs[0].closed&&o.inputs[0].afterPage==2&&o.inputs[0].paused);
            assert(o.inputs[1].afterPage==1&&o.inputs[1].paused&&o.inputs[2].afterPage==2&&o.inputs[2].paused);
            assert(o.inputs[3].accepted&&o.inputs[3].closed&&!o.inputs[3].paused&&o.timeAtClose==o.timeAtMenu&&o.lastWorldTime>o.timeAtClose&&o.guideDrawFrames>0&&o.drawFrames>o.guideDrawFrames&&o.pausedFrames>0);
        }
    }
    const auto& over=runtime.observations()[112].gameOver;
    std::cout<<"GAMEOVER_ACTUAL death="<<over.deathFrame<<" request="<<over.requestFrame<<" confirm="<<over.confirmFrame<<" base="<<over.baseFrame<<" requests="<<over.requests<<" enemyEmissions="<<over.enemyEmissions<<" damage="<<over.damageCalls<<" token="<<over.token<<" baseHP="<<over.baseHP<<" phase="<<over.baseDoorPhase<<" saves="<<over.savesBefore<<":"<<over.savesAtRequest<<":"<<over.savesAfterConfirm<<":"<<over.savesAtBase<<" overlap="<<metrics.gameOverFramesWithCombatActivity<<std::endl;
    assert(runtime.scenarios()[112].role==validation::ScenarioRole::gameOver&&over.initialHP==1&&over.targetInputAccepted&&over.deathFrame>0&&over.deathHP==0&&over.deathPlayerState==2&&over.enemyEmissions>0&&over.damageCalls>0);
    assert(over.requests==1&&over.token&&over.requestFrame>=over.deathFrame&&over.requestHP==0&&over.requestPlayerState==3&&over.requestWorldState==3&&!over.requestPaused&&over.requestStateTime==over.requestRenderTime);
    assert(over.confirmFrame==over.requestFrame+8&&over.waitAdvanced&&over.confirmAccepted&&over.confirmDoorPhase==0&&over.locationAtConfirm=="Base002"&&over.pendingCleared&&over.duplicateRejected);
    assert(over.baseFrame>over.confirmFrame&&over.baseDoorPhase==4&&over.baseId=="Base002"&&over.baseHP==256&&over.savedPayloadValid&&(over.savedBotHP==std::array<std::uint32_t,3>{}));
    assert(over.savesBefore==over.savesAtRequest&&over.savesAtRequest==over.savesAfterConfirm&&over.savesAtBase==over.savesBefore+1&&runtime.observations()[112].saves==over.savesAtBase&&metrics.gameOverFramesWithCombatActivity>=3);
    const auto& ranking=runtime.observations()[113].ranking;
    std::cout<<"RANKING_ACTUAL frame="<<ranking.frame<<" notices="<<ranking.notices<<" title="<<ranking.title<<" message="<<ranking.message<<" saves="<<ranking.savesBefore<<":"<<ranking.savesAfter<<" overlap="<<metrics.rankingFramesWithCombatActivity<<std::endl;
    assert(runtime.scenarios()[113].role==validation::ScenarioRole::rankingUnavailable&&ranking.notices==1&&ranking.title=="랭킹"&&ranking.message=="현재 랭킹 서비스를 이용할 수 없습니다.");
    assert(ranking.events==std::vector<std::string>({"notice","sfx0"})&&ranking.savesBefore==ranking.savesAfter&&ranking.slotUnchanged&&ranking.savedBytesUnchanged&&ranking.screenUnchanged&&ranking.transitionUnchanged&&ranking.unsupportedUnchanged&&metrics.rankingFramesWithCombatActivity==1);
    const auto& completed=runtime.observations()[114].completedMissions;
    std::cout<<"COMPLETED_ACTUAL popups="<<completed.popupFrames[0]<<':'<<completed.popupFrames[1]<<" ok="<<completed.okFrames[0]<<':'<<completed.okFrames[1]<<" list="<<completed.listFrame<<" base="<<completed.baseFrame<<" credit="<<completed.finalCredit<<" items="<<completed.repairItems<<" overlap="<<metrics.completedMissionFramesWithCombatActivity<<std::endl;
    assert((runtime.scenarios()[114].role==validation::ScenarioRole::completedMissions&&completed.ids==std::array<std::string,2>({"M002010","M001010"})));
    assert((completed.credits==std::array<int,2>({580,680})&&completed.experience==std::array<int,2>({200,400})&&completed.levels==std::array<int,2>({0,1})&&completed.rewardCounts==std::array<int,2>({2,3})));
    assert(completed.popupFrames[0]>0&&completed.doorReadyFrame>completed.popupFrames[0]&&completed.popupFrames[1]==completed.okFrames[0]&&completed.okFrames[0]==completed.doorReadyFrame+8&&completed.okFrames[1]==completed.popupFrames[1]+8);
    assert(completed.secondOKAccepted&&completed.finalOKAccepted&&!completed.listImmediatelyAfterFinalOK&&completed.listFrame==completed.okFrames[1]&&completed.baseFrame>completed.exitFrame&&completed.finalCredit==680&&completed.remainingAccepted==2&&completed.savedPayloadValid&&completed.repairItems==completed.initialRepairItems+3);
    assert(completed.popupSaves[0]==completed.savesBefore&&completed.popupSaves[1]==completed.savesBefore&&completed.savesAfter==completed.savesBefore+1&&metrics.completedMissionFramesWithCombatActivity>=3);
    assert(completed.sounds==std::vector<unsigned>({11,11,7,0,0,0}));
    recentRegressions(runtime);
    const auto priorTicks=runtime.observations()[0].ticks;runtime.setActive(false);runtime.tick();assert(runtime.observations()[0].ticks==priorTicks&&runtime.metrics().frames==frames);runtime.setActive(true);
    const auto missing=runtime.missingFeatures();assert(std::find(missing.begin(),missing.end(),"quick-item actual use")==missing.end()&&std::find(missing.begin(),missing.end(),"dash execution")==missing.end());
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
    std::cout<<"HM2_CONCURRENT PASS frames="<<frames<<" scenarios="<<runtime.scenarioCount()<<" combat=55 originalPlayerWeapons=110 firedDefinitions="<<fired.size()<<" activeWeaponKinds="<<metrics.activeWeaponKinds<<" maxSameFrameDefinitions="<<metrics.maxConcurrentWeaponDefinitions<<" maxSameFrameKinds="<<metrics.maxConcurrentWeaponKinds<<" maxSameFrameScenarios="<<metrics.maxConcurrentEmittingScenarios<<" framesFiveDefinitions="<<metrics.framesWithFiveWeaponDefinitions<<" framesFiveKinds="<<metrics.framesWithFiveWeaponKinds<<" menuFrames="<<metrics.menuActionFrames<<" menuWithCombat="<<metrics.menuFramesWithCombatActivity<<" emittingScenariosEver="<<progressed<<" emissions="<<emissions<<" damage="<<damage<<" kills="<<kills<<" completedMissions="<<complete<<" levelUps="<<levelUps<<" warmReads="<<metrics.warmReadMisses<<" warmExists="<<metrics.warmExistsMisses<<" seconds="<<seconds<<"\n";
}

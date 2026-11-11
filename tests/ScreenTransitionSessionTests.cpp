#include "PreparedTextStub.hpp"
#include "Core/Session/GameSession.hpp"
#include "Core/Progression/Progression.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
using namespace hm2;
namespace {
struct Files:ResourceAccess{
    std::filesystem::path root;unsigned reads=0;mutable unsigned probes=0;
    explicit Files(const char* path):root(path){}
    Bytes read(std::string_view name)override{++reads;std::ifstream file(root/std::string(name),std::ios::binary);if(!file)throw std::runtime_error("Missing original transition resource "+std::string(name));return {std::istreambuf_iterator<char>(file),{}};}
    bool exists(std::string_view name)const override{++probes;return std::filesystem::exists(root/std::string(name));}
};
struct Host:SessionHost{
    std::vector<SaveReason> writes;std::vector<std::string> events;Bytes stored;unsigned glyphs=0;std::function<void(SaveReason)> onSave;std::function<void(std::string_view)> onNamedSound;
    ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& request)override{++glyphs;return test::preparedTextStub(request);}
    void sound(std::string_view name,Vec2)override{events.emplace_back(name);if(onNamedSound)onNamedSound(name);}
    void soundID(unsigned,Vec2)override{}
    void alert(std::string_view)override{assert(false);}
    void requestName(unsigned)override{assert(false);}
    void requestDelete(unsigned)override{assert(false);}
    bool writeSave(std::span<const std::uint8_t> bytes,SaveReason reason)override{writes.push_back(reason);stored.assign(bytes.begin(),bytes.end());events.push_back("save:"+std::to_string(int(reason)));if(onSave)onSave(reason);return true;}
    std::uint32_t absoluteTimeLow()override{return 1;}
    std::uint32_t unixTime()override{return 1794268800;}
};
progression::SlotView slot(GameSession& game){return progression::SlotView(std::span(const_cast<std::uint8_t*>(game.workingSlot().data()),game.workingSlot().size()));}
void action(GameSession& game,unsigned id,ui::ButtonEventType type=ui::ButtonEventType::pop,int data=0){game.action({game.state().screen,ui::Panel::root,{type,{0,id,data},1,{}}});}
void loadingPictures(GameSession& game){
    Frame frame;game.appendOverlays(frame);unsigned count=0;
    for(const auto& image:frame.sprites)if(image.image.starts_with("Loading_")){++count;assert(image.visualLayer==13&&!image.atlasBatch&&image.strip.size()==4);}
    assert(count==(game.screenTransition().visible?5u:0u));
}
void settle(GameSession& game){unsigned ticks=0;while(game.screenTransition().phase!=8){game.tick();loadingPictures(game);game.drawWorld();assert(++ticks<300);}}
void titleToSlots(GameSession& game){
    assert(game.state().screen==ui::ScreenId::title&&game.screenTransition().phase==8&&!game.screenTransition().visible);
    game.action({ui::ScreenId::title,ui::Panel::root,{},true,true});assert(game.state().screen==ui::ScreenId::title&&game.screenTransition().phase==0);
    settle(game);assert(game.state().screen==ui::ScreenId::slots);
}
SaveData fixture(std::string_view place="Base001"){
    SaveData data;data.initializeSlot(0,"MACH","us",1794268800);progression::SlotView s(data.slot(0));s.string(0x2d,16,place);s.u32(0x68,0);s.hp(180);s.u32(0x64c,50);s.u32(0x1b60,1);s.string(0x8c+8*6,6,"10011");s.byte(0x53c+8,1);return data;
}
void savedSlotToRequestedScreen(GameSession& game){assert(game.state().screen==ui::ScreenId::slots);action(game,1001);assert(game.screenTransition().phase==0&&game.state().screen==ui::ScreenId::slots);settle(game);}
void closeFirstHelp(GameSession& game){assert(game.help()&&game.help()->state().page==2&&!game.help()->state().changePages&&game.help()->state().layer==9);assert(game.inputOverlay(PointerPhase::end,{384,512},7));assert(!game.help());}
void requestGatesAndInput(Files& files){
    Host host;DarwinWorldRandom rng(1);GameSession game(files,host,rng,fixture());
    assert(game.screenTransition().phase==8);game.action({ui::ScreenId::title,ui::Panel::root,{},true,true});assert(game.screenTransition().phase==0);
    unsigned ticks=0;while(game.screenTransition().phase!=7){
        const auto before=game.screenTransition();game.action({ui::ScreenId::title,ui::Panel::root,{},true,true});assert(game.screenTransition().phase==before.phase&&game.screenTransition().requestedScreen==before.requestedScreen);
        game.tick();loadingPictures(game);assert(++ticks<200);
    }
    assert(game.state().screen==ui::ScreenId::slots&&host.writes.empty());
    // phase7 accepts another real menu request before the opening animation
    // finishes. No phase8-only gate and no blanket UI event rejection.
    action(game,1004);assert(game.screenTransition().phase==0&&game.screenTransition().requestedScreen==1&&game.state().screen==ui::ScreenId::slots&&game.screenTransition().visible);
    game.tick();assert(game.screenTransition().phase==1&&game.screenTransition().left.x==-256);settle(game);assert(game.state().screen==ui::ScreenId::title&&host.writes.empty());
    std::cout<<"SESSION_TRANSITION initial8/request0to6Reject/phase7Accept/originalFivePictures/noGlobalActionBlock PASS\n";
}
void airliftPhaseFour(Files& files){
    Host host;DarwinWorldRandom rng(1);GameSession game(files,host,rng,fixture("Stage01_01"));titleToSlots(game);savedSlotToRequestedScreen(game);
    assert(game.state().screen==ui::ScreenId::field&&game.battle());closeFirstHelp(game);
    auto* oldScene=game.battle();const auto savedBefore=host.stored;const auto writesBefore=host.writes.size();
    const auto oldHP=slot(game).hp();const auto oldBotHP=slot(game).u32(0x64c);assert(oldBotHP==game.battle()->bots().at(0).hp&&oldHP<slot(game).maxHP()&&oldBotHP<unsigned(game.battle()->bots().at(0).maximumHP));
    action(game,1027);assert(game.state().airliftVisible&&game.battle()->isPaused());
    action(game,4002);const auto destination=game.state().airlifts[0];assert(destination.dataId=="Base001"&&destination.price==50&&game.state().selectedAirlift==0);
    const auto credits=slot(game).credit();action(game,4001);
    assert(game.screenTransition().phase==0&&game.state().screen==ui::ScreenId::field&&game.battle()==oldScene);
    assert(!game.state().airliftVisible&&game.state().selectedAirlift==-1&&slot(game).credit()==credits-50&&slot(game).string(0x2d,16)=="Base001");
    assert(game.battle()->isPaused()&&host.writes.size()==writesBefore&&host.stored==savedBefore);
    // A displayed HUD action is still delivered while Loading closes. This
    // action does not unpause/move the World and must not be globally dropped.
    action(game,1022,ui::ButtonEventType::push);assert(game.state().weaponChoiceType==2);
    const auto worldTime=oldScene->clock().worldTime();bool savedAtFour=false,openedAfterSave=false;
    host.onSave=[&](SaveReason reason){assert(reason==SaveReason::enterBase&&game.screenTransition().phase==4&&!game.battle());assert(game.state().screen==ui::ScreenId::base);assert(slot(game).hp()==slot(game).maxHP());for(unsigned i=0;i<3;++i)assert(slot(game).u32(0x64c+i*4)==0);assert(slot(game).string(0x63a,6)=="04201");savedAtFour=true;};
    host.onNamedSound=[&](std::string_view name){if(name=="Loading_Open.wav"){assert(savedAtFour&&game.screenTransition().phase==5&&!game.battle());openedAfterSave=true;}};
    unsigned frames=0;while(game.screenTransition().phase!=8){
        const auto entry=game.screenTransition().phase;
        if(entry<=4){assert(game.state().screen==ui::ScreenId::field&&game.battle()==oldScene&&host.writes.size()==writesBefore);assert(slot(game).hp()==oldHP&&slot(game).u32(0x64c)==oldBotHP);}
        game.tick();loadingPictures(game);assert(++frames<300);
        if(entry<4){assert(game.battle()==oldScene&&oldScene->clock().worldTime()==worldTime&&host.stored==savedBefore);}
    }
    assert(savedAtFour&&openedAfterSave&&game.state().screen==ui::ScreenId::base&&!game.battle()&&host.writes.size()==writesBefore+1);
    DarwinWorldRandom decode(1);auto envelope=SaveData::decode(host.stored,decode,false);progression::SlotView saved(envelope.slot(0));assert(saved.hp()==slot(game).maxHP()&&saved.u32(0x64c)==0&&saved.credit()==credits-50&&saved.string(0x2d,16)=="Base001");
    std::cout<<"SESSION_TRANSITION actualAirlift4001/oldSceneAndSaveThroughPhase4/pausedWorld/phase4BaseHP-BOT-save-beforeOpen/noGlobalHUDActionBlock PASS frames="<<frames<<"\n";
}
void actualFieldGateAndMapLoading(Files& files){
    Host host;DarwinWorldRandom rng(1);GameSession game(files,host,rng,fixture("Stage01_02"));titleToSlots(game);savedSlotToRequestedScreen(game);
    assert(game.state().screen==ui::ScreenId::field&&game.battle()&&game.help()&&!game.battle()->isPaused());assert(slot(game).u32(0x1b5c)==6);
    assert((host.writes==std::vector<SaveReason>{SaveReason::selectSlot}));
    // Explicit saved-slot Start saves BEFORE its +1 and first Help +5. The
    // initial isSave=false stage must not add a stageTransition write.
    DarwinWorldRandom firstDecodeRng(1);auto firstSaved=SaveData::decode(host.stored,firstDecodeRng,false);
    assert(progression::SlotView(firstSaved.slot(0)).u32(0x1b5c)==0);
    closeFirstHelp(game);const auto helpCounter=slot(game).u32(0x1b5c);const auto writesBefore=host.writes.size();
    const auto all=game.battle()->gates();const auto found=std::find_if(all.begin(),all.end(),[](const auto& g){return g.definition.number==0;});assert(found!=all.end()&&found->definition.linkMap=="Stage01_01"&&!found->definition.eventOnly);
    const auto gate=found->definition;auto* oldScene=game.battle();const auto* oldGateTile=oldScene->preparedTile("LinkGate.png");assert(oldGateTile);const Vec2 logical=oldScene->camera().toScreen(gate.position);
    assert(logical.x>=0&&logical.x<=768&&logical.y>=100&&logical.y<=1024);assert(game.inputWorld(PointerPhase::begin,logical,90));assert(game.inputWorld(PointerPhase::end,logical,90));
    unsigned movementFrames=0;while(!game.mapLoading()){assert(game.battle()==oldScene);game.tick();game.drawWorld();assert(++movementFrames<300);}
    assert(game.screenTransition().phase==8&&game.battle()==oldScene&&oldScene->isPaused()&&oldScene->gameState()==1&&slot(game).string(0x2d,16)=="Stage01_01"&&slot(game).u32(0x68)==unsigned(gate.linkGate));
    assert(host.writes.size()==writesBefore&&std::find(host.events.begin(),host.events.end(),"Title_TitleIn.wav")!=host.events.end());
    const auto pausedTime=oldScene->clock().worldTime();bool sawObjectsCleared=false,sawTilesStep=false,savedInSetStage=false,guiBeforeSetStage=false;unsigned overlayFrames=0;
    std::uint64_t guiRandomCalls=0,guiReseeds=0;std::vector<std::string> stageOrder;
    host.onSave=[&](SaveReason reason){assert(reason==SaveReason::stageTransition&&game.mapLoading()&&game.mapLoading()->state().step==4);assert(game.battle()&&game.battle()->isSave()&&game.battle()->gameState()==0&&slot(game).string(0x2d,16)=="Stage01_01");assert(slot(game).u32(0x1b5c)==helpCounter&&!game.help());
        assert(guiBeforeSetStage&&game.battle()->clock().worldTime()==pausedTime&&rng.reseeds()>guiReseeds&&rng.calls()>guiRandomCalls);
        stageOrder.push_back("save-newScene-sameWorldTime");savedInSetStage=true;};
    unsigned frames=0;while(game.mapLoading()){
        const int entry=game.mapLoading()->state().step;
        if(entry<=1){assert(game.battle()==oldScene&&oldScene->clock().worldTime()==pausedTime&&oldScene->gameState()==1);}
        if(entry==2){assert(!game.battle());assert(oldScene->preparedTile("LinkGate.png")==oldGateTile&&oldGateTile->pixels.width==64&&oldScene->clock().worldTime()==pausedTime);sawObjectsCleared=true;}
        if(entry==4){assert(!game.battle());sawTilesStep=true;}
        Frame overlay;game.appendOverlays(overlay);assert(std::count_if(overlay.sprites.begin(),overlay.sprites.end(),[](const auto& s){return s.image=="StageClearBG.png"&&s.visualLayer==13;})==1);++overlayFrames;
        game.tick([&]{
            // This controlled GuiGame callback tests the real interleave and
            // shared stream contract, not OutOfRange's separate RNG formula.
            const auto& guiState=game.state();assert(guiState.hasWorkingSlot);
            assert(game.mapLoading()&&game.mapLoading()->state().step==entry);
            if(entry<=1)assert(game.battle()==oldScene&&oldScene->clock().worldTime()==pausedTime);
            if(entry==4){assert(!game.battle()&&guiState.fieldWorldTime==pausedTime);guiReseeds=rng.reseeds();const auto before=rng.calls();(void)rng.random();assert(rng.calls()==before+1);guiRandomCalls=rng.calls();guiBeforeSetStage=true;stageOrder.push_back("GuiGame-sharedRNG-before-reseed");}
        });game.drawWorld();assert(++frames<300);
    }
    assert(sawObjectsCleared&&sawTilesStep&&savedInSetStage&&host.writes.size()==writesBefore+1&&game.battle()&&!game.battle()->isPaused()&&game.battle()->gameState()==0);
    assert((stageOrder==std::vector<std::string>{"GuiGame-sharedRNG-before-reseed","save-newScene-sameWorldTime"}));
    assert(game.battle()->clock().worldTime()>pausedTime);
    assert(slot(game).string(0x2d,16)=="Stage01_01"&&slot(game).u32(0x68)==1&&slot(game).u32(0x1b5c)==helpCounter&&!game.help());
    std::cout<<"SESSION_MAP_LOADING realGroundInput/gateCallbackDeferred/pausedOldWorld/clear1-clear2-setStage4/layer13/normalGateSaveAfterHelpCheck/noExtraHelp/preservedWorldTime/UI-before-reseed-save/state1-to0 PASS movementFrames="<<movementFrames<<" overlayFrames="<<overlayFrames<<"\n";
}
}
int main(int argc,char** argv){if(argc<2)return 77;Files files(argv[1]);requestGatesAndInput(files);airliftPhaseFour(files);actualFieldGateAndMapLoading(files);}

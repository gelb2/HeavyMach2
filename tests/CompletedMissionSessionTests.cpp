#include "PreparedTextStub.hpp"
#include "Core/Data/Descriptions.hpp"
#include "Core/Session/GameSession.hpp"
#include "Core/Progression/Progression.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
using namespace hm2;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;std::map<std::string,Bytes,std::less<>> overrides;
    explicit Files(const char* path):root(path){}
    Bytes read(std::string_view name)override{if(auto i=overrides.find(name);i!=overrides.end())return i->second;std::ifstream f(root/std::string(name),std::ios::binary);if(!f)throw std::runtime_error("Missing completed mission resource "+std::string(name));return {std::istreambuf_iterator<char>(f),{}};}
    bool exists(std::string_view name)const override{return overrides.contains(name)||std::filesystem::is_regular_file(root/std::string(name));}
    void replace(std::string name,std::string_view text){overrides[std::move(name)]={text.begin(),text.end()};}
};
struct Host:SessionHost {
    Bytes stored;std::vector<SaveReason> writes;std::vector<unsigned> sounds;std::vector<std::string> alerts,notices;std::function<void(std::string_view)> onAlert;
    std::vector<ui::TextRasterRequest> pickupRequests;
    void sound(std::string_view,Vec2)override{}void soundID(unsigned id,Vec2)override{sounds.push_back(id);}
    void alert(std::string_view key)override{alerts.emplace_back(key);if(onAlert)onAlert(key);}
    void notice(std::string_view,std::string_view message)override{notices.emplace_back(message);}
    void requestName(unsigned)override{assert(false);}void requestDelete(unsigned)override{assert(false);}
    bool writeSave(std::span<const std::uint8_t> bytes,SaveReason why)override{writes.push_back(why);stored.assign(bytes.begin(),bytes.end());return true;}
    std::uint32_t absoluteTimeLow()override{return 1;}std::uint32_t unixTime()override{return 1794268800;}
    ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& r)override{
        if(r.sourceId=="Player.pickup")pickupRequests.push_back(r);
        return test::preparedTextStub(r);
    }
};
struct Events:progression::Events{};
progression::Definitions definitions(Files& files){const auto table=[&](std::string_view name){return DataTable::parse(files.read(name));};return progression::Definitions::parse(table("ItemInfo.tble"),table("ItemRegen.tble"),table("MissionList.tble"),table,"kr");}
SaveData initial(){SaveData data;data.initializeSlot(0,"MISSIONS","kr",1794268800);return data;}
void order(progression::SlotView slot,std::initializer_list<std::string_view> ids){slot.u16(0x658,std::uint16_t(ids.size()));unsigned k=0;for(auto id:ids)slot.string(0x65c+21*k++,8,id);}
SaveData twoReady(Files& files,bool skips=true){
    auto data=initial();const auto defs=definitions(files);progression::SlotView slot(data.slot(0));Events events;progression::Progression prep(slot,defs,events);prep.restore();
    if(skips){assert(prep.missions.accept("M004010"));assert(prep.missions.accept("M001080"));assert(prep.inventory.add(*defs.findItem("05103"),1));}
    assert(prep.acceptMission("M002010","Base001")&&prep.acceptMission("M001010","Base001"));
    assert(prep.inventory.add(*defs.findItem("05001"),5)&&prep.inventory.add(*defs.findItem("05025"),5));for(unsigned i=0;i<5;++i)prep.killed("E_Tank_01",0,"Stage01_01");
    assert(prep.missions.state("M002010")->requireComplete&&prep.missions.state("M001010")->requireComplete);
    if(skips){assert(!prep.missions.state("M004010")->requireComplete&&prep.missions.state("M001080")->requireComplete);order(slot,{"M004010","M001080","M002010","M001010"});}
    else order(slot,{"M002010","M001010"});prep.prepareSave();return data;
}
void action(GameSession& game,unsigned id,std::string identity={},ui::ButtonEventType phase=ui::ButtonEventType::pop){game.action({game.state().screen,ui::Panel::root,{phase,{0,id,0,std::move(identity)},1,{}}});}
void settle(GameSession& game){unsigned ticks=0;while(game.screenTransition().phase!=8){game.tick();game.drawWorld();assert(++ticks<300);}}
void command(GameSession& game){assert(game.state().screen==ui::ScreenId::base);action(game,2001);action(game,2001);settle(game);assert(game.state().screen==ui::ScreenId::command);}
int count(GameSession& game,std::string_view id){int n=0;for(const auto& item:game.state().inventory)if(item.dataId==id)n+=item.count;return n;}
void orderedSweep(Files& files){
    Host host;DarwinWorldRandom random(1);GameSession game(files,host,random,twoReady(files));assert(game.selectSlot(0));game.enterBase();const auto saves=host.writes.size();const auto itemBefore=count(game,"01001");host.sounds.clear();command(game);
    assert(game.state().completedMission.dataId=="M002010"&&game.state().credits==580&&!game.state().commandListVisible&&game.state().commandListRevision==0);
    auto reward=game.state().completedMission.rewards;assert(reward.size()==2&&reward[0].type==0&&reward[0].value==80&&reward[0].image=="Icon00001.png"&&reward[1].type==1&&reward[1].value==200&&reward[1].image=="Icon00010.png");
    assert(count(game,"05001")==0&&count(game,"05025")==0&&host.sounds==std::vector<unsigned>({11}));
    for(unsigned i=0;i<30;++i){game.tick();game.drawWorld();(void)game.state();}assert(game.state().credits==580&&game.state().completedMission.dataId=="M002010"&&host.writes.size()==saves);
    const auto sounds=host.sounds;const auto unsupported=game.metrics().unsupportedActions;
    action(game,5007,"M002010",ui::ButtonEventType::push);action(game,5007,"M002010",ui::ButtonEventType::cancel);action(game,5007);action(game,5007,"stale");
    assert(game.state().credits==580&&host.sounds==sounds&&game.metrics().unsupportedActions==unsupported);
    host.sounds.clear();action(game,5007,"M002010");assert(game.state().completedMission.dataId=="M001010"&&game.state().credits==680&&game.state().level==1&&count(game,"01001")==itemBefore+3);
    assert(host.sounds==std::vector<unsigned>({11,7,0}));reward=game.state().completedMission.rewards;assert(reward.size()==3&&reward[2].type==2&&reward[2].value==3&&reward[2].image=="Icon01001.png");
    action(game,5007,"M002010");assert(game.state().completedMission.dataId=="M001010"&&game.state().credits==680&&host.writes.size()==saves);
    host.sounds.clear();action(game,5007,"M001010");assert(game.state().completedMission.dataId.empty()&&!game.state().commandListVisible&&game.state().commandListRevision==0&&host.sounds==std::vector<unsigned>({0}));
    game.tick();assert(game.state().commandListVisible&&game.state().commandListRevision==1&&game.state().acceptedMissions.size()==2&&game.state().acceptedMissions[0].dataId=="M004010"&&game.state().acceptedMissions[1].dataId=="M001080");
    for(unsigned i=0;i<20;++i)game.tick();assert(game.state().credits==680&&host.writes.size()==saves&&game.state().commandListRevision==1);
    action(game,2100);settle(game);assert(game.state().screen==ui::ScreenId::base&&host.writes.size()==saves+1&&host.writes.back()==SaveReason::enterBase);
    DarwinWorldRandom decoder(1);auto saved=SaveData::decode(host.stored,decoder,false);auto defs=definitions(files);Events events;progression::SlotView slot(saved.slot(0));progression::Progression restored(slot,defs,events);restored.restore();
    assert(slot.credit()==680&&slot.experience()==400&&restored.inventory.count(*defs.findItem("01001"))==itemBefore+3&&restored.missions.state("M002010")->complete&&restored.missions.state("M001010")->complete&&!restored.missions.state("M001080")->complete);
    Host restartedHost;DarwinWorldRandom restartedRandom(1);GameSession restarted(files,restartedHost,restartedRandom,saved);assert(restarted.selectSlot(0));restarted.enterBase();const auto restartSaves=restartedHost.writes.size();command(restarted);assert(restarted.state().credits==680&&restarted.state().completedMission.dataId.empty()&&restarted.state().commandListVisible&&restartedHost.writes.size()==restartSaves);
    std::cout<<"COMPLETED acceptedOrder/skipUnmetAndOtherBase/oneRewardPerPopup/5007PopIdentity/SFX11then7then0/deferredList/noSaveUntilExit/restartNoDuplicate PASS\n";
}
struct UIHost:ui::ScreensHost {
    GameSession& game;Host& host;ui::Screens* screens=nullptr;std::vector<ui::TextRasterRequest> requests;
    UIHost(GameSession& g,Host& h):game(g),host(h){}
    void sync(){screens->refresh(game.state(),game.renderTime());}
    void action(const ui::ScreenAction& a)override{game.action(a);sync();}
    ui::PreparedScreenText prepareText(const ui::TextRasterRequest& r)override{requests.push_back(r);return test::preparedTextStub(r);}
    void bgm(std::string_view)override{}void sound(std::string_view)override{}void sfx(unsigned id)override{host.soundID(id,{});}
};
ui::ButtonKey key(ui::Screens& ui,unsigned id){for(const auto& key:ui.buttons())if(key.id==id)return key;assert(false);return {};}
void click(ui::Screens& ui,unsigned id,float now){auto b=ui.button(key(ui,id));assert(b);const auto at=b->screenPosition();assert(ui.input(PointerPhase::begin,at,22,now));(void)ui.input(PointerPhase::end,at,22,now+.01f);}
const Sprite& sprite(const Frame& frame,std::string_view image){for(const auto& s:frame.sprites)if(s.image==image)return s;assert(false);std::abort();}
Size dimensions(const Sprite& sprite){if(sprite.strip.empty())return sprite.size;float left=sprite.strip[0].position.x,right=left,bottom=sprite.strip[0].position.y,top=bottom;for(const auto& vertex:sprite.strip){left=std::min(left,vertex.position.x);right=std::max(right,vertex.position.x);bottom=std::min(bottom,vertex.position.y);top=std::max(top,vertex.position.y);}return {right-left,top-bottom};}
void baseDescriptionAndNotices(Files& files){
    const auto descriptions=Descriptions::parse(DataTable::parse(files.read("Description_kr.tble")));
    const auto baseTable=DataTable::parse(files.read("Base001.tble"));
    Vec2 anchor;
    for(std::size_t row=0;row<baseTable.rowCount();++row)
        if(baseTable.value(row,"Type")=="Desc")anchor={baseTable.real(row,"X"),baseTable.real(row,"Y")};
    for(bool completed:{false,true}){
        Host host;DarwinWorldRandom random(1);
        GameSession game(files,host,random,completed?twoReady(files):initial());
        assert(game.selectSlot(0));game.enterBase();
        UIHost bridge(game,host);PreparedTiles tiles("base-ui");ui::Screens screens(files,tiles,bridge);
        bridge.screens=&screens;bridge.sync();
        auto draw=[&]{Frame frame;screens.append(frame,0);return frame;};
        auto has=[](const Frame& frame,std::string_view image){
            return std::any_of(frame.sprites.begin(),frame.sprites.end(),[&](const Sprite& s){return s.image==image;});
        };
        const auto command=screens.button(key(screens,2001))->screenPosition();
        screens.tick(.49f,random);
        assert(!has(draw(),"MissionNotice.png"));
        screens.tick(.5f,random);
        auto frame=draw();
        auto notice=[&](float offset,float topUV){
            return std::any_of(frame.sprites.begin(),frame.sprites.end(),[&](const Sprite& s){
                return s.image=="MissionNotice.png"&&s.center.x==command.x&&s.center.y==command.y+offset&&
                    !s.strip.empty()&&std::abs(s.strip[0].uv.y-topUV)<.001f;
            });
        };
        assert(notice(28,0)==completed);
        if(!completed)assert(notice(-28,.5f));
        screens.tick(1,random);
        assert(!has(draw(),"MissionNotice.png"));
        click(screens,2001,1);
        frame=draw();
        const auto& initialBG=sprite(frame,"BaseTextBG.png");
        assert(initialBG.center.x==384+(anchor.x>0?480:-480)&&initialBG.center.y==512+anchor.y);
        assert(!has(frame,"cpu-text:GuiBase.description"));
        const auto request=bridge.requests.back();
        assert(request.text==descriptions.lookup("DescCmdCenter")&&request.fontName=="TrebuchetMS-Bold"&&
            request.fontSize==18&&request.dimensions.width==280&&request.dimensions.height==184&&request.alignment==ui::TextAlign::left);
        screens.tick(1.01f,random);
        frame=draw();
        const float start=anchor.x>0?480:-480;
        assert(sprite(frame,"BaseTextBG.png").center.x==std::round(384+start+(anchor.x-start)/6.f));
        bridge.sync();
        assert(bridge.requests.back()==request&&bridge.requests.size()==1);
        for(unsigned i=0;i<100;++i)screens.tick(2+i/60.f,random);
        frame=draw();
        const auto& label=sprite(frame,"cpu-text:GuiBase.description");
        assert(label.center.x==384+anchor.x&&label.center.y==512+anchor.y&&std::abs(dimensions(label).height-184)<.001f);
        click(screens,2002,4);
        assert(bridge.requests.back().text==descriptions.lookup("DescShop"));
        assert(!has(draw(),"cpu-text:GuiBase.description"));
        action(game,2002,{},ui::ButtonEventType::cancel);bridge.sync();
        assert(!has(draw(),"BaseTextBG.png"));
    }
}
void popupInputAndGeometry(Files& files){
    Host host;DarwinWorldRandom random(1);GameSession game(files,host,random,twoReady(files));assert(game.selectSlot(0));game.enterBase();command(game);
    UIHost bridge(game,host);PreparedTiles tiles("completed-test");ui::Screens screens(files,tiles,bridge);bridge.screens=&screens;bridge.sync();const auto saves=host.writes.size();
    auto ok=screens.button(key(screens,5007));assert(ok->position.x==446&&ok->position.y==438&&ok->pushDepth==0&&ok->pushDelay==0&&ok->releaseHeight==0&&ok->releaseDelay==0&&ok->key().dataId=="M002010");
    Frame first;screens.append(first,game.renderTime());const auto& bg=sprite(first,"MissionCompletedBG.png");assert(bg.center.x==384&&bg.center.y==512&&dimensions(bg).width==512&&dimensions(bg).height==256);
    const auto& middle=sprite(first,"MissionList_Mid.png");
    const auto& bottom=sprite(first,"MissionList_Btm.png");
    const auto& top=sprite(first,"NewMissionList_Top.png");
    assert(middle.center.y==512&&dimensions(middle).height==64);
    assert(bottom.center.y==480&&top.center.y==544&&&middle<&bottom&&&bottom<&top&&&top<&bg);
    const auto collapsed=screens.buttons();
    assert(std::none_of(collapsed.begin(),collapsed.end(),[](const auto& k){return k.id>=5100&&k.id<=5204;}));
    const auto& cr=sprite(first,"Icon00001.png");const auto& exp=sprite(first,"Icon00010.png");assert(cr.center.x==300&&cr.center.y==502&&exp.center.x==400&&exp.center.y==502);
    const auto& button=sprite(first,"Common_Btn_OK.png");assert(button.columns==1&&button.rows==2&&button.tile==0);
    assert(&sprite(first,"Common_Btn_EXIT.png")<&bg&&&bg<&button&&&button<&cr&&&cr<&exp); // Actual painter order, not optional provenance metadata.
    auto request=[&](std::string_view suffix)->const ui::TextRasterRequest&{const auto id="Command.completed."+game.state().completedMission.dataId+std::string(suffix);for(auto i=bridge.requests.rbegin();i!=bridge.requests.rend();++i)if(i->sourceId==id)return *i;assert(false);std::abort();};
    assert(request(".title").fontName=="TrebuchetMS-Bold"&&request(".title").fontSize==16&&request(".title").dimensions.width==256&&request(".title").dimensions.height==20&&request(".title").alignment==ui::TextAlign::center);
    assert(request(".reward0").text=="80Cr"&&request(".reward0").fontSize==13&&request(".reward0").dimensions.width==100&&request(".reward0").dimensions.height==14&&request(".reward0").alignment==ui::TextAlign::left&&request(".reward1").text=="200");
    click(screens,5009,0);assert(host.notices.size()==1&&game.state().completedMission.dataId=="M002010"&&game.state().credits==580);
    // The callback rebuilds/removes the clicked receiver synchronously. Its
    // retained old identity must not advance the replacement result window.
    click(screens,5007,1);assert(game.state().completedMission.dataId=="M001010"&&game.state().credits==680&&!screens.button(ok->key()));
    Frame second;screens.append(second,game.renderTime());const auto& item=sprite(second,"Icon01001.png");assert(item.center.x==308&&item.center.y==458&&request(".reward2").text=="3"&&request(".reward2").dimensions.width==60);
    click(screens,2100,2);settle(game);assert(game.state().screen==ui::ScreenId::base&&host.writes.size()==saves+1&&game.state().completedMission.dataId.empty());
    command(game);
    bridge.sync();
    Frame list;screens.append(list,game.renderTime());
    const auto state=game.state();
    assert(std::any_of(state.acceptedMissions.begin(),state.acceptedMissions.end(),[](const auto& item){return item.complete;}));
    assert(std::none_of(list.sprites.begin(),list.sprites.end(),[](const auto& s){return s.image=="CompleteMark.png";}));
    auto field=state;
    field.screen=ui::ScreenId::field;
    field.missionVisible=true;
    screens.refresh(field);
    Frame fieldFrame;screens.append(fieldFrame,game.renderTime());
    assert(std::count_if(fieldFrame.sprites.begin(),fieldFrame.sprites.end(),[](const auto& s){return s.image=="CompleteMark.png";})==1);
    std::cout<<"COMPLETED actualScreensPointer/synchronousReplacement/ExitAndRankingRemainActive/popupPainterOrderAndRewardGeometry PASS\n";
}
void acceptRefreshOnly(Files& files){
    auto data=initial();auto defs=definitions(files);Events events;progression::SlotView slot(data.slot(0));progression::Progression prep(slot,defs,events);prep.restore();assert(prep.inventory.add(*defs.findItem("05001"),5)&&prep.inventory.add(*defs.findItem("05025"),5));prep.prepareSave();
    Host host;DarwinWorldRandom random(1);GameSession game(files,host,random,data);assert(game.selectSlot(0));game.enterBase();command(game);assert(game.state().commandListVisible&&game.state().completedMission.dataId.empty());
    UIHost bridge(game,host);PreparedTiles tiles("accept-test");ui::Screens screens(files,tiles,bridge);bridge.screens=&screens;bridge.sync();
    const auto& available=game.state().availableMissions;auto found=std::find_if(available.begin(),available.end(),[](const auto& mission){return mission.dataId=="M002010";});assert(found!=available.end());const auto row=unsigned(found-available.begin());
    action(game,5100+row,"M002010");bridge.sync();const auto revision=game.state().commandListRevision;const auto saves=host.writes.size();click(screens,5004,0);
    assert(game.state().credits==500&&game.state().completedMission.dataId.empty()&&game.state().acceptedMissions.size()==1&&game.state().commandListRevision==revision&&game.state().selectedMission==-1);
    // Actual existing list rows remain until the next Command tick.
    bool oldRow=false;for(const auto& k:screens.buttons())if(k.id==5100+row&&k.dataId=="M002010")oldRow=true;assert(oldRow);
    game.tick();bridge.sync();assert(game.state().commandListRevision==revision+1&&game.state().credits==500&&game.state().completedMission.dataId.empty());
    for(const auto& k:screens.buttons())assert(!(k.id>=5100&&k.id<=5104&&k.dataId=="M002010"));assert(key(screens,5200).dataId=="M002010");
    action(game,5004);assert(game.state().commandListRevision==revision+1);game.tick();assert(game.state().commandListRevision==revision+2&&game.state().credits==500&&host.writes.size()==saves);
    action(game,2100);settle(game);command(game);assert(game.state().completedMission.dataId=="M002010"&&game.state().credits==580);
    std::cout<<"COMPLETED accept5004OnlyRefreshesNextTick/noEarlyAward/nilSelectionRefresh/listReceiverIdentity/reentryClaims PASS\n";
}
void retainedRowSelection(Files& files){
    Host host;DarwinWorldRandom random(1);GameSession game(files,host,random,initial());assert(game.selectSlot(0));game.enterBase();command(game);
    UIHost bridge(game,host);PreparedTiles tiles("retained-mission-rows");ui::Screens screens(files,tiles,bridge);bridge.screens=&screens;bridge.sync();
    const auto available=game.state().availableMissions;assert(available.size()>=2&&available[0].dataId=="M001010"&&available[1].dataId=="M002010");
    click(screens,5100,0);click(screens,5004,1);assert(game.state().acceptedMissions.size()==1);
    // No tick: the displayed second receiver still owns M002010, even though
    // that mission is now index0 in the live available list.
    click(screens,5101,2);assert(game.state().missionTitle==available[1].name&&key(screens,5004).dataId=="M002010");
    click(screens,5004,3);assert(game.state().acceptedMissions.size()==2&&game.state().credits==500);
    game.tick();bridge.sync();assert(game.state().acceptedMissions.size()==2&&game.state().completedMission.dataId.empty());
    std::cout<<"COMPLETED retainedRowDataObject/selectionAndAcceptBeforeNextTick PASS\n";
}
void failedAcceptanceRefresh(Files& files){
    auto data=initial();auto defs=definitions(files);Events events;progression::SlotView slot(data.slot(0));progression::Progression prep(slot,defs,events);prep.restore();
    // Five incomplete item missions skip prerequisites only in this starting
    // save. The actual sixth acceptance must hit the original MissionFull gate.
    for(auto id:{"M003010","M003020","M003030","M003040","M003050"}){assert(prep.missions.accept(id));assert(!prep.missions.state(id)->requireComplete);}prep.prepareSave();
    Host host;DarwinWorldRandom random(1);GameSession game(files,host,random,data);assert(game.selectSlot(0));game.enterBase();command(game);assert(game.state().acceptedMissions.size()==5&&game.state().commandListVisible);
    UIHost bridge(game,host);PreparedTiles tiles("full-mission-rows");ui::Screens screens(files,tiles,bridge);bridge.screens=&screens;bridge.sync();const auto saves=host.writes.size();
    click(screens,5100,0);const auto revision=game.state().commandListRevision;click(screens,5004,1);
    assert(host.alerts==std::vector<std::string>({"MissionFull"})&&game.state().selectedMission==-1&&game.state().commandListRevision==revision&&game.state().acceptedMissions.size()==5);
    game.tick();bridge.sync();assert(game.state().commandListRevision==revision+1&&game.state().credits==500&&game.state().completedMission.dataId.empty()&&host.writes.size()==saves);
    std::cout<<"COMPLETED failedSixthAcceptance/MissionFull/clearSelection/nextTickListOnly/noSave PASS\n";
}
void commandDetailExpansion(Files& files){
    Host host;
    DarwinWorldRandom random(1);
    GameSession game(files,host,random,initial());
    assert(game.selectSlot(0));
    game.enterBase();
    command(game);
    UIHost bridge(game,host);
    PreparedTiles tiles("command-detail");
    ui::Screens screens(files,tiles,bridge);
    bridge.screens=&screens;
    bridge.sync();
    const auto saves=host.writes.size();
    const float rowY=screens.button(key(screens,5100))->position.y;
    const float secondY=screens.button(key(screens,5101))->position.y;
    const auto draw=[&]{
        Frame frame;
        screens.append(frame,game.renderTime());
        return frame;
    };
    const auto tick=[&]{
        game.tick([&]{
            screens.tick(game.renderTime(),random);
        });
    };
    click(screens,5100,0);
    auto frame=draw();
    assert(dimensions(sprite(frame,"MissionInfo.png")).height==0);
    assert(screens.button(key(screens,5100))->position.y==rowY);
    auto accept=screens.button(key(screens,5004));
    assert(accept->position.y==rowY-146-84&&accept->scale.y==1);
    tick();
    frame=draw();
    assert(dimensions(sprite(frame,"MissionInfo.png")).height==64);
    assert(screens.button(key(screens,5100))->position.y==rowY+32);
    assert(screens.button(key(screens,5101))->position.y==secondY-32);
    assert(accept->position.y==rowY-24-84&&accept->scale.y==1);
    bridge.sync(); // Unrelated presentation refresh preserves progress.
    tick();
    frame=draw();
    assert(dimensions(sprite(frame,"MissionInfo.png")).height==112);
    for(unsigned i=2;i<25;++i)tick();
    frame=draw();
    assert(dimensions(sprite(frame,"MissionInfo.png")).height<256);
    tick();
    frame=draw();
    assert(dimensions(sprite(frame,"MissionInfo.png")).height==256);
    click(screens,5100,1);
    frame=draw();
    assert(dimensions(sprite(frame,"MissionInfo.png")).height==0);
    assert(screens.button(key(screens,5100))->position.y==rowY+128);
    tick();
    assert(screens.button(key(screens,5100))->position.y==rowY+32);
    click(screens,5004,2);
    assert(game.state().acceptedMissions.size()==1&&host.writes.size()==saves);
    frame=draw();
    assert(dimensions(sprite(frame,"MissionInfo.png")).height==64);
    assert(screens.button(key(screens,5100))->position.y==rowY+32);
    assert(key(screens,5004).dataId=="M001010");
    assert(std::none_of(frame.sprites.begin(),frame.sprites.end(),[](const auto& s){return s.image=="SelMission.png";}));
    game.tick();
    bridge.sync();
    frame=draw();
    assert(std::none_of(frame.sprites.begin(),frame.sprites.end(),[](const auto& s){return s.image=="MissionInfo.png";}));
    click(screens,5200,3);
    const float acceptedY=screens.button(key(screens,5200))->position.y;
    tick();
    frame=draw();
    assert(sprite(frame,"MissionInfo.png").center.y==acceptedY-24);
    const auto buttons=screens.buttons();
    assert(std::none_of(buttons.begin(),buttons.end(),[](const auto& k){return k.id==5004;}));
    std::cout<<"COMMAND detail expansion/reselection/row and button geometry/early acceptance PASS\n";
}
void partialInventoryFailure(const char* root){
    Files files(root);
    // Synthetic original-format action order covers the original executor's
    // partial-add failure before later actions. No shipped mission is changed.
    files.replace("M001010_kr.tble","Category\tValue\nName\tFAILURE FIXTURE\nCompleteAct\tgiveItem 10011 3\nCompleteAct\tgiveMoney 45\nCompleteAct\tgiveExp 5\nCompleteAct\ttakeItem 05001 1\n");
    auto data=initial();auto defs=definitions(files);progression::SlotView slot(data.slot(0));Events events;progression::Progression prep(slot,defs,events);prep.restore();assert(prep.acceptMission("M001010","Base001"));assert(prep.inventory.add(*defs.findItem("05001"),2));
    for(int i=0;prep.inventory.emptySlots()>2&&i<75;++i)if(!prep.inventory.entry(i)->item)assert(prep.inventory.setItem(i,defs.findItem("11001"),1));assert(prep.inventory.emptySlots()==2);const auto weaponsBefore=prep.inventory.count(*defs.findItem("10011"));prep.prepareSave();
    Host host;DarwinWorldRandom random(1);GameSession game(files,host,random,data);assert(game.selectSlot(0));game.enterBase();const auto saves=host.writes.size();
    host.onAlert=[&](std::string_view message){assert(message=="TakeItemFail"&&game.state().completedMission.dataId=="M001010"&&game.state().credits==500);action(game,5007,"M001010");assert(game.state().completedMission.dataId=="M001010");};
    command(game);assert(host.alerts==std::vector<std::string>({"TakeItemFail"})&&game.state().credits==545&&count(game,"10011")==weaponsBefore+2&&count(game,"05001")==1&&game.state().completedMission.rewards.size()==3&&host.writes.size()==saves);
    assert(game.state().acceptedMissions.empty());action(game,5007,"M001010");game.tick();assert(game.state().completedMission.dataId.empty()&&game.state().commandListVisible&&game.state().credits==545&&host.alerts.size()==1&&host.writes.size()==saves);
    std::cout<<"COMPLETED partialItemFailureKeepsAddedItems/remainingActionsContinue/takeNotDisplayed/noRetryOrSave/reentrant5007Rejected PASS\n";
}
void missionObjects(Files& files){
    const auto defs=definitions(files);
    const auto expectedEnemyPosition=[](unsigned precedingItems){
        DarwinWorldRandom expected(1);
        // openMap initializes 64 pool particles before addMissionObjects.
        for(unsigned i=0;i<64+2*precedingItems;++i)(void)expected.random();
        const auto coordinate=[&]{
            const float unit=float(expected.random())*0x1p-31f;
            const float scaled=float(double(unit)*600.0);
            return scaled-300.f;
        };
        const float x=coordinate(),y=coordinate();
        return Vec2{x,y};
    };
    unsigned itemActions=0,enemyActions=0;
    for(const auto& mission:defs.missions()){
        for(const auto& action:mission.acceptActions){
            if(action.type!=5&&action.type!=6)continue;
            auto data=initial();
            progression::SlotView slot(data.slot(0));
            slot.u32(0x1b5c,5);
            Events events;
            progression::Progression progress(slot,defs,events);
            progress.restore();
            assert(progress.missions.accept(mission.id)&&!progress.missions.state(mission.id)->requireComplete);
            progress.prepareSave();
            Host host;
            DarwinWorldRandom random(1);
            GameSession game(files,host,random,data);
            assert(game.selectSlot(0)&&game.enterField("Stage01_01",0));
            assert(game.fieldItems().empty()&&game.battle()->enemies().empty());
            assert(game.enterField(action.place,0));
            const auto items=game.fieldItems();
            const auto enemies=game.battle()->enemies();
            if(action.type==5){
                ++itemActions;
                assert(enemies.empty()&&items.size()==1&&items[0].code==action.item&&items[0].value==1);
                assert(items[0].position.x==float(action.x)&&items[0].position.y==float(action.y));
                assert(game.enterField(action.place,0)&&game.fieldItems().size()==1);
            }else{
                ++enemyActions;
                assert(items.empty()&&enemies.size()==1&&enemies[0].name==action.enemy&&!enemies[0].boss);
                const auto expected=expectedEnemyPosition(0);
                if(enemies[0].position.x!=expected.x||enemies[0].position.y!=expected.y)
                    std::cerr<<mission.id<<" "<<action.place<<" expected="<<std::hexfloat<<expected.x<<","<<expected.y
                             <<" actual="<<enemies[0].position.x<<","<<enemies[0].position.y
                             <<" rngCalls="<<game.battle()->metrics().randomCalls<<"\n";
                assert(enemies[0].position.x==expected.x&&enemies[0].position.y==expected.y);
            }
        }
    }
    assert(itemActions==7&&enemyActions==7);

    // These original actions coexist on Stage02_06. The item consumes two
    // RNG draws only when it is pending and earlier in the accepted list.
    for(bool itemFirst:{false,true})for(bool itemReady:{false,true}){
        auto data=initial();
        progression::SlotView slot(data.slot(0));
        slot.u32(0x1b5c,5);
        Events events;
        progression::Progression progress(slot,defs,events);
        progress.restore();
        assert(progress.missions.accept("M021030")&&progress.missions.accept("M001160"));
        if(itemReady)assert(progress.inventory.add(*defs.findItem("05109"),1));
        assert(progress.missions.state("M021030")->requireComplete==itemReady);
        if(itemFirst)order(slot,{"M021030","M001160"});
        else order(slot,{"M001160","M021030"});
        progress.prepareSave();
        Host host;
        DarwinWorldRandom random(1);
        GameSession game(files,host,random,data);
        assert(game.selectSlot(0)&&game.enterField("Stage02_06",0));
        const auto enemies=game.battle()->enemies();
        assert(game.fieldItems().size()==(itemReady?0:1));
        const auto expected=expectedEnemyPosition(itemFirst&&!itemReady?1:0);
        assert(enemies.size()==1&&enemies[0].position.x==expected.x&&enemies[0].position.y==expected.y);
    }
    std::cout<<"MISSIONS all original item/enemy placements, location checks, accepted order, ready skip and reentry PASS\n";
}
void playMissionObject(Files& files,bool combat){
    const auto defs=definitions(files);
    const std::string_view id=combat?"M020030":"M001030";
    const auto def=std::find_if(defs.missions().begin(),defs.missions().end(),[&](const auto& m){return m.id==id;});
    assert(def!=defs.missions().end());
    const auto& placement=def->acceptActions.at(0);
    const auto maps=Maps::parse(Chunk::parse(files.read("MapInfo.mcb")));
    const auto& map=maps.map(placement.place);
    const Vec2 itemPosition{float(placement.x),float(placement.y)};
    const auto distanceSquared=[](Vec2 a,Vec2 b){const auto x=a.x-b.x,y=a.y-b.y;return x*x+y*y;};
    const auto gate=std::min_element(map.gates.begin(),map.gates.end(),[&](const auto& a,const auto& b){
        return distanceSquared(a.position,itemPosition)<distanceSquared(b.position,itemPosition);
    });
    assert(gate!=map.gates.end());
    auto data=initial();
    progression::SlotView slot(data.slot(0));
    slot.u32(0x1b5c,5);slot.credit(10000);
    slot.u32(0x1b60+(combat?2:0)*4,1); // Destination already visited.
    slot.u32(0x654,0);
    Events events;
    progression::Progression progress(slot,defs,events);
    progress.restore();
    assert(progress.missions.accept(id)&&!progress.missions.state(id)->requireComplete);
    if(combat){
        slot.level(99);slot.experience(progression::Experience::bounds(99).previous);
        const progression::Item* armor=nullptr;
        for(const auto& [code,item]:defs.items())
            if(item.type==13&&(!armor||item.param1>armor->param1))armor=&item;
        assert(armor);
        assert(!progress.inventory.entry(20)->item&&progress.inventory.setItem(20,armor,1)&&progress.inventory.setEquip(20,4));
        assert(progress.inventory.add(*defs.findItem("02007"),30));
        assert(progress.inventory.add(*defs.findItem("01006"),50));
    }
    progress.prepareSave();
    if(combat){slot.string(0x61c,6,"02007");slot.string(0x622,6,"01006");}
    Host host;
    DarwinWorldRandom random(1);
    GameSession game(files,host,random,data);
    assert(game.selectSlot(0));game.enterBase();
    assert(game.enterField(placement.place,gate->number));
    auto tick=[&]{game.tick();(void)game.drawWorld();};
    auto touch=[&](Vec2 position){
        const auto point=game.battle()->camera().toScreen(position);
        if(point.x<0||point.x>768||point.y<100||point.y>1024)return false;
        assert(game.inputWorld(PointerPhase::begin,point,81));
        (void)game.inputWorld(PointerPhase::end,point,81);
        return true;
    };
    auto completed=[&]{
        const auto& missions=game.state().acceptedMissions;
        return std::any_of(missions.begin(),missions.end(),[&](const auto& mission){return mission.dataId==id&&mission.complete;});
    };
    const auto experience=[&]{
        const auto bytes=game.workingSlot();
        return std::uint32_t(bytes[0x70])|(std::uint32_t(bytes[0x71])<<8)
            |(std::uint32_t(bytes[0x72])<<16)|(std::uint32_t(bytes[0x73])<<24);
    };
    const auto initialXP=experience();
    const auto initialStock=combat?count(game,"02007"):count(game,placement.item);
    const auto target=combat?game.battle()->enemies().at(0).id:0;
    const auto initialPosition=game.battle()->player().position;
    for(unsigned frame=0;frame<1800&&!completed();++frame){
        if(combat&&game.state().hpRate<.65f&&frame%12==0){
            action(game,6201,{},ui::ButtonEventType::push);
            assert(touch(game.battle()->player().position));
        }
        if(frame%45==0){
            auto destination=itemPosition;
            if(combat){
                const auto enemies=game.battle()->enemies();
                const auto enemy=std::find_if(enemies.begin(),enemies.end(),[&](const auto& e){return e.id==target;});
                assert(enemy!=enemies.end()&&enemy->hp>0);
                destination=enemy->position;
                const auto point=game.battle()->camera().toScreen(destination);
                if(point.x>0&&point.x<768&&point.y>100&&point.y<1024){
                    action(game,6200,{},ui::ButtonEventType::push);
                    assert(touch(destination));
                    tick();
                    continue;
                }
            }
            if(!touch(destination)){
                const auto player=game.battle()->player().position;
                assert(touch({player.x+(destination.x-player.x)*.25f,player.y+(destination.y-player.y)*.25f}));
            }
        }
        tick();
        if(game.battle()->playerHP()<=0)
            std::cerr<<"Mission "<<id<<" Player destroyed at "<<frame
                     <<", airStrikes="<<game.battle()->metrics().airStrikeRequests<<"\n";
        assert(game.battle()->playerHP()>0);
    }
    if(!completed()){
        const auto player=game.battle()->player().position;
        std::cerr<<"Mission "<<id<<" incomplete; gate="<<gate->number<<" from "<<initialPosition.x<<","<<initialPosition.y
                 <<" to "<<player.x<<","<<player.y<<" airStrikes="<<game.battle()->metrics().airStrikeRequests<<"\n";
    }
    assert(completed());
    if(combat){
        assert(count(game,"02007")<initialStock&&experience()>initialXP);
        const auto enemies=game.battle()->enemies();
        assert(std::any_of(enemies.begin(),enemies.end(),[&](const auto& e){return e.id==target&&e.corpse&&e.activeParts==1;}));
    }else{
        assert(initialStock==0&&count(game,placement.item)==1);
        const auto frame=game.drawWorld();
        const auto& pickup=sprite(frame,"PickItem.png");
        const auto& label=sprite(frame,"cpu-text:Player.pickup");
        const auto player=game.battle()->camera().toScreen(game.battle()->player().position);
        assert(pickup.center.x==player.x&&std::abs(pickup.center.y-player.y-50.f)<.001f);
        assert(label.center.x==std::round(pickup.center.x)&&label.center.y==std::round(pickup.center.y));
        assert(!host.pickupRequests.empty());
        const auto& request=host.pickupRequests.back();
        assert(request.text==defs.findItem(placement.item)->name&&request.fontName=="Arial"&&request.fontSize==12&&
            request.dimensions.width==120&&request.dimensions.height==14&&request.alignment==ui::TextAlign::center);
    }

    action(game,1027);action(game,combat?4004:4002,def->completePlace);action(game,4001);settle(game);
    assert(game.state().screen==ui::ScreenId::base);
    const auto credit=game.state().credits;
    const auto beforeRewardXP=experience();
    const auto beforeRewardBot=count(game,"04201");
    const auto saves=host.writes.size();
    command(game);
    assert(game.state().completedMission.dataId==id);
    assert(game.state().credits==credit+(combat?40000:300)&&experience()==beforeRewardXP+(combat?40000:400));
    if(!combat)assert(count(game,placement.item)==0&&count(game,"04201")==beforeRewardBot+1);
    action(game,5007,std::string(id));game.tick();
    action(game,2100);settle(game);
    assert(host.writes.size()==saves+1&&host.writes.back()==SaveReason::enterBase);
    DarwinWorldRandom decoder(1);
    auto saved=SaveData::decode(host.stored,decoder,false);
    progression::SlotView restoredSlot(saved.slot(0));
    progression::Progression restored(restoredSlot,defs,events);
    restored.restore();
    assert(restored.missions.state(id)->complete&&restoredSlot.credit()==game.state().credits);
    std::cout<<"MISSION "<<id<<" actual "<<(combat?"AirStrike/kill/XP":"movement/pickup")<<"/Airlift/command reward/save PASS\n";
}
}
int main(int argc,char** argv){if(argc<2)return 77;Files files(argv[1]);baseDescriptionAndNotices(files);orderedSweep(files);popupInputAndGeometry(files);acceptRefreshOnly(files);retainedRowSelection(files);failedAcceptanceRefresh(files);commandDetailExpansion(files);partialInventoryFailure(argv[1]);missionObjects(files);playMissionObject(files,false);playMissionObject(files,true);}

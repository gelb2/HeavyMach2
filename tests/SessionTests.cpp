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
#include <functional>
using namespace hm2;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;unsigned reads=0;mutable unsigned probes=0;
    explicit Files(std::filesystem::path p):root(std::move(p)){}
    Bytes read(std::string_view name)override{++reads;std::ifstream in(root/std::string(name),std::ios::binary);if(!in)throw std::runtime_error("Missing original test asset "+std::string(name));return Bytes(std::istreambuf_iterator<char>(in),{});}
    bool exists(std::string_view name)const override{++probes;return std::filesystem::exists(root/std::string(name));}
};
struct Host:SessionHost {
    ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& r)override{return test::preparedTextStub(r);}
    Bytes stored;std::vector<SaveReason> writes;std::vector<unsigned> sounds;
    unsigned names=0,deletes=0;bool fail=false;std::vector<std::string> alerts,namedSounds;
    void sound(std::string_view name,Vec2)override{namedSounds.emplace_back(name);}
    void soundID(unsigned id,Vec2)override{sounds.push_back(id);}
    void alert(std::string_view s)override{alerts.emplace_back(s);}
    void requestName(unsigned)override{++names;}
    void requestDelete(unsigned)override{++deletes;}
    bool writeSave(std::span<const std::uint8_t> bytes,SaveReason reason)override{writes.push_back(reason);if(fail)return false;stored.assign(bytes.begin(),bytes.end());return true;}
    std::uint32_t absoluteTimeLow()override{return 1;}
    std::uint32_t unixTime()override{return 1794268800;}
};
void settle(GameSession& session){unsigned ticks=0;while(session.screenTransition().phase!=8&&ticks++<200){session.tick();session.drawWorld();(void)session.state();}assert(session.screenTransition().phase==8);}
void button(GameSession& session,ui::ScreenId screen,unsigned id,ui::ButtonEventType type=ui::ButtonEventType::pop,int data=0){session.action({screen,ui::Panel::root,{type,{0,id,data},1,{}}});settle(session);}
void titleToSlots(GameSession& session){
    assert(session.state().screen==ui::ScreenId::title);
    session.action({ui::ScreenId::title,ui::Panel::root,{ui::ButtonEventType::pop,{},0,{}},true,false});assert(session.state().screen==ui::ScreenId::title);
    session.action({ui::ScreenId::title,ui::Panel::root,{ui::ButtonEventType::pop,{},0,{}},true,true});settle(session);assert(session.state().screen==ui::ScreenId::slots);
}
void action(GameSession& session,unsigned id,ui::ButtonEventType phase=ui::ButtonEventType::pop,std::string dataId={},Vec2 point={},int data=0,ui::Panel panel=ui::Panel::root){session.action({session.state().screen,panel,{phase,{0,id,data,std::move(dataId)},1,point}});settle(session);}
void drag(GameSession& session,int cell,Vec2 destination){const auto code=session.state().inventory[std::size_t(session.state().inventoryPage*25+cell)].dataId;action(session,6000+unsigned(cell),ui::ButtonEventType::push,code);action(session,6000+unsigned(cell),ui::ButtonEventType::move,code,destination);action(session,6000+unsigned(cell),ui::ButtonEventType::cancel,code,destination);}
int countItem(GameSession& session,std::string_view code){int total=0;for(const auto& item:session.state().inventory)if(item.dataId==code)total+=item.count;return total;}
std::size_t imageCount(const Frame& frame,std::string_view name){std::size_t result=0;for(const auto& sprite:frame.sprites)if(sprite.image==name)result+=sprite.atlasBatch?sprite.atlasBatch->quads.size():1;return result;}
SaveData fixture(std::string_view base="Base001"){
    SaveData data;data.initializeSlot(0,"MACH","us",1794268800);progression::SlotView slot(data.slot(0));slot.string(0x2d,16,base);
    constexpr std::string_view extras[]={"10111","11001","12001","04201","01001"};
    for(unsigned i=0;i<std::size(extras);++i){slot.string(0x8c+(8+i)*6,6,extras[i]);slot.byte(0x53c+8+i,1);}return data;
}
struct UIHost:ui::ScreensHost {
    GameSession& game;Host& host;ui::Screens* ui=nullptr;std::uint64_t revision=0;
    explicit UIHost(GameSession& g,Host& h):game(g),host(h){}
    void synchronize(){const auto& state=game.state();if(revision!=game.presentationRevision()){ui->refresh(state,game.renderTime());revision=game.presentationRevision();}else ui->updateHUD(state);}
    void action(const ui::ScreenAction& a)override{game.action(a);synchronize();}
    ui::PreparedScreenText prepareText(const ui::TextRasterRequest& r)override{auto b=std::make_shared<ui::TextBitmap>();b->request=r;b->layout=ui::textRasterLayout(r);b->alpha.resize(std::size_t(b->layout.width)*b->layout.height);return {"prepared:"+r.sourceId,b};}
    void bgm(std::string_view)override{}void sound(std::string_view)override{}void sfx(unsigned id)override{host.soundID(id,{});}
};
void click(ui::Screens& screens,ui::ButtonKey key,float now){const auto b=screens.button(key);assert(b);const auto pos=b->screenPosition();assert(screens.input(PointerPhase::begin,pos,23,now));(void)screens.input(PointerPhase::end,pos,23,now+.1f);}
ui::ButtonKey key(ui::Screens& screens,unsigned id,int data=0){for(const auto& key:screens.buttons())if(key.id==id&&key.intData==data)return key;assert(false);return {};}

}
int main(int argc,char** argv){
    if(argc<2)return 77;Files files(argv[1]);Host host;DarwinWorldRandom rng(77);GameSession session(files,host,rng);
    titleToSlots(session);const auto initialUnsupported=session.metrics().unsupportedActions;action(session,1001,ui::ButtonEventType::push);assert(session.metrics().unsupportedActions==initialUnsupported);action(session,99999);assert(session.metrics().unsupportedActions==initialUnsupported+1);button(session,ui::ScreenId::slots,1009,ui::ButtonEventType::push,0);button(session,ui::ScreenId::slots,1001);
    assert(host.names==1&&host.writes.empty());assert(!session.createSlot(3,"MACH","us"));assert(session.createSlot(0,"MACH","us"));assert((host.writes==std::vector<SaveReason>{SaveReason::selectSlot}));
    button(session,ui::ScreenId::slots,1001);assert(session.state().screen==ui::ScreenId::base);
    assert((host.writes==std::vector<SaveReason>{SaveReason::selectSlot,SaveReason::selectSlot,SaveReason::enterBase}));assert(session.metrics().saveCompletions==3);
    const auto saved=host.stored;DarwinWorldRandom loadRng(4);const auto decoded=SaveData::decode(saved,loadRng,false);
    assert(std::equal(decoded.slot(0).begin(),decoded.slot(0).end(),session.workingSlot().begin()));
    assert(session.state().slots[0].used&&session.state().slots[0].name=="MACH");assert(session.state().inventory[0].equipped&&session.state().inventory[2].equipped);
    assert(session.state().hpRate==1&&session.state().credits==500&&session.state().level==0);
    button(session,ui::ScreenId::base,2001);assert(session.state().screen==ui::ScreenId::base);button(session,ui::ScreenId::base,2001);assert(session.state().screen==ui::ScreenId::command);
    assert(session.acceptMission("M001010")&&!session.acceptMission("M001010"));assert(!session.claimMission("M001010"));assert(host.stored==saved); // mission changes are in memory
    session.enterBase();const auto afterBase=host.writes.size();
    button(session,ui::ScreenId::base,2005);button(session,ui::ScreenId::base,2005);
    assert(session.state().screen==ui::ScreenId::field&&session.battle());assert(host.writes.size()==afterBase);
    assert(std::abs(session.battle()->player().position.x+275)<.0001f&&std::abs(session.battle()->player().position.y+412)<.0001f);
    assert(std::abs(session.battle()->player().angle-1.57079637f)<.00001f);
    const auto reads=files.reads,probes=files.probes;
    for(unsigned i=0;i<600;++i){session.tick();if(!session.battle()->selectedEnemy())for(const auto& e:session.battle()->enemies())if(e.hp>0){session.battle()->selectEnemy(e.id);break;}session.drawWorld();}
    assert(session.battle()->metrics().playerEmissions>0&&session.battle()->metrics().enemyEmissions>0);
    assert(files.reads==reads&&files.probes==probes&&host.writes.size()==afterBase); // no shot/tick storage or asset IO
    button(session,ui::ScreenId::field,1021);const auto paused=session.battle()->clock().worldTime();session.tick();assert(session.battle()->clock().worldTime()==paused&&session.state().systemVisible);
    button(session,ui::ScreenId::field,1030);session.tick();assert(session.battle()->clock().worldTime()>paused&&!session.state().systemVisible);
    const auto beforeInactive=session.renderTime();session.setActive(false);session.tick();assert(session.renderTime()==beforeInactive);session.setActive(true);
    const auto storedBeforeFail=host.stored;host.fail=true;assert(!session.save(SaveReason::saveAndExit));assert(host.stored==storedBeforeFail&&session.metrics().saveCompletions+1==session.metrics().saveRequests);host.fail=false;assert(session.save(SaveReason::saveAndExit));
    // A second independent game uses a separate envelope and Darwin stream.
    // Original GimmeCr is an original-name cheat, not an invented fake award.
    Files secondFiles(argv[1]);Host secondHost;DarwinWorldRandom secondRng(99);GameSession second(secondFiles,secondHost,secondRng);
    assert(second.createSlot(1,"GimmeCr","us"));assert(second.envelope().slot(1)[0x6c]==51);
    assert(second.selectSlot(1));second.enterBase();
    // initSlot creates level50/EXP100m and selects once (level51).
    // The subsequent explicit start selects again, advancing once to52.
    assert(second.state().level==52&&second.state().credits==100000000);
    progression::SlotView secondSlot(std::span(const_cast<std::uint8_t*>(second.workingSlot().data()),second.workingSlot().size()));
    assert(secondSlot.hp()==secondSlot.maxHP()&&secondSlot.maxHP()==1296);
    const auto firstRng=rng.calls();const auto firstState=session.workingSlot();const Bytes firstBytes(firstState.begin(),firstState.end());
    assert(second.enterField("Stage01_01",0));for(unsigned i=0;i<360;++i){second.tick();if(!second.battle()->selectedEnemy())for(const auto& e:second.battle()->enemies())if(e.hp>0){second.battle()->selectEnemy(e.id);break;}second.drawWorld();}
    assert(rng.calls()==firstRng&&std::equal(firstBytes.begin(),firstBytes.end(),session.workingSlot().begin()));
    assert(secondHost.writes.size()==3&&second.battle()->metrics().kills>0);

    // Real menu action phases operate on original progression/equipment state.
    Files menuFiles(argv[1]);Host menuHost;DarwinWorldRandom menuRng(3);GameSession menus(menuFiles,menuHost,menuRng,fixture());
    assert(menus.selectSlot(0));menus.enterBase();const auto menuSaves=menuHost.writes.size();
    action(menus,2001);action(menus,2001);assert(menus.state().screen==ui::ScreenId::command);
    const auto available=menus.state().availableMissions;assert(!available.empty());
    const auto unsupportedBefore=menus.metrics().unsupportedActions;
    action(menus,5100,ui::ButtonEventType::push,available[0].dataId);action(menus,5100,ui::ButtonEventType::release,available[0].dataId);
    assert(menus.state().selectedMission==-1&&menus.metrics().unsupportedActions==unsupportedBefore);
    action(menus,5100,ui::ButtonEventType::pop,"stale-id");assert(menus.state().selectedMission==-1);
    action(menus,5100,ui::ButtonEventType::pop,available[0].dataId);assert(menus.state().selectedMission==0&&!menus.state().missionBody.empty());
    action(menus,5004,ui::ButtonEventType::push,available[0].dataId);assert(menus.state().acceptedMissions.empty());
    action(menus,5004,ui::ButtonEventType::pop,available[0].dataId);assert(menus.state().acceptedMissions.size()==1&&menus.state().acceptedMissions[0].dataId==available[0].dataId);
    action(menus,5009,ui::ButtonEventType::push);assert(menus.metrics().unsupportedActions==unsupportedBefore);
    const auto rankingAlerts=menuHost.alerts.size();
    action(menus,5009);assert(menus.metrics().unsupportedActions==unsupportedBefore);
    assert(menuHost.alerts.size()==rankingAlerts+1&&menuHost.alerts.back()=="현재 랭킹 서비스를 이용할 수 없습니다.");
    assert(menuHost.writes.size()==menuSaves);action(menus,2100);assert(menus.state().screen==ui::ScreenId::base&&menuHost.writes.size()==menuSaves+1);
    action(menus,2002);action(menus,2002);assert(menus.state().screen==ui::ScreenId::shop&&menus.state().inventoryVisible);
    action(menus,2104,ui::ButtonEventType::push,{}, {},4);assert(menus.state().shopCategory==4&&menus.state().shopPage==0);
    const auto& stock=menus.state().shopItems;const auto repair=std::find_if(stock.begin(),stock.end(),[](const auto& item){return item.dataId=="01001";});assert(repair!=stock.end());
    const auto stockIndex=unsigned(repair-stock.begin());assert(stockIndex<24&&repair->description.ends_with("Price : 10cr"));
    const int credit=menus.state().credits,repairCount=countItem(menus,"01001");
    action(menus,2200+stockIndex,ui::ButtonEventType::push,"01001");action(menus,2102);assert(menus.state().tradeVisible&&menus.state().tradeCount==1&&menus.state().tradePrice==10);
    action(menus,2111,ui::ButtonEventType::push);assert(menus.state().tradeCount==1); // Original Pop, not Push.
    action(menus,2111);assert(menus.state().tradeCount==2&&menus.state().tradePrice==20);
    action(menus,2104,ui::ButtonEventType::push,{}, {},0);assert(menus.state().shopCategory==4); // Trade blocks underlying controls.
    action(menus,2108);assert(!menus.state().tradeVisible&&menus.state().credits==credit-20&&countItem(menus,"01001")==repairCount+2);
    const auto repairItem=std::find_if(menus.state().inventory.begin(),menus.state().inventory.end(),[](const auto& item){return item.dataId=="01001";});const auto repairPos=int(repairItem-menus.state().inventory.begin());const int selectedRepairCount=repairItem->count;assert(repairPos<25&&repairItem->description.ends_with("SellPrice : 5cr"));
    const auto soundsBefore=menuHost.sounds.size();action(menus,6000+unsigned(repairPos),ui::ButtonEventType::push,"01001");assert(menuHost.sounds.size()==soundsBefore+2); // DlgInven then GuiShop source notifications.
    action(menus,2103);assert(menus.state().tradeVisible&&!menus.state().buyMode&&menus.state().tradeCount==selectedRepairCount);
    action(menus,2110);const int sold=selectedRepairCount-1;action(menus,2109);assert(countItem(menus,"01001")==repairCount+2-sold&&menus.state().credits==credit-20+sold*5);
    action(menus,6000,ui::ButtonEventType::push,"11001");assert(!menus.state().inventory[0].enabled);action(menus,2103);assert(!menus.state().tradeVisible); // Equipped item cannot sell.
    assert(menuHost.writes.size()==menuSaves+1);action(menus,2100);

    // Maintenance uses source target geometry and commits on Cancel only.
    Files maintenanceFiles(argv[1]);Host maintenanceHost;DarwinWorldRandom maintenanceRng(4);GameSession maintenance(maintenanceFiles,maintenanceHost,maintenanceRng,fixture("Base004"));
    assert(maintenance.selectSlot(0));maintenance.enterBase();action(maintenance,2003);action(maintenance,2003);assert(maintenance.state().screen==ui::ScreenId::maintenance);
    const auto maintenanceWrites=maintenanceHost.writes.size();
    drag(maintenance,8,{651,515});assert(maintenance.state().inventory[9].dataId=="10111"&&maintenance.state().inventory[8].dataId=="11001");
    drag(maintenance,9,{180,555});assert(maintenance.state().equipment[2].dataId=="10111");
    const auto botsBefore=countItem(maintenance,"04201");drag(maintenance,11,{337,444});assert(maintenance.state().bots[2].dataId=="04201"&&countItem(maintenance,"04201")==botsBefore-1);
    action(maintenance,6302,ui::ButtonEventType::push,"04201");action(maintenance,6310);assert(maintenance.state().bots[2].dataId.empty()&&countItem(maintenance,"04201")==botsBefore);
    action(maintenance,6091,ui::ButtonEventType::pop);assert(maintenance.state().inventoryPage==0);
    action(maintenance,6091,ui::ButtonEventType::push);assert(maintenance.state().inventoryPage==1);
    action(maintenance,6090,ui::ButtonEventType::push);action(maintenance,6093,ui::ButtonEventType::push);assert(maintenance.state().equipment[2].dataId=="10111"&&maintenanceHost.writes.size()==maintenanceWrites);

    // Actual shared Screens inputs replace the field part/controller without a
    // world reset and request exactly one equip sound per original action.
    Files fieldFiles(argv[1]);Host fieldHost;DarwinWorldRandom fieldRng(5);GameSession field(fieldFiles,fieldHost,fieldRng,fixture());
    assert(field.selectSlot(0));field.enterBase();assert(field.enterField("Stage01_01",0));field.tick();
    PreparedTiles fieldTiles("session-ui-test");UIHost sink(field,fieldHost);ui::Screens screens(fieldFiles,fieldTiles,sink);sink.ui=&screens;sink.synchronize();
    Frame hud;screens.append(hud,field.renderTime());assert(imageCount(hud,"Icon01001.png")==1&&imageCount(hud,"Icon02004.png")==1&&imageCount(hud,"Icon02005.png")==1);
    assert(imageCount(hud,"IconB10011.png")==1&&imageCount(hud,"IconB10111.png")==1);
    const auto fieldScene=field.battle();const auto originalPose=fieldScene->player().position;const auto selectedBefore=fieldScene->selectedEnemy();const auto randomBefore=fieldRng.calls();
    auto beforeFrame=field.drawWorld();assert(imageCount(beforeFrame,"P_Wpn_Gun_101.png")==1&&imageCount(beforeFrame,"P_Wpn_Msl_101.png")==1);
    const auto sfx4Before=std::count(fieldHost.sounds.begin(),fieldHost.sounds.end(),4u);
    click(screens,key(screens,1022),field.renderTime()+1);assert(field.state().weaponChoiceType==2);
    click(screens,key(screens,1024,8),field.renderTime()+2);assert(field.state().equipment[2].dataId=="10111"&&field.state().weaponChoiceType==0);
    assert(field.battle()==fieldScene&&fieldRng.calls()==randomBefore&&fieldScene->player().position.x==originalPose.x&&fieldScene->player().position.y==originalPose.y&&fieldScene->selectedEnemy()==selectedBefore);
    field.tick();auto afterFrame=field.drawWorld();assert(imageCount(afterFrame,"P_Wpn_Gun_101.png")==0&&imageCount(afterFrame,"P_Wpn_Msl_101.png")==2);
    assert(std::count(fieldHost.sounds.begin(),fieldHost.sounds.end(),4u)==sfx4Before+2); // Selector1 + actual equip1.
    const auto dynamicRevision=field.presentationRevision();for(unsigned i=0;i<20;++i){field.tick();(void)field.state();}assert(field.presentationRevision()==dynamicRevision);
    action(field,1025);const auto pausedAt=field.battle()->clock().worldTime();field.tick();assert(field.state().inventoryVisible&&field.battle()->clock().worldTime()==pausedAt);
    action(field,6005,ui::ButtonEventType::push,"01001");action(field,6005,ui::ButtonEventType::move,"01001",{739,29});assert(field.state().extQuick==4);
    const auto cleanupSounds=fieldHost.sounds.size(),cleanupSaves=fieldHost.writes.size();const auto cleanupRandom=fieldRng.calls();
    field.setActive(false);assert(field.state().extQuick==-1&&field.state().extInventory==-1&&field.state().extEquipment==-1&&field.state().extBot==-1&&field.state().selectedInventory==5);
    assert(field.state().quick[4].dataId.empty()&&fieldHost.sounds.size()==cleanupSounds&&fieldHost.writes.size()==cleanupSaves&&fieldRng.calls()==cleanupRandom);
    field.setActive(true);action(field,6005,ui::ButtonEventType::cancel,"01001",{739,29});assert(field.state().quick[4].dataId.empty());
    drag(field,5,{739,29});assert(field.state().quick[4].dataId=="01001"&&countItem(field,"01001")==6);
    const auto restrictions=fieldHost.alerts.size();drag(field,10,{188,514});assert(fieldHost.alerts.size()==restrictions+1&&fieldHost.alerts.back()=="EquipParts");
    drag(field,11,{326,590});assert(fieldHost.alerts.size()==restrictions+2&&fieldHost.alerts.back()=="EquipParts");
    const int pickerStock=countItem(field,"04201");
    drag(field,11,{739,29});assert(field.state().quick[4].dataId=="04201"&&pickerStock>0);
    action(field,6092,ui::ButtonEventType::pop);assert(field.state().inventoryVisible);action(field,6092,ui::ButtonEventType::push);assert(!field.state().inventoryVisible);
    sink.synchronize();
    const auto unsupportedQuick=field.metrics().unsupportedActions;
    const auto quickSaveCount=fieldHost.writes.size();
    const auto quickSfx=std::count(fieldHost.sounds.begin(),fieldHost.sounds.end(),0u);
    click(screens,key(screens,6204),field.renderTime()+1);
    assert(field.state().selectedQuick==4&&!field.state().messageVisible&&countItem(field,"04201")==pickerStock);
    click(screens,key(screens,6204),field.renderTime()+2);
    assert(field.state().selectedQuick==-1&&countItem(field,"04201")==pickerStock);
    assert(std::count(fieldHost.sounds.begin(),fieldHost.sounds.end(),0u)==quickSfx+2);
    click(screens,key(screens,6200),field.renderTime()+3);
    const auto repairMessage=field.state().message;
    assert(field.state().selectedQuick==0&&!repairMessage.empty());
    click(screens,key(screens,6204),field.renderTime()+4);
    assert(field.state().selectedQuick==4&&field.state().message==repairMessage);
    const auto hpBeforeQuick=field.battle()->playerHP();
    const auto playerPoint=field.battle()->camera().toScreen(field.battle()->player().position);
    assert(field.inputWorld(PointerPhase::begin,playerPoint,88));
    assert(field.battle()->playerHP()==hpBeforeQuick&&countItem(field,"04201")==pickerStock-1&&countItem(field,"01001")==6);
    assert(field.state().selectedQuick==-1&&!field.state().messageVisible&&fieldHost.writes.size()==quickSaveCount);
    assert(field.metrics().unsupportedActions==unsupportedQuick);
    const auto saveGateWrites=fieldHost.writes.size();assert(!field.battle()->isSave());action(field,1031);assert(field.state().screen==ui::ScreenId::title&&!field.battle()&&fieldHost.writes.size()==saveGateWrites);
    assert(field.enterField("Stage01_01",0));field.battle()->setIsSave(true);action(field,1031);assert(fieldHost.writes.size()==saveGateWrites+1&&fieldHost.writes.back()==SaveReason::saveAndExit);
    // Six original AidKit definitions run through real quick input, actual actor,
    // shared world pool and slot/HUD. No direct HP result or inventory injection.
    constexpr const char* aidCodes[]={"01001","01002","01003","01004","01005","01006"};
    constexpr int aidAmount[]={120,240,500,30,50,100};
    for(unsigned kit=0;kit<6;++kit){
        Files aidFiles(argv[1]);Host aidHost;DarwinWorldRandom aidRng(31);auto start=fixture();progression::SlotView original(start.slot(0));
        original.hp(64);original.string(0x8c+5*6,6,aidCodes[kit]);original.byte(0x53c+5,5);original.string(0x61c,6,aidCodes[kit]);
        // Remove fixture's additional AidKit so aggregate count is exactly5.
        original.string(0x8c+12*6,6,"");original.byte(0x53c+12,0);
        GameSession aid(aidFiles,aidHost,aidRng,start);assert(aid.selectSlot(0)&&aid.enterField("Stage01_01",0));aid.tick();
        auto* battle=aid.battle();const auto& initial=aid.state();const int maximum=initial.equipmentStats.baseHP+initial.equipmentStats.bonusHP;
        const int beforeHP=battle->playerHP(),beforeCount=countItem(aid,aidCodes[kit]);assert(beforeCount==5);
        const auto reads=aidFiles.reads,probes=aidFiles.probes;const auto writes=aidHost.writes.size();const auto randomCalls=aidRng.calls();
        const auto allocations=battle->metrics().particleAllocations;
        const auto repairs=std::count(aidHost.sounds.begin(),aidHost.sounds.end(),13u);
        action(aid,6200,ui::ButtonEventType::pop);assert(aid.state().selectedQuick==-1);
        action(aid,6200,ui::ButtonEventType::push);assert(aid.state().selectedQuick==0&&!aid.state().message.empty()&&aid.state().messageVisible&&!aid.state().messageAutoHide&&aid.state().messageBackground==0);
        const auto messageRevision=aid.state().messageRevision;
        const auto logical=battle->camera().toScreen(battle->player().position);
        assert(aid.inputWorld(PointerPhase::begin,logical,88));
        const int healed=kit<3?aidAmount[kit]:maximum*aidAmount[kit]/100;
        assert(battle->playerHP()==std::min(maximum,beforeHP+healed));
        std::array<std::uint8_t,SaveData::slotSize> copied{};std::copy(aid.workingSlot().begin(),aid.workingSlot().end(),copied.begin());progression::SlotView actual(copied);
        assert(actual.hp()==battle->playerHP()&&aid.state().hpRate==float(actual.hp())/float(maximum));
        assert(countItem(aid,aidCodes[kit])==4&&aid.state().quick[0].count==4&&aid.state().selectedQuick==-1&&aid.state().message.empty()&&!aid.state().messageVisible&&aid.state().messageRevision==messageRevision+1);
        assert(battle->metrics().particleAllocations==allocations+1&&aidRng.calls()==randomCalls+1);
        assert(std::count(aidHost.sounds.begin(),aidHost.sounds.end(),13u)==repairs+1);
        assert(aidHost.writes.size()==writes&&aidFiles.reads==reads&&aidFiles.probes==probes);
        aid.tick();const auto frame=aid.drawWorld();
        const auto bots=battle->bots();assert(bots.size()==1&&bots.front().name=="Bot_P01");
        assert(imageCount(frame,"HPGauge.png")==2+2*bots.size()&&imageCount(frame,std::string("Icon")+aidCodes[kit]+".png")==1);
        std::size_t playerGauges=0,botGauges=0;
        for(const auto& command:frame.sprites)if(command.image=="HPGauge.png"){
            assert(command.atlasBatch);const auto& quads=command.atlasBatch->quads;assert(quads.size()%2==0);
            const float ratio=float(battle->playerHP())/float(maximum);const auto position=battle->camera().toScreen(battle->player().position);
            for(std::size_t pair=0;pair<quads.size();pair+=2){const auto& bg=quads[pair].vertices;const auto& fg=quads[pair+1].vertices;
                const float width=bg[2].position.x-bg[0].position.x;
                if(std::abs(width-42.f)<.0002f){++playerGauges;
                    assert(std::abs((fg[2].position.x-fg[0].position.x)-42.f*ratio)<.0002f);
                    assert(std::abs((fg[2].position.x+fg[0].position.x)*.5f-(position.x+ratio*21.f-21.f))<.0002f);
                    assert(std::abs((bg[0].position.y+bg[1].position.y)*.5f-(position.y+30.f))<.0002f);
                    assert(std::abs((fg[0].position.y+fg[1].position.y)*.5f-(position.y+31.f))<.0002f);
                }else{++botGauges;assert(std::abs(width-21.f)<.0002f);const auto& bot=bots.front();const auto point=battle->camera().toScreen(bot.position);
                    assert(std::abs((bg[2].position.x+bg[0].position.x)*.5f-point.x)<.0002f);
                    assert(std::abs((bg[0].position.y+bg[1].position.y)*.5f-(point.y+20.f))<.0002f);
                    assert(std::abs((fg[2].position.x-fg[0].position.x)-42.f*bot.gauge.scaleX)<.0002f);
                    assert(std::abs((fg[2].position.x+fg[0].position.x)*.5f-(point.x+bot.gauge.x))<.0002f);
                    assert(std::abs((fg[0].position.y+fg[1].position.y)*.5f-(point.y+21.f))<.0002f);
                }
            }
        }
        assert(playerGauges==1&&botGauges==bots.size());
        // Arm/toggle the actual AirStrike selector without a world target.
        const int airCount=countItem(aid,"02004");const auto unsupported=aid.metrics().unsupportedActions;
        action(aid,6201,ui::ButtonEventType::push);assert(countItem(aid,"02004")==airCount&&aid.state().selectedQuick==1&&aid.metrics().unsupportedActions==unsupported);
        action(aid,6201,ui::ButtonEventType::push);assert(aid.state().selectedQuick==-1&&countItem(aid,"02004")==airCount);
    }
    // Actual three airstrikes: original inventory and real world target input,
    // existing objectBase scheduler, shared GobBullet and impact consumer.
    constexpr const char* airCodes[]={"02004","02005","02007"};constexpr unsigned airShots[]={10,10,1};
    for(unsigned strike=0;strike<3;++strike){
        Files airFiles(argv[1]);Host airHost;DarwinWorldRandom airRng(31);auto start=fixture();progression::SlotView original(start.slot(0));
        for(unsigned i=0;i<200;++i)if(original.string(0x8c+i*6,6)==airCodes[strike]){original.string(0x8c+i*6,6,"");original.byte(0x53c+i,0);}
        original.string(0x8c+5*6,6,airCodes[strike]);original.byte(0x53c+5,5);original.string(0x61c,6,airCodes[strike]);
        GameSession air(airFiles,airHost,airRng,start);assert(air.selectSlot(0)&&air.enterField("Stage01_01",0));air.tick();auto* battle=air.battle();
        auto enemies=battle->enemies();assert(!enemies.empty());auto target=enemies.front();float nearest=1e30f;
        for(const auto& enemy:enemies){const auto logical=battle->camera().toScreen(enemy.position);if(logical.x<0||logical.x>768||logical.y<100||logical.y>1024)continue;
            const auto player=battle->player().position;const float dx=enemy.position.x-player.x,dy=enemy.position.y-player.y;const float d=dx*dx+dy*dy;if(d<nearest){nearest=d;target=enemy;}}
        assert(nearest<1e30f);const auto logical=battle->camera().toScreen(target.position);(void)air.state(); // Finish initial UI preparation before frame/use IO gate.
        const auto reads=airFiles.reads,probes=airFiles.probes;const auto writes=airHost.writes.size();const auto randomCalls=airRng.calls();const auto allocations=battle->metrics().particleAllocations;
        const auto sfx9=std::count(airHost.sounds.begin(),airHost.sounds.end(),9u);
        action(air,6200,ui::ButtonEventType::push);assert(countItem(air,airCodes[strike])==5&&air.state().selectedQuick==0);
        assert(air.inputWorld(PointerPhase::begin,logical,91));assert(countItem(air,airCodes[strike])==4&&air.state().selectedQuick==-1);
        assert(battle->activeAirStrikes()==1&&battle->metrics().airStrikeRequests==1&&battle->metrics().airStrikeProjectiles==0);
        assert(airRng.calls()==randomCalls+1&&battle->metrics().particleAllocations==allocations+1&&std::count(airHost.sounds.begin(),airHost.sounds.end(),9u)==sfx9+1);
        const auto frozenWorld=battle->clock().worldTime();const auto actorTicks=battle->metrics().airStrikeTicks;
        battle->setPaused(true);for(unsigned i=0;i<3;++i)air.tick();
        assert(battle->clock().worldTime()==frozenWorld&&battle->metrics().airStrikeTicks==actorTicks+3&&battle->metrics().airStrikeProjectiles==0);
        battle->setPaused(false);const auto inactiveTicks=battle->metrics().airStrikeTicks;air.setActive(false);air.tick();assert(battle->metrics().airStrikeTicks==inactiveTicks);air.setActive(true);
        bool plane=false,marker=false;for(unsigned i=0;i<360;++i){air.tick();const auto frame=air.drawWorld();plane|=imageCount(frame,"Airplane.png")>0;marker|=imageCount(frame,"Target.png")>0;}
        std::cout<<"HM2_SESSION_AIRSTRIKE "<<airCodes[strike]<<" projectiles="<<battle->metrics().airStrikeProjectiles<<" actualDamageCalls="<<battle->metrics().airStrikeDamageCalls<<" enemyHPRemoved="<<battle->metrics().airStrikeEnemyHPRemoved<<" plane="<<plane<<" marker="<<marker<<" remaining="<<battle->activeAirStrikes()<<"\n";
        assert(battle->metrics().playerEmissions==0&&battle->metrics().airStrikeEnemyHPRemoved>0&&battle->metrics().airStrikeProjectiles==airShots[strike]&&battle->metrics().airStrikeDamageCalls>0&&battle->activeAirStrikes()==0&&plane&&marker);
        assert(airFiles.reads==reads&&airFiles.probes==probes&&airHost.writes.size()==writes&&countItem(air,airCodes[strike])==4);
    }
    std::cout<<"HM2_SESSION six actual AidKit quick/world input HP+slot+gauge/particle/count/SFX/save+IO gates PASS\n";
    std::cout<<"HM2_SESSION menu phases/mission/trade/drag/actualFrame weapon replacement/independent save-bit/scalarRevision/originalIcons PASS\n";
    std::cout<<"HM2_SESSION slots/base/mission/battle shared RNG, independent game state, explicit save/failure, warmIO=0 PASS\n";
}

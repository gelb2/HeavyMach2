#include "Core/UI/Screens.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
using namespace hm2;
using namespace hm2::ui;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;unsigned reads=0;mutable unsigned probes=0;
    explicit Files(const char* p):root(p){}
    Bytes read(std::string_view n)override{++reads;std::ifstream f(root/std::string(n),std::ios::binary);assert(f);return Bytes(std::istreambuf_iterator<char>(f),{});}
    bool exists(std::string_view n)const override{++probes;return std::filesystem::exists(root/std::string(n));}
};
// Text layout test double: deliberately does not claim original glyph raster QA.
struct Host:ScreensHost {
    unsigned preparations=0;std::vector<ScreenAction> actions;std::vector<std::string> songs,sounds;
    std::vector<TextRasterRequest> requests;
    std::vector<unsigned> ids;std::function<void(const ScreenAction&)> callback;
    void action(const ScreenAction& e)override{actions.push_back(e);if(callback)callback(e);}
    PreparedScreenText prepareText(const TextRasterRequest& r)override{
        ++preparations;requests.push_back(r);auto b=std::make_shared<TextBitmap>();b->request=r;b->layout=textRasterLayout(r);
        b->alpha.resize(std::size_t(b->layout.width)*b->layout.height);
        return {"prepared:"+r.sourceId,std::move(b)};
    }
    void bgm(std::string_view s)override{songs.emplace_back(s);}
    void sound(std::string_view s)override{sounds.emplace_back(s);}
    void sfx(unsigned s)override{ids.push_back(s);}
};
struct Random:WorldRandom {unsigned calls=0;std::uint32_t random()override{++calls;return 7;}};
std::vector<ButtonKey> ids(Screens& ui,unsigned id){auto keys=ui.buttons();std::erase_if(keys,[&](const auto& k){return k.id!=id;});return keys;}
const Sprite* sprite(const Frame& f,std::string_view name){for(const auto& s:f.sprites)if(s.image==name)return &s;return nullptr;}
void fieldMessage(const char* root){
    Files files(root);Host host;PreparedTiles tiles("message-test");Screens ui(files,tiles,host);Random random;
    ScreensState state;state.screen=ScreenId::field;state.selectedSlot=0;ui.refresh(state,0);
    const auto draw=[&](){Frame frame;ui.append(frame,999);return frame;};
    const auto box=[&](const Frame& frame){return sprite(frame,"MessageBox.png");};
    const auto label=[&](const Frame& frame){return sprite(frame,"prepared:GuiGame.message");};
    const auto width=[](const Sprite* s){assert(s&&s->strip.size()==4);return s->strip[2].position.x-s->strip[0].position.x;};
    const auto height=[](const Sprite* s){assert(s&&s->strip.size()==4);return s->strip[0].position.y-s->strip[1].position.y;};
    assert(!box(draw()));state.message="Tap the Mach. to repair.";state.messageVisible=true;state.messageRevision=1;state.messageWorldTime=1.25;ui.refresh(state,100);
    auto frame=draw();assert(box(frame)&&label(frame));assert(box(frame)->center.x==405&&box(frame)->center.y==158&&width(box(frame))==512&&height(box(frame))==64&&box(frame)->strip[0].uv.y==0&&box(frame)->strip[1].uv.y==.5f);
    assert(label(frame)->size.width==512&&label(frame)->size.height==24&&label(frame)->center.x==405);
    const auto& request=host.requests.back();assert(request.fontName=="TrebuchetMS-Bold"&&request.fontSize==16&&request.alignment==TextAlign::center&&!request.retina);
    assert(std::count(host.ids.begin(),host.ids.end(),10)==1);const auto reads=files.reads,probes=files.probes,preparations=host.preparations;
    state.messageWorldTime=1.35;ui.updateHUD(state);ui.tick(1000,random);frame=draw();assert(box(frame)->center.y==166&&label(frame)->center.y==166&&random.calls==0);
    ui.refresh(state,1001);frame=draw();assert(box(frame)->center.y==166&&std::count(host.ids.begin(),host.ids.end(),10)==1); // unrelated HUD refresh keeps lifetime.
    state.messageRevision=2;state.messageWorldTime=1.4;ui.refresh(state,1002);assert(box(draw())->center.y==166&&std::count(host.ids.begin(),host.ids.end(),10)==2); // set does not reset Y.
    state.messageVisible=false;state.messageRevision=3;ui.refresh(state,1003);frame=draw();assert(box(frame)&&!label(frame)&&height(box(frame))==64);
    const double frozen=state.messageWorldTime;ui.tick(1004,random);frame=draw();assert(std::abs(height(box(frame))-64*.7725f)<.0001f); // world pause does not stop GUI folding.
    for(unsigned i=0;i<64;++i){state.messageWorldTime=frozen;ui.updateHUD(state);ui.tick(1005+float(i),random);}assert(!box(draw())&&random.calls==0);
    assert(files.reads==reads&&files.probes==probes); // Same message may prepare a wrapper, never reload PNG.
    state.messageVisible=true;state.messageAutoHide=true;state.messageRevision=4;state.messageWorldTime=20;ui.refresh(state,2000);
    state.messageWorldTime=30;ui.updateHUD(state);ui.tick(2001,random);assert(label(draw()));
    state.messageWorldTime=20+double(std::nextafter(10.f,11.f));ui.updateHUD(state);ui.tick(2002,random);assert(!label(draw())&&box(draw()));
    state.messageAutoHide=false;state.messageRevision=5;state.messageWorldTime=16777216.;ui.refresh(state,3000);
    state.messageWorldTime=16777216.25;ui.updateHUD(state);ui.tick(3001,random);frame=draw();assert(std::abs(box(frame)->center.y-161.9598f)<.001f&&label(frame)->center.y==162); // double current time survives.
    state.messageRevision=6;state.messageWorldTime=0;ui.refresh(state,4000);const auto beforeSentinel=draw();const float sentinelY=box(beforeSentinel)->center.y;
    state.messageWorldTime=1;ui.updateHUD(state);ui.tick(4001,random);frame=draw();assert(box(frame)->center.y==sentinelY&&label(frame)); // original msgTime0 sentinel.
    state.messageVisible=false;state.messageRevision=7;ui.refresh(state,4002);for(unsigned i=0;i<100;++i)ui.tick(4003+float(i),random);assert(box(draw()));
    assert(host.preparations>=preparations&&random.calls==0);
    std::cout<<"FIELD_MESSAGE original512x64/512x24/font/SFX10/floatDoubleClock/bounce/hide/pause/refresh/sentinel PASS\n";
}
}
int main(int argc,char** argv){
    ItemDescriptionValues desc;desc.type=1;desc.param1=30;desc.price=100;
    assert(Screens::itemDescription(desc,1)=="Repair Kit\n\n30 point Repair.\n\nPrice : 100cr");
    desc.param2=1;assert(Screens::itemDescription(desc,2)=="Repair Kit\n\n30% Repair.\n\nSellPrice : 50cr");
    desc.param2=2;assert(!Screens::itemDescription(desc,0));
    desc={};desc.type=11;desc.param1=101;desc.param2=13;desc.bodyHP=99;desc.bodyDefense=16;
    assert(Screens::itemDescription(desc,0)=="Body Parts\n\nHP : 101 (+2)\nDef : 13 (-3)\n\n");
    WeaponInfo weapon;weapon.attackPoint=9;weapon.damage=10;weapon.shotCount=2;weapon.shotDelay=.25f;weapon.reloadDelay=1.5f;
    desc={};desc.type=10;desc.weapon=&weapon;desc.leftAttack=4;desc.rightAttack=12;
    assert(Screens::itemDescription(desc,0)=="Machine-gun\n\nAtk 9 (L:+5 R:-3)\n10 / 2 / 0.25s / 1.5s\n\n");
    weapon.kind=WeaponKind::sniper;assert(!Screens::itemDescription(desc,0));
    assert(Screens::missionDescription("Original comment",{{2,"E_Tank_01",3,5},{3,"Repair",1,2}},false)=="Original comment\n\nMission Objects\n  - destroy E_Tank_01 x5\n  - Repair x2\n");
    assert(Screens::missionDescription("Original comment",{{2,"E_Tank_01",3,5}},true)=="Original comment\n\nMission Objects\n  - destroy E_Tank_01 (3/5)\n");
    assert(!Screens::missionDescription("Original",{{1,"unresolved",0,1}},false));
    if(argc<2)return 77;
    fieldMessage(argv[1]);
    Files files(argv[1]);Host host;PreparedTiles tiles("screens-test");Screens ui(files,tiles,host);Random random;
    ScreensState state;ui.refresh(state,0);assert(ui.buttons().empty()&&host.songs.back()=="STAGE1.wav");
    ui.input(PointerPhase::end,{100,100},1,1);assert(host.actions.back().titleTap&&!host.actions.back().titleReady);
    ui.input(PointerPhase::end,{100,100},2,2);assert(host.actions.back().titleReady);
    state.screen=ScreenId::slots;ui.refresh(state,2);assert(ui.buttons().size()==6&&ids(ui,1009).size()==3);
    const auto slot=ids(ui,1009).front();const auto before=ui.button(slot);
    assert(before->position.x==384&&before->position.y==760);
    host.callback=[&](const ScreenAction& a){if(a.button.type==ButtonEventType::push){state.selectedSlot=a.button.key.intData;ui.refresh(state,3);}};
    ui.input(PointerPhase::begin,before->position,7,3);assert(ui.button(slot)==before&&before->capturedPointer()==7);
    ui.input(PointerPhase::end,before->position,7,4);assert(!before->capturedPointer());host.callback={};
    unsigned baseButtons=0;state.screen=ScreenId::base;
    for(unsigned i=1;i<=6;++i){state.baseId="Base00"+std::to_string(i);ui.refresh(state,5);baseButtons+=unsigned(ui.buttons().size());Frame f;ui.append(f,5);assert(!f.sprites.empty());}
    assert(baseButtons==27);
    state={};state.screen=ScreenId::field;state.hpBarWidth=100;state.hpBarFilled=80;state.spBarWidth=120;state.spBarFilled=60;
    ui.refresh(state,6);assert(ui.buttons().size()==10);const auto stable=ui.button(ids(ui,1021).front());
    const auto reads=files.reads,probes=files.probes,prepares=host.preparations;
    for(unsigned i=0;i<100;++i){state.hpBarFilled=int(i);state.experienceRate=float(i)/100;state.enemyHpRate=.5f;state.credits=int(i);state.outOfRangeLeft=true;
        assert(ui.updateHUD(state));ui.tick(float(i+7),random);Frame f;ui.append(f,float(i+7));assert(sprite(f,"EnemyGauge.png")&&sprite(f,"OutofRange.png"));}
    assert(files.reads==reads&&files.probes==probes&&host.preparations==prepares&&ui.button(ids(ui,1021).front())==stable);
    state.weaponChoiceType=2;ItemView a;a.dataId="original-item-A";a.position=3;state.weaponChoices={a};ui.refresh(state,108);
    const auto keyA=ids(ui,1024).front();assert(keyA.intData==3&&keyA.dataId==a.dataId);auto first=ui.button(keyA);assert(first->position.x==405);
    ui.tick(109,random);assert(first->position.x==381&&first->position.y>90);
    state.weaponChoices[0].dataId="original-item-B";ui.refresh(state,110);assert(!ui.button(keyA));
    state.weaponChoiceType=0;state.inventoryVisible=true;ui.refresh(state,111);assert(ids(ui,6000).size()==1&&ids(ui,6100).size()==1&&ids(ui,6092).size()==1);
    state.inventoryVisible=false;state.airliftVisible=true;state.airlifts[0].dataId="Base001";state.airlifts[0].name="Original base";
    ui.refresh(state,112);assert(ids(ui,4001).empty()&&ids(ui,4002).size()==1);state.selectedAirlift=0;ui.refresh(state,113);assert(ids(ui,4001).size()==1);
    state={};state.screen=ScreenId::shop;state.shopItems={a};ui.refresh(state,114);assert(ids(ui,2104).size()==5&&ids(ui,2200).size()==1);
    state.screen=ScreenId::maintenance;state.equipmentStats={7,8,100,20,2,3,30,80,1};
    ui.refresh(state,115);assert(ids(ui,6000).size()==1&&ids(ui,6302).size()==1);
    Frame maintenanceFrame;ui.append(maintenanceFrame,115);const auto* statGlyphs=sprite(maintenanceFrame,"NumSet01b.png");
    assert(statGlyphs&&statGlyphs->atlasBatch&&statGlyphs->atlasBatch->quads.size()==19); // 7-8 / 100-20-5 / 30-80-10
    unsigned separators=0;for(const auto& q:statGlyphs->atlasBatch->quads)if(q.vertices[0].uv.x==13.f/16)++separators;
    assert(separators==5);
    state.screen=ScreenId::command;ItemView m;m.dataId="M001010";m.name="Original mission";state.availableMissions={m};state.selectedMission=0;ui.refresh(state,116);
    assert(ids(ui,5100).front().dataId==m.dataId&&ids(ui,5004).front().dataId==m.dataId);
    const auto coverage=Screens::coverage();assert(std::any_of(coverage.begin(),coverage.end(),[](const auto& c){return !c.implemented;}));
    std::cout<<"SCREENS original27BaseButtons, slot callback refresh/capture, raw item identities, original formats, HUD100ticks warmIO0/noTextBake PASS\n";
}

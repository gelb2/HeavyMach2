// Offline diagnostic. Reuses CPU fixtures and production camera/UI/navigation.
// No device input, file save, map mutation, or product behavior changes.
#include "../tests/PreparedTextStub.hpp"
#include "Core/Session/GameSession.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include "Core/Progression/Progression.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cassert>
using namespace hm2;
struct Files:ResourceAccess {
 std::filesystem::path root;explicit Files(const char* p):root(p){}
 Bytes read(std::string_view n)override{std::ifstream f(root/std::string(n),std::ios::binary);if(!f)throw std::runtime_error("Missing resource "+std::string(n));return {std::istreambuf_iterator<char>(f),{}};}
 bool exists(std::string_view n)const override{return std::filesystem::exists(root/std::string(n));}
};
struct Host:SessionHost,ui::ScreensHost {
 unsigned actions=0;void action(const ui::ScreenAction&)override{++actions;}
 ui::PreparedScreenText prepareText(const ui::TextRasterRequest& r)override{return test::preparedTextStub(r);}
 ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& r)override{return prepareText(r);}
 void bgm(std::string_view)override{}void sound(std::string_view)override{}void sfx(unsigned)override{}void sound(std::string_view,Vec2)override{}void soundID(unsigned,Vec2)override{}void alert(std::string_view)override{}void requestName(unsigned)override{}void requestDelete(unsigned)override{}
 bool writeSave(std::span<const std::uint8_t>,SaveReason)override{return true;}
 std::uint32_t absoluteTimeLow()override{return 1;}std::uint32_t unixTime()override{return 1794268800;}
};
struct GateHooks final:CombatSceneHooks {
 std::vector<int> links;
 void sound(std::string_view,Vec2)override{} void soundID(unsigned,Vec2)override{}
 void dropEnemyLoot(const MachineInfo&,Vec2,double,float,WorldRandom&)override{}
 void rewardEnemyKill(const MachineInfo&,Vec2,double,float,WorldRandom&)override{}
 int playerDamage(int,WorldRandom&)override{return 0;} int damageBonus()const override{return 0;}
 ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& r)override{return test::preparedTextStub(r);}
 void linkGate(const MapGate& g)override{links.push_back(g.number);}
};
bool inBox(Vec2 p,ClipRect b){return p.x>=b.x&&p.y>=b.y&&p.x<=b.x+b.width&&p.y<=b.y+b.height;}
int main(int argc,char** argv){
 const bool verifyPhone=argc>2&&std::string_view(argv[2])=="--verify-phone";unsigned verified=0,failed=0;
 if(argc<2)return 77;Files files(argv[1]);const auto maps=Maps::parse(Chunk::parse(files.read("MapInfo.mcb")));
 std::cout<<std::setprecision(9);
 if(argc==4){DarwinWorldRandom random(1);std::ifstream f(argv[3],std::ios::binary);Bytes bytes{std::istreambuf_iterator<char>(f),{}};auto save=SaveData::decode(bytes,random,false);for(unsigned n=0;n<3;++n){progression::SlotView slot(save.slot(n));std::cout<<"{\"saveProfile\":\""<<argv[2]<<"\",\"slot\":"<<n<<",\"map\":\""<<slot.string(0x2d,16)<<"\",\"gate\":"<<slot.u32(0x68)<<",\"level\":"<<slot.level()<<"}\n";}return 0;}
 const std::array<std::pair<const char*,DisplayConfiguration>,3> profiles{{{"installed-ipad-legacy",{}},{"installed-iphone",DisplayConfiguration::surface({912,420},{68,20,68,0})},{"current-ipad-fixture",DisplayConfiguration::surface({1024,1366},{0,20,0,24})}}};
 for(const auto& [label,config]:profiles){
  if(verifyPhone&&std::string_view(label)!="installed-iphone")continue;
  Host uiHost;DarwinWorldRandom random(1);GameSession game(files,uiHost,random);game.createSlot(0,"Audit","kr");game.setDisplay(config);game.enterField("Stage01_01",0);PreparedTiles tiles(label);ui::Screens ui(files,tiles,uiHost);ui.setDisplay(config);auto state=game.state();state.screen=ui::ScreenId::field;ui.refresh(state,0);
  DisplayLayout layout(config);const auto hudScale=layout.ui(DisplayRegion::bottom).scale;const auto hudOrigin=layout.ui(DisplayRegion::bottom).map({0,0});
  std::cout<<"{\"profile\":\""<<label<<"\",\"kind\":\"layout\",\"canvas\":["<<config.canvas.width<<","<<config.canvas.height<<"],\"hudScale\":"<<hudScale<<",\"hudOrigin\":["<<hudOrigin.x<<","<<hudOrigin.y<<"]}\n";
  for(const auto& [name,map]:maps.all())if(name.starts_with("Stage"))for(const auto& gate:map.gates){
   GateHooks hooks;CombatSetup setup;setup.map=name;setup.gate=gate.number;setup.offsetInsideGate=true;setup.openEventGate=true;CombatScene scene(files,hooks,setup);scene.setDisplay(config);scene.setPaused(true);for(unsigned tick=0;tick<600;++tick)scene.tick();
   const auto camera=scene.camera();const auto transform=scene.cameraPresentation().worldToCanvas;const auto center=transform.map(camera.toScreen(gate.position));const auto player=scene.player().position;const auto area=config.enabled?layout.clip(DisplayRegion::world).value():ClipRect{0,0,768,1024};
   float lastInsideY=std::nextafter(gate.position.y+30.f,gate.position.y);while(std::abs(lastInsideY-gate.position.y)>=30.f)lastInsideY=std::nextafter(lastInsideY,gate.position.y);assert(std::abs(lastInsideY-gate.position.y)<30.f);
   const auto entryTop=transform.map(camera.toScreen({gate.position.x,lastInsideY})).y;
   bool opaqueHUD=false;for(unsigned i=0;i<hud::count;++i)opaqueHUD|=inBox(center,hud::visualBounds({},i,layout));
   const float floor=transform.map({0,camera.inputMinimumY}).y;bool uiHit=ui.input(PointerPhase::begin,center,1234,0);ui.discardInputs();
   scene.setPaused(false);const bool accepted=scene.input(PointerPhase::begin,camera.toScreen(gate.position),1235);scene.input(PointerPhase::cancel,{},1235);scene.setPaused(true);
   std::cout<<"{\"profile\":\""<<label<<"\",\"kind\":\"gate\",\"map\":\""<<name<<"\",\"gate\":"<<gate.number<<",\"to\":\""<<gate.linkMap<<"\",\"eventOnly\":"<<gate.eventOnly<<",\"world\":["<<gate.position.x<<","<<gate.position.y<<"],\"player\":["<<player.x<<","<<player.y<<"],\"camera\":["<<camera.center.x<<","<<camera.center.y<<"],\"viewport\":["<<camera.viewport.width<<","<<camera.viewport.height<<"],\"gateCanvas\":["<<center.x<<","<<center.y<<"],\"entryTopCanvas\":"<<entryTop<<",\"inputFloorCanvas\":"<<floor<<",\"entryEntirelyBelowInput\":"<<(entryTop<floor)<<",\"HUDPlacementBoundsCover\":"<<opaqueHUD<<",\"HUDButtonConsumedCenter\":"<<uiHit<<",\"sceneAcceptedCenter\":"<<accepted<<",\"centerInsideWorldClip\":"<<inBox(center,area)<<",\"lowerOverscan\":"<<camera.lowerOverscan<<"}\n";
   bool anyLinked=false;
   if(verifyPhone||(name=="Stage01_01"&&gate.number==0)||(name=="Stage01_03"&&gate.number==1)||(name=="Stage01_04"&&gate.number==0)||(name=="Stage01_05"&&gate.number==0)||(name=="Stage03_03"&&gate.number==1)||(name=="Stage04_02"&&gate.number==0)||(name=="Stage04_07"&&gate.number==0))for(float dx:{-29.f,0.f,29.f})for(float dy:{0.f,15.f,29.f}){
    GateHooks walkHooks;CombatScene walk(files,walkHooks,setup);walk.setDisplay(config);walk.setPaused(true);for(unsigned t=0;t<600;++t)walk.tick();
    const auto target=Vec2{gate.position.x+dx,gate.position.y+dy};const auto logical=walk.camera().toScreen(target);const auto canvas=walk.cameraPresentation().worldToCanvas.map(logical);
    const bool uiConsumed=ui.input(PointerPhase::begin,canvas,9001,1);ui.discardInputs();walk.setPaused(false);
    const bool moved=!uiConsumed&&walk.input(PointerPhase::begin,logical,9002);if(moved)walk.input(PointerPhase::end,logical,9002);
    bool linked=false;unsigned ticks=0;for(;ticks<600&&!linked;++ticks){walk.tick();linked=std::find(walkHooks.links.begin(),walkHooks.links.end(),gate.number)!=walkHooks.links.end();}
    anyLinked|=linked;const auto final=walk.player().position;
    std::cout<<"{\"profile\":\""<<label<<"\",\"kind\":\"walk\",\"map\":\""<<name<<"\",\"gate\":"<<gate.number<<",\"targetOffsetX\":"<<dx<<",\"targetOffsetY\":"<<dy<<",\"HUDConsumed\":"<<uiConsumed<<",\"moveAccepted\":"<<moved<<",\"portalTriggered\":"<<linked<<",\"ticks\":"<<ticks<<",\"finalPlayer\":["<<final.x<<","<<final.y<<"]}\n";
   }
   // A portal behind an interactive HUD button remains reachable by the
   // original path gesture: begin on clear world, drag to the destination.
   // No link injection, map edits, dash or player position setters.
   if(verifyPhone&&!anyLinked)for(float dx:{-29.f,0.f,29.f})for(float dy:{0.f,15.f,29.f}){
    GateHooks pathHooks;CombatScene walk(files,pathHooks,setup);walk.setDisplay(config);walk.setPaused(true);for(unsigned t=0;t<600;++t)walk.tick();walk.setPaused(false);
    const Vec2 target{gate.position.x+dx,gate.position.y+dy},start{gate.position.x,gate.position.y+150};
    const auto startPoint=walk.camera().toScreen(start),endPoint=walk.camera().toScreen(target);const auto transform=walk.cameraPresentation().worldToCanvas;
    const unsigned actionsBefore=uiHost.actions;bool uiLeak=ui.input(PointerPhase::begin,transform.map(startPoint),9100,1);ui.discardInputs();
    bool moved=!uiLeak&&walk.input(PointerPhase::begin,startPoint,9100);
    if(moved){for(const auto phase:{PointerPhase::move,PointerPhase::end}){const bool consumed=ui.input(phase,transform.map(endPoint),9100,2);uiLeak|=consumed;walk.input(consumed?PointerPhase::cancel:phase,endPoint,9100);}}
    uiLeak|=uiHost.actions!=actionsBefore;bool linked=false;unsigned ticks=0;
    for(;ticks<600&&!linked;++ticks){walk.tick();linked=std::find(pathHooks.links.begin(),pathHooks.links.end(),gate.number)!=pathHooks.links.end();}
    anyLinked|=linked&&moved&&!uiLeak;
    std::cout<<"{\"profile\":\""<<label<<"\",\"kind\":\"path\",\"map\":\""<<name<<"\",\"gate\":"<<gate.number<<",\"targetOffsetX\":"<<dx<<",\"targetOffsetY\":"<<dy<<",\"portalTriggered\":"<<linked<<",\"UILeak\":"<<uiLeak<<",\"ticks\":"<<ticks<<"}\n";
   }
   if(verifyPhone){++verified;if(!anyLinked)++failed;}
  }
 }
 if(verifyPhone){std::cerr<<"PHONE_PORTALS verified="<<verified<<" failed="<<failed<<"\n";return verified!=82||failed?1:0;}
}

#include "PreparedTextStub.hpp"
#include "Core/Session/GameSession.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace hm2;
namespace {
bool near(float a,float b){return std::abs(a-b)<.002f;}
void fullScreenWorld(){
 for(auto config:{DisplayConfiguration::surface({667,375}),DisplayConfiguration::surface({912,420},{68,20,68,0}),DisplayConfiguration::surface({912,420},{0,20,68,0})}){
  DisplayLayout layout(config);
  for(float zoom:{1.f,1.3f,1.6f,2.f}){
   const auto t=layout.worldAtScale(zoom);const auto v=layout.worldViewportAtScale(zoom);
   const auto low=t.map({0,0}),high=t.map({v.width,v.height});
   if(!near(low.x,0)||!near(low.y,0)||!near(high.x,config.canvas.width)||!near(high.y,config.canvas.height))std::cerr<<"WORLD_LETTERBOX zoom="<<zoom<<" low="<<low.x<<","<<low.y<<" high="<<high.x<<","<<high.y<<std::endl;
   assert(near(low.x,0)&&near(low.y,0)&&near(high.x,config.canvas.width)&&near(high.y,config.canvas.height));
  }
  assert(layout.ui(DisplayRegion::bottom).scale>.65f);
 }
}
struct Files:ResourceAccess{std::filesystem::path root;unsigned reads=0;mutable unsigned probes=0;explicit Files(const char* p):root(p){}Bytes read(std::string_view n)override{++reads;std::ifstream f(root/std::string(n),std::ios::binary);if(!f)throw std::runtime_error("Missing display fixture "+std::string(n));return {std::istreambuf_iterator<char>(f),{}};}bool exists(std::string_view n)const override{++probes;return std::filesystem::exists(root/std::string(n));}};
struct Host:SessionHost,ui::ScreensHost{
 std::vector<ui::ScreenAction> events;unsigned writes=0;GameSession* game=nullptr;
 void action(const ui::ScreenAction& a)override{events.push_back(a);}
 ui::PreparedScreenText prepareText(const ui::TextRasterRequest& r)override{return test::preparedTextStub(r);}
 ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& r)override{return prepareText(r);}
 void bgm(std::string_view)override{}void sound(std::string_view)override{}void sfx(unsigned)override{}void sound(std::string_view,Vec2)override{}void soundID(unsigned,Vec2)override{}void alert(std::string_view)override{}void requestName(unsigned)override{}void requestDelete(unsigned)override{}bool writeSave(std::span<const std::uint8_t>,SaveReason)override{++writes;return true;}std::uint32_t absoluteTimeLow()override{return 1;}std::uint32_t unixTime()override{return 1794268800;}
};
void transforms(){
 const DisplayLayout legacy;assert(legacy.ui(DisplayRegion::bottom).identity());assert(legacy.worldAtScale(1).identity());
 for(auto config:{DisplayConfiguration::surface({667,375}),DisplayConfiguration::surface({852,393},{59,21,59,0}),DisplayConfiguration::surface({912,420},{68,20,68,0}),DisplayConfiguration::surface({1024,1366},{0,20,0,24})}){
  DisplayLayout layout(config);const auto area=config.content();for(unsigned i=0;i<=unsigned(DisplayRegion::banner);++i){const auto t=layout.ui(DisplayRegion(i));for(Vec2 p: {Vec2{0,0},Vec2{384,512},Vec2{768,1024}}){auto q=t.inverse(t.map(p));assert(near(q.x,p.x)&&near(q.y,p.y));}}
  assert(near(layout.worldViewportAtScale(1).width*1.f,config.compact?config.canvas.width:area.width));
  Sprite source;source.center={40,80};source.size={20,30};source.clip=ClipRect{20,30,40,50};source.strip.push_back({{15,17},{.3f,.7f},{1,.5f,.25f,1}});
  auto batch=std::make_shared<AtlasBatch>();batch->quads.emplace_back();batch->quads[0].vertices[0].position={25,27};batch->quads[0].vertices[0].uv={.2f,.4f};source.atlasBatch=batch;
  auto mapped=source;const auto t=layout.worldAtScale(1.3f);transformSprite(mapped,t,area);assert(near(mapped.center.x,t.map(source.center).x));assert(near(mapped.strip[0].position.y,t.map({15,17}).y));assert(mapped.strip[0].uv.x==.3f&&mapped.atlasBatch->quads[0].vertices[0].uv.y==.4f);assert(source.atlasBatch->quads[0].vertices[0].position.x==25);
 }
 const auto a=DisplayLayout(DisplayConfiguration::surface({768,1024})),b=DisplayLayout(DisplayConfiguration::surface({1024,1366}));assert(a.worldAtScale(1).scale==b.worldAtScale(1).scale&&b.worldViewportAtScale(1).width>a.worldViewportAtScale(1).width);
 bool refused=false;try{(void)DisplayConfiguration::surface({100,100},{50,0,50,0});}catch(const std::invalid_argument&){refused=true;}assert(refused);
}
void screens(Files& files,DisplayConfiguration config){
 Host host;PreparedTiles tiles("universal-test");ui::Screens screens(files,tiles,host);screens.setDisplay(config);const auto area=config.content();
 ui::ScreensState state;state.locale="kr";state.screen=ui::ScreenId::field;state.hpBarWidth=100;state.hpBarFilled=75;state.spBarWidth=100;state.spBarFilled=50;state.enemyName="E-10";state.enemyNameRevision=1;state.enemyHpRate=.7f;
 for(unsigned id=1;id<=7;++id){state.screen=ui::ScreenId(id);screens.refresh(state,12);Frame frame;screens.append(frame,12);assert(frame.width==config.canvas.width&&frame.height==config.canvas.height&&!frame.sprites.empty());for(const auto& s:frame.sprites){assert(s.clip&&s.clip->x>=0&&s.clip->y>=0&&s.clip->x+s.clip->width<=config.canvas.width+.01f&&s.clip->y+s.clip->height<=config.canvas.height+.01f);
   // These full-width atlas tiles slide through the left boundary of the
   // original768-wide screen. Centering HUD must move that boundary too.
   if(s.image=="GameUI_GaugeBack.png"){const auto hud=DisplayLayout(config).ui(DisplayRegion::bottom);const float left=std::max(area.x,hud.map({0,0}).x),right=std::min(area.x+area.width,hud.map({768,0}).x);if(s.clip->x<left-.01f)std::cerr<<"HUD_GAUGE_ESCAPE clipLeft="<<s.clip->x<<" hudLeft="<<left<<std::endl;assert(s.clip->x>=left-.01f&&s.clip->x+s.clip->width<=right+.01f);}
 }}
 state.screen=ui::ScreenId::field;screens.refresh(state,12);const auto beforeReads=files.reads,beforeProbes=files.probes;for(unsigned i=0;i<100;++i){state.hpBarFilled=10+int(i%80);state.credits=int(i);screens.updateHUD(state);Frame frame;screens.append(frame,12);assert(frame.width==config.canvas.width);}assert(files.reads==beforeReads&&files.probes==beforeProbes);
 for(unsigned id:{1022u,1023u,1025u}){for(const auto& key:screens.buttons())if(key.id==id){const auto p=screens.buttonScreenPosition(key);host.events.clear();screens.input(PointerPhase::begin,p,99,12);screens.input(PointerPhase::end,p,99,12);assert(!host.events.empty()&&host.events.front().button.key.id==id&&host.events.front().button.canvasPosition);break;}}
 state.screen=ui::ScreenId::shop;state.inventoryVisible=true;state.inventory.resize(25);state.shopItems.resize(24);for(unsigned i=0;i<24;++i){state.shopItems[i].dataId=std::to_string(i);state.shopItems[i].icon="Icon10011.png";}screens.refresh(state,12);unsigned inv=0,stock=0;for(auto key:screens.buttons()){inv+=key.id>=6000&&key.id<6025;stock+=key.id>=2200&&key.id<2224;}assert(inv==25&&stock==24);
 for(float pan:{0.f,screens.horizontalPanLimit()}){screens.setHorizontalPan(pan);Frame frame;screens.append(frame,12);assert(!frame.sprites.empty());}
 screens.setHorizontalPan(screens.horizontalPanLimit());for(const auto& key:screens.buttons())if(key.id==6000){const auto p=screens.buttonScreenPosition(key);host.events.clear();screens.input(PointerPhase::begin,p,101,12);screens.input(PointerPhase::move,{p.x+30,p.y-30},101,12);screens.input(PointerPhase::end,{p.x+30,p.y-30},101,12);assert(!host.events.empty());for(const auto& a:host.events)if(a.button.type==ui::ButtonEventType::move){assert(a.dropCoordinates);auto original=DisplayLayout(config).ui(DisplayRegion::shopInventory,screens.horizontalPanLimit()).inverse({p.x+30,p.y-30});assert(near(original.x,a.dropCoordinates->inventory.x));}break;}
 state=ui::ScreensState{};state.locale="kr";state.screen=ui::ScreenId::command;state.availableMissions.resize(5);state.acceptedMissions.resize(5);for(unsigned i=0;i<5;++i){state.availableMissions[i].dataId="Mnew"+std::to_string(i);state.availableMissions[i].name="Mission";state.acceptedMissions[i].dataId="Mown"+std::to_string(i);state.acceptedMissions[i].name="Mission";}state.selectedMission=0;state.missionTitle="Mission";state.missionBody="Details";screens.refresh(state,12);DarwinWorldRandom rng(1);for(unsigned i=0;i<120;++i)screens.tick(12+float(i)/60,rng);
 for(const auto& key:screens.buttons())if(key.id>=5100&&key.id<5205){screens.revealButton(key);const auto point=screens.buttonScreenPosition(key);const auto clip=DisplayLayout(config).clip(DisplayRegion::list).value_or(area);assert(point.y>=clip.y&&point.y<=clip.y+clip.height);host.events.clear();screens.input(PointerPhase::begin,point,202,12);screens.input(PointerPhase::end,point,202,12);assert(!host.events.empty()&&host.events.front().button.key==key);}
 state=ui::ScreensState{};state.locale="kr";state.screen=ui::ScreenId::field;state.weaponChoiceType=2;for(unsigned i=0;i<30;++i){ui::ItemView item;item.dataId=std::to_string(i);item.position=int(i);item.name="Weapon";state.weaponChoices.push_back(item);}screens.refresh(state,12);for(unsigned i=0;i<120;++i)screens.tick(12+float(i)/60,rng);for(const auto& key:screens.buttons())if(key.id==1024){screens.revealButton(key);const auto point=screens.buttonScreenPosition(key);const auto clip=DisplayLayout(config).clip(DisplayRegion::chooser).value_or(area);assert(point.y>=clip.y&&point.y<=clip.y+clip.height);host.events.clear();screens.input(PointerPhase::begin,point,203,12);screens.input(PointerPhase::end,point,203,12);assert(!host.events.empty()&&host.events.front().button.key==key);}
 state=ui::ScreensState{};state.locale="kr";
 state.screen=ui::ScreenId::field;state.inventoryVisible=false;state.airliftVisible=true;for(unsigned i=0;i<6;++i){state.airlifts[i].dataId="Base00"+std::to_string(i+1);state.airlifts[i].name="Base";}screens.refresh(state,12);unsigned air=0;for(auto key:screens.buttons())air+=key.id>=4002&&key.id<4008;assert(air==6);
}
void help(Files& files,DisplayConfiguration config){unsigned closed=0;ui::Help help(files,"kr",-1,[&]{++closed;});help.setDisplay(config);const auto a=config.content();const Vec2 p{a.x+a.width*.5f,a.y+a.height*.5f};Frame frame;help.append(frame);assert(!frame.sprites.empty()&&frame.sprites[0].clip);const auto y=frame.sprites[0].strip[0].position.y;
 help.input(PointerPhase::begin,p,1);help.input(PointerPhase::move,{p.x,p.y+100},1);help.input(PointerPhase::end,{p.x,p.y+100},1);assert(!closed&&help.state().guidanceVisible);Frame scrolled;help.append(scrolled);if(config.compact)assert(scrolled.sprites[0].strip[0].position.y>y);
 help.input(PointerPhase::begin,p,22);help.discardInputs();help.input(PointerPhase::begin,p,2);help.input(PointerPhase::end,p,2);assert(!help.state().guidanceVisible&&!closed);help.input(PointerPhase::begin,p,3);help.input(PointerPhase::end,p,3);assert(closed==1);
}
void entryCamera(Files& files){
 Host host;DarwinWorldRandom random(1);SaveData saved;saved.initializeSlot(0,"Camera","kr",1794268800);saved.slot(0)[0x1b5c]=5;
 GameSession game(files,host,random,std::move(saved));assert(game.selectSlot(0));assert(game.enterField("Stage01_01",0));const auto config=DisplayConfiguration::surface({912,420},{68,20,68,0});game.setDisplay(config);
 const auto area=config.content();const DisplayLayout layout(config);
 for(unsigned i=0;i<240;++i){game.tick();auto* scene=game.battle();assert(scene);const auto p=game.worldCanvasPoint(scene->camera().toScreen(scene->player().position));const float bottom=area.y+138*layout.ui(DisplayRegion::bottom).scale;
   if(p.y<bottom||p.y>area.y+area.height||p.x<area.x||p.x>area.x+area.width)std::cerr<<"PLAYER_CAMERA_ESCAPE frame="<<i<<" canvas="<<p.x<<","<<p.y<<" floor="<<bottom<<" world="<<scene->player().position.x<<","<<scene->player().position.y<<" camera="<<scene->camera().center.x<<","<<scene->camera().center.y<<" zoom="<<scene->cameraPresentation().worldScale<<std::endl;
   assert(p.y>=bottom&&p.y<=area.y+area.height&&p.x>=area.x&&p.x<=area.x+area.width);
   const auto frame=game.drawWorld();assert(frame.width==config.canvas.width&&frame.height==config.canvas.height);
   const auto& terrain=frame.sprites.front();assert(terrain.clip&&terrain.clip->x==0&&terrain.clip->y==0&&terrain.clip->width==config.canvas.width&&terrain.clip->height==config.canvas.height);
   assert(terrain.strip.size()==4&&terrain.strip[0].position.x<=0&&terrain.strip[0].position.y<=0&&terrain.strip[3].position.x>=config.canvas.width&&terrain.strip[3].position.y>=config.canvas.height);
 }
}
void phoneMenus(Files& files){
 const auto config=DisplayConfiguration::surface({912,420},{68,20,68,0});const auto area=config.content();
 Host host;PreparedTiles tiles("fullscreen-menu-test");ui::Screens screens(files,tiles,host);screens.setDisplay(config);DarwinWorldRandom random(1);
 for(unsigned base=1;base<=6;++base){
  ui::ScreensState state;state.locale="kr";state.screen=ui::ScreenId::base;state.baseId="Base00"+std::to_string(base);state.selectedBaseButton=1;state.baseDescription="설명 전체와 나가기 버튼은 겹치지 않아야 한다.";
  screens.refresh(state,12);for(unsigned i=0;i<120;++i)screens.tick(12+float(i)/60,random);Frame frame;screens.append(frame,15);
  bool background=false,description=false;float descLeft=0,descBottom=0,descTop=0;
  for(const auto& sprite:frame.sprites){
   if(sprite.image==state.baseId+".png"){background=true;assert(sprite.clip&&sprite.clip->x==0&&sprite.clip->width==config.canvas.width);}
   if(sprite.image=="BaseTextBG.png"){description=true;descLeft=sprite.center.x-sprite.size.width*.5f;descBottom=sprite.center.y-sprite.size.height*.5f;descTop=sprite.center.y+sprite.size.height*.5f;assert(descLeft>=area.x&&descBottom>=area.y&&descTop<=area.y+area.height);}
  }
  assert(background&&description);
  for(const auto& key:screens.buttons()){
   const auto b=screens.button(key);const auto point=screens.buttonScreenPosition(key);const auto bounds=b->bounds();
   assert(point.x+bounds.left>=area.x&&point.x+bounds.right<=descLeft&&point.y+bounds.bottom>=area.y&&point.y+bounds.top<=area.y+area.height);
   host.events.clear();screens.input(PointerPhase::begin,point,701,15);screens.input(PointerPhase::end,point,701,15);assert(!host.events.empty()&&host.events.front().button.key==key);
  }
  const auto key=screens.buttons().front();screens.setDisplay(DisplayConfiguration::surface({667,375}));const auto moved=screens.buttonScreenPosition(key);assert(moved.x<220&&moved.y<375);screens.setDisplay(DisplayConfiguration::surface({1024,1366}));const auto original=screens.buttonScreenPosition(key);assert(original.x!=moved.x||original.y!=moved.y);screens.setDisplay(config);
 }
 ui::ScreensState state;state.locale="kr";state.screen=ui::ScreenId::command;
 for(unsigned i=0;i<3;++i){ui::ItemView item;item.dataId="accepted"+std::to_string(i);item.name="진행 중인 임무";state.acceptedMissions.push_back(item);}
 screens.refresh(state,12);const auto clip=DisplayLayout(config).clip(DisplayRegion::list).value();
 for(const auto& key:screens.buttons())if(key.id>=5200&&key.id<=5202){const auto p=screens.buttonScreenPosition(key);assert(p.y>=clip.y+24&&p.y<=clip.y+clip.height-24);}
}
void session(Files& files,DisplayConfiguration config){Host host;DarwinWorldRandom random(1);GameSession game(files,host,random);assert(game.createSlot(0,"Layout","kr"));assert(game.enterField("Stage01_01",0));game.setDisplay(config);auto* scene=game.battle();assert(scene);const auto before=Bytes(game.workingSlot().begin(),game.workingSlot().end());const auto writes=host.writes;const auto old=scene->player().position;auto logical=scene->camera().toScreen({old.x,old.y+100});const auto point=game.worldCanvasPoint(logical);assert(game.inputWorld(PointerPhase::begin,point,77));game.inputWorld(PointerPhase::end,point,77);assert(near(scene->player().destination.x,old.x)&&near(scene->player().destination.y,old.y+100));assert(game.inputWorld(PointerPhase::begin,point,78));game.inputWorld(PointerPhase::end,{point.x,0},78);assert(scene->dashState().movePointer==0);assert(game.inputWorld(PointerPhase::begin,point,79));game.inputWorld(PointerPhase::cancel,{0,0},79);assert(scene->dashState().movePointer==0);
 if(config.compact&&config.insets.left>0){const Vec2 edge{config.insets.left*.5f,config.canvas.height*.75f};assert(game.inputWorld(PointerPhase::begin,edge,880));game.inputWorld(PointerPhase::end,edge,880);}
 const auto initialZoom=scene->cameraPresentation().worldScale;
 for(unsigned i=0;i<100;++i){game.tick();auto frame=game.drawWorld();assert(frame.width==config.canvas.width&&frame.height==config.canvas.height);game.appendOverlays(frame);}assert(host.writes==writes);const auto z=scene->cameraPresentation().worldScale;const auto minimum=std::min(1.f,std::max(config.canvas.width,config.canvas.height)/1022.f);assert(config.focusZoom?(z>=minimum&&z<=1.f):z==1);
 if(config.focusZoom&&config.canvas.width==912){std::cout<<"PHONE_FOCUS_ZOOM initial="<<initialZoom<<" after="<<z<<std::endl;assert(initialZoom==1&&z<=initialZoom);}
 const auto pos=scene->player().position;const auto hp=scene->playerHP();game.setDisplay(DisplayConfiguration::surface({1024,1366}));assert(scene->player().position.x==pos.x&&scene->playerHP()==hp&&host.writes==writes);assert(game.workingSlot().size()==before.size());
}
}
int main(int argc,char** argv){if(argc<2)return 77;fullScreenWorld();transforms();Files files(argv[1]);entryCamera(files);phoneMenus(files);for(auto config:{DisplayConfiguration::surface({667,375}),DisplayConfiguration::surface({852,393},{59,21,59,0}),DisplayConfiguration::surface({912,420},{68,20,68,0}),DisplayConfiguration::surface({1024,1366},{0,20,0,24})}){screens(files,config);help(files,config);session(files,config);}std::cout<<"HM2_DISPLAY PASS 4 canvases / full-screen world / terrain edge coverage / HUD size / six base menus / description-vs-buttons / first mission rows / safe-area / inverse-hit / HD25+24+6 / drag / warm100 / help-scroll / common-world / no-display-save\n";}

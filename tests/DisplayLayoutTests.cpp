#include "PreparedTextStub.hpp"
#include "Core/Session/GameSession.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include "Core/Presentation/MenuArtwork.hpp"
#include "Core/Presentation/MenuGridLayout.hpp"
#include "Core/Progression/Progression.hpp"
#include "Core/Data/ResourceName.hpp"
#include "Assets/Image.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
using namespace hm2;
namespace {
bool near(float a,float b){return std::abs(a-b)<.002f;}
ClipRect quadBounds(const Sprite& s){if(s.strip.empty())return {s.center.x-s.size.width*.5f,s.center.y-s.size.height*.5f,s.size.width,s.size.height};assert(s.strip.size()==4);float l=s.strip[0].position.x,r=l,b=s.strip[0].position.y,t=b;for(const auto& v:s.strip){l=std::min(l,v.position.x);r=std::max(r,v.position.x);b=std::min(b,v.position.y);t=std::max(t,v.position.y);}return {l,b,r-l,t-b};}
void uniformCover(const Sprite& s,Size canvas,float ratio=1.5f,std::optional<ClipRect> visible={}){
 assert(s.strip.size()==4);const auto o=s.strip[0].position,a=s.strip[2].position,b=s.strip[1].position;const Vec2 x{a.x-o.x,a.y-o.y},y{b.x-o.x,b.y-o.y};const float xx=x.x*x.x+x.y*x.y,yy=y.x*y.x+y.y*y.y;
 assert(near(std::sqrt(xx)/std::sqrt(yy),ratio));
 const auto r=visible.value_or(ClipRect{0,0,canvas.width,canvas.height});
 for(const auto p:{Vec2{r.x,r.y},Vec2{r.x+r.width,r.y},Vec2{r.x,r.y+r.height},Vec2{r.x+r.width,r.y+r.height}}){const Vec2 d{p.x-o.x,p.y-o.y};const float u=(d.x*x.x+d.y*x.y)/xx,v=(d.x*y.x+d.y*y.y)/yy;assert(u>=-.0001f&&u<=1.0001f&&v>=-.0001f&&v<=1.0001f);}
}
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
 std::vector<ui::ScreenAction> events;unsigned writes=0;GameSession* game=nullptr;ui::Screens* screens=nullptr;
 void action(const ui::ScreenAction& a)override{events.push_back(a);if(game){game->action(a);if(screens)screens->refresh(game->state(),game->renderTime());}}
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
   if(s.image=="GameUI_GaugeBack.png"){const auto hud=DisplayLayout(config).ui(DisplayRegion::bottom);const auto bounds=config.compact?ClipRect{0,0,config.canvas.width,config.canvas.height}:area;const float left=std::max(bounds.x,hud.map({0,0}).x),right=std::min(bounds.x+bounds.width,hud.map({768,0}).x);if(s.clip->x<left-.01f)std::cerr<<"HUD_GAUGE_ESCAPE clipLeft="<<s.clip->x<<" hudLeft="<<left<<std::endl;assert(s.clip->x>=left-.01f&&s.clip->x+s.clip->width<=right+.01f);}
 }}
 state.screen=ui::ScreenId::field;screens.refresh(state,12);const auto beforeReads=files.reads,beforeProbes=files.probes;for(unsigned i=0;i<100;++i){state.hpBarFilled=10+int(i%80);state.credits=int(i);screens.updateHUD(state);Frame frame;screens.append(frame,12);assert(frame.width==config.canvas.width);}assert(files.reads==beforeReads&&files.probes==beforeProbes);
 for(unsigned id:{1022u,1023u,1025u}){for(const auto& key:screens.buttons())if(key.id==id){const auto p=screens.buttonScreenPosition(key);host.events.clear();screens.input(PointerPhase::begin,p,99,12);screens.input(PointerPhase::end,p,99,12);assert(!host.events.empty()&&host.events.front().button.key.id==id&&host.events.front().button.canvasPosition);break;}}
 state.screen=ui::ScreenId::shop;state.inventoryVisible=true;state.inventory.resize(25);state.shopItems.resize(24);for(unsigned i=0;i<24;++i){state.shopItems[i].dataId=std::to_string(i);state.shopItems[i].icon="Icon10011.png";}screens.refresh(state,12);unsigned inv=0,stock=0;for(auto key:screens.buttons()){inv+=key.id>=6000&&key.id<6025;stock+=key.id>=2200&&key.id<2224;}assert(inv==(menu::phone(config)?menu::visibleRows(config)*5:25)&&stock==(menu::phone(config)?menu::visibleRows(config)*4:24));
 for(float pan:{0.f,screens.horizontalPanLimit()}){screens.setHorizontalPan(pan);Frame frame;screens.append(frame,12);assert(!frame.sprites.empty());}
 screens.setHorizontalPan(screens.horizontalPanLimit());for(const auto& key:screens.buttons())if(key.id==6000){const auto p=screens.buttonScreenPosition(key);host.events.clear();screens.input(PointerPhase::begin,p,101,12);screens.input(PointerPhase::move,{p.x+30,p.y-30},101,12);screens.input(PointerPhase::end,{p.x+30,p.y-30},101,12);assert(!host.events.empty());for(const auto& a:host.events)if(a.button.type==ui::ButtonEventType::move){assert(a.dropCoordinates);auto original=config.compact?Vec2{408+30/DisplayLayout(config).ui(DisplayRegion::phoneMenu).scale,432-30/DisplayLayout(config).ui(DisplayRegion::phoneMenu).scale}:DisplayLayout(config).ui(DisplayRegion::tabletMenu).inverse({p.x+30,p.y-30});assert(near(original.x,a.dropCoordinates->inventory.x));}break;}
 state=ui::ScreensState{};state.locale="kr";state.screen=ui::ScreenId::command;state.availableMissions.resize(5);state.acceptedMissions.resize(5);for(unsigned i=0;i<5;++i){state.availableMissions[i].dataId="Mnew"+std::to_string(i);state.availableMissions[i].name="Mission";state.acceptedMissions[i].dataId="Mown"+std::to_string(i);state.acceptedMissions[i].name="Mission";}state.selectedMission=0;state.missionTitle="Mission";state.missionBody="Details";screens.refresh(state,12);DarwinWorldRandom rng(1);for(unsigned i=0;i<120;++i)screens.tick(12+float(i)/60,rng);
 for(const auto& key:screens.buttons())if(key.id>=5100&&key.id<5205){screens.revealButton(key);const auto point=screens.buttonScreenPosition(key);const auto clip=menu::phone(config)?ClipRect{0,0,config.canvas.width,config.canvas.height}:DisplayLayout(config).clip(DisplayRegion::list).value_or(area);assert(point.y>=clip.y&&point.y<=clip.y+clip.height);host.events.clear();screens.input(PointerPhase::begin,point,202,12);screens.input(PointerPhase::end,point,202,12);assert(!host.events.empty()&&host.events.front().button.key==key);}
 state=ui::ScreensState{};state.locale="kr";state.screen=ui::ScreenId::field;state.weaponChoiceType=2;for(unsigned i=0;i<30;++i){ui::ItemView item;item.dataId=std::to_string(i);item.position=int(i);item.name="Weapon";state.weaponChoices.push_back(item);}screens.refresh(state,12);for(unsigned i=0;i<120;++i)screens.tick(12+float(i)/60,rng);for(const auto& key:screens.buttons())if(key.id==1024){screens.revealButton(key);const auto point=screens.buttonScreenPosition(key);const auto clip=DisplayLayout(config).clip(DisplayRegion::chooser).value_or(area);assert(point.y>=clip.y&&point.y<=clip.y+clip.height);host.events.clear();screens.input(PointerPhase::begin,point,203,12);screens.input(PointerPhase::end,point,203,12);assert(!host.events.empty()&&host.events.front().button.key==key);}
 state=ui::ScreensState{};state.locale="kr";
 state.screen=ui::ScreenId::field;state.inventoryVisible=false;state.airliftVisible=true;for(unsigned i=0;i<6;++i){state.airlifts[i].dataId="Base00"+std::to_string(i+1);state.airlifts[i].name="Base";}screens.refresh(state,12);unsigned air=0;for(auto key:screens.buttons())air+=key.id>=4002&&key.id<4008;assert(air==6);
}
// Regression: iPhone surface adaptation must not leave the original HD menu
// at 1:1 in the middle of a larger tablet. Test real composed background quads
// and the same inverse hit mapping used by the dispatcher, not file presence.
void tabletMenuCoverage(Files& files){
 for(auto config:{DisplayConfiguration::surface({768,1024}),DisplayConfiguration::surface({1024,1366},{0,20,0,24}),DisplayConfiguration::surface({834,1194},{0,20,0,24})}){
  config.device=DisplayDevice::tablet;Host host;PreparedTiles tiles("tablet-menu-regression");ui::Screens ui(files,tiles,host);ui.setDisplay(config);DarwinWorldRandom random(1);
  for(unsigned base=1;base<=6;++base)for(auto id:{ui::ScreenId::base,ui::ScreenId::shop,ui::ScreenId::command}){
   ui::ScreensState state;state.screen=id;state.baseId="Base00"+std::to_string(base);state.locale="kr";state.inventoryVisible=id==ui::ScreenId::shop;
   ui.refresh(state,0);for(unsigned tick=0;tick<180;++tick)ui.tick(float(tick)/60,random);Frame frame;ui.append(frame,3);
   const auto background=id==ui::ScreenId::base?state.baseId+".png":id==ui::ScreenId::shop?std::string("ShopBG.png"):std::string("CmdCenterBG.png");
   bool found=false;for(const auto& sprite:frame.sprites)if(sprite.image==background){found=true;const auto b=quadBounds(sprite);
    if(b.x>.01f||b.y>.01f||b.x+b.width<config.canvas.width-.01f||b.y+b.height<config.canvas.height-.01f)std::cerr<<"TABLET_MENU_LETTERBOX "<<background<<" bounds="<<b.x<<","<<b.y<<","<<b.width<<","<<b.height<<" canvas="<<config.canvas.width<<","<<config.canvas.height<<std::endl;
    uniformCover(sprite,config.canvas,.75f);assert(sprite.clip&&near(sprite.clip->x,0)&&near(sprite.clip->y,0)&&near(sprite.clip->width,config.canvas.width)&&near(sprite.clip->height,config.canvas.height));
   }assert(found);
   for(auto key:ui.buttons())if(key.id==(id==ui::ScreenId::base?2004u:2100u)){auto p=ui.buttonScreenPosition(key);assert(p.x>=0&&p.x<=config.canvas.width&&p.y>=0&&p.y<=config.canvas.height);host.events.clear();ui.input(PointerPhase::begin,p,443,3);ui.input(PointerPhase::end,p,443,3.1f);assert(!host.events.empty()&&host.events.front().button.key==key);break;}
  }
 }
 std::cout<<"TABLET_MENU_COVERAGE 54 contexts / uniform original HD / full canvas / inverse hit PASS\n";
}
void help(Files& files,DisplayConfiguration config){unsigned closed=0;ui::Help help(files,"kr",-1,[&]{++closed;});help.setDisplay(config);const auto a=config.content();const Vec2 p{a.x+a.width*.5f,a.y+a.height*.5f};Frame frame;help.append(frame);assert(!frame.sprites.empty()&&frame.sprites[0].clip);const auto y=frame.sprites[0].strip[0].position.y;
 help.input(PointerPhase::begin,p,1);help.input(PointerPhase::move,{p.x,p.y+100},1);help.input(PointerPhase::end,{p.x,p.y+100},1);assert(!closed&&help.state().guidanceVisible);Frame scrolled;help.append(scrolled);if(config.compact)assert(scrolled.sprites[0].strip[0].position.y>y);
 help.input(PointerPhase::begin,p,22);help.discardInputs();help.input(PointerPhase::begin,p,2);help.input(PointerPhase::end,p,2);assert(!help.state().guidanceVisible&&!closed);help.input(PointerPhase::begin,p,3);help.input(PointerPhase::end,p,3);assert(closed==1);
}
void entryCamera(Files& files){
 Host host;DarwinWorldRandom random(1);SaveData saved;saved.initializeSlot(0,"Camera","kr",1794268800);saved.slot(0)[0x1b5c]=5;
 GameSession game(files,host,random,std::move(saved));assert(game.selectSlot(0));assert(game.enterField("Stage01_01",0));const auto config=DisplayConfiguration::surface({912,420},{68,20,68,0});game.setDisplay(config);
 const DisplayLayout layout(config);
 for(unsigned i=0;i<240;++i){game.tick();auto* scene=game.battle();if(i==60){const auto enemies=scene->enemies();if(!enemies.empty())scene->selectEnemy(enemies.front().id);}assert(scene);const auto p=game.worldCanvasPoint(scene->camera().toScreen(scene->player().position));
   if(!(p.y>=0&&p.y<=config.canvas.height&&p.x>=0&&p.x<=config.canvas.width))std::cerr<<"ENTRY_EDGE tick="<<i<<" player="<<scene->player().position.x<<","<<scene->player().position.y<<" center="<<scene->camera().center.x<<","<<scene->camera().center.y<<" offset="<<scene->camera().offset.x<<","<<scene->camera().offset.y<<" screen="<<p.x<<","<<p.y<<std::endl;
   assert(p.y>=0&&p.y<=config.canvas.height&&p.x>=0&&p.x<=config.canvas.width);
   // Check the visible player against each HUD group at the finite map edge.
   for(unsigned group=0;group<hud::count;++group){const auto b=hud::visualBounds({},group,layout);assert(!(p.x>=b.x&&p.x<=b.x+b.width&&p.y>=b.y&&p.y<=b.y+b.height));}
   const auto frame=game.drawWorld();assert(frame.width==config.canvas.width&&frame.height==config.canvas.height);
   const auto& terrain=frame.sprites.front();assert(terrain.clip&&terrain.clip->x==0&&terrain.clip->y==0&&terrain.clip->width==config.canvas.width&&terrain.clip->height==config.canvas.height);
   assert(terrain.strip.size()==4);for(const auto& v:terrain.strip)assert(v.uv.x>=0&&v.uv.x<=1&&v.uv.y>=0&&v.uv.y<=1);assert(terrain.center.x-terrain.size.width*.5f<=0&&terrain.center.y-terrain.size.height*.5f<=0&&terrain.center.x+terrain.size.width*.5f>=config.canvas.width&&terrain.center.y+terrain.size.height*.5f>=config.canvas.height);
 }
}
void phoneMenus(Files& files){
 const auto config=DisplayConfiguration::surface({912,420},{68,20,68,0});const auto area=config.content();
 Host host;PreparedTiles tiles("fullscreen-menu-test");ui::Screens screens(files,tiles,host);screens.setDisplay(config);DarwinWorldRandom random(1);
 for(unsigned base=1;base<=6;++base){
  ui::ScreensState state;state.locale="kr";state.screen=ui::ScreenId::base;state.baseId="Base00"+std::to_string(base);state.selectedBaseButton=1;state.baseDescription="설명 전체와 나가기 버튼은 겹치지 않아야 한다.";
  screens.refresh(state,12);for(unsigned i=0;i<120;++i)screens.tick(12+float(i)/60,random);Frame frame;screens.append(frame,15);
  bool background=false,description=false;
  for(const auto& sprite:frame.sprites){
   if(sprite.image=="PhoneMenu/"+state.baseId+".jpg"){background=true;assert(sprite.clip&&sprite.clip->x==0&&sprite.clip->width==config.canvas.width);uniformCover(sprite,config.canvas);}
   if(sprite.image=="PhoneMenu/BaseTextBG.png"){description=true;const auto left=sprite.center.x-sprite.size.width*.5f,bottom=sprite.center.y-sprite.size.height*.5f,top=sprite.center.y+sprite.size.height*.5f;assert(left>=area.x&&bottom>=area.y&&top<=area.y+area.height);}
  }
  assert(background&&description);
  const auto original=DataTable::parse(files.read("PhoneMenu/"+state.baseId+".tble"));const auto hd=DataTable::parse(files.read(state.baseId+".tble"));unsigned expected=0;
  for(std::size_t row=0;row<hd.rowCount();++row)expected+=hd.value(row,"Type")=="Button";
  assert(screens.buttons().size()==expected);
  // Phone artwork, highlight and facilities retain their original relative
  // positions. Road buttons keep their relative spacing as one group.
  for(const auto& key:screens.buttons()){
   std::size_t row=0;for(;row<original.rowCount();++row)if(original.value(row,"Type")=="Button"&&original.integer(row,"ID")==int(key.id)-2000)break;assert(row<original.rowCount());
   const auto before=screens.buttonScreenPosition(key);const DisplayLayout originalLayout(config);const auto originalTransform=originalLayout.ui(DisplayRegion::artScene);const Vec2 source{240+original.real(row,"X"),160+original.real(row,"Y")};const auto bound=screens.button(key)->bounds();const auto expectedPosition=key.id==2004?originalLayout.keepControlVisible(DisplayRegion::artScene,source,{bound.left,bound.bottom,bound.right-bound.left,bound.top-bound.bottom}):source;
   if(key.id<=2004)assert(near(before.x,originalTransform.map(expectedPosition).x));
   screens.revealButton(key);const auto point=screens.buttonScreenPosition(key);assert(point.x>=0&&point.x<=config.canvas.width&&point.y>=0&&point.y<=config.canvas.height);
   host.events.clear();screens.input(PointerPhase::begin,point,701,15);screens.input(PointerPhase::end,point,701,15);if(host.events.empty()||host.events.front().button.key!=key)std::cerr<<"PHONE_BASE_HIT base="<<base<<" wanted="<<key.id<<" at="<<point.x<<","<<point.y<<" got="<<(host.events.empty()?0:host.events.front().button.key.id)<<std::endl;assert(!host.events.empty()&&host.events.front().button.key==key);
  }
  // Drags must neither pan the scene nor change its button page.
  const auto key=screens.buttons().front();host.events.clear();const Vec2 gesture{config.canvas.width*.5f,config.canvas.height*.5f};
  screens.input(PointerPhase::begin,gesture,702,16);screens.input(PointerPhase::move,{gesture.x,gesture.y+1000},702,16.1f);screens.input(PointerPhase::end,{gesture.x,gesture.y+1000},702,16.2f);assert(std::none_of(host.events.begin(),host.events.end(),[](const auto& e){return e.button.type==ui::ButtonEventType::pop;}));
  const auto top=screens.buttonScreenPosition(key);Frame upper;screens.append(upper,16.2f);const Sprite* bg=nullptr;for(const auto& image:upper.sprites)if(image.image=="PhoneMenu/"+state.baseId+".jpg")bg=&image;assert(bg);uniformCover(*bg,config.canvas);assert((screens.menuScrollOffsets()==std::array<float,2>{}));
  screens.input(PointerPhase::begin,gesture,703,17);screens.input(PointerPhase::move,{gesture.x,gesture.y-1000},703,17.1f);screens.input(PointerPhase::end,{gesture.x,gesture.y-1000},703,17.2f);const auto bottom=screens.buttonScreenPosition(key);assert(near(top.x,bottom.x)&&near(top.y,bottom.y));assert((screens.menuScrollOffsets()==std::array<float,2>{}));
  Frame lower;screens.append(lower,17.2f);for(const auto& image:lower.sprites)if(image.image=="PhoneMenu/"+state.baseId+".jpg")uniformCover(image,config.canvas);
  screens.setDisplay(DisplayConfiguration::surface({1024,1366}));Frame tablet;screens.append(tablet,17.3f);bool hdBackground=false;for(const auto& image:tablet.sprites)hdBackground|=image.image==state.baseId+".png";assert(hdBackground);screens.setDisplay(config);
 }
 ui::ScreensState state;state.locale="kr";state.screen=ui::ScreenId::command;
 for(unsigned i=0;i<3;++i){ui::ItemView item;item.dataId="accepted"+std::to_string(i);item.name="진행 중인 임무";state.acceptedMissions.push_back(item);}
 screens.refresh(state,12);const auto clip=DisplayLayout(config).clip(DisplayRegion::list).value();
 for(const auto& key:screens.buttons())if(key.id>=5200&&key.id<=5202){const auto p=screens.buttonScreenPosition(key);assert(p.y>=clip.y+24&&p.y<=clip.y+clip.height-24);}
}
void allMapEdges(Files& files){
 const auto maps=Maps::parse(Chunk::parse(files.read("MapInfo.mcb")));unsigned count=0;
 for(const auto& [name,map]:maps.all()){if(!name.starts_with("Stage"))continue;MapPreview preview(files,name);++count;
  for(const auto canvas:{Size{912,420},Size{1280,360}}){CameraPresentation policy;const auto config=DisplayConfiguration::surface(canvas);policy.configure(config);const auto view=policy.snapshot();
   for(const Vec2 corner:{Vec2{-512,-512},Vec2{512,-512},Vec2{-512,512},Vec2{512,512}}){MapCamera c;c.viewport=view.viewport;c.fitWorld=c.fillSurface=true;c.lowerOverscan=0;c.center=corner;constrainCamera(c,corner);const auto frame=preview.compose(c);const auto& ground=frame.sprites.front();if(!(ground.center.x-ground.size.width*.5f<=.01f&&ground.center.y-ground.size.height*.5f<=.01f&&ground.center.x+ground.size.width*.5f>=c.viewport.width-.01f&&ground.center.y+ground.size.height*.5f>=c.viewport.height-.01f))std::cerr<<"MAP_COVER "<<name<<" image="<<ground.image<<" center="<<ground.center.x<<","<<ground.center.y<<" size="<<ground.size.width<<","<<ground.size.height<<" viewport="<<c.viewport.width<<","<<c.viewport.height<<std::endl;assert(ground.strip.size()==4);for(const auto& v:ground.strip)assert(v.uv.x>=0&&v.uv.x<=1&&v.uv.y>=0&&v.uv.y<=1);assert(ground.center.x-ground.size.width*.5f<=.01f&&ground.center.y-ground.size.height*.5f<=.01f&&ground.center.x+ground.size.width*.5f>=c.viewport.width-.01f&&ground.center.y+ground.size.height*.5f>=c.viewport.height-.01f);}
  }
 }assert(count==38);
}
void fullCanvasBanners(Files& files){
 for(auto config:{DisplayConfiguration::surface({667,375},{44,21,44,0}),DisplayConfiguration::surface({912,420},{68,20,68,0}),DisplayConfiguration::surface({1280,360},{68,20,68,0})}){
  config.device=DisplayDevice::phone;const DisplayLayout layout(config);
  for(auto region:{DisplayRegion::artBanner,DisplayRegion::banner}){const auto clip=layout.clip(region).value();assert(clip.x==0&&clip.y==0&&clip.width==config.canvas.width&&clip.height==config.canvas.height);}
  const auto spec=menu::title(config);const auto logo=layout.ui(DisplayRegion::artBanner);
  assert(logo.map({240+spec.bigStart-512,160}).x>=config.canvas.width-.01f);assert(logo.map({240+spec.bigEnd+512,160}).x<=.01f);
  const auto stage=layout.ui(DisplayRegion::banner);const auto start=stage.map(menu::placeNamePosition(config,{-1024,512})),end=stage.map(menu::placeNamePosition(config,{900,512}));
  assert(start.x-128*stage.scale>=config.canvas.width);assert(end.x+128*stage.scale<=.01f);
  Host host;PreparedTiles tiles("full-canvas-title");ui::Screens ui(files,tiles,host);ui.setDisplay(config);ui::ScreensState state;state.screen=ui::ScreenId::title;ui.refresh(state,0);Frame frame;ui.append(frame,0);bool found=false;
  for(const auto& sprite:frame.sprites)if(sprite.image=="PhoneMenu/Main_TitleBig.png"){found=true;assert(sprite.clip&&sprite.clip->x==0&&sprite.clip->width==config.canvas.width);}assert(found);
 }
}
void phoneArtwork(Files& files){
 assert(validResourceName("PhoneMenu/Main_BG.png"));for(const auto bad:{"/Main_BG.png","../Main_BG.png","PhoneMenu/../Main_BG.png","PhoneMenu/sub/Main_BG.png","PhoneMenu/..","PhoneMenu/","PhoneMenu\\Main_BG.png"})assert(!validResourceName(bad));assert(!validResourceName(std::string_view("PhoneMenu/a\0.png",16)));
 const auto config=DisplayConfiguration::surface({912,420},{68,20,68,0});Host host;PreparedTiles tiles("phone-art");ui::Screens ui(files,tiles,host);ui.setDisplay(config);DarwinWorldRandom random(1);
 for(const auto screen:{ui::ScreenId::title,ui::ScreenId::slots,ui::ScreenId::command,ui::ScreenId::shop,ui::ScreenId::maintenance}){
  ui::ScreensState state;state.screen=screen;ui.refresh(state,0);Frame prep;ui.appendPreparation(prep);const std::string name=screen==ui::ScreenId::title?"Main_BG.png":screen==ui::ScreenId::slots?"PhoneMenu/SelSlot_BG.jpg":screen==ui::ScreenId::command?"PhoneMenu/CmdCenterBG.jpg":screen==ui::ScreenId::shop?"PhoneMenu/ShopBG.jpg":"PhoneMenu/MaintenanceBG.jpg";bool found=false;
  for(const auto& image:prep.sprites)if(image.image==name){found=true;const auto info=imageInfo(files.read(name));assert(info.width==(screen==ui::ScreenId::title?768u:960u)&&info.height==(screen==ui::ScreenId::title?1024u:640u));assert(decodeImage(files.read(name)).width==info.width);}assert(found);
  for(unsigned tick=0;tick<800;++tick){ui.tick(float(tick)/60,random);Frame f;ui.append(f,float(tick)/60);for(const auto& image:f.sprites)if(image.image==name){
       if(screen==ui::ScreenId::maintenance){
           assert(image.clip&&image.strip.size()==4);
           const auto& a=image.strip[0];const auto& b=image.strip[3];
           assert(near((b.position.x-a.position.x)/((b.uv.x-a.uv.x)*480),(a.position.y-b.position.y)/((b.uv.y-a.uv.y)*320)));
           for(const auto& vertex:image.strip)
               assert(vertex.uv.x>=-.001f&&vertex.uv.x<=1.001f&&vertex.uv.y>=-.001f&&vertex.uv.y<=1.001f);
       }
       else uniformCover(image,config.canvas,screen==ui::ScreenId::title?.75f:1.5f);if(screen==ui::ScreenId::title){const auto b=quadBounds(image);assert(near(b.y+b.height,config.canvas.height));}}}
 }
 MapLoading loading(files,nullptr,true);const auto prep=loading.preparationFrame();assert(prep.sprites.size()==1&&prep.sprites[0].image=="PhoneMenu/StageClearBG.png"&&loading.artworkViewport().width==480);
}
void phoneMenuGrids(Files& files){
 const auto config=DisplayConfiguration::surface({912,420},{68,20,68,0});Host host;PreparedTiles tiles("phone-menu-grids");ui::Screens ui(files,tiles,host);ui.setDisplay(config);ui::ScreensState state;state.locale="kr";state.screen=ui::ScreenId::shop;state.inventoryVisible=true;state.inventoryPages=2;
 for(unsigned i=0;i<25;++i){ui::ItemView item;item.dataId="inventory"+std::to_string(i);item.icon="Icon10011.png";item.count=1;state.inventory.push_back(item);if(i<24){item.dataId="stock"+std::to_string(i);state.shopItems.push_back(item);}}
 for(auto& item:state.equipment){item.icon="Icon10011.png";item.dataId="equipment";item.count=1;}for(auto& item:state.bots){item.icon="Icon04201.png";item.dataId="bot";item.count=1;}ui.refresh(state,0);
 const auto key=[&](unsigned id){for(const auto& k:ui.buttons())if(k.id==id)return k;throw std::runtime_error("Missing phone menu button "+std::to_string(id));};
 const auto tap=[&](unsigned id){auto k=key(id);ui.revealButton(k);const auto p=ui.buttonScreenPosition(k);ui.input(PointerPhase::begin,p,900+id,1);ui.input(PointerPhase::end,p,900+id,1.1f);};
 unsigned inv=0,stock=0;for(auto k:ui.buttons()){inv+=k.id>=6000&&k.id<6025;stock+=k.id>=2200&&k.id<2224;}assert(inv==(menu::phone(config)?menu::visibleRows(config)*5:25)&&stock==(menu::phone(config)?menu::visibleRows(config)*4:24));
 Frame f;ui.append(f,0);unsigned backgrounds=0;bool skin=false;for(const auto& s:f.sprites){backgrounds+=s.image=="PhoneMenu/ShopBG.jpg";assert(s.image!="ShopBG.png"&&s.image!="MaintenanceBG.png"&&s.image!="InvenBG.png"&&s.image!="ItemBox.png");if(s.image=="Icon10011.png"&&s.strip.empty())skin|=near(s.size.width,32*std::min(config.canvas.width/480,config.canvas.height/320));}assert(backgrounds==3&&skin);
 for(unsigned i=1;i<(24+menu::visibleRows(config)*4-1)/(menu::visibleRows(config)*4);++i)tap(2106);for(unsigned i=1;i<(25+menu::visibleRows(config)*5-1)/(menu::visibleRows(config)*5);++i)tap(6091);const auto lastStock=ui.buttonScreenPosition(key(2223)),lastInventory=ui.buttonScreenPosition(key(6024));const DisplayLayout layout(config);const auto shopClip=layout.clip(DisplayRegion::phoneStock).value(),invClip=layout.clip(DisplayRegion::phoneInventory).value();assert(lastStock.y>=shopClip.y&&lastStock.y<=shopClip.y+shopClip.height&&lastInventory.y>=invClip.y&&lastInventory.y<=invClip.y+invClip.height);
 host.events.clear();const auto begin=ui.buttonScreenPosition(key(6024)),destination=ui.buttonScreenPosition(key(6020));ui.input(PointerPhase::begin,begin,999,2);ui.input(PointerPhase::move,destination,999,2.1f);ui.input(PointerPhase::end,destination,999,2.2f);bool mapped=false;for(const auto& e:host.events)if(e.button.type==ui::ButtonEventType::move&&e.dropCoordinates){assert(near(e.dropCoordinates->inventory.x,408)&&near(e.dropCoordinates->inventory.y,192));mapped=true;}assert(mapped);
 tap(9204);assert(ui.button(key(6100))->visible&&!ui.button(key(2223))->visible);f={};ui.append(f,2);backgrounds=0;for(const auto& s:f.sprites){backgrounds+=s.image=="PhoneMenu/MaintenanceBG.jpg";assert(s.image!="MaintenanceBG.png"&&s.image!="InvenBG.png"&&s.image!="ItemBox.png");}assert(backgrounds==7);
 for(const auto pair:{std::pair{6100u,Vec2{188,388}},std::pair{6302u,Vec2{326,270}}}){const auto from=ui.buttonScreenPosition(key(6024)),to=ui.buttonScreenPosition(key(pair.first));host.events.clear();ui.input(PointerPhase::begin,from,1001,3);ui.input(PointerPhase::move,to,1001,3.1f);ui.input(PointerPhase::end,to,1001,3.2f);mapped=false;for(const auto& e:host.events)if(e.button.type==ui::ButtonEventType::move&&e.dropCoordinates){assert(near(e.dropCoordinates->inventory.x,pair.second.x)&&near(e.dropCoordinates->inventory.y,pair.second.y));mapped=true;}assert(mapped);}
 tap(9205);assert(ui.button(key(2223))->visible&&!ui.button(key(6100))->visible);
 std::cout<<"PHONE_MENU_GRIDS originalBakedCells/HD24and25/70OriginalAssets/noDuplicatePanels/buttonPages/equipmentTab/HDdragInverse PASS\n";
}
void phoneButtonPages(Files& files){
 for(const auto extent:{Size{667,375},Size{912,420},Size{1280,360}}){
  const auto config=DisplayConfiguration::surface(extent);Host host;PreparedTiles tiles("phone-button-pages");ui::Screens ui(files,tiles,host);ui.setDisplay(config);ui::ScreensState state;state.screen=ui::ScreenId::shop;state.inventoryVisible=true;state.inventoryPages=1;
  for(unsigned i=0;i<25;++i){ui::ItemView item;item.dataId=std::to_string(i);item.icon="Icon10011.png";item.count=1;state.inventory.push_back(item);if(i<24)state.shopItems.push_back(item);}ui.refresh(state,0);
  const auto press=[&](unsigned id){for(auto k:ui.buttons())if(k.id==id){auto p=ui.buttonScreenPosition(k);ui.input(PointerPhase::begin,p,44000+id,1);ui.input(PointerPhase::end,p,44000+id,1);return;}throw std::runtime_error("Missing pager "+std::to_string(id));};
  std::set<unsigned> stock,inventory;
  for(unsigned page=0;page<7;++page){for(auto k:ui.buttons()){if(k.id>=2200&&k.id<2224)stock.insert(k.id);if(k.id>=6000&&k.id<6025)inventory.insert(k.id);}if((page+1)*menu::visibleRows(config)*4<24)press(2106);if((page+1)*menu::visibleRows(config)*5<25)press(6091);}
  assert(stock.size()==24&&inventory.size()==25);
  // HD retains its original empty trailing page for an exact24-item count.
  // Repeated NEXT at that boundary must not create empty local subpages.
  state.shopPage=1;ui.refresh(state,1);press(2106);host.events.clear();press(2106);bool forwarded=false;for(const auto& event:host.events)forwarded|=event.button.key.id==2106&&event.button.type==ui::ButtonEventType::push;assert(forwarded);

  for(const auto end:{Vec2{extent.width-5,extent.height*.5f},Vec2{5,extent.height-5},Vec2{5,5}}){std::vector<Vec2> before;for(auto k:ui.buttons())before.push_back(ui.buttonScreenPosition(k));ui.input(PointerPhase::begin,{5,extent.height*.5f},331,2);ui.input(PointerPhase::move,end,331,2.1f);ui.input(PointerPhase::end,end,331,2.2f);unsigned i=0;for(auto k:ui.buttons()){auto p=ui.buttonScreenPosition(k);assert(near(p.x,before[i].x)&&near(p.y,before[i].y));++i;}assert((ui.menuScrollOffsets()==std::array<float,2>{}));}
 }
 std::cout<<"PHONE_BUTTON_PAGES 3 surfaces / all24stock+25inventory / no horizontal or vertical scroll PASS\n";
}
void phoneMenuResize(Files& files){
 const auto shortPhone=DisplayConfiguration::surface({912,420},{68,20,68,0});
 const auto tallPhone=DisplayConfiguration::surface({912,550},{68,20,68,0});
 assert(menu::visibleRows(shortPhone)==2&&menu::visibleRows(tallPhone)==3);
 Host host;
 PreparedTiles tiles("phone-menu-resize");
 ui::Screens screens(files,tiles,host);
 screens.setDisplay(shortPhone);
 ui::ScreensState state;
 state.screen=ui::ScreenId::shop;
 state.baseId="Base003";
 state.shopPage=state.inventoryPage=1;
 state.inventoryVisible=true;
 state.inventoryPages=2;
 for(unsigned i=0;i<50;++i){
  ui::ItemView item;
  item.icon="Icon10011.png";
  item.dataId="inventory"+std::to_string(i);
  item.count=1;
  state.inventory.push_back(item);
  if(i<46){
   item.dataId="stock"+std::to_string(i);
   state.shopItems.push_back(item);
  }
 }
 screens.refresh(state,0);
 const auto key=[&](unsigned id){
  for(const auto& value:screens.buttons())if(value.id==id)return value;
  throw std::runtime_error("Missing resize fixture button "+std::to_string(id));
 };
 const auto press=[&](unsigned id){
  const auto point=screens.buttonScreenPosition(key(id));
  screens.input(PointerPhase::begin,point,45000+id,1);
  screens.input(PointerPhase::end,point,45000+id,1.1f);
 };
 const auto expectCells=[&](unsigned base,unsigned capacity,unsigned first,unsigned count){
  std::vector<unsigned> actual,expected;
  for(const auto& value:screens.buttons())if(value.id>=base&&value.id<base+capacity)actual.push_back(value.id-base);
  for(unsigned i=first;i<first+count;++i)expected.push_back(i);
  assert(actual==expected);
 };
 press(2106);press(2106);press(6091);press(6091);
 expectCells(2200,24,16,6);
 expectCells(6000,25,20,5);
 screens.setDisplay(tallPhone);
 expectCells(2200,24,12,10);
 expectCells(6000,25,15,10);
 screens.setDisplay(shortPhone);
 expectCells(2200,24,8,8);
 expectCells(6000,25,10,10);

 screens.setDisplay(tallPhone);
 press(2106);
 state.selectedShop=21;
 state.shopDetailVisible=true;
 screens.refresh(state,2);
 screens.setDisplay(shortPhone);
 expectCells(2200,24,16,6);
 assert(key(2221).dataId=="stock45"&&screens.button(key(2102))->visible);
 state.selectedShop=-1;
 state.shopDetailVisible=false;
 screens.refresh(state,3);
 screens.setDisplay(tallPhone);
 press(6091);
 state.selectedInventory=24;
 state.detailInventory=49;
 state.detailItem=state.inventory.back();
 screens.refresh(state,4);
 screens.setDisplay(shortPhone);
 expectCells(6000,25,20,5);
 assert(key(6024).dataId=="inventory49"&&screens.button(key(2103))->visible);
 state.selectedInventory=state.detailInventory=-1;
 state.detailItem={};
 screens.refresh(state,5);
 const auto source=screens.buttonScreenPosition(key(6024));
 const auto target=screens.buttonScreenPosition(key(6020));
 host.events.clear();
 screens.input(PointerPhase::begin,source,45800,5);
 screens.input(PointerPhase::move,target,45800,5.1f);
 screens.input(PointerPhase::end,target,45800,5.2f);
 bool mapped=false;
 for(const auto& event:host.events)if(event.button.type==ui::ButtonEventType::move&&event.dropCoordinates){
  assert(event.button.key.id==6024&&near(event.dropCoordinates->inventory.x,408)&&near(event.dropCoordinates->inventory.y,192));
  mapped=true;
 }
 assert(mapped);

 auto stock=std::move(state.shopItems);
 state.shopItems.clear();
 screens.refresh(state,6);
 screens.setDisplay(tallPhone);
 expectCells(2200,24,0,0);
 state.shopItems=std::move(stock);
 screens.refresh(state,7);
 expectCells(2200,24,0,12);

 state.screen=ui::ScreenId::maintenance;
 state.inventory.clear();
 screens.setDisplay(shortPhone);
 screens.refresh(state,8);
 const auto shortCells=5*menu::equipmentLayout(shortPhone).inventoryRows;
 const auto tallCells=5*menu::equipmentLayout(tallPhone).inventoryRows;
 const auto lastWindow=(25-1)/shortCells;
 for(unsigned i=0;i<lastWindow;++i)press(6091);
 const auto first=lastWindow*shortCells;
 expectCells(6000,25,first,25-first);
 screens.setDisplay(tallPhone);
 const auto nextFirst=first/tallCells*tallCells;
 expectCells(6000,25,nextFirst,std::min(tallCells,25-nextFirst));
 std::cout<<"PHONE_MENU_RESIZE 2/3 rows / HD page 2 / partial and empty / detail and drop PASS\n";
}
void phoneLiveMenuSession(Files& files){
 Host host;DarwinWorldRandom random(1);SaveData saved;saved.initializeSlot(0,"Menus","kr",1794268800);progression::SlotView slot(saved.slot(0));slot.string(0x2d,16,"Base001");slot.u32(0x1b5c,5);slot.credit(500000);slot.string(0x8c+8*6,6,"01001");slot.byte(0x53c+8,3);
 GameSession game(files,host,random,std::move(saved));PreparedTiles tiles("live-phone-menu");ui::Screens ui(files,tiles,host);host.game=&game;host.screens=&ui;const auto phone=DisplayConfiguration::surface({912,420},{68,20,68,0});game.setDisplay(phone);ui.setDisplay(phone);assert(game.selectSlot(0));game.enterBase();
 const auto settle=[&]{for(unsigned n=0;game.screenTransition().phase!=8&&n<250;++n){game.tick();(void)game.drawWorld();}assert(game.screenTransition().phase==8);ui.refresh(game.state(),game.renderTime());};settle();
 const auto key=[&](unsigned id){for(const auto& k:ui.buttons())if(k.id==id)return k;throw std::runtime_error("Missing live phone menu button "+std::to_string(id));};
 const auto has=[&](unsigned id){for(const auto& k:ui.buttons())if(k.id==id)return true;return false;};
 const auto tap=[&](unsigned id){auto k=key(id);ui.revealButton(k);const auto p=ui.buttonScreenPosition(k);ui.input(PointerPhase::begin,p,20000+id,game.renderTime());ui.input(PointerPhase::end,p,20000+id,game.renderTime());};
 tap(2002);tap(2002);settle();assert(game.state().screen==ui::ScreenId::shop);const auto writes=host.writes;const Bytes before(game.workingSlot().begin(),game.workingSlot().end());
 const auto tapVisible=[&](unsigned id){
  const auto k=key(id);assert(ui.button(k)->visible);
  const auto point=ui.buttonScreenPosition(k);
  assert(ui.input(PointerPhase::begin,point,20000+id,game.renderTime()));
  ui.input(PointerPhase::end,point,20000+id,game.renderTime());
 };
 const auto overlaps=[](ClipRect a,ClipRect b){
  return a.x<b.x+b.width&&b.x<a.x+a.width&&a.y<b.y+b.height&&b.y<a.y+a.height;
 };
 const auto inside=[](ClipRect a,ClipRect b){
  return a.x>=b.x-.01f&&a.y>=b.y-.01f&&a.x+a.width<=b.x+b.width+.01f&&a.y+a.height<=b.y+b.height+.01f;
 };
 const auto verifyDetail=[&](unsigned actionID){
  for(unsigned n=0;n<30;++n){game.tick();ui.tick(game.renderTime(),random);}
  Frame frame;ui.append(frame,game.renderTime());
  std::optional<ClipRect> background,action;
  for(const auto& sprite:frame.sprites){
   if(sprite.image=="PhoneMenu/ItemDesc.png")background=quadBounds(sprite);
   if(sprite.image==(actionID==2102?"PhoneMenu/Btn_ShopBuy.png":"PhoneMenu/Btn_ShopSell.png"))action=quadBounds(sprite);
  }
  assert(background&&action&&inside(*background,phone.content())&&inside(*action,phone.content()));
  const auto hit=ui.buttonScreenBounds(key(actionID));
  const auto detailHit=ui.buttonScreenBounds(key(2112));
  const float reference=menu::phoneReferenceScale(phone);
  assert(near(background->width,256*reference)&&near(background->height,128*reference));
  assert(near(detailHit.width,206*reference)&&near(detailHit.height,128*reference));
  assert(near(action->width,32*reference)&&near(action->height,57*reference));
  const auto originalAction=menu::phoneReferenceUI(phone).map(menu::tradeAction);
  assert(near(action->x+action->width*.5f,originalAction.x));
  assert(near(action->y+action->height*.5f,originalAction.y));
  assert(inside(hit,*action)&&inside(*action,hit));
  assert(!overlaps(detailHit,*action));
  for(const auto& k:ui.buttons()){
   if(!ui.button(k)->visible)continue;
   const auto bounds=ui.buttonScreenBounds(k);
   if(k.id==2100||k.id==9204||k.id==9205||k.id==2104||k.id==2105||k.id==2106||k.id==6090||k.id==6091)
    assert(!overlaps(detailHit,bounds));
   if((k.id>=6000&&k.id<6025)||(k.id>=2200&&k.id<2224))assert(!overlaps(*action,bounds));
  }
 };
 assert(game.state().inventory[0].equipped&&!game.state().inventory[0].enabled);
 tapVisible(6000);assert(game.state().detailInventory==0&&!has(2103));
 tapVisible(2200);assert(game.state().shopDetailVisible&&game.state().selectedShop==0);verifyDetail(2102);
 tapVisible(9204);assert(!game.state().shopDetailVisible&&game.state().selectedShop==-1&&game.state().detailInventory==-1&&!has(2102));
 const auto equipLayout=menu::equipmentLayout(phone);
 const auto hitTab=ui.buttonScreenBounds(key(9205));
 assert(!has(9204)||!ui.button(key(9204))->visible);
 for(unsigned i=0;i<3;++i){
  const auto bot=ui.buttonScreenBounds(key(6300+i));
  assert(inside(bot,equipLayout.bands[1].clip)&&!overlaps(bot,hitTab));
 }
 for(unsigned i=0;i<6;++i)assert(inside(ui.buttonScreenBounds(key(6100+i)),equipLayout.bands[1].clip));
 Frame equipmentFrame;ui.append(equipmentFrame,game.renderTime());
 const auto& statClip=equipLayout.bands[1].clip;
 std::set<int> statRows;
 for(const auto& sprite:equipmentFrame.sprites)if(sprite.image=="NumSet01b.png"&&sprite.atlasBatch){
  for(const auto& quad:sprite.atlasBatch->quads){
   const auto p=quad.vertices[0].position;
   if(p.x<statClip.width){assert(p.y>statClip.y);statRows.insert(int(std::round(p.y)));}
  }
 }
 assert(statRows.size()==3);
 if(!has(6008))tapVisible(6091);
 tapVisible(6008);assert(game.state().detailInventory==8&&!has(2103));
 tapVisible(9205);assert(game.state().detailInventory==-1);
 tapVisible(6008);assert(game.state().detailInventory==8&&has(2103));verifyDetail(2103);
 tapVisible(6091);assert(game.state().detailInventory==-1);
 tapVisible(6090);
 // Open the original buy-confirmation flow, then attempt the new controls behind it.
 for(const auto& k:ui.buttons())if(k.id==2104&&k.intData==4){ui.revealButton(k);const auto p=ui.buttonScreenPosition(k);ui.input(PointerPhase::begin,p,777,game.renderTime());ui.input(PointerPhase::end,p,777,game.renderTime());break;}
 tapVisible(2200);tapVisible(2102);assert(game.state().tradeVisible);{Frame tradeFrame;ui.append(tradeFrame,game.renderTime());bool backdrop=false;for(const auto& sprite:tradeFrame.sprites)if(sprite.image=="PhoneMenu/TradeItemBG.png"){backdrop=true;auto b=quadBounds(sprite);std::cout<<"PHONE_TRADE_PANEL bounds="<<b.x<<","<<b.y<<","<<b.width<<","<<b.height<<" alpha="<<sprite.color.a<<" clip="<<(sprite.clip?sprite.clip->height:0)<<"\n";assert(b.y>=0&&b.y+b.height<=phone.canvas.height);}assert(backdrop);}const auto first=ui.buttonScreenPosition(key(6000));const auto selected=game.state().selectedShop;const auto shopVisible=ui.button(key(2200))->visible;
 for(unsigned id:{6091u,9204u}){const auto p=ui.buttonScreenPosition(key(id));ui.input(PointerPhase::begin,p,888+id,game.renderTime());ui.input(PointerPhase::end,p,888+id,game.renderTime());}
 assert(game.state().tradeVisible&&game.state().selectedShop==selected&&ui.button(key(2200))->visible==shopVisible);const auto after=ui.buttonScreenPosition(key(6000));assert(near(first.x,after.x)&&near(first.y,after.y));tap(2107);assert(!game.state().tradeVisible);
 assert(host.writes==writes&&Bytes(game.workingSlot().begin(),game.workingSlot().end())==before);
 // Purchase and sell through the same rendered Phone controls and real Session.
 const auto countItem=[&](const std::string& code){int result=0;for(const auto& item:game.state().inventory)if(item.dataId==code)result+=item.count;return result;};
 tapVisible(2200);tapVisible(2102);assert(game.state().tradeVisible);auto code=game.state().tradeItemId;auto count=countItem(code);auto quantity=game.state().tradeCount;auto credit=game.state().credits;auto price=game.state().tradePrice;assert(price>0&&credit>=price);tap(2108);assert(!game.state().tradeVisible&&game.state().credits==credit-price&&countItem(code)==count+quantity);
 tapVisible(6008);tapVisible(2103);assert(game.state().tradeVisible&&!game.state().buyMode);credit=game.state().credits;price=game.state().tradePrice;code=game.state().tradeItemId;count=countItem(code);quantity=game.state().tradeCount;tap(2109);assert(!game.state().tradeVisible&&game.state().credits==credit+price&&countItem(code)==count-quantity);
 // A reused button must restore the HD snap contract across both surface profiles.
 tap(6008);for(unsigned n=0;n<3;++n){game.tick();ui.tick(game.renderTime(),random);}const auto current=game.state();ui.setDisplay(DisplayConfiguration::surface({1024,1366}));for(auto k:ui.buttons())assert(ui.button(k)->positionSnap==ui::PositionSnap::none);ui.setDisplay(phone);for(auto k:ui.buttons())assert(ui.button(k)->positionSnap==ui::PositionSnap::none);
 // Background gestures must leave the always visible EXIT unchanged.
 ui.input(PointerPhase::begin,{20,200},33445,game.renderTime());ui.input(PointerPhase::move,{20,340},33445,game.renderTime());ui.input(PointerPhase::end,{20,340},33445,game.renderTime());const auto exit=ui.buttonScreenPosition(key(2100));assert(exit.y>0&&exit.y<phone.canvas.height);ui.input(PointerPhase::begin,exit,33446,game.renderTime());ui.input(PointerPhase::end,exit,33446,game.renderTime());settle();assert(game.state().screen==ui::ScreenId::base);
 std::cout<<"PHONE_LIVE_MENU SessionCallbacks/detailPushClose/tradeModalBlocksTabsAndScroll/surfaceReuse/noSelectionWrites/realPurchaseAndSale PASS\n";
}
void phoneDirectMaintenance(Files& files){
 Host host;DarwinWorldRandom random(1);SaveData saved;saved.initializeSlot(0,"Equipment","kr",1794268800);
 progression::SlotView slot(saved.slot(0));slot.string(0x2d,16,"Base004");slot.u32(0x1b5c,5);
 GameSession game(files,host,random,std::move(saved));PreparedTiles tiles("phone-direct-equipment");
 ui::Screens screens(files,tiles,host);host.game=&game;host.screens=&screens;
 const auto display=DisplayConfiguration::surface({912,420});game.setDisplay(display);screens.setDisplay(display);
 assert(game.selectSlot(0));game.enterBase();
 const auto settle=[&]{
  for(unsigned n=0;game.screenTransition().phase!=8&&n<250;++n){game.tick();(void)game.drawWorld();}
  assert(game.screenTransition().phase==8);screens.refresh(game.state(),game.renderTime());
 };
 const auto tap=[&](unsigned id){
  for(const auto& key:screens.buttons())if(key.id==id&&screens.button(key)->visible){
   const auto point=screens.buttonScreenPosition(key);
   assert(screens.input(PointerPhase::begin,point,61000+id,game.renderTime()));
   screens.input(PointerPhase::end,point,61000+id,game.renderTime());
   return;
  }
  assert(false&&"Missing visible phone facility button");
 };
 settle();tap(2003);tap(2003);settle();
 assert(game.state().screen==ui::ScreenId::maintenance);
 for(unsigned tries=0;game.state().inventoryPage<1&&tries<6;++tries)tap(6091);
 assert(game.state().inventoryPage==1);
 tap(6091);
 const auto writes=host.writes;
 const auto unsupported=game.metrics().unsupportedActions;
 const auto before=screens.buttons();
 const auto first=std::find_if(before.begin(),before.end(),[](const auto& key){return key.id>=6000&&key.id<6025;});
 assert(first!=before.end());
 tap(9205);
 assert(game.state().screen==ui::ScreenId::shop&&game.state().inventoryPage==1&&host.writes==writes);
 assert(game.metrics().unsupportedActions==unsupported);
 const auto after=screens.buttons();
 assert(std::any_of(after.begin(),after.end(),[&](const auto& key){return key.id==first->id;}));
}
void originalHDWorldParity(Files& files){
 Host oldHost,newHost;DarwinWorldRandom oldRandom(1),newRandom(1);GameSession original(files,oldHost,oldRandom),restored(files,newHost,newRandom);
 for(auto* game:{&original,&restored}){assert(game->createSlot(0,"Parity","kr"));assert(game->enterField("Stage01_01",0));}
 restored.setDisplay(DisplayConfiguration::surface({1024,1366},{0,20,0,24}));const auto writes=newHost.writes;
 for(unsigned i=0;i<180;++i){original.tick();restored.tick();auto a=original.drawWorld(),b=restored.drawWorld();const auto* x=original.battle();const auto* y=restored.battle();const auto& c=y->camera();
  assert(c.viewport.width==768&&c.viewport.height==1024&&c.inputMinimumY==100&&c.lowerOverscan==94&&!c.fitWorld&&!c.keepFocusVisible&&!c.fillSurface);
  assert(x->camera().center.x==c.center.x&&x->camera().center.y==c.center.y&&x->player().position.x==y->player().position.x&&x->player().position.y==y->player().position.y&&x->playerHP()==y->playerHP());
  assert(x->metrics().enemyEmissions==y->metrics().enemyEmissions&&x->metrics().damageCalls==y->metrics().damageCalls);assert(a.width==768&&a.height==1024&&b.width==1024&&b.height==1366);
 }assert(newHost.writes==writes);std::cout<<"HD_WORLD_PARITY originalCamera/visibility/player/enemyDamage180frames/noDisplaySave PASS\n";
}
void session(Files& files,DisplayConfiguration config){Host host;DarwinWorldRandom random(1);GameSession game(files,host,random);assert(game.createSlot(0,"Layout","kr"));assert(game.enterField("Stage01_01",0));game.setDisplay(config);auto* scene=game.battle();assert(scene);const auto before=Bytes(game.workingSlot().begin(),game.workingSlot().end());const auto writes=host.writes;const auto old=scene->player().position;auto logical=scene->camera().toScreen({old.x,old.y+100});const auto point=game.worldCanvasPoint(logical);assert(game.inputWorld(PointerPhase::begin,point,77));game.inputWorld(PointerPhase::end,point,77);assert(near(scene->player().destination.x,old.x)&&near(scene->player().destination.y,old.y+100));assert(game.inputWorld(PointerPhase::begin,point,78));game.inputWorld(PointerPhase::end,{point.x,0},78);assert(scene->dashState().movePointer==0);assert(game.inputWorld(PointerPhase::begin,point,79));game.inputWorld(PointerPhase::cancel,{0,0},79);assert(scene->dashState().movePointer==0);
 if(config.compact&&config.insets.left>0){const Vec2 edge{config.insets.left*.5f,config.canvas.height*.75f};assert(game.inputWorld(PointerPhase::begin,edge,880));game.inputWorld(PointerPhase::end,edge,880);}
 const auto initialZoom=scene->cameraPresentation().worldScale;
 for(unsigned i=0;i<100;++i){game.tick();auto frame=game.drawWorld();assert(frame.width==config.canvas.width&&frame.height==config.canvas.height);game.appendOverlays(frame);}assert(host.writes==writes);const auto z=scene->cameraPresentation().worldScale;const auto minimum=std::min(1.f,std::max(config.canvas.width,config.canvas.height)/1022.f);assert(config.focusZoom?(z>=minimum&&z<=1.f):near(z,std::max(config.canvas.width/768.f,config.canvas.height/1024.f)));
 if(config.focusZoom&&config.canvas.width==912){std::cout<<"PHONE_FOCUS_ZOOM initial="<<initialZoom<<" after="<<z<<std::endl;assert(initialZoom==1&&z<=initialZoom);}
 const auto pos=scene->player().position;const auto hp=scene->playerHP();game.setDisplay(DisplayConfiguration::surface({1024,1366}));assert(scene->player().position.x==pos.x&&scene->playerHP()==hp&&host.writes==writes);assert(game.workingSlot().size()==before.size());
}
}
int main(int argc,char** argv){if(argc<2)return 77;fullScreenWorld();transforms();Files files(argv[1]);originalHDWorldParity(files);tabletMenuCoverage(files);entryCamera(files);allMapEdges(files);fullCanvasBanners(files);phoneArtwork(files);phoneMenus(files);phoneMenuGrids(files);phoneButtonPages(files);phoneMenuResize(files);phoneLiveMenuSession(files);phoneDirectMaintenance(files);for(auto config:{DisplayConfiguration::surface({667,375}),DisplayConfiguration::surface({852,393},{59,21,59,0}),DisplayConfiguration::surface({912,420},{68,20,68,0}),DisplayConfiguration::surface({1024,1366},{0,20,0,24})}){screens(files,config);help(files,config);session(files,config);}std::cout<<"HM2_DISPLAY PASS 4 canvases / full-screen world / terrain edge coverage / HUD size / six base menus / Phone-original-art/no-menu-scroll/Retina-JPEG / first mission rows / safe-area / inverse-hit / HD25+24+6 / drag / warm100 / help-scroll / common-world / no-display-save\n";}

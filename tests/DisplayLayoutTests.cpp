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
using namespace hm2;
namespace {
bool near(float a,float b){return std::abs(a-b)<.002f;}
ClipRect quadBounds(const Sprite& s){if(s.strip.empty())return {s.center.x-s.size.width*.5f,s.center.y-s.size.height*.5f,s.size.width,s.size.height};assert(s.strip.size()==4);float l=s.strip[0].position.x,r=l,b=s.strip[0].position.y,t=b;for(const auto& v:s.strip){l=std::min(l,v.position.x);r=std::max(r,v.position.x);b=std::min(b,v.position.y);t=std::max(t,v.position.y);}return {l,b,r-l,t-b};}
void uniformCover(const Sprite& s,Size canvas){
 assert(s.strip.size()==4);const auto o=s.strip[0].position,a=s.strip[2].position,b=s.strip[1].position;const Vec2 x{a.x-o.x,a.y-o.y},y{b.x-o.x,b.y-o.y};const float xx=x.x*x.x+x.y*x.y,yy=y.x*y.x+y.y*y.y;
 assert(near(std::sqrt(xx)/std::sqrt(yy),1.5f));
 for(const auto p:{Vec2{0,0},Vec2{canvas.width,0},Vec2{0,canvas.height},Vec2{canvas.width,canvas.height}}){const Vec2 d{p.x-o.x,p.y-o.y};const float u=(d.x*x.x+d.y*x.y)/xx,v=(d.x*y.x+d.y*y.y)/yy;assert(u>=-.0001f&&u<=1.0001f&&v>=-.0001f&&v<=1.0001f);}
}
void phoneMenuGeometry(){
 for(const auto config:{DisplayConfiguration::surface({667,375}),DisplayConfiguration::surface({852,393},{59,21,60,0}),DisplayConfiguration::surface({912,420},{68,20,68,0}),DisplayConfiguration::surface({1280,360},{0,20,0,0})}){
  const menu::PhoneLayout p(config);const auto a=config.content();const DisplayLayout layout(config);
  const auto inside=[](ClipRect r,ClipRect bounds){return r.x>=bounds.x-.01f&&r.y>=bounds.y-.01f&&r.x+r.width<=bounds.x+bounds.width+.01f&&r.y+r.height<=bounds.y+bounds.height+.01f;};
  assert(inside(p.picture,{a.x,0,a.width,config.canvas.height}));assert(near(p.picture.width/p.picture.height,1.5f));const auto full=layout.clip(DisplayRegion::phoneCanvas).value();assert(full.x==0&&full.y==0&&full.width==config.canvas.width&&full.height==config.canvas.height);
  const auto t=layout.ui(DisplayRegion::artScene);assert(near(t.map({0,0}).x,p.picture.x)&&near(t.map({480,320}).y,p.picture.y+p.picture.height));
  const auto dragged=layout.ui(DisplayRegion::artScene,10000);assert(near(dragged.offset.x,t.offset.x)&&near(dragged.offset.y,t.offset.y));
  for(bool inventory:{false,true})for(unsigned i=0;i<(inventory?25u:24u);++i){const auto cell=p.cell(i,inventory);assert(inside(cell,i<(inventory?20u:16u)?p.picture:inventory?p.rightRail:p.leftRail));for(unsigned j=0;j<i;++j){const auto other=p.cell(j,inventory);assert(cell.x+cell.width<=other.x+.01f||other.x+other.width<=cell.x+.01f||cell.y+cell.height<=other.y+.01f||other.y+other.height<=cell.y+.01f);}}
 }
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
 std::vector<ui::ScreenAction> events;std::vector<ui::TextRasterRequest> texts;unsigned writes=0;GameSession* game=nullptr;ui::Screens* screens=nullptr;
 void action(const ui::ScreenAction& a)override{events.push_back(a);if(game){game->action(a);if(screens)screens->refresh(game->state(),game->renderTime());}}
 ui::PreparedScreenText prepareText(const ui::TextRasterRequest& r)override{texts.push_back(r);return test::preparedTextStub(r);}
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
 state.screen=ui::ScreenId::shop;state.inventoryVisible=true;state.inventory.resize(25);state.shopItems.resize(24);for(unsigned i=0;i<24;++i){state.shopItems[i].dataId=std::to_string(i);state.shopItems[i].icon="Icon10011.png";}screens.refresh(state,12);unsigned inv=0,stock=0;for(auto key:screens.buttons()){inv+=key.id>=6000&&key.id<6025;stock+=key.id>=2200&&key.id<2224;}assert(inv==25&&stock==24);
 for(float pan:{0.f,screens.horizontalPanLimit()}){screens.setHorizontalPan(pan);Frame frame;screens.append(frame,12);assert(!frame.sprites.empty());}
 screens.setHorizontalPan(screens.horizontalPanLimit());for(const auto& key:screens.buttons())if(key.id==6000){const auto p=screens.buttonScreenPosition(key);host.events.clear();screens.input(PointerPhase::begin,p,101,12);screens.input(PointerPhase::move,{p.x+30,p.y-30},101,12);screens.input(PointerPhase::end,{p.x+30,p.y-30},101,12);assert(!host.events.empty());for(const auto& a:host.events)if(a.button.type==ui::ButtonEventType::move){assert(a.dropCoordinates);auto original=config.compact?Vec2{408+30/DisplayLayout(config).ui(DisplayRegion::phoneMenu).scale,432-30/DisplayLayout(config).ui(DisplayRegion::phoneMenu).scale}:DisplayLayout(config).ui(DisplayRegion::shopInventory,screens.horizontalPanLimit()).inverse({p.x+30,p.y-30});assert(near(original.x,a.dropCoordinates->inventory.x));}break;}
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
 for(const auto config:{DisplayConfiguration::surface({667,375}),DisplayConfiguration::surface({912,420},{68,20,68,0})}){
 const auto area=config.content();const menu::PhoneLayout geometry(config);Host host;PreparedTiles tiles("fixed-menu-test");ui::Screens screens(files,tiles,host);screens.setDisplay(config);DarwinWorldRandom random(1);
 for(unsigned base=1;base<=6;++base){
  ui::ScreensState state;state.locale="kr";state.screen=ui::ScreenId::base;state.baseId="Base00"+std::to_string(base);state.selectedBaseButton=1;state.baseDescription="시설 설명과 모든 도로를 화면 안에 표시한다.";state.baseHasCompletedMission=base%2==0;host.texts.clear();
  screens.refresh(state,12);for(unsigned n=0;n<120;++n)screens.tick(12+float(n)/60,random);Frame f;screens.append(f,15);bool background=false,description=false;
  for(const auto& sprite:f.sprites){if(sprite.image=="PhoneMenu/"+state.baseId+".jpg"){background=true;const auto b=quadBounds(sprite);assert(near(b.x,geometry.picture.x)&&near(b.y,geometry.picture.y)&&near(b.width,geometry.picture.width)&&near(b.height,geometry.picture.height));}if(sprite.image=="cpu-text:GuiBase.description"){description=true;const auto b=quadBounds(sprite);assert(b.x>=area.x-.01f&&b.x+b.width<=area.x+area.width+.01f&&b.y>=-.01f&&b.y+b.height<=geometry.footer.height+.01f);}}
  bool guidance=false,progress=false;for(const auto& request:host.texts){if(request.sourceId=="GuiBase.location"){guidance=request.text=="시설을\n선택하세요.";assert(request.text.find(state.baseId)==std::string::npos);}if(request.sourceId=="GuiBase.progress")progress=request.text==(state.baseHasCompletedMission?"완료한 임무를\n보고하세요.":"임무를 받아\n출격하세요.");}assert(guidance&&progress);
  assert(background&&description);float covered=0;for(const auto& image:f.sprites)if(image.image=="MissionList_Mid.png"&&image.clip&&image.clip->y==0&&image.clip->height==config.canvas.height){assert(near(image.clip->x,covered));covered+=image.clip->width;}assert(near(covered,config.canvas.width));
  const auto phone=DataTable::parse(files.read("PhoneMenu/"+state.baseId+".tble")),hd=DataTable::parse(files.read(state.baseId+".tble"));unsigned expected=0;for(std::size_t row=0;row<hd.rowCount();++row)expected+=hd.value(row,"Type")=="Button";assert(screens.buttons().size()==expected);
  for(const auto& key:screens.buttons()){std::size_t row=0;for(;row<phone.rowCount();++row)if(phone.value(row,"Type")=="Button"&&phone.integer(row,"ID")==int(key.id)-2000)break;assert(row<phone.rowCount());
   const auto expected=geometry.artwork.map({240+phone.real(row,"X"),160+phone.real(row,"Y")});const auto point=screens.buttonScreenPosition(key);assert(near(point.x,expected.x)&&near(point.y,expected.y));
   host.events.clear();screens.input(PointerPhase::begin,point,701,15);screens.input(PointerPhase::end,point,701,15);assert(!host.events.empty()&&host.events.front().button.key==key);
   // Moving an untouched menu area never pans image, facilities or paths.
   const auto old=screens.buttonScreenPosition(key);screens.input(PointerPhase::begin,{area.x+2,100},702,16);screens.input(PointerPhase::move,{area.x+2,300},702,16.1f);screens.input(PointerPhase::end,{area.x+2,300},702,16.2f);const auto after=screens.buttonScreenPosition(key);assert(near(old.x,after.x)&&near(old.y,after.y));
  }
  screens.setDisplay(DisplayConfiguration::surface({1024,1366}));Frame tablet;screens.append(tablet,17);bool hdBackground=false;for(const auto& image:tablet.sprites)hdBackground|=image.image==state.baseId+".png";assert(hdBackground);screens.setDisplay(config);
 }
 }
}
void allMapEdges(Files& files){
 const auto maps=Maps::parse(Chunk::parse(files.read("MapInfo.mcb")));unsigned count=0;
 for(const auto& [name,map]:maps.all()){if(!name.starts_with("Stage"))continue;MapPreview preview(files,name);++count;
  for(const auto canvas:{Size{912,420},Size{1280,360}}){CameraPresentation policy;const auto config=DisplayConfiguration::surface(canvas);policy.configure(config);const auto view=policy.snapshot();
   for(const Vec2 corner:{Vec2{-512,-512},Vec2{512,-512},Vec2{-512,512},Vec2{512,512}}){MapCamera c;c.viewport=view.viewport;c.fitWorld=c.fillSurface=true;c.lowerOverscan=0;c.center=corner;constrainCamera(c,corner);const auto frame=preview.compose(c);const auto& ground=frame.sprites.front();if(!(ground.center.x-ground.size.width*.5f<=.01f&&ground.center.y-ground.size.height*.5f<=.01f&&ground.center.x+ground.size.width*.5f>=c.viewport.width-.01f&&ground.center.y+ground.size.height*.5f>=c.viewport.height-.01f))std::cerr<<"MAP_COVER "<<name<<" image="<<ground.image<<" center="<<ground.center.x<<","<<ground.center.y<<" size="<<ground.size.width<<","<<ground.size.height<<" viewport="<<c.viewport.width<<","<<c.viewport.height<<std::endl;assert(ground.strip.size()==4);for(const auto& v:ground.strip)assert(v.uv.x>=0&&v.uv.x<=1&&v.uv.y>=0&&v.uv.y<=1);assert(ground.center.x-ground.size.width*.5f<=.01f&&ground.center.y-ground.size.height*.5f<=.01f&&ground.center.x+ground.size.width*.5f>=c.viewport.width-.01f&&ground.center.y+ground.size.height*.5f>=c.viewport.height-.01f);}
  }
 }assert(count==38);
}
void phoneArtwork(Files& files){
 assert(validResourceName("PhoneMenu/Main_BG.png"));for(const auto bad:{"/Main_BG.png","../Main_BG.png","PhoneMenu/../Main_BG.png","PhoneMenu/sub/Main_BG.png","PhoneMenu/..","PhoneMenu/","PhoneMenu\\Main_BG.png"})assert(!validResourceName(bad));assert(!validResourceName(std::string_view("PhoneMenu/a\0.png",16)));
 const auto config=DisplayConfiguration::surface({912,420},{68,20,68,0});Host host;PreparedTiles tiles("phone-art");ui::Screens ui(files,tiles,host);ui.setDisplay(config);DarwinWorldRandom random(1);
 for(const auto screen:{ui::ScreenId::title,ui::ScreenId::slots,ui::ScreenId::command,ui::ScreenId::shop,ui::ScreenId::maintenance}){
  ui::ScreensState state;state.screen=screen;ui.refresh(state,0);Frame prep;ui.appendPreparation(prep);const std::string name=screen==ui::ScreenId::title?"PhoneMenu/Main_BG.png":screen==ui::ScreenId::slots?"PhoneMenu/SelSlot_BG.jpg":screen==ui::ScreenId::command?"PhoneMenu/CmdCenterBG.jpg":screen==ui::ScreenId::shop?"PhoneMenu/ShopBG.jpg":"PhoneMenu/MaintenanceBG.jpg";bool found=false;
  for(const auto& image:prep.sprites)if(image.image==name){found=true;const auto info=imageInfo(files.read(name));assert(info.width==(screen==ui::ScreenId::title?480u:960u)&&info.height==(screen==ui::ScreenId::title?320u:640u));assert(decodeImage(files.read(name)).width==info.width);}assert(found);
  for(unsigned tick=0;tick<800;++tick){ui.tick(float(tick)/60,random);Frame f;ui.append(f,float(tick)/60);for(const auto& image:f.sprites)if(image.image==name){if(screen==ui::ScreenId::title||screen==ui::ScreenId::slots)uniformCover(image,config.canvas);else{const auto b=quadBounds(image),expected=menu::PhoneLayout(config).picture;assert(near(b.x,expected.x)&&near(b.y,expected.y)&&near(b.width,expected.width)&&near(b.height,expected.height));}}}
 }
 MapLoading loading(files,nullptr,true);const auto prep=loading.preparationFrame();assert(prep.sprites.size()==1&&prep.sprites[0].image=="PhoneMenu/StageClearBG.png"&&loading.artworkViewport().width==480);
}
void phoneMenuGrids(Files& files){
 for(const auto config:{DisplayConfiguration::surface({667,375}),DisplayConfiguration::surface({852,393},{59,21,60,0}),DisplayConfiguration::surface({912,420},{68,20,68,0})}){
 Host host;PreparedTiles tiles("phone-menu-grids");ui::Screens ui(files,tiles,host);ui.setDisplay(config);ui::ScreensState state;state.locale="kr";state.screen=ui::ScreenId::shop;state.inventoryVisible=true;state.inventoryPages=2;
 for(unsigned i=0;i<25;++i){ui::ItemView item;item.dataId="inventory"+std::to_string(i);item.icon="Icon10011.png";item.count=1;state.inventory.push_back(item);if(i<24){item.dataId="stock"+std::to_string(i);state.shopItems.push_back(item);}}
 for(auto& item:state.equipment){item.icon="Icon10011.png";item.dataId="equipment";item.count=1;}for(auto& item:state.bots){item.icon="Icon04201.png";item.dataId="bot";item.count=1;}ui.refresh(state,0);const menu::PhoneLayout geometry(config);
 const auto key=[&](unsigned id){for(const auto& k:ui.buttons())if(k.id==id)return k;throw std::runtime_error("Missing fixed menu button "+std::to_string(id));};
 const auto tap=[&](unsigned id){auto k=key(id);const auto p=ui.buttonScreenPosition(k);ui.input(PointerPhase::begin,p,900+id,1);ui.input(PointerPhase::end,p,900+id,1.1f);};
 for(bool inventory:{false,true})for(unsigned i=0;i<(inventory?25u:24u);++i){const auto k=key((inventory?6000u:2200u)+i);const auto box=geometry.cell(i,inventory);const auto p=ui.buttonScreenPosition(k);assert(near(p.x,box.x+box.width*.5f)&&near(p.y,box.y+box.height*.5f));host.events.clear();tap(k.id);assert(!host.events.empty()&&host.events.front().button.key==k&&host.events.front().button.key.dataId==(inventory?"inventory":"stock")+std::to_string(i));}
 for(const auto& k:ui.buttons())assert(k.id<9200||k.id>9203);assert(ui.horizontalPanLimit()==0);
 Frame f;ui.append(f,1);for(const auto& image:f.sprites){assert(image.image!="InvenBG.png"&&image.image!="ShopBG.png");if(image.image=="ItemBox.png"){const auto b=quadBounds(image);assert(b.x+b.width<=geometry.picture.x+.01f||b.x>=geometry.picture.x+geometry.picture.width-.01f);}}
 // The supplementary fifth HD row maps back to the original25-cell consumer.
 host.events.clear();const auto begin=ui.buttonScreenPosition(key(6024)),destination=ui.buttonScreenPosition(key(6020));ui.input(PointerPhase::begin,begin,999,2);ui.input(PointerPhase::move,destination,999,2.1f);ui.input(PointerPhase::end,destination,999,2.2f);bool mapped=false;for(const auto& e:host.events)if(e.button.type==ui::ButtonEventType::move&&e.dropCoordinates){assert(near(e.dropCoordinates->inventory.x,408)&&near(e.dropCoordinates->inventory.y,192));mapped=true;}assert(mapped);
 tap(9204);assert(ui.button(key(6100))->visible&&!ui.button(key(2200))->visible&&!ui.button(key(2223))->visible);
 for(const auto pair:{std::pair{6100u,Vec2{188,388}},std::pair{6302u,Vec2{326,270}}}){const auto from=ui.buttonScreenPosition(key(6024)),to=ui.buttonScreenPosition(key(pair.first));host.events.clear();ui.input(PointerPhase::begin,from,1001,3);ui.input(PointerPhase::move,to,1001,3.1f);ui.input(PointerPhase::end,to,1001,3.2f);mapped=false;for(const auto& e:host.events)if(e.button.type==ui::ButtonEventType::move&&e.dropCoordinates){assert(near(e.dropCoordinates->inventory.x,pair.second.x)&&near(e.dropCoordinates->inventory.y,pair.second.y));mapped=true;}assert(mapped);}
 tap(9205);assert(ui.button(key(2223))->visible&&!ui.button(key(6100))->visible);
 }
 std::cout<<"PHONE_MENU_GRIDS original16/20+HD8/5/all49directHits/noScroll/HDdragInverse PASS\n";
}
void phoneCommandPages(Files& files){
 const auto config=DisplayConfiguration::surface({667,375});const menu::PhoneLayout geometry(config);Host host;PreparedTiles tiles("phone-command-pages");ui::Screens screens(files,tiles,host);screens.setDisplay(config);ui::ScreensState state;state.screen=ui::ScreenId::command;state.locale="kr";
 for(unsigned i=0;i<5;++i){ui::ItemView entry;entry.dataId="new"+std::to_string(i);entry.name="새 임무 "+std::to_string(i);state.availableMissions.push_back(entry);entry.dataId="own"+std::to_string(i);entry.name="진행 임무 "+std::to_string(i);entry.did=3;entry.total=10;state.acceptedMissions.push_back(entry);}
 state.selectedMission=0;state.selectedMissionDataId="new0";for(unsigned n=0;n<600;++n)state.missionBody+="가";screens.refresh(state,0);
 const auto key=[&](unsigned id){for(const auto& k:screens.buttons())if(k.id==id)return k;throw std::runtime_error("Missing command page button "+std::to_string(id));};
 const auto tap=[&](unsigned id){const auto k=key(id);const auto p=screens.buttonScreenPosition(k);assert(p.x>=0&&p.x<=config.canvas.width&&p.y>=0&&p.y<=config.canvas.height);screens.input(PointerPhase::begin,p,30000+id,1);screens.input(PointerPhase::end,p,30000+id,1.1f);};
 for(unsigned first:{5100u,5200u}){for(unsigned i=0;i<4;++i){host.events.clear();tap(first+i);assert(!host.events.empty()&&host.events.back().button.key.dataId==(first==5100?"new":"own")+std::to_string(i));}tap(first==5100?9401:9403);host.events.clear();tap(first+4);assert(!host.events.empty()&&host.events.back().button.key.dataId==(first==5100?"new4":"own4"));}
 const auto lastText=[&](std::string_view id){for(auto it=host.texts.rbegin();it!=host.texts.rend();++it)if(it->sourceId==id)return it->text;throw std::runtime_error("Missing paged command text");};
 const auto body=lastText("MissionInfo.body");tap(9405);const auto next=lastText("Command.bodyPage");assert(next.starts_with("2 / ")&&!lastText("MissionInfo.body").empty()&&!body.empty());tap(9404);assert(lastText("Command.bodyPage").starts_with("1 / "));
 host.events.clear();tap(5004);assert(!host.events.empty()&&host.events.back().button.key.dataId=="new0");
 Frame fixed;screens.append(fixed,1);for(const auto& image:fixed.sprites)if(image.image=="MissionInfo.png"){const auto b=quadBounds(image);const auto raw=imageInfo(files.read(image.image));assert(near(b.width/b.height,float(raw.width)/float(raw.height)));assert(b.x>=geometry.picture.x&&b.y>=geometry.picture.y&&b.x+b.width<=geometry.picture.x+geometry.picture.width+.01f&&b.y+b.height<=geometry.picture.y+geometry.picture.height+.01f);}
 const auto point=screens.buttonScreenPosition(key(5004));screens.input(PointerPhase::begin,{config.canvas.width*.5f,100},65500,2);screens.input(PointerPhase::move,{config.canvas.width*.5f,350},65500,2.1f);screens.input(PointerPhase::end,{config.canvas.width*.5f,350},65500,2.2f);const auto after=screens.buttonScreenPosition(key(5004));assert(point.x==after.x&&point.y==after.y);
 state.commandListVisible=false;state.completedMission.dataId="done";state.completedMission.title="임무 완료";for(unsigned i=0;i<9;++i)state.completedMission.rewards.push_back({1,int(100+i),"Icon10011.png"});screens.refresh(state,3);
 for(unsigned page=0;page<3;++page){for(unsigned i=page*4;i<std::min(9u,page*4+4);++i)assert(lastText("Command.completed.done.reward"+std::to_string(i))==std::to_string(100+i));if(page<2)tap(9407);}
 Frame completion;screens.append(completion,3);for(const auto& image:completion.sprites)if(image.image=="MissionCompletedBG.png"){const auto b=quadBounds(image);const auto raw=imageInfo(files.read(image.image));assert(near(b.width/b.height,float(raw.width)/float(raw.height)));}host.events.clear();tap(5007);assert(!host.events.empty()&&host.events.back().button.key.dataId=="done");
 std::cout<<"PHONE_COMMAND_PAGES 5+5rawMissionIDs/details/accept/9rewards/complete/noScroll PASS\n";
}
void phoneLiveMenuSession(Files& files){
 Host host;DarwinWorldRandom random(1);SaveData saved;saved.initializeSlot(0,"Menus","kr",1794268800);progression::SlotView slot(saved.slot(0));slot.string(0x2d,16,"Base001");slot.u32(0x1b5c,5);slot.string(0x8c+8*6,6,"01001");slot.byte(0x53c+8,3);
 GameSession game(files,host,random,std::move(saved));PreparedTiles tiles("live-phone-menu");ui::Screens ui(files,tiles,host);host.game=&game;host.screens=&ui;const auto phone=DisplayConfiguration::surface({912,420},{68,20,68,0});game.setDisplay(phone);ui.setDisplay(phone);assert(game.selectSlot(0));game.enterBase();
 const auto settle=[&]{for(unsigned n=0;game.screenTransition().phase!=8&&n<250;++n){game.tick();(void)game.drawWorld();}assert(game.screenTransition().phase==8);ui.refresh(game.state(),game.renderTime());};settle();
 const auto key=[&](unsigned id){for(const auto& k:ui.buttons())if(k.id==id)return k;throw std::runtime_error("Missing live phone menu button "+std::to_string(id));};
 const auto has=[&](unsigned id){for(const auto& k:ui.buttons())if(k.id==id)return true;return false;};
 const auto tap=[&](unsigned id){auto k=key(id);ui.revealButton(k);const auto p=ui.buttonScreenPosition(k);ui.input(PointerPhase::begin,p,20000+id,game.renderTime());ui.input(PointerPhase::end,p,20000+id,game.renderTime());};
 tap(2002);tap(2002);settle();assert(game.state().screen==ui::ScreenId::shop);const auto writes=host.writes;const Bytes before(game.workingSlot().begin(),game.workingSlot().end());
 tap(2200);assert(game.state().shopDetailVisible&&game.state().selectedShop==0);for(unsigned n=0;n<30;++n){game.tick();ui.tick(game.renderTime(),random);}Frame popup;ui.append(popup,game.renderTime());bool panel=false;for(const auto& image:popup.sprites)if(image.image=="PhoneMenu/ItemDesc.png"){const auto bounds=quadBounds(image),area=phone.content();assert(bounds.x>=area.x-.01f&&bounds.y>=area.y-.01f&&bounds.x+bounds.width<=area.x+area.width+.01f&&bounds.y+bounds.height<=area.y+area.height+.01f);panel=true;}assert(panel);const auto verifyAction=[&](unsigned id){Frame f;ui.append(f,game.renderTime());std::optional<ClipRect> panel,action;for(const auto& image:f.sprites){if(image.image=="PhoneMenu/ItemDesc.png")panel=quadBounds(image);if(image.image==(id==2102?"Common_Btn_BUY.png":"Common_Btn_SELL.png"))action=quadBounds(image);}assert(panel&&action);std::cerr<<"TRADE_BOUNDS id="<<id<<" panel="<<panel->x<<","<<panel->y<<","<<panel->width<<","<<panel->height<<" action="<<action->x<<","<<action->y<<","<<action->width<<","<<action->height<<std::endl;assert(action->x+action->width<=panel->x||panel->x+panel->width<=action->x||action->y+action->height<=panel->y||panel->y+panel->height<=action->y);const auto a=phone.content();assert(action->x>=a.x-.01f&&action->y>=a.y-.01f&&action->x+action->width<=a.x+a.width+.01f&&action->y+action->height<=a.y+a.height+.01f);const auto point=ui.buttonScreenPosition(key(id));assert(near(point.x,action->x+action->width*.5f)&&near(point.y,action->y+action->height*.5f));};verifyAction(2102);tap(9204);assert(!game.state().shopDetailVisible&&game.state().selectedShop==-1&&game.state().detailInventory==-1&&!has(2102));
 tap(6008);assert(game.state().detailInventory==8&&has(2103));for(unsigned n=0;n<30;++n){game.tick();ui.tick(game.renderTime(),random);}verifyAction(2103);tap(9205);assert(game.state().detailInventory==-1&&!has(2103));
 // Open the original buy-confirmation flow, then attempt the new controls behind it.
 for(const auto& k:ui.buttons())if(k.id==2104&&k.intData==4){ui.revealButton(k);const auto p=ui.buttonScreenPosition(k);ui.input(PointerPhase::begin,p,777,game.renderTime());ui.input(PointerPhase::end,p,777,game.renderTime());break;}
 tap(2200);tap(2102);assert(game.state().tradeVisible);const auto first=ui.buttonScreenPosition(key(6000));const auto selected=game.state().selectedShop;const auto shopVisible=ui.button(key(2200))->visible;
 for(unsigned id:{9204u}){const auto p=ui.buttonScreenPosition(key(id));ui.input(PointerPhase::begin,p,888+id,game.renderTime());ui.input(PointerPhase::end,p,888+id,game.renderTime());}
 assert(game.state().tradeVisible&&game.state().selectedShop==selected&&ui.button(key(2200))->visible==shopVisible);const auto after=ui.buttonScreenPosition(key(6000));assert(near(first.x,after.x)&&near(first.y,after.y));tap(2107);assert(!game.state().tradeVisible);
 assert(host.writes==writes&&Bytes(game.workingSlot().begin(),game.workingSlot().end())==before);
 // Actual source callbacks buy and sell the original item; no success injection.
 const auto count=[&](std::string_view id){int count=0;for(const auto& item:game.state().inventory)if(item.dataId==id)count+=item.count;return count;};
 const auto previousCount=count("01001"),credit=game.state().credits;const auto equipmentBefore=game.state().equipment;
 tap(2200);assert(game.state().shopItems[0].dataId=="01001");tap(2102);assert(game.state().tradeVisible&&game.state().tradeItemId=="01001"&&game.state().tradeCount==1);const auto buyPrice=game.state().tradePrice;tap(2108);
 assert(!game.state().tradeVisible&&game.state().credits==credit-buyPrice&&count("01001")==previousCount+1);
 unsigned sellSlot=0;for(;sellSlot<25;++sellSlot)if(game.state().inventory[sellSlot].dataId=="01001"&&game.state().inventory[sellSlot].count>0&&!game.state().inventory[sellSlot].equipped)break;assert(sellSlot<25);
 const auto absolute=game.state().inventory[sellSlot].position;tap(6000+sellSlot);assert(game.state().detailInventory==int(sellSlot)&&game.state().detailItem.dataId=="01001");tap(2103);assert(game.state().tradeVisible&&game.state().tradeItemId=="01001");
 while(game.state().tradeCount>1)tap(2110);const auto sellPrice=game.state().tradePrice;tap(2109);assert(!game.state().tradeVisible&&game.state().credits==credit-buyPrice+sellPrice&&count("01001")==previousCount);assert(game.state().inventory[sellSlot].dataId=="01001"&&game.state().inventory[sellSlot].position==absolute);
 for(unsigned i=0;i<6;++i)assert(game.state().equipment[i].dataId==equipmentBefore[i].dataId);assert(host.writes==writes);
 // A reused button must restore the HD snap contract across both surface profiles.
 tap(6008);for(unsigned n=0;n<3;++n){game.tick();ui.tick(game.renderTime(),random);}const auto current=game.state();ui.setDisplay(DisplayConfiguration::surface({1024,1366}));for(auto k:ui.buttons())assert(ui.button(k)->positionSnap==ui::PositionSnap::none);ui.setDisplay(phone);for(auto k:ui.buttons())assert(ui.button(k)->positionSnap==ui::PositionSnap::none);
 // The original bottom controls are accessible without moving the illustration.
 ui.input(PointerPhase::begin,{20,200},33445,game.renderTime());ui.input(PointerPhase::move,{20,340},33445,game.renderTime());ui.input(PointerPhase::end,{20,340},33445,game.renderTime());const auto exit=ui.buttonScreenPosition(key(2100));assert(exit.y>0&&exit.y<phone.canvas.height);ui.input(PointerPhase::begin,exit,33446,game.renderTime());ui.input(PointerPhase::end,exit,33446,game.renderTime());settle();assert(game.state().screen==ui::ScreenId::base);
 std::cout<<"PHONE_LIVE_MENU SessionCallbacks/detailPushClose/tradeModalBlocksTabs/buyAndSellRawIDAndCredits/surfaceReuse/noPerTradeWrites PASS\n";
}
void session(Files& files,DisplayConfiguration config){Host host;DarwinWorldRandom random(1);GameSession game(files,host,random);assert(game.createSlot(0,"Layout","kr"));assert(game.enterField("Stage01_01",0));game.setDisplay(config);auto* scene=game.battle();assert(scene);const auto before=Bytes(game.workingSlot().begin(),game.workingSlot().end());const auto writes=host.writes;const auto old=scene->player().position;auto logical=scene->camera().toScreen({old.x,old.y+100});const auto point=game.worldCanvasPoint(logical);assert(game.inputWorld(PointerPhase::begin,point,77));game.inputWorld(PointerPhase::end,point,77);assert(near(scene->player().destination.x,old.x)&&near(scene->player().destination.y,old.y+100));assert(game.inputWorld(PointerPhase::begin,point,78));game.inputWorld(PointerPhase::end,{point.x,0},78);assert(scene->dashState().movePointer==0);assert(game.inputWorld(PointerPhase::begin,point,79));game.inputWorld(PointerPhase::cancel,{0,0},79);assert(scene->dashState().movePointer==0);
 if(config.compact&&config.insets.left>0){const Vec2 edge{config.insets.left*.5f,config.canvas.height*.75f};assert(game.inputWorld(PointerPhase::begin,edge,880));game.inputWorld(PointerPhase::end,edge,880);}
 const auto initialZoom=scene->cameraPresentation().worldScale;
 for(unsigned i=0;i<100;++i){game.tick();auto frame=game.drawWorld();assert(frame.width==config.canvas.width&&frame.height==config.canvas.height);game.appendOverlays(frame);}assert(host.writes==writes);const auto z=scene->cameraPresentation().worldScale;const auto minimum=std::min(1.f,std::max(config.canvas.width,config.canvas.height)/1022.f);assert(config.focusZoom?(z>=minimum&&z<=1.f):z==1);
 if(config.focusZoom&&config.canvas.width==912){std::cout<<"PHONE_FOCUS_ZOOM initial="<<initialZoom<<" after="<<z<<std::endl;assert(initialZoom==1&&z<=initialZoom);}
 const auto pos=scene->player().position;const auto hp=scene->playerHP();game.setDisplay(DisplayConfiguration::surface({1024,1366}));assert(scene->player().position.x==pos.x&&scene->playerHP()==hp&&host.writes==writes);assert(game.workingSlot().size()==before.size());
}
}
int main(int argc,char** argv){if(argc<2)return 77;phoneMenuGeometry();fullScreenWorld();transforms();Files files(argv[1]);entryCamera(files);allMapEdges(files);phoneArtwork(files);phoneMenus(files);phoneMenuGrids(files);phoneCommandPages(files);phoneLiveMenuSession(files);for(auto config:{DisplayConfiguration::surface({667,375}),DisplayConfiguration::surface({852,393},{59,21,59,0}),DisplayConfiguration::surface({912,420},{68,20,68,0}),DisplayConfiguration::surface({1024,1366},{0,20,0,24})}){screens(files,config);help(files,config);session(files,config);}std::cout<<"HM2_DISPLAY PASS 4 canvases / full-screen world / terrain edge coverage / HUD size / six base menus / Phone-original-art/contain-hotspots/Retina-JPEG / first mission rows / safe-area / inverse-hit / HD25+24+6 / drag / warm100 / help-scroll / common-world / no-display-save\n";}

#include "PreparedTextStub.hpp"
#include "Core/Session/GameSession.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include "Core/Presentation/MenuArtwork.hpp"
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
ClipRect quadBounds(const Sprite& s){assert(s.strip.size()==4);float l=s.strip[0].position.x,r=l,b=s.strip[0].position.y,t=b;for(const auto& v:s.strip){l=std::min(l,v.position.x);r=std::max(r,v.position.x);b=std::min(b,v.position.y);t=std::max(t,v.position.y);}return {l,b,r-l,t-b};}
void uniformCover(const Sprite& s,Size canvas){
 assert(s.strip.size()==4);const auto o=s.strip[0].position,a=s.strip[2].position,b=s.strip[1].position;const Vec2 x{a.x-o.x,a.y-o.y},y{b.x-o.x,b.y-o.y};const float xx=x.x*x.x+x.y*x.y,yy=y.x*y.x+y.y*y.y;
 assert(near(std::sqrt(xx)/std::sqrt(yy),1.5f));
 for(const auto p:{Vec2{0,0},Vec2{canvas.width,0},Vec2{0,canvas.height},Vec2{canvas.width,canvas.height}}){const Vec2 d{p.x-o.x,p.y-o.y};const float u=(d.x*x.x+d.y*x.y)/xx,v=(d.x*y.x+d.y*y.y)/yy;assert(u>=-.0001f&&u<=1.0001f&&v>=-.0001f&&v<=1.0001f);}
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
   if(s.image=="GameUI_GaugeBack.png"){const auto hud=DisplayLayout(config).ui(DisplayRegion::bottom);const auto bounds=config.compact?ClipRect{0,0,config.canvas.width,config.canvas.height}:area;const float left=std::max(bounds.x,hud.map({0,0}).x),right=std::min(bounds.x+bounds.width,hud.map({768,0}).x);if(s.clip->x<left-.01f)std::cerr<<"HUD_GAUGE_ESCAPE clipLeft="<<s.clip->x<<" hudLeft="<<left<<std::endl;assert(s.clip->x>=left-.01f&&s.clip->x+s.clip->width<=right+.01f);}
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
   if(sprite.image=="BaseTextBG.png"){description=true;const auto left=sprite.center.x-sprite.size.width*.5f,bottom=sprite.center.y-sprite.size.height*.5f,top=sprite.center.y+sprite.size.height*.5f;assert(left>=area.x&&bottom>=area.y&&top<=area.y+area.height);}
  }
  assert(background&&description);
  const auto original=DataTable::parse(files.read("PhoneMenu/"+state.baseId+".tble"));const auto hd=DataTable::parse(files.read(state.baseId+".tble"));unsigned expected=0;
  for(std::size_t row=0;row<hd.rowCount();++row)expected+=hd.value(row,"Type")=="Button";
  assert(screens.buttons().size()==expected);
  // Phone artwork, highlight and facilities retain their original relative
  // positions. Pan the whole illustration to reveal cropped top/bottom roads.
  for(const auto& key:screens.buttons()){
   std::size_t row=0;for(;row<original.rowCount();++row)if(original.value(row,"Type")=="Button"&&original.integer(row,"ID")==int(key.id)-2000)break;assert(row<original.rowCount());
   const auto before=screens.buttonScreenPosition(key);const auto originalTransform=DisplayLayout(config).ui(DisplayRegion::artScene);
   assert(near(before.x,originalTransform.map({240+original.real(row,"X"),160}).x));
   screens.revealButton(key);const auto point=screens.buttonScreenPosition(key);assert(point.x>=0&&point.x<=config.canvas.width&&point.y>=0&&point.y<=config.canvas.height);
   host.events.clear();screens.input(PointerPhase::begin,point,701,15);screens.input(PointerPhase::end,point,701,15);assert(!host.events.empty()&&host.events.front().button.key==key);
  }
  // A user drag moves image and button together without activating a facility.
  const auto key=screens.buttons().front();host.events.clear();const Vec2 gesture{config.canvas.width*.5f,config.canvas.height*.5f};
  screens.input(PointerPhase::begin,gesture,702,16);screens.input(PointerPhase::move,{gesture.x,gesture.y+1000},702,16.1f);screens.input(PointerPhase::end,{gesture.x,gesture.y+1000},702,16.2f);assert(std::none_of(host.events.begin(),host.events.end(),[](const auto& e){return e.button.type==ui::ButtonEventType::pop;}));
  const auto top=screens.buttonScreenPosition(key);Frame upper;screens.append(upper,16.2f);const Sprite* bg=nullptr;for(const auto& image:upper.sprites)if(image.image=="PhoneMenu/"+state.baseId+".jpg")bg=&image;assert(bg&&near(quadBounds(*bg).y,0));
  screens.input(PointerPhase::begin,gesture,703,17);screens.input(PointerPhase::move,{gesture.x,gesture.y-1000},703,17.1f);screens.input(PointerPhase::end,{gesture.x,gesture.y-1000},703,17.2f);const auto bottom=screens.buttonScreenPosition(key);assert(top.y>bottom.y);
  Frame lower;screens.append(lower,17.2f);for(const auto& image:lower.sprites)if(image.image=="PhoneMenu/"+state.baseId+".jpg")assert(near(quadBounds(image).y+quadBounds(image).height,config.canvas.height));
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
void phoneArtwork(Files& files){
 assert(validResourceName("PhoneMenu/Main_BG.png"));for(const auto bad:{"/Main_BG.png","../Main_BG.png","PhoneMenu/../Main_BG.png","PhoneMenu/sub/Main_BG.png","PhoneMenu/..","PhoneMenu/","PhoneMenu\\Main_BG.png"})assert(!validResourceName(bad));assert(!validResourceName(std::string_view("PhoneMenu/a\0.png",16)));
 const auto config=DisplayConfiguration::surface({912,420},{68,20,68,0});Host host;PreparedTiles tiles("phone-art");ui::Screens ui(files,tiles,host);ui.setDisplay(config);DarwinWorldRandom random(1);
 for(const auto screen:{ui::ScreenId::title,ui::ScreenId::slots,ui::ScreenId::command,ui::ScreenId::shop,ui::ScreenId::maintenance}){
  ui::ScreensState state;state.screen=screen;ui.refresh(state,0);Frame prep;ui.appendPreparation(prep);const std::string name=screen==ui::ScreenId::title?"PhoneMenu/Main_BG.png":screen==ui::ScreenId::slots?"PhoneMenu/SelSlot_BG.jpg":screen==ui::ScreenId::command?"PhoneMenu/CmdCenterBG.jpg":screen==ui::ScreenId::shop?"PhoneMenu/ShopBG.jpg":"PhoneMenu/MaintenanceBG.jpg";bool found=false;
  for(const auto& image:prep.sprites)if(image.image==name){found=true;const auto info=imageInfo(files.read(name));assert(info.width==(screen==ui::ScreenId::title?480u:960u)&&info.height==(screen==ui::ScreenId::title?320u:640u));assert(decodeImage(files.read(name)).width==info.width);}assert(found);
  for(unsigned tick=0;tick<800;++tick){ui.tick(float(tick)/60,random);Frame f;ui.append(f,float(tick)/60);for(const auto& image:f.sprites)if(image.image==name){uniformCover(image,config.canvas);}}
 }
 MapLoading loading(files,nullptr,true);const auto prep=loading.preparationFrame();assert(prep.sprites.size()==1&&prep.sprites[0].image=="PhoneMenu/StageClearBG.png"&&loading.artworkViewport().width==480);
}
void session(Files& files,DisplayConfiguration config){Host host;DarwinWorldRandom random(1);GameSession game(files,host,random);assert(game.createSlot(0,"Layout","kr"));assert(game.enterField("Stage01_01",0));game.setDisplay(config);auto* scene=game.battle();assert(scene);const auto before=Bytes(game.workingSlot().begin(),game.workingSlot().end());const auto writes=host.writes;const auto old=scene->player().position;auto logical=scene->camera().toScreen({old.x,old.y+100});const auto point=game.worldCanvasPoint(logical);assert(game.inputWorld(PointerPhase::begin,point,77));game.inputWorld(PointerPhase::end,point,77);assert(near(scene->player().destination.x,old.x)&&near(scene->player().destination.y,old.y+100));assert(game.inputWorld(PointerPhase::begin,point,78));game.inputWorld(PointerPhase::end,{point.x,0},78);assert(scene->dashState().movePointer==0);assert(game.inputWorld(PointerPhase::begin,point,79));game.inputWorld(PointerPhase::cancel,{0,0},79);assert(scene->dashState().movePointer==0);
 if(config.compact&&config.insets.left>0){const Vec2 edge{config.insets.left*.5f,config.canvas.height*.75f};assert(game.inputWorld(PointerPhase::begin,edge,880));game.inputWorld(PointerPhase::end,edge,880);}
 const auto initialZoom=scene->cameraPresentation().worldScale;
 for(unsigned i=0;i<100;++i){game.tick();auto frame=game.drawWorld();assert(frame.width==config.canvas.width&&frame.height==config.canvas.height);game.appendOverlays(frame);}assert(host.writes==writes);const auto z=scene->cameraPresentation().worldScale;const auto minimum=std::min(1.f,std::max(config.canvas.width,config.canvas.height)/1022.f);assert(config.focusZoom?(z>=minimum&&z<=1.f):z==1);
 if(config.focusZoom&&config.canvas.width==912){std::cout<<"PHONE_FOCUS_ZOOM initial="<<initialZoom<<" after="<<z<<std::endl;assert(initialZoom==1&&z<=initialZoom);}
 const auto pos=scene->player().position;const auto hp=scene->playerHP();game.setDisplay(DisplayConfiguration::surface({1024,1366}));assert(scene->player().position.x==pos.x&&scene->playerHP()==hp&&host.writes==writes);assert(game.workingSlot().size()==before.size());
}
}
int main(int argc,char** argv){if(argc<2)return 77;fullScreenWorld();transforms();Files files(argv[1]);entryCamera(files);allMapEdges(files);phoneArtwork(files);phoneMenus(files);for(auto config:{DisplayConfiguration::surface({667,375}),DisplayConfiguration::surface({852,393},{59,21,59,0}),DisplayConfiguration::surface({912,420},{68,20,68,0}),DisplayConfiguration::surface({1024,1366},{0,20,0,24})}){screens(files,config);help(files,config);session(files,config);}std::cout<<"HM2_DISPLAY PASS 4 canvases / full-screen world / terrain edge coverage / HUD size / six base menus / Phone-original-art/hotspot-pan/Retina-JPEG / first mission rows / safe-area / inverse-hit / HD25+24+6 / drag / warm100 / help-scroll / common-world / no-display-save\n";}

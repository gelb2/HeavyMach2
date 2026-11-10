#include "Core/Session/GameSession.hpp"
#include "Core/Progression/Progression.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace hm2;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;std::map<std::string,Bytes,std::less<>> cache;unsigned reads=0;mutable unsigned probes=0;
    explicit Files(const char* p):root(p){}
    Bytes read(std::string_view name)override{++reads;auto found=cache.find(name);if(found!=cache.end())return found->second;std::ifstream f(root/std::string(name),std::ios::binary);if(!f)throw std::runtime_error("Missing original Dash Session asset "+std::string(name));Bytes b{std::istreambuf_iterator<char>(f),{}};cache.emplace(std::string(name),b);return b;}
    bool exists(std::string_view name)const override{++probes;return cache.contains(name)||std::filesystem::exists(root/std::string(name));}
};
struct Host:SessionHost {
    unsigned saves=0;std::vector<unsigned> sounds;
    void sound(std::string_view,Vec2)override{}void soundID(unsigned id,Vec2)override{sounds.push_back(id);}
    void alert(std::string_view)override{}void requestName(unsigned)override{}void requestDelete(unsigned)override{}
    bool writeSave(std::span<const std::uint8_t>,SaveReason)override{++saves;return true;}
    std::uint32_t absoluteTimeLow()override{return 155;}std::uint32_t unixTime()override{return 1794268800;}
};
SaveData fixture(std::string_view foot="12001"){
    SaveData data;data.initializeSlot(0,"DASH","us",1794268800);progression::SlotView slot(data.slot(0));
    slot.string(0x8c+1*6,6,foot);return data; // Real equipped original inventory position1.
}
Frame step(GameSession& game){game.tick();return game.drawWorld();}
void steps(GameSession& game,unsigned n){for(unsigned i=0;i<n;++i)(void)step(game);}
Vec2 originalWorld(const MapCamera& c,Vec2 screen){const Vec2 b{c.center.x-c.viewport.width*.5f,c.center.y-c.viewport.height*.5f};const Vec2 o{b.x+c.offset.x,b.y+c.offset.y};return {o.x+screen.x,o.y+screen.y};}
Vec2 ground(GameSession& game){
    auto* scene=game.battle();const auto p=scene->player().position;const auto enemies=scene->enemies();
    constexpr Vec2 offsets[]={{100,0},{100,60},{-100,0},{0,100},{60,100},{-80,80}};
    for(const auto d:offsets){const auto screen=scene->camera().toScreen({p.x+d.x,p.y+d.y});if(screen.x<0||screen.x>768||screen.y<100||screen.y>1024)continue;
        const auto world=originalWorld(scene->camera(),screen);bool clear=true;for(const auto& e:enemies)if(e.hp>0){const float x=e.position.x-world.x,y=e.position.y-world.y;if(x*x+y*y<10000)clear=false;}if(clear)return screen;
    }throw std::runtime_error("No valid original ground target in Dash fixture");
}
void tap(GameSession& game,Vec2 p,std::uintptr_t id){assert(game.inputWorld(PointerPhase::begin,p,id));(void)game.inputWorld(PointerPhase::end,p,id);}
std::size_t imageCount(const Frame& frame,std::string_view name){std::size_t total=0;for(const auto& s:frame.sprites)if(s.image==name)total+=s.atlasBatch?s.atlasBatch->quads.size():1;return total;}
bool near(float a,float b){return std::abs(a-b)<.0001f;}
void actualSP(Files& files){
    Host host;DarwinWorldRandom rng(7);GameSession game(files,host,rng,fixture());assert(game.selectSlot(0)&&game.enterField("Stage01_01",0));(void)game.state();
    const auto reads=files.reads,probes=files.probes,saves=host.saves;steps(game,99);auto* scene=game.battle();assert(scene->playerHP()>0&&scene->dashState().special==99&&scene->metrics().playerEmissions==0);
    auto target=ground(game);tap(game,target,17);tap(game,target,18);assert(scene->metrics().dashStarts==0&&scene->dashState().special==99);
    (void)step(game);assert(scene->dashState().special==100);target=ground(game);const auto world=originalWorld(scene->camera(),target),player=scene->player().position;
    const float dx=world.x-player.x,dy=world.y-player.y,length=std::sqrt(dx*dx+dy*dy);const Vec2 direction{dx/length,dy/length};
    const auto randomCalls=rng.calls();const auto sfx=std::count(host.sounds.begin(),host.sounds.end(),14u);const float tapTime=scene->dashState().tapTime;
    assert(game.inputWorld(PointerPhase::begin,target,19));auto dash=scene->dashState();assert(scene->metrics().dashStarts==1&&dash.special==0&&dash.tapTime==tapTime&&dash.movePointer==0&&dash.pathCount>0);
    assert(dash.velocity.x==direction.x&&dash.velocity.y==direction.y&&rng.calls()==randomCalls&&std::count(host.sounds.begin(),host.sounds.end(),14u)==sfx+1);
    const auto old=scene->player();BipodMotion ordinary(old);const int wall=dash.currentTerrain;ordinary.tick(wall<0?nullptr:&scene->terrain().edges().at(std::size_t(wall)));
    const auto normal=ordinary.state().position;const Vec2 raw{normal.x+direction.x*10,normal.y+direction.y*10};const auto corrected=scene->terrain().resolve(raw,-1);
    const auto first=step(game);dash=scene->dashState();
    assert(near(scene->player().position.x,corrected.position.x)&&near(scene->player().position.y,corrected.position.y));
    assert(dash.currentTerrain==corrected.current&&dash.lastEffectPosition.x==normal.x&&dash.lastEffectPosition.y==normal.y);
    assert(dash.velocity.x==direction.x+(0-direction.x)/8&&dash.velocity.y==direction.y+(0-direction.y)/8);
    assert(scene->metrics().dashMoveTicks==1&&scene->metrics().dashParticleAllocations==1&&scene->metrics().dashRandomCalls==1&&imageCount(first,"Efx_Dash.png")==0);
    const auto second=step(game);assert(imageCount(second,"Efx_Dash.png")>0);assert(scene->dashState().special==2);
    const auto beforePause=scene->dashState();const auto worldTime=scene->clock().worldTime();const auto moveTicks=scene->metrics().dashMoveTicks;
    scene->setPaused(true);steps(game,3);assert(scene->clock().worldTime()==worldTime&&scene->metrics().dashMoveTicks==moveTicks);
    assert(scene->dashState().velocity.x==beforePause.velocity.x&&scene->dashState().velocity.y==beforePause.velocity.y&&scene->dashState().special==beforePause.special+3);
    assert(!game.inputWorld(PointerPhase::begin,target,20));scene->setPaused(false);
    const auto frozen=scene->dashState();const auto inactiveRandom=rng.calls();game.setActive(false);game.tick();assert(scene->dashState().special==frozen.special&&scene->dashState().velocity.x==frozen.velocity.x&&rng.calls()==inactiveRandom);game.setActive(true);
    steps(game,50);assert(scene->dashState().velocity.x==0&&scene->dashState().velocity.y==0&&scene->metrics().dashMoveTicks==36&&scene->metrics().playerEmissions==0);
    assert(files.reads==reads&&files.probes==probes&&host.saves==saves);
    std::cout<<"HM2_DASH_SESSION actualSP99reject/100consume callbackSFX14 RNG0; baseMove->Dash10->terrain; effectPool/RNG1 nextFrameGPUQuad; pauseRegen/dashStop inactiveStop/end36 warmIO0 save0 PASS\n";
}
void timing(Files& files,unsigned gap){
    Host host;DarwinWorldRandom rng(7);GameSession game(files,host,rng,fixture());assert(game.selectSlot(0)&&game.enterField("Stage01_01",0));(void)game.state();steps(game,100);auto* scene=game.battle();
    tap(game,ground(game),31);const float before=scene->dashState().tapTime;assert(before!=0&&scene->metrics().dashStarts==0);steps(game,gap);
    const double elapsed=scene->clock().worldTime()-double(before);const float sp=scene->dashState().special;const auto sounds=std::count(host.sounds.begin(),host.sounds.end(),14u);tap(game,ground(game),32);
    assert(scene->metrics().playerEmissions==0);if(gap==30){assert(elapsed<.5&&scene->metrics().dashStarts==1&&scene->dashState().special==sp-100&&scene->dashState().tapTime==before&&scene->dashState().movePointer==0&&scene->dashState().pathCount>0);assert(std::count(host.sounds.begin(),host.sounds.end(),14u)==sounds+1);}
    else{assert(elapsed>.5&&scene->metrics().dashStarts==0&&scene->dashState().special==sp&&scene->dashState().tapTime==float(scene->clock().worldTime()));assert(std::count(host.sounds.begin(),host.sounds.end(),14u)==sounds);}
    std::cout<<"HM2_DASH_SESSION gap="<<gap<<" elapsed="<<elapsed<<" starts="<<scene->metrics().dashStarts<<" SP="<<scene->dashState().special<<" PASS\n";
}
void wallCollision(Files& files){
    Host host;DarwinWorldRandom rng(7);GameSession game(files,host,rng,fixture());assert(game.selectSlot(0)&&game.enterField("Stage01_01",0));(void)game.state();steps(game,100);auto* scene=game.battle();
    const auto start=scene->player().position;const auto enemies=scene->enemies();Vec2 target;int selectedEdge=-1;float nearest=10000;
    const auto& edges=scene->terrain().edges();
    for(std::size_t i=0;i<edges.size();++i){const auto& e=edges[i];if(e.length<40)continue;
        const float projection=std::clamp((start.x-e.origin.x)*e.dir.x+(start.y-e.origin.y)*e.dir.y,20.f,e.length-20.f);
        const Vec2 world{e.origin.x+e.dir.x*projection-e.normal.x*8,e.origin.y+e.dir.y*projection-e.normal.y*8};
        const float dx=world.x-start.x,dy=world.y-start.y,d=dx*dx+dy*dy;if(d<=900||d>=nearest||e.overlap(world,16)==0)continue;
        const auto screen=scene->camera().toScreen(world);if(screen.x<0||screen.x>768||screen.y<100||screen.y>1024)continue;
        bool clear=true;for(const auto& enemy:enemies)if(enemy.hp>0){const float x=enemy.position.x-world.x,y=enemy.position.y-world.y;if(x*x+y*y<10000)clear=false;}
        if(clear){nearest=d;target=screen;selectedEdge=int(i);}
    }
    assert(selectedEdge>=0);std::cout<<"HM2_DASH_SESSION wallTargetEdge="<<selectedEdge<<" distance="<<std::sqrt(nearest)<<"\n";
    tap(game,target,51);tap(game,target,52);assert(scene->metrics().dashStarts==1);unsigned corrections=0;
    for(unsigned i=0;i<40;++i){
        const auto before=scene->player();const auto dash=scene->dashState();BipodMotion ordinary(before);ordinary.tick(dash.currentTerrain<0?nullptr:&scene->terrain().edges().at(std::size_t(dash.currentTerrain)));
        const auto p=ordinary.state().position;const Vec2 raw{p.x+dash.velocity.x*10,p.y+dash.velocity.y*10};const auto resolved=scene->terrain().resolve(raw,-1);
        (void)step(game);assert(near(scene->player().position.x,resolved.position.x)&&near(scene->player().position.y,resolved.position.y));
        if((dash.velocity.x!=0||dash.velocity.y!=0)&&resolved.corrections)corrections+=resolved.corrections;
    }
    std::cout<<"HM2_DASH_SESSION actualWallCorrections="<<corrections<<" final="<<scene->player().position.x<<":"<<scene->player().position.y<<"\n";assert(corrections>0&&scene->metrics().playerEmissions==0);
}
void dragAndCapture(Files& files){
    Host host;DarwinWorldRandom rng(7);GameSession game(files,host,rng,fixture());assert(game.selectSlot(0)&&game.enterField("Stage01_01",0));steps(game,120);auto* scene=game.battle();
    const auto p=ground(game);assert(game.inputWorld(PointerPhase::begin,p,41));assert(scene->dashState().tapTime!=0&&scene->dashState().movePointer==41);
    assert(game.inputWorld(PointerPhase::move,p,41));assert(scene->dashState().tapTime==0);(void)game.inputWorld(PointerPhase::end,p,41);
    tap(game,p,42);assert(scene->metrics().dashStarts==0);const float timestamp=scene->dashState().tapTime;
    const auto playerScreen=scene->camera().toScreen(scene->player().position);tap(game,playerScreen,43);assert(scene->metrics().dashStarts==0&&scene->dashState().tapTime==timestamp);
    assert(!game.inputWorld(PointerPhase::begin,{300,99},44)&&scene->dashState().tapTime==timestamp);
    std::cout<<"HM2_DASH_SESSION ownedTinyDragClearsTime/playerCapture/bottomUIexcluded PASS\n";
}
}
int main(int argc,char** argv){if(argc<2)return 77;Files files(argv[1]);actualSP(files);timing(files,30);timing(files,31);wallCollision(files);dragAndCapture(files);}

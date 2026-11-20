#include "Core/UI/Help.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
using namespace hm2;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;unsigned reads=0;mutable unsigned probes=0;
    std::vector<std::string> readNames;mutable std::vector<std::string> probeNames;
    explicit Files(const char* p):root(p){}
    Bytes read(std::string_view name)override{++reads;readNames.emplace_back(name);std::ifstream f(root/std::string(name),std::ios::binary);if(!f)throw std::runtime_error("Missing original Help image "+std::string(name));return {std::istreambuf_iterator<char>(f),{}};}
    bool exists(std::string_view name)const override{++probes;probeNames.emplace_back(name);return std::filesystem::exists(root/std::string(name));}
};
Frame draw(const ui::Help& help){Frame frame;help.append(frame);return frame;}
void geometry(const Sprite& s){
    assert(s.center.x==384&&s.center.y==512&&s.strip.size()==4&&!s.atlasBatch);
    const Vec2 corners[]={{0,1024},{0,0},{768,1024},{768,0}};const Vec2 uv[]={{0,0},{0,1},{1,0},{1,1}};
    for(unsigned i=0;i<4;++i){assert(s.strip[i].position.x==corners[i].x&&s.strip[i].position.y==corners[i].y&&s.strip[i].uv.x==uv[i].x&&s.strip[i].uv.y==uv[i].y);assert(s.strip[i].color.r==1&&s.strip[i].color.g==1&&s.strip[i].color.b==1&&s.strip[i].color.a==1);}
    assert(s.blend.source==BlendFactor::one&&s.blend.destination==BlendFactor::oneMinusSourceAlpha);
}
void localeAndPages(Files& files,std::string locale){
    const auto before=files.reads;ui::Help help(files,locale,-1);assert(files.reads==before+4&&help.locale()==locale);
    assert((std::vector<std::string>(files.readNames.end()-4,files.readNames.end())==std::vector<std::string>{"HelpScr01_"+locale+".png","HelpScr02_"+locale+".png","HelpScr03_"+locale+".png","HelpHelp.png"}));
    assert(help.state().page==0&&help.state().changePages&&help.state().guidanceVisible&&!help.state().removed&&help.state().layer==13);
    const auto prep=help.preparationFrame();assert(prep.sprites.size()==4);for(const auto& image:prep.sprites)assert(image.size.width==768&&image.size.height==1024);
    for(int i=0;i<3;++i){help.setPage(i);const auto frame=draw(help);assert(frame.sprites.size()==2&&frame.sprites[0].image=="HelpScr0"+std::to_string(i+1)+"_"+locale+".png"&&frame.sprites[1].image=="HelpHelp.png");for(const auto& image:frame.sprites)geometry(image);}
    // First end only hides guidance, even on a close/next/previous coordinate.
    help.setPage(2);assert(help.input(PointerPhase::end,{204,-10000},0));assert(help.state().page==2&&!help.state().guidanceVisible&&!help.state().removed);
    assert(draw(help).sprites.size()==1);help.setPage(std::numeric_limits<int>::min());assert(help.state().page==0);help.setPage(std::numeric_limits<int>::max());assert(help.state().page==2);
    const auto reads=files.reads,probes=files.probes;
    for(unsigned i=0;i<1000;++i){help.setPage(int(i%3));assert(help.input(PointerPhase::begin,{0,0},0));assert(help.input(PointerPhase::move,{768,1024},1));assert(help.input(PointerPhase::cancel,{384,512},1));(void)draw(help);(void)help.preparationFrame();}
    assert(files.reads==reads&&files.probes==probes);
    std::array<VisualLayer,14> layers;help.append(layers);assert(layers[13].visualCount()==1&&layers[9].visualCount()==0);for(auto& l:layers)(void)l.drain();help.setLayer(9);help.append(layers);assert(layers[9].visualCount()==1&&layers[13].visualCount()==0);
    ui::Help other(files,locale,2);const auto name="HelpScr03_"+locale+".png";assert(help.tiles().visual(*help.tiles().lookup(name)).identity!=other.tiles().visual(*other.tiles().lookup(name)).identity);
    std::cout<<"HM2_HELP "<<locale<<" originalPages3+overlay768x1024/directGeometry/POTUV/ownManager/layers9+13/warmRead0 PASS\n";
}
void boundaries(Files& files){
    const float xs[]={std::nextafter(204.f,0.f),204.f,std::nextafter(204.f,1000.f),std::nextafter(564.f,0.f),564.f,std::nextafter(564.f,1000.f)};
    for(float x:xs){unsigned closed=0;ui::Help help(files,"us",-1,[&]{++closed;});help.setPage(1);
        for(auto phase:{PointerPhase::begin,PointerPhase::move,PointerPhase::cancel}){assert(help.input(phase,{x,-5000},0));assert(help.state().guidanceVisible&&help.state().page==1&&!closed);}
        assert(help.input(PointerPhase::end,{x,-5000},0));assert(!help.state().guidanceVisible&&help.state().page==1&&!closed);
        assert(help.input(PointerPhase::end,{x,5000},999));
        if(x<204)assert(help.state().page==0&&!closed&&!help.state().removed);else if(x<=564)assert(help.state().page==1&&closed==1&&help.state().removed&&draw(help).sprites.empty());else assert(help.state().page==2&&!closed&&!help.state().removed);
    }
    ui::Help clamp(files,"us",-1);clamp.input(PointerPhase::end,{0,0});for(unsigned i=0;i<5;++i)clamp.input(PointerPhase::end,{0,0});assert(clamp.state().page==0);for(unsigned i=0;i<5;++i)clamp.input(PointerPhase::end,{768,0});assert(clamp.state().page==2);
    for(int initial:{-2,0,1,2,99})for(float x:{-1000.f,204.f,564.f,2000.f}){const auto reads=files.reads;unsigned closed=0;ui::Help fixed(files,"kr",initial,[&]{++closed;});assert(files.reads==reads+3&&fixed.preparationFrame().sprites.size()==3&&!fixed.state().changePages&&!fixed.state().guidanceVisible&&fixed.state().page==std::clamp(initial,0,2));fixed.setLayer(9);assert(fixed.input(PointerPhase::end,{x,42}));assert(closed==1&&fixed.state().removed);for(unsigned i=0;i<5;++i)assert(fixed.input(PointerPhase::end,{x,42}));assert(closed==1);}
    std::cout<<"HM2_HELP strict204/564+nextafter/pageClamps/endOnly/allEventsConsumed/firstEventGuidanceOnly/fixedPageAnyX/onceClose PASS\n";
}
void desktopFit(Files& files){
    ui::Help help(files,"kr",0);
    auto display=DisplayConfiguration::surface({1200,768});
    display.device=DisplayDevice::desktop;
    help.setDisplay(display);
    const auto frame=draw(help);
    assert(frame.sprites.size()==1);
    const auto& vertices=frame.sprites.front().strip;
    assert(vertices.size()==4);
    assert(vertices[0].position.x==312&&vertices[0].position.y==768);
    assert(vertices[3].position.x==888&&vertices[3].position.y==0);
    assert(help.input(PointerPhase::end,{600,384},1)&&help.state().removed);
}
void lifetimeAndMissing(Files& files){
    ui::Help* borrowed=nullptr;unsigned closes=0;ui::Help reentrant(files,"us",2,[&]{assert(borrowed->state().removed&&draw(*borrowed).sprites.empty());++closes;assert(borrowed->input(PointerPhase::end,{384,512}));});borrowed=&reentrant;assert(reentrant.input(PointerPhase::end,{384,512})&&closes==1);
    std::unique_ptr<ui::Help> destroy;destroy=std::make_unique<ui::Help>(files,"kr",2,[&]{++closes;destroy.reset();});auto* raw=destroy.get();assert(raw->input(PointerPhase::end,{0,0})&&!destroy&&closes==2);
    const auto reads=files.reads,probes=files.probes;ui::Help missing(files,"unavailable-original-locale",2);assert(files.reads==reads&&files.probes==probes+3&&draw(missing).sprites.empty()&&missing.preparationFrame().sprites.empty());
    assert(files.probeNames[files.probeNames.size()-3]=="HelpScr01_unavailable-original-locale.png");assert(missing.input(PointerPhase::end,{384,512})&&missing.state().removed);
    // Disposing a different dedicated manager must not invalidate live Help.
    ui::Help alive(files,"us",2);{ui::Help temporary(files,"kr",-1);assert(draw(temporary).sprites.size()==2);}assert(draw(alive).sprites.at(0).image=="HelpScr03_us.png");
    std::cout<<"HM2_HELP reentrant/selfDestroyClose/originalNilLocale/noFallback/independentManagerLifetime PASS; actualSession/autoHelp/1029/nativeGPU pending\n";
}
}
int main(int argc,char** argv){if(argc<2)return 77;Files files(argv[1]);localeAndPages(files,"us");localeAndPages(files,"kr");boundaries(files);desktopFit(files);lifetimeAndMissing(files);}

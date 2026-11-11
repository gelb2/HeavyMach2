#include "Core/Presentation/MapLoading.hpp"
#include <cassert>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>
using namespace hm2;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;unsigned reads=0;mutable unsigned probes=0;
    explicit Files(const char* path):root(path){}
    Bytes read(std::string_view name)override{++reads;assert(name=="StageClearBG.png");std::ifstream file(root/std::string(name),std::ios::binary);assert(file);return {std::istreambuf_iterator<char>(file),{}};}
    bool exists(std::string_view name)const override{++probes;return std::filesystem::exists(root/std::string(name));}
};
bool exact(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
Frame draw(const MapLoading& loading){Frame frame;loading.append(frame);return frame;}
void geometry(const MapLoading& loading){
    const auto frame=draw(loading);assert(frame.sprites.size()==1);const auto& s=frame.sprites.front();
    assert(s.image=="StageClearBG.png"&&s.visualLayer==13&&!s.atlasBatch&&s.strip.size()==4&&s.center.x==384&&s.center.y==512);
    assert(s.blend.source==BlendFactor::one&&s.blend.destination==BlendFactor::oneMinusSourceAlpha);
    const Vec2 xy[]={{0,1024},{0,0},{768,1024},{768,0}},uv[]={{0,0},{0,1},{1,0},{1,1}};
    for(unsigned i=0;i<4;++i){assert(s.strip[i].position.x==xy[i].x&&s.strip[i].position.y==xy[i].y&&s.strip[i].uv.x==uv[i].x&&s.strip[i].uv.y==uv[i].y);assert(exact(s.strip[i].color.a,loading.state().alpha));}
}
void fullPath(Files& files){
    const auto reads=files.reads,probes=files.probes;MapLoading loading(files);assert(files.reads==reads+1&&files.probes==probes+1);
    assert(loading.state().step==0&&loading.state().alpha==0&&!loading.state().removed);geometry(loading);
    const auto prepared=loading.preparationFrame();assert(prepared.sprites.size()==1&&prepared.sprites[0].image=="StageClearBG.png"&&prepared.sprites[0].size.width==768&&prepared.sprites[0].size.height==1024);
    // Pure callback ordering model only. Actual Scene lifetime is verified by
    // ScreenTransitionSessionTests; this model never runs game physics.
    bool oldWorldAlive=true,oldTilesAlive=true,newWorldAlive=false;
    std::vector<int> events,steps{0};
    MapLoadingCallbacks callbacks{
        [&]{assert(loading.state().step==1&&loading.state().alpha==1&&oldWorldAlive&&oldTilesAlive);events.push_back(1);oldWorldAlive=false;},
        [&]{assert(loading.state().step==2&&!oldWorldAlive&&oldTilesAlive&&!newWorldAlive);events.push_back(2);oldTilesAlive=false;},
        [&]{assert(loading.state().step==4&&!oldWorldAlive&&!oldTilesAlive&&!newWorldAlive);events.push_back(4);newWorldAlive=true;}};
    const auto warmReads=files.reads,warmProbes=files.probes;float expected=0;unsigned frames=0,fadeIn=0,fadeOut=0;
    while(!loading.state().removed){
        const auto entry=loading.state().step;
        if(entry==0){assert(oldWorldAlive&&oldTilesAlive&&!newWorldAlive);expected+=(1.f-expected)/20.f;if(expected>.99f)expected=1;++fadeIn;}
        if(entry==5){assert(!oldWorldAlive&&!oldTilesAlive&&newWorldAlive);expected+=(0.f-expected)/20.f;if(expected<.01f)expected=0;++fadeOut;}
        loading.tick(callbacks);assert(exact(loading.state().alpha,expected));assert(++frames<300);
        if(steps.back()!=loading.state().step)steps.push_back(loading.state().step);
        if(!loading.state().removed)geometry(loading);else assert(draw(loading).sprites.empty());
        assert(files.reads==warmReads&&files.probes==warmProbes);
    }
    assert((events==std::vector<int>{1,2,4})&&(steps==std::vector<int>{0,1,2,4,5,6})&&fadeIn==90&&fadeOut==90&&frames==184);
    const auto final=loading.state();for(unsigned i=0;i<1000;++i){loading.tick(callbacks);assert(loading.state().step==final.step&&loading.state().alpha==0&&loading.state().removed);assert(draw(loading).sprites.empty());(void)loading.preparationFrame();}
    assert(files.reads==warmReads&&files.probes==warmProbes&&events.size()==3);
    std::cout<<"MAP_LOADING floatBits/fade90+90/steps0-1-2-4-5-6/callbackOrder/StageClearBG-layer13/worldCallbackLifetime/remove-nextTick/warmIO0 PASS frames="<<frames<<"\n";
}
void boundaries(Files& files){
    MapLoading loading(files);auto& fixture=const_cast<MapLoadingState&>(loading.state());
    // Boundary-state injection belongs only to this float-state unit test.
    for(float initial:{0.f,.98f,.99f,std::nextafter(.99f,1.f)}){
        fixture={0,initial,false};float expected=initial;expected+=(1.f-expected)/20.f;const bool finishes=expected>.99f;if(finishes)expected=1;
        loading.tick({});assert(exact(loading.state().alpha,expected)&&loading.state().step==(finishes?1:0));
    }
    for(float initial:{1.f,.011f,.01f,std::nextafter(.01f,0.f)}){
        fixture={5,initial,false};float expected=initial;expected+=(0.f-expected)/20.f;const bool finishes=expected<.01f;if(finishes)expected=0;
        loading.tick({});assert(exact(loading.state().alpha,expected)&&loading.state().step==(finishes?6:5));
    }
    for(int entry:{1,2,4}){
        fixture={entry,1,false};bool failed=false;try{loading.tick({});}catch(const std::logic_error&){failed=true;}assert(failed&&loading.state().step==entry&&!loading.state().removed);
        MapLoadingCallbacks fail{[]{throw std::runtime_error("clear objects failed");},[]{throw std::runtime_error("clear tiles failed");},[]{throw std::runtime_error("set stage failed");}};
        failed=false;try{loading.tick(fail);}catch(const std::runtime_error&){failed=true;}assert(failed&&loading.state().step==entry&&loading.state().alpha==1);
    }
    fixture={6,0,false};assert(!draw(loading).sprites.empty());loading.tick({});assert(loading.state().removed&&draw(loading).sprites.empty());
    struct Missing:ResourceAccess{unsigned reads=0;mutable unsigned probes=0;Bytes read(std::string_view)override{++reads;return{};}bool exists(std::string_view)const override{++probes;return false;}} missing;
    MapLoading nil(missing);assert(missing.probes==1&&missing.reads==0&&draw(nil).sprites.empty()&&nil.preparationFrame().sprites.empty());
    std::cout<<"MAP_LOADING strictFloatThresholds/callbackExceptionPreservesStep/noExtraStages/originalNil-noFallback PASS\n";
}
}
int main(int argc,char** argv){if(argc<2)return 77;Files files(argv[1]);fullPath(files);boundaries(files);}

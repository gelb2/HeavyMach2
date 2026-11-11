#include "Core/Presentation/ScreenTransition.hpp"
#include <algorithm>
#include <cassert>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
using namespace hm2;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;unsigned reads=0;mutable unsigned probes=0;
    std::vector<std::string> names;
    explicit Files(const char* path):root(path){}
    Bytes read(std::string_view name)override{
        ++reads;names.emplace_back(name);std::ifstream file(root/std::string(name),std::ios::binary);
        if(!file)throw std::runtime_error("Missing original loading image "+std::string(name));
        return {std::istreambuf_iterator<char>(file),{}};
    }
    bool exists(std::string_view name)const override{++probes;return std::filesystem::exists(root/std::string(name));}
};
constexpr std::array<std::string_view,5> names={"Loading_Left.png","Loading_Right.png","Loading_Top.png","Loading_Bottom.png","Loading_Center.png"};
bool equal(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
void equal(Vec2 a,Vec2 b){assert(equal(a.x,b.x)&&equal(a.y,b.y));}
void equal(const ScreenTransitionState& a,const ScreenTransitionState& b){
    assert(a.phase==b.phase&&a.requestedScreen==b.requestedScreen&&a.previousScreen==b.previousScreen&&a.visible==b.visible);
    equal(a.top,b.top);equal(a.bottom,b.bottom);equal(a.left,b.left);equal(a.right,b.right);equal(a.centerScale,b.centerScale);assert(equal(a.centerAngle,b.centerAngle));
}
// Test-only injection through a const view of this non-const object. Production
// has no phase setter; phase6's external entry remains unproven in the source.
ScreenTransitionState& fixture(ScreenTransition& transition){return const_cast<ScreenTransitionState&>(transition.state());}
Frame draw(const ScreenTransition& transition){Frame frame;transition.append(frame);return frame;}
ScreenTransitionState closed(int phase){
    ScreenTransitionState state;state.phase=phase;state.requestedScreen=3;state.previousScreen=1;state.visible=true;
    state.top={384,640};state.bottom={384,384};state.left={256,512};state.right={512,512};state.centerScale={1,1};return state;
}
// Independent transcription of the retained HD GameMain.tick branch operations.
// Check before easing, with float stores at the same operation boundaries.
void expectedTick(ScreenTransitionState& s){
    const auto move=[](float value,float target,float divisor){
        if(value==target)return target;
        const float difference=value-target;if(std::fabs(difference)<=.1f)return target;
        const float delta=(target-value)/divisor;return value+delta;
    };
    const int entry=s.phase;
    if(entry==0){s.visible=true;s.top={384,1408};s.bottom={384,-384};s.left={-256,512};s.right={1024,512};s.centerScale={1.5f,1.5f};s.centerAngle=0;s.phase=1;}
    else if(entry==1){const float dl=(256.f-s.left.x)/5.f,dr=(512.f-s.right.x)/5.f;s.left.x=s.left.x+dl;s.right.x=s.right.x+dr;if(s.left.x>=200)s.phase=2;}
    else if(entry==2){s.top.y=move(s.top.y,640,6);s.bottom.y=move(s.bottom.y,384,6);s.left.x=move(s.left.x,256,5);s.right.x=move(s.right.x,512,5);if(s.top.y==640){s.bottom.y=384;s.phase=3;}}
    else if(entry==3){const float change=(.9f-s.centerScale.x)/5.f;s.centerScale.x=s.centerScale.x+change;s.centerScale.y=s.centerScale.x;if(s.centerScale.x<1){s.centerScale={1,1};s.phase=4;}}
    else if(entry==4)s.phase=5;
    else if(entry==5){const float change=(1.5f-s.centerScale.x)/5.f;s.centerScale.x=s.centerScale.x+change;s.centerScale.y=s.centerScale.x;if(s.centerScale.x>1.4f)s.phase=7;}
    else if(entry==6){s.left.x=move(s.left.x,-256,10);s.right.x=move(s.right.x,1024,10);if(s.left.x<0)s.phase=7;}
    else if(entry==7){const float change=(1.5f-s.centerScale.x)/5.f;s.centerScale.x=s.centerScale.x+change;s.centerScale.y=s.centerScale.x;s.top.y=move(s.top.y,1408,10);s.bottom.y=move(s.bottom.y,-384,10);s.left.x=move(s.left.x,-256,10);s.right.x=move(s.right.x,1024,10);if(s.top.y==1408){s.centerScale={1.5f,1.5f};s.visible=false;s.phase=8;}}
}
void geometry(const ScreenTransition& transition){
    const auto& s=transition.state();const auto frame=draw(transition);assert(frame.sprites.size()==5);
    const auto prep=transition.preparationFrame();assert(prep.sprites.size()==5);
    const Vec2 positions[]={s.left,s.right,s.top,s.bottom,{s.bottom.x,s.bottom.y+168}};
    const Vec2 scales[]={{2,2},{2,2},{1.5f,1.5f},{1.5f,1.5f},s.centerScale};
    for(unsigned i=0;i<5;++i){
        const auto& sprite=frame.sprites[i];assert(sprite.image==names[i]&&sprite.visualLayer==13&&sprite.strip.size()==4&&!sprite.atlasBatch);equal(sprite.center,positions[i]);
        const auto original=std::find_if(prep.sprites.begin(),prep.sprites.end(),[&](const auto& image){return image.image==names[i];});assert(original!=prep.sprites.end());
        const float hw=(original->size.width*.5f)*scales[i].x,hh=(original->size.height*.5f)*scales[i].y;
        const Vec2 local[]={{-hw,hh},{-hw,-hh},{hw,hh},{hw,-hh}};const Vec2 uv[]={{0,0},{0,1},{1,0},{1,1}};
        const float c=std::cos(i==4?s.centerAngle:0.f),sn=std::sin(i==4?s.centerAngle:0.f);
        for(unsigned v=0;v<4;++v){equal(sprite.strip[v].position,{positions[i].x+(local[v].x*c-local[v].y*sn),positions[i].y+(local[v].x*sn+local[v].y*c)});equal(sprite.strip[v].uv,uv[v]);assert(sprite.strip[v].color.a==1);}
        assert(sprite.blend.source==BlendFactor::one&&sprite.blend.destination==BlendFactor::oneMinusSourceAlpha);
    }
    std::array<VisualLayer,14> layers;transition.append(layers);assert(layers[13].visualCount()==5);for(unsigned i=0;i<13;++i)assert(layers[i].visualCount()==0);
    const auto output=layers[13].drain();assert(output.atlases.empty()&&output.nonAtlas.size()==5);for(const auto& sprite:output.nonAtlas)assert(sprite.visualLayer==13);
    VisualLayer single;transition.append(single);assert(single.visualCount()==5);for(const auto& sprite:single.drain().nonAtlas)assert(sprite.visualLayer==13);
}
void normalPath(Files& files){
    const auto beforeReads=files.reads,beforeProbes=files.probes;ScreenTransition transition(files);
    assert(transition.state().phase==8&&!transition.state().visible&&transition.state().requestedScreen==1&&transition.state().previousScreen==1);
    assert(draw(transition).sprites.empty()&&transition.preparationFrame().sprites.size()==5);
    assert(files.reads==beforeReads+5&&files.probes==beforeProbes+5);
    for(unsigned i=0;i<5;++i)assert(files.names[beforeReads+i]==names[i]);
    assert(ScreenTransition::soundResources[0]=="Loading_Open.wav"&&ScreenTransition::soundResources[1]=="Loading_Close.wav");
    assert(transition.request(3));assert(draw(transition).sprites.empty());
    auto expected=transition.state();std::vector<std::string> events;std::vector<int> phases={0};int liveScreen=1;unsigned replacements=0;
    ScreenTransitionCallbacks host;
    host.replaceScreen=[&](int target){assert(transition.state().phase==4&&target==3&&liveScreen==1&&transition.state().visible);assert(!transition.request(2));assert(transition.state().centerScale.x==1);events.emplace_back("Replace3:phase4");liveScreen=target;++replacements;};
    host.playSound=[&](std::string_view name){if(name=="Loading_Open.wav"){assert(liveScreen==3&&transition.state().phase==5);events.emplace_back("Open:phase5");}else{assert(name=="Loading_Close.wav"&&liveScreen==1);events.emplace_back("Close:phase"+std::to_string(transition.state().phase));}};
    const auto warmReads=files.reads,warmProbes=files.probes;std::array<unsigned,9> ticks{};unsigned frames=0;
    while(transition.state().phase!=8){const auto entry=transition.state().phase;assert(entry!=6);++ticks[entry];if(entry<4)assert(liveScreen==1&&replacements==0);expectedTick(expected);transition.tick(host);equal(transition.state(),expected);++frames;assert(frames<1000);if(phases.back()!=expected.phase)phases.push_back(expected.phase);if(expected.visible)geometry(transition);else assert(draw(transition).sprites.empty());}
    assert((phases==std::vector<int>{0,1,2,3,4,5,7,8}));assert((events==std::vector<std::string>{"Close:phase1","Close:phase2","Replace3:phase4","Open:phase5"}));assert(liveScreen==3&&replacements==1);
    const auto idle=transition.state();for(unsigned i=0;i<1000;++i){transition.tick(host);equal(transition.state(),idle);(void)draw(transition);(void)transition.preparationFrame();}
    assert(files.reads==warmReads&&files.probes==warmProbes);
    std::cout<<"HM2_SCREEN_TRANSITION originalFloatBits/allPhaseTicks/close2-replace1-open1/synchronousPhase4To5/layer13/nonAtlas/order5/centerIndependent/warmReadProbe0 PASS frames="<<frames<<" phaseTicks=";
    for(unsigned i=0;i<ticks.size();++i)std::cout<<i<<":"<<ticks[i]<<",";std::cout<<"\n";
}
void requestGate(Files& files){
    ScreenTransition transition(files,3);assert(transition.state().previousScreen==3&&transition.state().requestedScreen==3);
    for(int phase=0;phase<=8;++phase){for(int target:{std::numeric_limits<int>::min(),-1,0,1,2,3,4,5,6,7,8,9,std::numeric_limits<int>::max()}){
        fixture(transition)=closed(phase);const auto before=transition.state();const bool accepted=transition.request(target);assert(accepted==(phase>6&&target>=1&&target<=8));
        if(accepted){assert(transition.state().phase==0&&transition.state().previousScreen==3&&transition.state().requestedScreen==target);auto expected=before;expected.phase=0;expected.previousScreen=3;expected.requestedScreen=target;equal(transition.state(),expected);}else equal(transition.state(),before);
    }}
    fixture(transition)=closed(7);fixture(transition).left.x=99;assert(transition.request(3));assert(transition.state().visible);ScreenTransitionCallbacks host{[](int){},[](auto){}};transition.tick(host);assert(transition.state().phase==1&&transition.state().left.x==-256&&transition.state().centerScale.x==1.5f);
    bool failed=false;const auto before=files.reads;try{ScreenTransition invalid(files,0);}catch(const std::out_of_range&){failed=true;}assert(failed&&files.reads==before);
    std::cout<<"HM2_SCREEN_TRANSITION requestPhase0to6Reject/7+8Accept/ID1to8/sameScreenAccept/restartState7/initialPhase8 PASS\n";
}
void boundaries(Files& files){
    ScreenTransition transition(files);std::vector<std::string> sounds;unsigned replacements=0;
    ScreenTransitionCallbacks host{[&](int){++replacements;},[&](auto name){sounds.emplace_back(name);}};
    const auto check=[&](ScreenTransitionState input){fixture(transition)=input;expectedTick(input);transition.tick(host);equal(transition.state(),input);};
    for(float x:{185.9f,186.f,std::nextafter(186.f,256.f)}){auto s=closed(1);s.left.x=x;check(s);assert(transition.state().phase==(x<186?1:2));}
    auto s=closed(2);s.top.y=std::nextafter(640.1f,1000.f);s.bottom.y=100;check(s);assert(transition.state().phase==2&&std::abs(transition.state().top.y-640.f)<=.1f);transition.tick(host);assert(transition.state().phase==3&&transition.state().top.y==640&&transition.state().bottom.y==384);
    for(float x:{std::nextafter(256.1f,256.f),std::nextafter(256.1f,1000.f)}){s=closed(2);s.top.y=900;s.left.x=x;check(s);assert((transition.state().left.x==256)==(std::abs(x-256)<=.1f));}
    s=closed(3);s.centerScale={1.025f,77};check(s);assert(transition.state().phase==3&&transition.state().centerScale.x==1);transition.tick(host);assert(transition.state().phase==4&&transition.state().centerScale.x==1&&replacements==0);
    s=closed(5);s.centerScale={1.375f,77};check(s);assert(transition.state().phase==5&&transition.state().centerScale.x==1.4f);transition.tick(host);assert(transition.state().phase==7);
    for(float x:{32.f,0.f,-255.95f,-256.f}){s=closed(6);s.left.x=x;check(s);assert(transition.state().phase==(transition.state().left.x<0?7:6));}
    s=closed(7);s.top.y=std::nextafter(1407.9f,0.f);check(s);assert(transition.state().phase==7&&std::abs(1408.f-transition.state().top.y)<=.1f);transition.tick(host);assert(transition.state().phase==8&&!transition.state().visible&&transition.state().centerScale.x==1.5f);
    // Render independent Center rotation/scale without inheriting Bottom1.5.
    s=closed(3);s.centerAngle=.375f;s.centerScale={1.2f,.8f};fixture(transition)=s;geometry(transition);
    std::array<VisualLayer,13> tooFew;bool failed=false;try{transition.append(tooFew);}catch(const std::out_of_range&){failed=true;}assert(failed);fixture(transition).visible=false;transition.append(tooFew);
    std::cout<<"HM2_SCREEN_TRANSITION strict200/scale1+1.4/preEase0.1/forcedBottom384/state6Preserved/centerRotation/layerBounds PASS\n";
}
void failedReplacementAndMissing(Files& files){
    ScreenTransition transition(files);fixture(transition)=closed(4);unsigned opens=0;ScreenTransitionCallbacks failing{[&](int){assert(transition.state().phase==4);throw std::runtime_error("host screen replacement failed");},[&](auto){++opens;}};
    bool failed=false;try{transition.tick(failing);}catch(const std::runtime_error&){failed=true;}assert(failed&&transition.state().phase==4&&opens==0);
    std::vector<int> order;ScreenTransitionCallbacks recovered{[&](int id){assert(id==3&&transition.state().phase==4);order.push_back(4);},[&](auto name){assert(name=="Loading_Open.wav"&&transition.state().phase==5);order.push_back(5);}};transition.tick(recovered);assert((order==std::vector<int>{4,5}));
    struct Missing:ResourceAccess {unsigned reads=0;mutable unsigned probes=0;Bytes read(std::string_view)override{++reads;return {};}bool exists(std::string_view)const override{++probes;return false;}} missing;
    ScreenTransition nil(missing);assert(nil.preparationFrame().sprites.empty()&&missing.probes==5&&missing.reads==0);assert(nil.request(3));nil.tick({[](int){},[](auto){}});assert(nil.state().visible&&draw(nil).sprites.empty());assert(missing.probes==5&&missing.reads==0);
    std::cout<<"HM2_SCREEN_TRANSITION failedReplacePreservesPhase4/noEarlyOpen/recoveryOrder/originalNil/noFallback PASS; actualSession/nativeGPU/nativeSound pending\n";
}
}
int main(int argc,char** argv){if(argc<2)return 77;Files files(argv[1]);normalPath(files);requestGate(files);boundaries(files);failedReplacementAndMissing(files);}

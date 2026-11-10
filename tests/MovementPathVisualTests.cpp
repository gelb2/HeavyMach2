#include "Core/Presentation/MovementPathVisual.hpp"
#include "Core/Data/Maps.hpp"
#include <limits>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace hm2;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;unsigned reads=0;mutable unsigned probes=0;
    explicit Files(const char* p):root(p){}
    Bytes read(std::string_view n)override{++reads;std::ifstream f(root/std::string(n),std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
    bool exists(std::string_view n)const override{++probes;return std::filesystem::exists(root/std::string(n));}
};
void near(float a,float b){assert(std::abs(a-b)<.0001f);}
Frame draw(MovementPathVisual& visual,const MapCamera* camera=nullptr){VisualLayer l1,l7;visual.appendWorldLayers(l1,l7,camera);Frame f;l1.drain().appendBatches(f);l7.drain().appendBatches(f);return f;}
void append(MovementPathVisual& visual,MovementPathState& s,Vec2 p,Vec2 player,bool reset){
    if(reset){s.start=0;s.count=0;}else if(s.count>11)s.count=11;
    s.points[(s.start+s.count)%12]=p;++s.count;visual.acceptedSetMovePath(s,player,reset);
}
void advance(MovementPathVisual& v,MovementPathState& s,Vec2 player,double time){auto before=s;--s.count;s.start=(s.start+1)%12;v.processMyMach(player,time,before,s);}
void ringAndAnimation(Files& files){
    PreparedTiles cache;MovementPathVisual visual;visual.prepare(files,cache);assert(files.reads==3&&files.probes==3);
    Frame prep;visual.appendPreparation(prep);assert(prep.sprites.size()==3&&cache.lookup("DestPos.png")->columns==2);
    assert(draw(visual).sprites.empty());MovementPathState state;
    append(visual,state,{96,0},{0,0},true);auto m=visual.observations()[0];assert(m.tile==1&&m.shown&&m.lineShown);near(m.scale.x,5);near(m.scale.y,5);
    auto first=draw(visual);assert(first.sprites.size()==2&&first.sprites[0].image=="MovePath.png"&&first.sprites[1].image=="DestPos.png");
    visual.processMyMach({0,0},.5,state,state);m=visual.observations()[0];near(m.scale.x,4.2f);near(m.scale.y,4.2f);near(m.alpha,.95f);near(m.angle,std::sin(1.f));
    append(visual,state,{192,0},{0,0},false);visual.processMyMach({1,2},.6,state,state);
    m=visual.observations()[1];near(m.scale.x,4.1f);near(m.scale.y,4.1f);near(m.alpha,.93f);assert(m.tile==0);
    near(visual.observations()[0].lineEnd.x,1);near(visual.observations()[0].lineEnd.y,2);
    advance(visual,state,{96,0},1);assert(!visual.observations()[0].shown&&state.start==1&&state.count==1);
    m=visual.observations()[1];assert(m.tile==1);near(m.scale.x,1.4f);near(m.alpha,.95f);near(m.lineEnd.x,96);
    // New head's old line end stays unchanged until the next actual phase.
    visual.processMyMach({100,4},1.1,state,state);near(visual.observations()[1].lineEnd.x,100);near(visual.observations()[1].lineEnd.y,4);
    const auto reads=files.reads,probes=files.probes;
    for(unsigned i=0;i<20;++i){append(visual,state,{300.f+32.f*i,64},{100,4},false);assert(state.count<=12);}
    assert(state.count==12);const auto tail=(state.start+11)%12;near(visual.observations()[tail].position.x,908);
    // Ring wraps; the original scaleY bug addresses count-1, not tail.
    assert(tail==0);near(visual.observations()[0].scale.x,5);near(visual.observations()[11].scale.y,5);
    visual.processMyMach({100,4},2,state,state);for(const auto& x:visual.observations())near(x.scale.x,x.scale.y);
    for(unsigned i=0;i<12;++i)advance(visual,state,state.points[state.start],2+i*.1);
    assert(state.count==0&&draw(visual).sprites.empty());
    const auto last=visual.observations();visual.processMyMach({0,0},20,state,state);for(unsigned i=0;i<12;++i){near(last[i].alpha,visual.observations()[i].alpha);near(last[i].scale.x,visual.observations()[i].scale.x);}
    append(visual,state,{64,0},{0,0},true);append(visual,state,{128,0},{0,0},false);visual.clearMovePath(0,2);assert(draw(visual).sprites.empty());
    for(unsigned i=0;i<120;++i){visual.processMyMach({0,0},i*.016,state,state);(void)draw(visual);Frame f;visual.appendPreparation(f);}
    assert(files.reads==reads&&files.probes==probes);
    std::cout<<"movement ring12/reset/append/tailReplace/actualArrival/firstTick/easing/hidden/warmRead0/warmExists0 PASS\n";
}
void trimAndSelection(Files& files){
    PreparedTiles cache;MovementPathVisual visual;visual.prepare(files,cache);MovementPathState state;
    for(unsigned i=0;i<12;++i)append(visual,state,{float(100+i*64),64},{0,0},i==0);
    advance(visual,state,state.points[state.start],1);advance(visual,state,state.points[state.start],2);
    assert(state.start==2&&state.count==10);
    const unsigned closest=3,oldCount=state.count;const auto retained=state.points[(state.start+closest)%12];
    visual.clearMovePath((state.start+closest)%12,oldCount-closest);state.count=closest;
    append(visual,state,retained,{0,0},false);assert(state.count==4&&state.start==2);
    for(unsigned i=0;i<12;++i)assert(visual.observations()[i].shown==(i>=2&&i<=5));
    near(visual.observations()[5].scale.x,5);near(visual.observations()[5].alpha,1);
    // First enemy selection displays TargetPath without clearing movement.
    visual.setTargetPathShown(true);visual.processTarget({0,0},Vec2{256,256});assert(draw(visual).sprites.size()==9);
    // Same enemy re-tap's actual clear(start,12) hides all 12 physical pairs.
    visual.clearMovePath(state.start,12);state.count=0;assert(draw(visual).sprites.size()==1);
    std::cout<<"movement closestTrim/hideReadd/wrappedPhysicalRange/firstTargetKeep/sameTargetClear12 PASS\n";
}

void lineGeometry(Files& files){
    PreparedTiles cache;MovementPathVisual visual;visual.prepare(files,cache);MovementPathState state;
    append(visual,state,{80,0},{0,0},true);const auto f=draw(visual);const auto& line=f.sprites.front();
    assert(line.strip.size()==8&&line.blend.source==BlendFactor::sourceAlpha&&line.blend.destination==BlendFactor::one&&!line.atlasBatch);
    const float xs[]={80,80,48,48,16,16,0,0},ys[]={-4,4,-4,4,-4,4,-4,4};
    const float vs[]={1,1,0,0,1,1,.5f,.5f};
    for(unsigned i=0;i<8;++i){near(line.strip[i].position.x,xs[i]);near(line.strip[i].position.y,ys[i]);near(line.strip[i].uv.x,float(i%2));near(line.strip[i].uv.y,vs[i]);assert(line.strip[i].color.a==1);}
    // Diagonal direction, exact full block extra endpoint, target width4.
    visual.setTargetPathShown(true);visual.processTarget({10,20},Vec2{10,84});auto target=draw(visual).sprites.back();
    assert(target.image=="TargetPath.png"&&target.strip.size()==8);near(target.strip[0].position.x,8);near(target.strip[1].position.x,12);near(target.strip.back().position.y,84);near(target.strip.back().uv.y,1);
    visual.processTarget({10,20},std::nullopt);assert(!visual.targetPathShown());
    visual.processTarget({10,20},Vec2{20,30});assert(!visual.targetPathShown());
    // Original line culling always returns true; marker uses unscaled bounds.
    MapCamera camera;camera.center={100000,100000};auto off=draw(visual,&camera);assert(off.sprites.size()==1&&off.sprites.front().image=="MovePath.png");
    append(visual,state,{0,0},{0,0},true);auto zero=draw(visual);assert(zero.sprites.size()==1&&zero.sprites[0].image=="DestPos.png");
    append(visual,state,{2048,0},{0,0},true);assert(draw(visual).sprites.front().strip.size()==132);
    append(visual,state,{3072,2048},{0,0},true);const auto diagonal=draw(visual).sprites.front();
    const float length=std::sqrt(3072.f*3072.f+2048.f*2048.f);assert(diagonal.strip.size()==(unsigned(length/32.f)+2)*2);
    const auto n=diagonal.strip.size();assert(std::abs((diagonal.strip[n-1].position.x+diagonal.strip[n-2].position.x)*.5f)<.01f);assert(std::abs((diagonal.strip[n-1].position.y+diagonal.strip[n-2].position.y)*.5f)<.01f);
    const auto capacities=visual.lineCapacities();const auto reads=files.reads,probes=files.probes;
    for(unsigned i=0;i<120;++i){(void)draw(visual);assert(visual.lineCapacities()==capacities);}
    assert(files.reads==reads&&files.probes==probes);
    std::cout<<"movement originalQobLine/whiteFloat/UV/width8+4/nonAtlas/blend2/zeroArea/64blockSafeGrowth/diagonal/readonlyCapacity/camera PASS\n";
}
void staticMapBounds(Files& files){
    const auto maps=Maps::parse(Chunk::parse(files.read("MapInfo.mcb")));
    PreparedTiles cache;MovementPathVisual visual;visual.prepare(files,cache);
    float maxDiagonal=0;std::string largest;unsigned count=0;
    for(const auto& [name,map]:maps.all()){
        Vec2 low{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity()},high{-low.x,-low.y};
        auto point=[&](Vec2 p){low.x=std::min(low.x,p.x);low.y=std::min(low.y,p.y);high.x=std::max(high.x,p.x);high.y=std::max(high.y,p.y);};
        for(const auto& gate:map.gates)point(gate.position);
        for(const auto& spawner:map.spawners)point(spawner.position);
        for(const auto& polygon:map.collisionPolygons)for(const auto p:polygon)point(p);
        if(!std::isfinite(low.x))continue;++count;
        const auto diagonal=std::hypot(high.x-low.x,high.y-low.y);
        if(diagonal>maxDiagonal){maxDiagonal=diagonal;largest=name;}
        MovementPathState state;append(visual,state,high,low,true);const auto f=draw(visual);
        assert(f.sprites.front().strip.size()==(unsigned(diagonal/32.f)+2)*2);
    }
    // Original fixed camera limits: centerX[-128,128], centerY[-94,0].
    // Compare corner bounds; this is geometric coverage, not a runtime claim
    // that every extremal point is walkable or targetable.
    for(const Size viewport:{Size{768,1024},Size{1920,1080}}){
        MovementPathState state;const Vec2 low{-128-viewport.width/2,-94-viewport.height/2};
        const Vec2 high{128+viewport.width/2,viewport.height/2};
        append(visual,state,high,low,true);const auto diagonal=std::hypot(high.x-low.x,high.y-low.y);
        assert(draw(visual).sprites.front().strip.size()==(unsigned(diagonal/32.f)+2)*2);
        std::cout<<"camera geometric bound viewport="<<viewport.width<<"x"<<viewport.height<<" diagonal="<<diagonal<<" originalBlocks="<<unsigned(diagonal/32.f)<<" PASS\n";
    }
    assert(count==43);std::cout<<"static maps="<<count<<" coordinateBoundsMaxDiagonal="<<maxDiagonal<<" map="<<largest<<" PASS (bounds, not port runtime)\n";
}

}
int main(int argc,char** argv){if(argc<2)return 77;Files files(argv[1]);ringAndAnimation(files);trimAndSelection(files);lineGeometry(files);staticMapBounds(files);}

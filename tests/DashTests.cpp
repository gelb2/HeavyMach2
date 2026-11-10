#include "Core/Simulation/Dash.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include "Core/Presentation/PreparedTiles.hpp"
#include "Core/Data/PlayerEquipment.hpp"
#include "Core/Audio/SoundIDs.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace hm2;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;unsigned reads=0;mutable unsigned probes=0;
    explicit Files(const char* p):root(p){}
    Bytes read(std::string_view name)override{++reads;std::ifstream f(root/std::string(name),std::ios::binary);if(!f)throw std::runtime_error("Missing original dash asset "+std::string(name));return {std::istreambuf_iterator<char>(f),{}};}
    bool exists(std::string_view name)const override{++probes;return std::filesystem::exists(root/std::string(name));}
};
struct Start:DashStartHost {
    Vec2 direction;float special=0;unsigned pointer=9;std::vector<std::string> events;
    void setDashVelocity(Vec2 v)override{direction=v;events.push_back("velocity");}
    void clearMovePointer()override{pointer=0;events.push_back("pointer0");}
    void setSpecial(float v)override{special=v;events.push_back("SP");}
    void sound(unsigned id)override{assert(id==14);events.push_back("SFX14");}
};
struct Random:WorldRandom {
    unsigned calls=0;std::vector<std::string>* events=nullptr;
    std::uint32_t random()override{++calls;if(events)events->push_back("R");return 0x40000000u;}
};
struct Motion:DashMotionHost {
    Vec2 pos;PreparedTiles& tiles;Random random;ParticlePool pool{0,random};std::vector<std::string> events;
    explicit Motion(PreparedTiles& t):tiles(t){random.events=&events;}
    Vec2 position()const override{return pos;}
    Particle* allocateParticle(float time)override{events.push_back("pool");return pool.allocate(time,random);}
    TileImage* preparedTile(std::string_view name)override{assert(name=="Efx_Dash.png");events.push_back("tile");return tiles.lookup(name);}
    void addPosition(Vec2 d)override{events.push_back("move");pos.x+=d.x;pos.y+=d.y;}
};
void gesture(){
    Start host;DashGesture g;assert(!g.groundBegin({300,400},{},0,200,host)&&g.tapTime()==0);
    assert(!g.groundBegin({300,400},{},.125,200,host)&&host.events.empty());
    assert(g.groundBegin({300,400},{},.25,100,host));
    assert((host.events==std::vector<std::string>{"velocity","pointer0","SP","SFX14"}));
    assert(host.direction.x==.6f&&host.direction.y==.8f&&host.special==0&&host.pointer==0&&g.tapTime()==.125f);
    host.events.clear();assert(g.groundBegin({-300,400},{},.375,320,host)&&host.special==220&&host.direction.x==-.6f&&g.tapTime()==.125f);
    host.events.clear();assert(!g.groundBegin({300,400},{},.625,320,host)&&host.events.empty()&&g.tapTime()==.625f); // Strict half-second.
    assert(!g.groundBegin({300,400},{},.75,std::nextafter(100.f,0.f),host)&&g.tapTime()==.75f&&host.events.empty());
    g.pathDrag(false,1);assert(g.tapTime()==.75f);g.pathDrag(true,0);assert(g.tapTime()==.75f);
    g.pathDrag(true,1);assert(g.tapTime()==0);assert(!g.groundBegin({300,400},{},.8,320,host));
    DashGesture precise;assert(!precise.groundBegin({300,400},{},16777216.25,320,host)&&precise.tapTime()==16777216.f);
    assert(!precise.groundBegin({300,400},{},16777216.5,320,host)); // Stored float, not double interval.
}
void motion(PreparedTiles& tiles){
    Motion host(tiles);DashMotion dash;dash.setVelocity({1,0});
    const auto rng=host.random.calls;assert(rng==64);unsigned steps=0;float total=0;
    while(dash.velocity().x!=0){const float old=dash.velocity().x;assert(dash.tick(1,{},host));total+=old*10;assert(++steps<100);}
    assert(steps==36&&host.pos.x==total&&host.pos.y==0&&total>79&&total<80); // Tick movement, no time-scaled substitute.
    assert(!dash.tick(1,{},host));
    Motion gate(tiles);DashMotion held;held.setVelocity({1,-1});gate.pos={100,100};
    const DashTick blocked[]={{false,false,true,0,0},{true,true,true,0,0},{true,false,false,0,0},{true,false,true,-1,0},{true,false,true,2,0}};
    for(const auto& tick:blocked){assert(!held.tick(2,tick,gate));assert(gate.pos.x==100&&gate.pos.y==100&&held.velocity().x==1&&gate.random.calls==64&&gate.events.empty());}
    assert(held.tick(2,{true,false,false,0,2},gate)); // Original aircraft bypasses inScreen.
    assert(gate.pos.x==110&&gate.pos.y==90&&held.velocity().x==.875f&&held.velocity().y==-.875f);
    assert((gate.events==std::vector<std::string>{"pool","R","tile","move"}));
    const auto& p=gate.pool.particles()[0].state();assert(p.show&&p.useAtlas&&p.fadeOut&&p.blendType==2&&p.life==.2f&&p.startTime==2&&p.tileNumber==0);
    assert(p.tile&&p.tile->name=="Efx_Dash.png"&&p.tile->columns==1&&p.tile->rows==1&&p.position.x==100&&p.position.y==100);
    assert(p.scale.x==.5f&&p.scale.y==.5f&&p.scaleVelocity==.05f&&p.angle==float(std::atan2(-10.,10.)));
    assert(gate.random.calls==65&&gate.pool.allocations()==1);
    const auto previous=held.lastEffectPosition();held.setVelocity({0,1});assert(held.lastEffectPosition().x==previous.x&&held.lastEffectPosition().y==previous.y);
    Motion threshold(tiles);DashMotion exact;threshold.pos={20,20};exact.setVelocity({.01f,-.01f});
    assert(exact.tick(3,{},threshold)&&threshold.pool.allocations()==0&&exact.velocity().x==0&&exact.velocity().y==0); // Exactly800 and min equality.
    threshold.pos={20,std::nextafter(20.f,std::numeric_limits<float>::infinity())};exact.setVelocity({1,0});
    assert(exact.tick(3,{},threshold)&&threshold.pool.allocations()==1);
    Motion full(tiles);for(auto& particle:full.pool.particles())particle.state().show=true;
    DashMotion saturated;full.pos={100,200};saturated.setVelocity({1,0});assert(saturated.tick(4,{},full));
    assert(full.random.calls==64&&full.pool.allocations()==0&&full.pos.x==110&&saturated.lastEffectPosition().x==100&&saturated.lastEffectPosition().y==200);
    assert((full.events==std::vector<std::string>{"pool","move"}));
    full.pool.particles()[0].state().show=false;full.events.clear();assert(saturated.tick(4,{},full));
    assert(full.pool.allocations()==0&&full.events==std::vector<std::string>{"move"}); // Full-pool effect anchor is retained.
}
void bottomData(Files& files){
    const auto table=DataTable::parse(files.read("ItemInfo.tble"));const auto equipment=PlayerEquipment::parse(table);
    const auto models=Models::parse(Chunk::parse(files.read("PartsInfo.mcb")),Chunk::parse(files.read("MachInfo.mcb")));
    unsigned count=0;float minSP=1e9f,maxSP=0,minRegen=1e9f,maxRegen=0,minStep=1e9f,maxStep=0;
    for(std::size_t row=0;row<table.rowCount();++row)if(table.value(row,"Type")=="PtBtm"){
        ++count;const auto code=table.value(row,"Code").value();const auto& item=equipment.item(code);const auto& part=models.part(item.part);
        assert(part.type==2&&item.parameter3==table.real(row,"Param3")); // All23 share original paired-feet branch.
        auto codes=PlayerEquipment::startingCodes;codes[1]=code;const auto mounts=equipment.mounts(models,codes);
        assert(mounts.size()==6&&mounts[1].part==item.part&&mounts[2].part==item.part&&mounts[1].parent==0&&mounts[2].parent==0&&mounts[1].parentSocket==1&&mounts[2].parentSocket==2);
        assert(models.mountedPose(mounts).size()==6);
        const float sp=table.real(row,"Param1"),regen=table.real(row,"Param2"),step=table.real(row,"Param3");
        minSP=std::min(minSP,sp);maxSP=std::max(maxSP,sp);minRegen=std::min(minRegen,regen);maxRegen=std::max(maxRegen,regen);minStep=std::min(minStep,step);maxStep=std::max(maxStep,step);
        Start host;DashGesture g;assert(!g.groundBegin({300,400},{},.125,sp,host));assert(g.groundBegin({300,400},{},.25,sp,host)&&host.special==sp-100&&host.direction.x==.6f&&host.direction.y==.8f);
    }
    assert(count==23&&minSP==180&&maxSP==320&&minRegen==1&&maxRegen==2.5f&&minStep==20&&maxStep==35);
    std::cout<<"HM2_DASH PtBtm23 actualMCBtype2 shared paired-feet mount/pose SP180..320 regen1..2.5 step20..35 PASS; animated/platform comparison remains pending\n";
}
}
int main(int argc,char** argv){
    if(argc<2)return 77;Files files(argv[1]);PreparedTiles tiles("dash-test");auto* tile=tiles.prepare(files,"Efx_Dash.png");assert(tile&&tile->pixels.width==64&&tile->pixels.height==64);
    assert(audio::soundFilename(14)=="SFX_Dash.wav");gesture();bottomData(files);const auto reads=files.reads,probes=files.probes;motion(tiles);assert(files.reads==reads&&files.probes==probes);
    std::cout<<"HM2_DASH originalGesture SP100 strictHalfSecond/floatTime/moveClear callbackOrder; move10/ease8/min.01/end36; sourceGates/particle800/fullPool/RNG1/tile64 warmIO0 PASS; Session/Scene integration pending\n";
}

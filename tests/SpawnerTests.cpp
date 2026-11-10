#include "Core/Simulation/EnemySpawner.hpp"
#include "Core/Simulation/GameClock.hpp"
#include <cassert>
#include <cmath>
using namespace hm2;
struct Host:EnemySpawnerHost {
    mutable unsigned cameraChecks=0;unsigned spawns=0,randomCalls=0;bool camera=true,success=true;std::uint32_t next=0;
    std::uint32_t random()override{++randomCalls;return next;}
    bool inCamera(Vec2)const override{++cameraChecks;return camera;}
    bool spawn(const MapSpawner& d,bool boss)override{assert(d.model.starts_with(boss?"Boss":"E_"));++spawns;return success;}
};
int main(){
    Host h;EnemySpawner first({"E_Tank_01",{-284,-79}},false);GameClock clock;clock.step();first.tick(clock.worldTime(),{-275,-462},h);
    assert(h.spawns==1&&h.cameraChecks==0&&first.state().regenCount==1);first.tick(10000,{},h);assert(h.spawns==1);
    first.enemyDead(1,h);assert(h.randomCalls==1&&first.state().regenCount==1&&first.state().regenTime==30);first.tick(10001,{},h);assert(h.spawns==1);
    EnemySpawner max({"E_Tank_01",{}},true);h.next=0x7fffffffu;max.enemyDead(3,h);assert(max.state().regenTime==110);
    Host zero;EnemySpawner delayed({"E_Tank_01",{}},false);delayed.tick(0,{},zero);assert(zero.spawns==0&&delayed.state().checkTime==0);
    delayed.tick(.5,{},zero);assert(zero.spawns==0);delayed.tick(std::nextafter(.5,1.0),{},zero);assert(zero.spawns==1);
    for(Vec2 point: {Vec2{1024,0},Vec2{0,-1024}}){Host edge;EnemySpawner s({"E_Tank_01",point},false);s.tick(.0166,{},edge);assert(!edge.spawns);s.tick(.5,point,edge);assert(!edge.spawns);s.tick(1,point,edge);assert(edge.spawns==1);}
    Host inside;EnemySpawner strict({"E_Tank_01",{std::nextafter(1024.f,0.f),0}},false);strict.tick(.0166,{},inside);assert(inside.spawns==1);
    Host failure;failure.success=false;EnemySpawner failed({"E_Tank_01",{}},false);failed.tick(.0166,{},failure);assert(failed.state().regenCount==1);
    Host bossHost;EnemySpawner boss({"Boss_01",{}},false,{0,-1,0,1});boss.tick(10,{},bossHost);assert(!bossHost.spawns&&boss.state().checkTime==-1);
    Host renewed;EnemySpawner camera({"E_Tank_01",{}},false,{0,-1,0,1});camera.tick(1,{},renewed);assert(!renewed.spawns&&renewed.cameraChecks==0);camera.tick(2,{},renewed);assert(!renewed.spawns&&renewed.cameraChecks==1);renewed.camera=false;camera.tick(3,{},renewed);assert(renewed.spawns==1);
    const auto oldWorld=clock.worldTime();const auto oldRender=clock.renderTime();clock.step(true);assert(clock.worldTime()==oldWorld&&clock.renderTime()>oldRender);const auto n=clock.frames();assert(!clock.step(false,false)&&clock.frames()==n);
    for(unsigned i=0;i<100000;++i)clock.step();assert(clock.worldTime()!=double(clock.renderTime())); // Separate original accumulation precision.
}

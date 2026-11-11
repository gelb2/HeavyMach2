#include "Core/Simulation/VehicleMotion.hpp"
#include <cassert>
#include <bit>
#include <cmath>
#include <numbers>
#include <vector>
using namespace hm2;
struct Random:WorldRandom {std::vector<std::uint32_t> values;std::size_t calls=0;std::uint32_t random()override{assert(calls<values.size());return values[calls++];}};
int main(){
    Random r;r.values={1};
    VehicleMotion stopped;stopped.tick(r);assert(r.calls==0);
    VehicleMotion edge({{0,0},{1,-1},{},0,9});edge.setDestination({1,-1});edge.tick(r);
    assert(edge.state().state==0&&edge.state().speed==0&&r.calls==0);
    VehicleMotion close;close.setDestination({30,0});close.tick(r);
    assert(close.state().speed==.5f/60&&close.state().position.x==close.state().speed&&r.calls==1);
    r.calls=0;VehicleMotion far;far.setDestination({30.001f,0});far.tick(r);
    assert(far.state().speed==.3f/100&&r.calls==1);
    Random jitter;jitter.values={0,0,1073741824,2147483647};VehicleMotion shaking;shaking.setDestination({100,0});shaking.tick(jitter);
    assert(jitter.calls==4&&shaking.state().position.x==.3f/100-.5f&&shaking.state().position.y==0&&shaking.state().scaleState==1.01f);
    TerrainEdge wall;wall.normal={1,0};wall.dir={0,1};
    Random wallRandom;wallRandom.values={1,1};VehicleMotion up;up.setDestination({100,100});up.tick(wallRandom,&wall);
    const float a=float(std::atan2(1.0,0.0))-.1f;assert(up.state().angle==a/30);
    VehicleMotion down;down.setDestination({100,-100});down.tick(wallRandom,&wall);
    const float desired=float(double(float(std::atan2(1.0,0.0)))+std::numbers::pi+double(.1f));
    assert(down.state().angle==(desired-std::bit_cast<float>(0x40c90fdau))/30+std::bit_cast<float>(0x40c90fdau));
    Random idle;idle.values={0};EnemyWeaponAim aim;
    assert(!aim.tick(0,{0,0},0,std::nullopt,idle));assert(idle.calls==1&&aim.state().angle==.01f);
    Random unused;EnemyWeaponAim tracking;const auto ready=tracking.tick(0,{},0,Vec2{100,0},unused);
    assert(ready&&*ready&&unused.calls==0);
    EnemyWeaponAim turn;const auto notReady=turn.tick(0,{},0,Vec2{0,100},unused);
    assert(notReady&&!*notReady&&turn.state().angle==std::numbers::pi_v<float>/20);
    const auto previous=turn.state().angle;assert(!turn.tick(1,{},0,std::nullopt,unused,true,true,0));assert(turn.state().angle==previous);
}

#include "Core/Simulation/Trail.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace hm2;
namespace {
struct Random:WorldRandom {
    unsigned calls=0;
    std::uint32_t value=0x40000000u;
    std::uint32_t random()override{++calls;return value;}
};
void near(float a,float b){assert(std::abs(a-b)<.00001f);}
template<class F>void rejects(F f){bool threw=false;try{f();}catch(const std::exception&){threw=true;}assert(threw);}
TileImage tile{"Efx_Trail04.png",{17,9}};
Trail straight(float length,Random& rng,TrailConfig config={}){
    Trail trail(&tile,{10,20},config);trail.state().wind=trail.state().diffuse=0;trail.state().blendType=2;
    trail.add({10,20},0,rng);trail.add({10+length,20},0,rng);return trail;
}
}
int main(){
    Random rng;MapCamera camera;camera.viewport={200,100};camera.center={3,4};camera.offset={2,1};
    auto shortTrail=straight(63,rng);Frame frame;shortTrail.append(frame,camera);
    assert(shortTrail.lastPosition()==1&&frame.sprites.empty()&&rng.calls==0);
    auto trail=straight(125,rng);assert(trail.lastPosition()==2&&trail.rows().size()==3&&rng.calls==0);
    near(trail.rows()[0].point.position.x,0);near(trail.rows()[1].point.position.x,64);near(trail.rows()[2].point.position.x,125);
    for(const auto& row:trail.rows()){near(row.point.normal.x,0);near(row.point.normal.y,-.5f);}
    trail.append(frame,camera);assert(frame.sprites.size()==1);const auto& sprite=frame.sprites[0];
    assert(sprite.image==tile.name&&sprite.strip.size()==6);
    assert(sprite.blend.source==BlendFactor::sourceAlpha&&sprite.blend.destination==BlendFactor::one);
    const Vec2 origin=camera.toScreen({10,20});
    near(sprite.strip[0].position.x,origin.x);near(sprite.strip[0].position.y,origin.y-.5f);
    near(sprite.strip[5].position.x,origin.x+125);near(sprite.strip[5].position.y,origin.y+.5f);
    // The renderer converts these content-normalized UVs to the POT maxS/T.
    for(std::size_t i=0;i<3;++i){
        near(sprite.strip[2*i].uv.x,0);near(sprite.strip[2*i+1].uv.x,1);
        near(sprite.strip[2*i].uv.y,i%2?0:1);near(sprite.strip[2*i+1].uv.y,i%2?0:1);
    }
    // Original zero-length normals survive, including terminal exact multiples.
    auto exact=straight(128,rng);assert(exact.lastPosition()==3);
    assert(std::isnan(exact.rows().back().point.normal.x)&&std::isnan(exact.rows()[0].point.normal.x));
    frame.sprites.clear();exact.append(frame,camera);assert(frame.sprites[0].strip.size()==8);
    assert(std::isnan(frame.sprites[0].strip.back().position.x));
    exact.add({138,20},0,rng);assert(std::isnan(exact.rows().back().point.normal.x)&&rng.calls==0);
    // Floats just below/above a multiple exercise the original subtraction loop.
    Trail below(&tile,{}),above(&tile,{});
    below.add({},0,rng);below.add({std::nextafter(128.f,0.f),0},0,rng);
    above.add({},0,rng);above.add({std::nextafter(128.f,1000.f),0},0,rng);
    assert(below.lastPosition()==2&&above.lastPosition()==3);
    // Shift all row attributes together; the terminal write also shifts if full.
    auto bounded=straight(321,rng,{1,64,2,0,.2f});assert(bounded.lastPosition()==2);
    near(bounded.rows()[0].point.position.x,256);near(bounded.rows()[1].point.position.x,320);near(bounded.rows()[2].point.position.x,321);
    near(bounded.rows()[0].vertices[0].uv.y,1);near(bounded.rows()[1].vertices[0].uv.y,0);
    Trail shifted(&tile,{}, {1,64,2,0,.2f});shifted.state().wind=shifted.state().diffuse=0;
    shifted.add({},0,rng);shifted.add({125,0},.05,rng);shifted.tick(.1,false,rng);
    const float priorAlpha=shifted.rows()[1].vertices[0].color.a;
    shifted.add({190,0},.15,rng);near(shifted.rows()[0].point.position.x,64);
    near(shifted.rows()[0].point.timestamp,.05f);near(shifted.rows()[0].vertices[0].color.a,priorAlpha);
    near(shifted.rows()[1].point.position.x,128);near(shifted.rows()[1].point.timestamp,.15f);
    // A downward turn consumes join RNG; the opposite cross sign does not.
    Trail bend(&tile,{});bend.state().wind=bend.state().diffuse=0;
    bend.add({},0,rng);bend.add({64,0},0,rng);const auto beforeBend=rng.calls;
    bend.add({64,-65},0,rng);assert(rng.calls==beforeBend+1);
    near(bend.rows()[1].point.normal.x,-std::sqrt(2.f));near(bend.rows()[1].point.normal.y,-std::sqrt(2.f));
    // Noncommitted terminal records overwrite the first normal, not the join.
    near(bend.rows()[0].point.normal.x,-.5f);near(bend.rows()[0].point.normal.y,0);
    Trail other(&tile,{});other.add({},0,rng);other.add({64,0},0,rng);
    const auto beforeOther=rng.calls;other.add({64,65},0,rng);assert(rng.calls==beforeOther);
    // At the delay boundary alpha remains the newly written 1, even at point0.
    auto fade=straight(125,rng,{1,64,16,.25f,.5f});fade.tick(.25,false,rng);
    for(const auto& row:fade.rows())near(row.vertices[0].color.a,1);
    fade.tick(std::nextafter(.25f,1.f),false,rng);near(fade.rows()[0].vertices[0].color.a,0);
    fade.tick(.5,false,rng);near(fade.rows()[1].vertices[0].color.a,.5f);assert(!fade.state().removed);
    fade.tick(.75,false,rng);assert(fade.state().removed);
    frame.sprites.clear();fade.append(frame,camera);assert(frame.sprites.empty());
    auto instant=straight(125,rng,{1,64,16,0,0});instant.tick(0,false,rng);assert(!instant.state().removed);
    instant.tick(.001,false,rng);assert(instant.state().removed);
    // Timestamp is float(worldTime), age is float(doubleTime-doubleTimestamp).
    Trail stamp(&tile,{});stamp.state().wind=stamp.state().diffuse=0;
    stamp.add({},.1,rng);stamp.add({125,0},.1,rng);stamp.tick(.1,false,rng);
    near(stamp.rows()[0].vertices[0].color.a,1);stamp.tick(.100000004,false,rng);near(stamp.rows()[0].vertices[0].color.a,0);
    // Diffusion widens each vertex pair; wind then shifts both by the same value.
    Trail moving(&tile,{});moving.state().diffuse=.05f;moving.state().wind=20;
    moving.add({},.25,rng);moving.add({125,0},.25,rng);const auto beforeTick=rng.calls;
    moving.tick(.25,false,rng);assert(rng.calls==beforeTick+6);
    const float windShift=(std::sin(.25f*30.f)/(20.f+(.5f*20.f)*.5f))*(-.5f);
    near(moving.rows()[0].vertices[0].position.y,(-.5f-.0125f)+windShift);
    near(moving.rows()[0].vertices[1].position.y,(.5f+.0125f)+windShift);
    near(moving.rows()[0].point.normal.y,-.5f);near(moving.rows()[0].point.position.y,0);
    const auto pausedVertex=moving.rows()[1].vertices[0].position;
    moving.tick(100,true,rng);assert(rng.calls==beforeTick+6&&!moving.state().removed);
    near(moving.rows()[1].vertices[0].position.y,pausedVertex.y);
    moving.tick(.35,false,rng);assert(rng.calls==beforeTick+10);near(moving.rows()[0].vertices[0].color.a,0);
    // Hidden trails still simulate when called, but produce no Sprite.
    moving.state().show=false;moving.tick(.45,false,rng);assert(moving.state().removed);
    // Blend3 differs from Tile2D's direct blend3.
    trail.state().blendType=3;frame.sprites.clear();trail.append(frame,camera);
    assert(frame.sprites[0].blend.source==BlendFactor::oneMinusSourceAlpha&&frame.sprites[0].blend.destination==BlendFactor::oneMinusSourceAlpha);
    trail.state().blendType=0;frame.sprites.clear();trail.append(frame,camera);assert(frame.sprites[0].blend.source==BlendFactor::sourceAlpha);
    trail.state().blendType=1;frame.sprites.clear();trail.append(frame,camera);assert(frame.sprites[0].blend.source==BlendFactor::sourceColor);
    trail.state().blendType=4;rejects([&]{trail.append(frame,camera);});
    rejects([&]{Trail invalid(&tile,{}, {1,0,16,0,.2f});});
    rejects([&]{Trail invalid(&tile,{}, {1,64,0,0,.2f});});
    rejects([&]{trail.add({std::numeric_limits<float>::infinity(),0},0,rng);});
    // Repeated small-capacity shifts retain the same finite geometry and bounds.
    auto stress=straight(100000,rng,{1,64,2,0,.2f});assert(stress.rows().size()==3);
    for(unsigned i=0;i<1000;++i)stress.add({100011.f+float(i)*65.f,20},0,rng);
    assert(stress.lastPosition()==2&&stress.rows().size()==3);
}

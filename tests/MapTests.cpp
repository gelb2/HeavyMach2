#include "Core/Presentation/MapPreview.hpp"
#include <cassert>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
using namespace hm2;
namespace {
void word(Chunk& c,std::size_t at,std::uint16_t v){c.data.at(at)=v&255;c.data.at(at+1)=v>>8;}
void real(Chunk& c,std::size_t at,float v){const auto n=std::bit_cast<std::uint32_t>(v);for(unsigned i=0;i<4;++i)c.data.at(at+i)=n>>(i*8);}
void text(Chunk& c,std::size_t at,std::string_view v){for(const auto ch:v)c.data.at(at++)=ch;}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}assert(caught);}
void same(float a,float b){assert(std::abs(a-b)<.0001f);}
}
int main(){
    Chunk root;root.type=0;Chunk m;m.type=1;m.data.resize(56);text(m,0,"Fixture");text(m,24,"Description");
    Chunk gate;gate.type=2;gate.data.resize(25);word(gate,0,2);word(gate,2,std::uint16_t(-301));word(gate,4,430);text(gate,6,"Destination");word(gate,22,3);gate.data[24]=1;
    Chunk polygon;polygon.type=3;polygon.data.resize(26);word(polygon,0,3);real(polygon,2,-4.25f);real(polygon,6,5.5f);real(polygon,10,10);real(polygon,14,-20);real(polygon,18,30);real(polygon,22,40);
    Chunk spawn;spawn.type=4;spawn.data.resize(22);word(spawn,0,99);text(spawn,2,"E_Tank_01");word(spawn,18,std::uint16_t(-284));word(spawn,20,std::uint16_t(-79));
    Chunk unknown;unknown.type=999;unknown.data={1,2,3};m.children={gate,polygon,spawn,unknown};root.children={m};
    const auto maps=Maps::parse(root);const auto& map=maps.map("Fixture");assert(map.description=="Description");assert(map.gates.size()==1&&map.spawners.size()==1&&map.collisionPolygons.size()==1);
    assert(map.gates[0].number==2&&map.gates[0].linkGate==3&&map.gates[0].eventOnly&&map.gates[0].linkMap=="Destination");same(map.gates[0].position.x,-301);same(map.gates[0].position.y,430);
    same(map.spawners[0].position.x,-284);same(map.spawners[0].position.y,-79);assert(map.spawners[0].model=="E_Tank_01");same(map.collisionPolygons[0][0].x,-4.25);same(map.collisionPolygons[0][0].y,5.5);
    assert(maps.raw().encode()==root.encode()); // No field/unknown-child cleanup.
    auto duplicate=m;text(duplicate,24,"Replacement");root.children.push_back(duplicate);assert(Maps::parse(root).map("Fixture").description=="Replacement");
    auto longDescription=root;longDescription.children[0].data.resize(56);text(longDescription.children[0],24,"1234567890123456789012345678901");longDescription.children.resize(1);assert(Maps::parse(longDescription).map("Fixture").description=="1234567890123456789012345678901");
    rejects([&]{(void)maps.map("Missing");});
    auto bad=root;bad.children[0].children[0].data.pop_back();rejects([&]{(void)Maps::parse(bad);});
    bad=root;word(bad.children[0].children[1],0,4);rejects([&]{(void)Maps::parse(bad);});
    bad=root;word(bad.children[0].children[1],0,65535);rejects([&]{(void)Maps::parse(bad);});
    bad=root;real(bad.children[0].children[1],2,std::numeric_limits<float>::quiet_NaN());rejects([&]{(void)Maps::parse(bad);});
    // Independent known points, negative coordinates and camera offsets. Both
    // device display configurations preserve the same HD world coordinates.
    for(const Size size:{Size{480,320},Size{768,1024}}){
        const MapCamera camera{{-301,430},{7,-11},size};const auto p=camera.toScreen({-284,-79});same(p.x,10+size.width/2);same(p.y,-498+size.height/2);
        for(const Vec2 world:{Vec2{-512,-512},Vec2{512,512},Vec2{0,0},Vec2{-.5f,1.25f}}){const auto roundtrip=camera.toWorld(camera.toScreen(world));same(world.x,roundtrip.x);same(world.y,roundtrip.y);}
    }
    rejects([]{(void)MapCamera{{},{},{0,320}}.toWorld({});});
}

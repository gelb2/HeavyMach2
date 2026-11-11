#include "Core/Simulation/TerrainCollision.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
using namespace hm2;
void same(float a,float b){assert(std::abs(a-b)<.0001f);}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(...){caught=true;}assert(caught);}
int main(){
    const auto edge=TerrainEdge::fromPoints({0,0},{100,0});same(edge.length,100);same(edge.normal.x,0);same(edge.normal.y,-1);
    same(edge.overlap({50,8},16),8);same(edge.overlap({50,-8},16),8);same(edge.overlap({50,16},16),0);same(edge.overlap({50,16.01f},16),0);
    same(edge.overlap({-.01f,0},16),0);same(edge.overlap({0,0},16),16);same(edge.overlap({116,0},16),16);same(edge.overlap({116.01f,0},16),0);
    rejects([]{(void)TerrainEdge::fromPoints({1,1},{1,1});});rejects([&]{(void)edge.overlap({},-1);});
    Map map;map.collisionPolygons={{{0,-100},{0,100},{100,100},{100,-100}}};const TerrainCollision terrain(map);
    assert(terrain.edges().size()==4&&terrain.edges()[0].previous==-1&&terrain.edges()[0].next==1&&terrain.edges()[3].previous==2&&terrain.edges()[3].next==-1);
    auto contact=terrain.resolve({-8,0});same(contact.position.x,-16);same(contact.position.y,0);assert(contact.current==0&&contact.corrections==1);
    contact=terrain.resolve({-16,0},2);same(contact.position.x,-16);assert(contact.current==-1&&contact.previous==2&&contact.corrections==0);
    // Both cells on the x=128 boundary retain nearby edges; lookup truncates
    // just below -512 toward zero exactly as the source ARM conversion does.
    map.collisionPolygons={{{-512,-512},{-512,-400},{-400,-400},{-400,-512}}};const TerrainCollision corner(map);
    assert(!corner.candidates({-512,-512}).empty());assert(!corner.candidates({-512.25f,-512.25f}).empty());assert(corner.candidates({-640,0}).empty());assert(corner.candidates({512,0}).empty());
    rejects([&]{(void)corner.resolve({std::numeric_limits<float>::quiet_NaN(),0});});
}

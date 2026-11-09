#include "Core/Simulation/TerrainCollision.hpp"
#include <cmath>
#include <stdexcept>

namespace hm2 {
namespace {
float dot(Vec2 a,Vec2 b){return a.x*b.x+a.y*b.y;}
void finite(Vec2 p){if(!std::isfinite(p.x)||!std::isfinite(p.y))throw std::runtime_error("Nonfinite terrain coordinate");}
}
TerrainEdge TerrainEdge::fromPoints(Vec2 a,Vec2 b){
    finite(a);finite(b);TerrainEdge e;e.origin=a;e.second=b;
    e.dir={b.x-a.x,b.y-a.y};e.length=std::sqrt(dot(e.dir,e.dir));
    if(!std::isfinite(e.length)||e.length<=0)throw std::runtime_error("Degenerate terrain edge");
    e.dir.x/=e.length;e.dir.y/=e.length;
    e.normal={e.dir.y,-e.dir.x};return e;
}
float TerrainEdge::overlap(Vec2 p,float radius)const{
    finite(p);if(!std::isfinite(radius)||radius<0)throw std::runtime_error("Invalid terrain radius");
    const Vec2 relative{p.x-origin.x,p.y-origin.y};const float projection=dot(relative,dir);
    if(projection>length+radius||projection<0)return 0;
    const float depth=radius-std::abs(dot(normal,relative));return depth<0?0:depth;
}
TerrainCollision::TerrainCollision(const Map& map){
    for(const auto& polygon:map.collisionPolygons){
        if(polygon.size()<2)continue;
        const auto start=edges_.size();
        for(std::size_t i=1;i<polygon.size();++i)edges_.push_back(TerrainEdge::fromPoints(polygon[i-1],polygon[i]));
        edges_.push_back(TerrainEdge::fromPoints(polygon.back(),polygon.front()));
        // Original readColBox links sequential edges plus closing edge. It does
        // not link the closing edge back to the first with setPrev/setNext.
        for(std::size_t i=start;i<edges_.size();++i){if(i>start)edges_[i].previous=int(i-1);if(i+1<edges_.size())edges_[i].next=int(i+1);}
    }
    // GobWorld::addColVector0x11534. Preserve the source fourth duplicated
    // sample (96,32); raw literal table at0x50b9c, not an assumed square.
    constexpr Vec2 samples[]={{32,32},{96,32},{96,96},{96,32}};
    for(unsigned i=0;i<edges_.size();++i){const auto& e=edges_[i];
        const Vec2 end{e.origin.x+e.dir.x*e.length,e.origin.y+e.dir.y*e.length};
        for(unsigned x=0;x<8;++x)for(unsigned y=0;y<8;++y){
            const float bx=float(x)*128-512,by=float(y)*128-512;
            const auto contained=[&](Vec2 p){return p.x>=bx-64&&p.x<=bx+196&&p.y>=by-64&&p.y<=by+196;};
            bool present=contained(e.origin)||contained(end);
            if(!present)for(const auto p:samples)if(e.overlap({bx+p.x,by+p.y},128)!=0){present=true;break;}
            if(present)cells_[y*8+x].push_back(i);
        }
    }
}
std::span<const unsigned> TerrainCollision::candidates(Vec2 p)const{
    finite(p);const float x=(p.x+512)*.0078125f,y=(p.y+512)*.0078125f;
    // Original conversion truncates toward zero, including just below -512.
    if(x<=-1||x>=8||y<=-1||y>=8)return {};
    return cells_[std::size_t(int(y)*8+int(x))];
}
TerrainContact TerrainCollision::resolve(Vec2 p,int previous)const{
    TerrainContact out{p,-1,previous,0};const auto selected=candidates(p);
    for(auto i=selected.rbegin();i!=selected.rend();++i){const auto& e=edges_[*i];const float depth=e.overlap(out.position,16);
        if(depth!=0){out.position.x-=e.normal.x*depth;out.position.y-=e.normal.y*depth;out.previous=out.current;out.current=int(*i);++out.corrections;}
    }
    return out;
}
}

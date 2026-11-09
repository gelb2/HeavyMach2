#include "Core/Data/Maps.hpp"
#include <bit>
#include <cmath>
#include <stdexcept>

namespace hm2 {
namespace {
int signed16(const Chunk& c,std::size_t at){const std::uint16_t word=std::uint16_t(c.byte(at))|std::uint16_t(std::uint16_t(c.byte(at+1))<<8);return std::bit_cast<std::int16_t>(word);}
Vec2 point(const Chunk& c,std::size_t at){const Vec2 p{c.real32(at),c.real32(at+4)};if(!std::isfinite(p.x)||!std::isfinite(p.y))throw std::runtime_error("Nonfinite map vertex");return p;}
}
Maps Maps::parse(Chunk root){
    if(root.type!=0||!root.data.empty())throw std::runtime_error("Unexpected HM2 map root");
    Maps out;
    for(const auto& c:root.children){
        if(c.type!=1)continue; // Original dictionary ignores other root records.
        if(c.data.size()!=56)throw std::runtime_error("Invalid HM2 map payload");
        Map m;m.name=c.text(0,24);m.description=c.text(24,32);
        if(m.name.empty())throw std::runtime_error("Empty HM2 map ID");
        for(const auto& child:c.children){
            if(child.type==2){
                if(child.data.size()!=25||!child.children.empty())throw std::runtime_error("Invalid HM2 gate payload");
                m.gates.push_back({signed16(child,0),signed16(child,22),{float(signed16(child,2)),float(signed16(child,4))},child.text(6,16),child.byte(24)!=0});
            }else if(child.type==3){
                if(child.data.size()<2||!child.children.empty())throw std::runtime_error("Invalid HM2 collision payload");
                const auto count=signed16(child,0);
                if(count<0||child.data.size()!=2+std::size_t(count)*8)throw std::runtime_error("Invalid HM2 collision vertex count");
                std::vector<Vec2> polygon;polygon.reserve(std::size_t(count));
                for(int i=0;i<count;++i)polygon.push_back(point(child,2+std::size_t(i)*8));
                m.collisionPolygons.push_back(std::move(polygon));
            }else if(child.type==4){
                if(child.data.size()!=22||!child.children.empty())throw std::runtime_error("Invalid HM2 spawner payload");
                m.spawners.push_back({child.text(2,16),{float(signed16(child,18)),float(signed16(child,20))}});
            } // Unknown children remain raw and are skipped by original openMap.
        }
        // Original NSMutableDictionary inserts by name: last matching row wins.
        out.maps_.insert_or_assign(m.name,std::move(m));
    }
    out.raw_=std::move(root);return out;
}
const Map& Maps::map(std::string_view name)const{auto i=maps_.find(name);if(i==maps_.end())throw std::runtime_error("Unresolved HM2 map: "+std::string(name));return i->second;}
}

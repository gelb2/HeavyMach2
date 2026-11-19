#pragma once
#include "Core/Data/Chunk.hpp"
#include "Core/Types.hpp"
#include <map>
#include <string_view>

namespace hm2 {
struct MapGate {
    int number=0,linkGate=0;
    Vec2 position;
    std::string linkMap;
    bool eventOnly=false;
};
struct MapSpawner {std::string model;Vec2 position;float activationRadius=0;};
struct Map {
    std::string name,description;
    std::vector<MapGate> gates;
    std::vector<std::vector<Vec2>> collisionPolygons;
    std::vector<MapSpawner> spawners;
};
// GlobalInfo::openMapInfo 0x154a4 / GobWorld::openMap 0x14120.
// Raw bytes, unused fields and stale size headers stay in raw(). Gates, collision
// vertices and spawner coordinates use their own original readers, not imagery.
class Maps {
    Chunk raw_;
    std::map<std::string,Map,std::less<>> maps_;
public:
    static Maps parse(Chunk root);
    [[nodiscard]] const auto& all()const{return maps_;}
    [[nodiscard]] const Chunk& raw()const{return raw_;}
    [[nodiscard]] const Map& map(std::string_view)const;
};
}

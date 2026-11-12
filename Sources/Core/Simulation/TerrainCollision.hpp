#pragma once
#include "Core/Data/Maps.hpp"
#include <array>
#include <span>

namespace hm2 {
struct TerrainEdge {
    Vec2 origin,second,dir,normal;
    float length=0;
    int previous=-1,next=-1;
    static TerrainEdge fromPoints(Vec2,Vec2);
    // HD ColVector::checkCollide:ColRadius:0x3bf18. Asymmetric segment
    // projection and unsigned normal distance are intentional original rules.
    [[nodiscard]] float overlap(Vec2,float radius)const;
};
struct TerrainContact {Vec2 position;int current=-1,previous=-1;unsigned corrections=0;};
class TerrainCollision {
    std::vector<TerrainEdge> edges_;
    std::array<std::vector<unsigned>,64> cells_;
public:
    explicit TerrainCollision(const Map&);
    [[nodiscard]] const auto& edges()const{return edges_;}
    [[nodiscard]] std::span<const unsigned> candidates(Vec2)const;
    // Matches source cell snapshot/reverse order; no generic physics solver.
    [[nodiscard]] TerrainContact resolve(Vec2,int priorPrevious=-1)const;
};
}

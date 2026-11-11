#pragma once
#include "Core/Presentation/MapPreview.hpp"
#include "Core/Presentation/TileImage.hpp"
#include "Core/Simulation/WorldRandom.hpp"
#include <array>
#include <span>

namespace hm2 {
struct TrailConfig {
    float width=1,length=64;
    unsigned blocks=16;
    float fadeDelay=0,fadeTime=.2f;
};
struct TrailState {
    const TileImage* tile=nullptr;
    Vec2 position;
    float wind=1,diffuse=1;
    int blendType=0;
    bool show=true,removed=false;
};
struct TrailPoint {Vec2 position,normal;float timestamp=0;};
struct TrailRow {TrailPoint point;std::array<StripVertex,2> vertices;};
// GobTrailEffect 0x1eadc/0x1ec08/0x1f100/0x1ed38/0x1eff0. Rows retain
// the original committed-point count plus one replaceable terminal record.
// Degenerate zero-length normals deliberately remain IEEE NaNs.
class Trail {
    TrailState state_;
    TrailConfig config_;
    std::vector<TrailRow> rows_;
    std::size_t last_=0;
    bool odd_=false,hasPoints_=false;
    void addValid(Vec2 local,bool commit,double time,WorldRandom&);
    void updateVertices(std::size_t);
public:
    Trail(const TileImage* tile,Vec2 position,TrailConfig config={});
    void configure(TrailConfig);
    [[nodiscard]] TrailState& state(){return state_;}
    [[nodiscard]] const TrailState& state()const{return state_;}
    [[nodiscard]] const TrailConfig& config()const{return config_;}
    [[nodiscard]] std::size_t lastPosition()const{return last_;}
    [[nodiscard]] bool oddCoordinate()const{return odd_;}
    [[nodiscard]] std::span<const TrailRow> rows()const{return {rows_.data(),hasPoints_?last_+1:0};}
    void add(Vec2 worldPosition,double worldTime,WorldRandom&);
    void tick(double worldTime,bool paused,WorldRandom&);
    void append(Frame&,const MapCamera&)const;
};
}

#pragma once
#include "Core/Types.hpp"
namespace hm2 {
// A prepared original Tile2D resource. Pixel dimensions and split grid remain
// distinct from each consumer's original game/display scale.
struct TileImage {
    std::string name;
    Size pixels;
    unsigned columns=1,rows=1;
    Vec2 origin;
    bool forRetina=false;
    unsigned pixelFormat=1;
};
}

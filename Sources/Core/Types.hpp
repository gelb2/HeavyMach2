#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace hm2 {
struct Vec2 {float x=0,y=0;};
struct Size {float width=0,height=0;};
struct Sprite {
    std::string image;
    Vec2 center;
    Size size;
    float angle=0; // radians, original positive-Y-up coordinates
    unsigned tile=0,columns=1;
    bool reverseY=false;
};
struct Frame {
    float width=768,height=1024;
    std::vector<Sprite> sprites;
};
}

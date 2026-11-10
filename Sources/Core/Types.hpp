#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace hm2 {
struct AtlasBatch;
struct Vec2 {float x=0,y=0;};
struct Size {float width=0,height=0;};
struct Color {float r=1,g=1,b=1,a=1;};
// Compositor viewport for independent validation scenes. Normal game sprites
// leave this empty. Coordinates use the containing Frame's positive-Y-up space.
struct ClipRect {float x=0,y=0,width=0,height=0;bool operator==(const ClipRect&)const=default;};
// Explicit GL factors, resolved by the original Tile2D/trail consumer. Their
// numeric blendType0/3 meanings differ and must not be merged into one enum.
enum class BlendFactor {zero,one,sourceAlpha,oneMinusSourceAlpha,sourceColor};
struct Blend {BlendFactor source=BlendFactor::one,destination=BlendFactor::oneMinusSourceAlpha;};
struct StripVertex {Vec2 position,uv;Color color={};};
struct Sprite {
    std::string image;
    Vec2 center;
    Size size;
    float angle=0; // radians, original positive-Y-up coordinates
    unsigned tile=0,columns=1;
    bool reverseY=false;
    unsigned rows=1;
    Color color={};
    Blend blend={};
    // GobTrailEffect uses the same prepared texture with an ordered triangle
    // strip. Empty vertices retain the existing original sprite path.
    std::vector<StripVertex> strip={};
    std::shared_ptr<const AtlasBatch> atlasBatch={};
    std::optional<ClipRect> clip={};
};
struct Frame {
    float width=768,height=1024;
    std::vector<Sprite> sprites;
};
}

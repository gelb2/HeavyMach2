#pragma once
#include "Core/Types.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace hm2 {
struct VisualTile {
    // Identity of the prepared original Tile2D instance. Equal file names do
    // not by themselves establish original object identity.
    std::string identity,image;
    Size content,storage;
    unsigned columns=1,rows=1,pixelFormat=1;
    bool forRetina=false;
    Vec2 origin{};
    Color color{};
    TextureSource textureSource={};
};
struct VisualImage {
    VisualTile tile;
    unsigned tileNo=0,blendType=0,reverse=0;
    Vec2 screenPosition,scale{1,1};
    float angle=0,alpha=1;
    bool useAtlas=false,show=true,isInScreen=true;
};
struct ColorBytes {
    std::uint8_t r=255,g=255,b=255,a=255;
    bool operator==(const ColorBytes&)const=default;
};
// Original TQuadInfo layout: float XY, RGBA unsigned bytes, float UV.
struct AtlasVertex {Vec2 position;ColorBytes color;Vec2 uv;};
struct AtlasQuad {std::array<AtlasVertex,4> vertices;}; // TL, BL, TR, BR
static_assert(sizeof(AtlasVertex)==20&&offsetof(AtlasVertex,color)==8&&offsetof(AtlasVertex,uv)==12);
static_assert(sizeof(AtlasQuad)==80);
struct AtlasBatch {
    std::string tileIdentity,image;
    unsigned blendType=0,pixelFormat=1,capacity=32;
    Size content,storage;
    Blend blend;
    std::vector<AtlasQuad> quads;
    std::vector<std::uint16_t> indices; // Six indices per live quad.
    TextureSource textureSource={};
};
struct VisualLayerFrame {
    std::vector<Sprite> nonAtlas;
    std::vector<AtlasBatch> atlases; // Persistent group creation order.
    // Compatibility collector for the existing Sprite GPU consumer. Explicit
    // UVs are converted to content-normalized coordinates because that consumer
    // already applies content/POT in its texture matrix. Native batch consumers
    // use AtlasBatch UVs directly and must not crop a second time.
    void append(Frame&)const;
    // Native GPU batches preserve original POT UV, byte colors and indexed
    // triangle geometry. Renderer must not apply its content UV crop again.
    void appendBatches(Frame&)const &;
    void appendBatches(Frame&)&&;
};
[[nodiscard]] Blend originalTileBlend(unsigned type,unsigned pixelFormat);
[[nodiscard]] Blend originalAtlasBlend(unsigned type,unsigned pixelFormat);
[[nodiscard]] std::uint8_t originalAtlasColorByte(float);
[[nodiscard]] AtlasQuad originalAtlasQuad(const VisualImage&);

// QVisualLayer's default zOrderSort=false path. It draws registered non-atlas
// objects first, then persistent atlas groups in their original creation order.
// Scene/camera resolves screen positions and the previous-tick isInScreen gate.
// No renderer, OS views, game logic, or camera policy is introduced here.
class VisualLayer {
    struct Group {VisualTile tile;unsigned blendType=0,capacity=32;std::vector<AtlasQuad> quads;};
    std::vector<Sprite> nonAtlas_;
    std::vector<Group> groups_;
    unsigned unitSize_,visualCapacity_;
    std::size_t group(const VisualTile&,unsigned);
public:
    explicit VisualLayer(unsigned unitSize=100);
    // Equivalent to makeCleanAtlas: (which does not clear an existing atlas).
    void prepareAtlas(const VisualTile&);
    // Existing non-atlas consumers may provide their fully resolved commands.
    void submitNonAtlas(Sprite);
    // Atlas submission updates tileNo when the original clamps to the last tile.
    // Non-atlas draw clamps its local index and leaves the object's index alone.
    void submitImage(VisualImage&);
    [[nodiscard]] std::size_t atlasCount()const{return groups_.size();}
    [[nodiscard]] unsigned atlasCapacity(std::size_t)const;
    [[nodiscard]] unsigned visualCapacity()const{return visualCapacity_;}
    [[nodiscard]] std::size_t visualCount()const{return nonAtlas_.size();}
    void clearVisuals(){nonAtlas_.clear();}
    void removeAllAtlases(){groups_.clear();}
    VisualLayerFrame drain(); // Copies live commands, then resets counts only.
};
}

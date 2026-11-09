#pragma once
#include "Core/Data/Models.hpp"
#include <cstdint>
#include <map>
#include <span>

namespace hm2 {
using Bytes=std::vector<std::uint8_t>;
struct ResourceAccess {
    virtual ~ResourceAccess()=default;
    virtual Bytes read(std::string_view name)=0;
};
struct AssemblyCase {std::string model;Vec2 position;float machineAngle=0,weaponAngle=0;};
// First restoration component, deliberately not a gameplay/level-up simulator.
// Uses the original common individual Tile2D fallback; atlas/shadows are pending.
class AssemblyPreview {
    Models models_;
    std::map<std::string,Size,std::less<>> imageSizes_;
    Frame frame_;
public:
    AssemblyPreview(ResourceAccess&,std::span<const AssemblyCase>,Size viewport);
    [[nodiscard]] const Models& models()const{return models_;}
    [[nodiscard]] const Frame& frame()const{return frame_;}
    [[nodiscard]] Frame compose(std::span<const AssemblyCase>,Size viewport)const;
    [[nodiscard]] static Size pngSize(std::span<const std::uint8_t>);
};
}

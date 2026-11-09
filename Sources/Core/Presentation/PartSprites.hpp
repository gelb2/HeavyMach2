#pragma once
#include "Core/Data/Models.hpp"
#include "Core/Data/Resources.hpp"
#include <optional>

namespace hm2 {
// Shared individual Tile2D fallback for MCB models and player equipment.
class PartSprites {
    std::map<std::string,std::optional<Size>,std::less<>> sizes_;
public:
    void prepare(ResourceAccess&,std::span<const PartPose>);
    void append(Frame&,std::span<const PartPose>,Vec2 position)const;
};
}

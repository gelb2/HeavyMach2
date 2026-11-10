#pragma once
#include "Core/Data/Models.hpp"
#include "Core/Data/Machines.hpp"
#include "Core/Data/Resources.hpp"
#include "Core/Presentation/PreparedTiles.hpp"
#include "Core/Presentation/Visibility.hpp"

namespace hm2 {
// Shared individual Tile2D fallback for MCB models and player equipment.
class PartSprites {
    PreparedTiles tiles_{"preview-parts"};
public:
    void prepare(ResourceAccess&,std::span<const PartPose>);
    void append(Frame&,std::span<const PartPose>,Vec2 position)const;
};
// GobMachParts child tick. Position is already the original part's world
// center; only translation is inherited. Each child retains its own cull cache.
void collectOriginalPartShadow(VisualLayer&,const PreparedTiles&,const PartPose&,
                               Vec2 partWorldCenter,const MapCamera*,VisibilityState&,
                               MachineKind ownerKind=MachineKind::bipod);
// setPartsInfo bounds are prepared once from unrotated raw socket0.
[[nodiscard]] VisibilityState originalPartVisibility(const TileImage*,const Part&);
// Original setHvMach: type0 body is direct; nonzero types use atlas. The
// direct path consumes the refreshed cull state, the atlas path its prior one.
void collectOriginalPart(VisualLayer& bodyLayer,VisualLayer& shadowLayer,const PreparedTiles&,
                         const PartPose&,Vec2 partWorldCenter,const MapCamera*,
                         VisibilityState& body,VisibilityState& shadow,MachineKind);
}

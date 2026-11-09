#pragma once
#include "Core/Data/PlayerEquipment.hpp"
#include "Core/Presentation/MapPreview.hpp"
#include "Core/Presentation/PartSprites.hpp"

namespace hm2 {
struct PlayerAssemblyCase {
    EquipmentCodes equipment=PlayerEquipment::startingCodes;
    Vec2 position;
    float machineAngle=0;
    PlayerPartAngles angles;
};
// Map and equipped Player snapshots; no AI, movement, combat or progression.
class PlayerMapPreview {
    MapPreview map_;
    Models models_;
    PlayerEquipment equipment_;
    PartSprites sprites_;
public:
    PlayerMapPreview(ResourceAccess&,std::string_view map,std::span<const PlayerAssemblyCase> preparedCases);
    [[nodiscard]] const MapPreview& map()const{return map_;}
    [[nodiscard]] const Models& models()const{return models_;}
    [[nodiscard]] Frame compose(const MapCamera&,std::span<const PlayerAssemblyCase>)const;
};
}

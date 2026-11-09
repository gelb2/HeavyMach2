#include "Core/Presentation/PlayerMapPreview.hpp"

namespace hm2 {
PlayerMapPreview::PlayerMapPreview(ResourceAccess& resources,std::string_view map,std::span<const PlayerAssemblyCase> cases)
:map_(resources,map),models_(Models::parse(Chunk::parse(resources.read("PartsInfo.mcb")),Chunk::parse(resources.read("MachInfo.mcb")))),equipment_(PlayerEquipment::parse(DataTable::parse(resources.read("ItemInfo.tble")))){
    for(const auto& c:cases)sprites_.prepare(resources,models_.mountedPose(equipment_.mounts(models_,c.equipment,c.angles,c.offsets),c.machineAngle));
}
Frame PlayerMapPreview::compose(const MapCamera& camera,std::span<const PlayerAssemblyCase> cases)const{
    auto frame=map_.compose(camera);
    for(const auto& c:cases)sprites_.append(frame,models_.mountedPose(equipment_.mounts(models_,c.equipment,c.angles,c.offsets),c.machineAngle),camera.toScreen(c.position));
    return frame;
}
}

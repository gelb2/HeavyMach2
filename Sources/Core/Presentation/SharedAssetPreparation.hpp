#pragma once
#include "Core/Presentation/PreparedTiles.hpp"
#include "Core/Data/Weapons.hpp"
#include <vector>

namespace hm2 {
// GlobalInfo.openWeaponInfo0x14bc4 prepares all weapon assets once in the
// shared manager. The original table order is retained, including duplicate
// filename requests; PreparedTiles/SoundManager own their cache hits.
void prepareOriginalWeaponAssets(ResourceAccess&,PreparedTiles&,const Weapons&);
[[nodiscard]] std::vector<std::string> originalStartupSoundNames(const Weapons&);
}

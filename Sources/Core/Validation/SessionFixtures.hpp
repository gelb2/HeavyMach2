#pragma once
#include "Core/Data/SaveData.hpp"
#include "Core/Progression/Progression.hpp"

namespace hm2::validation {
// Original-format starting data for a real damage/OS-alert/return-to-base
// validation. It never mutates a live actor or injects a death result.
inline SaveData gameOverFixture(std::uint32_t timestamp){
    SaveData data;data.initializeSlot(0,"GAMEOVER","kr",timestamp);
    progression::SlotView slot(data.slot(0));slot.hp(1);slot.u32(0x1b5c,5);
    slot.string(0x2d,16,"Stage01_01");slot.string(0x55,16,"Base002");
    slot.u32(0x64c,50);slot.u32(0x650,31);slot.u32(0x654,17);
    return data;
}
}

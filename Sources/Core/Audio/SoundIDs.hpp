#pragma once
#include <array>
#include <stdexcept>
#include <string_view>
namespace hm2::audio {
// GlobalInfo::init0x15ef4 writes these slots in a different load order.
inline std::string_view soundFilename(unsigned id){
    static constexpr std::array<std::string_view,22> files={
        "UI_Click.wav","UI_Btn_OK.wav","UI_Btn_Cancel.wav","Game_GetItem.wav","Game_ChangeWeapon.wav","Game_WeaponEnd.wav",
        "Title_TapToStart.wav","Game_LevelUp.wav","Game_LowHP.wav","SFX_AirStrike.wav","SFX_OnMessage.wav","SFX_MissionComplete.wav",
        "SFX_QuickSlot.wav","SFX_Repair.wav","SFX_Dash.wav","SFX_HitBody01.wav","SFX_HitBody02.wav","SFX_HitBody03.wav","SFX_HitBody04.wav",
        "SFX_GroundHit.wav","SFX_ExplGround.wav","SFX_ExplMach.wav"};
    if(id>=files.size())throw std::out_of_range("Original global SFX ID");return files[id];
}
}

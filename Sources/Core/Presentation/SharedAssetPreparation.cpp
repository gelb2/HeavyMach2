#include "Core/Presentation/SharedAssetPreparation.hpp"
#include "Core/Audio/SoundIDs.hpp"

namespace hm2 {
void prepareOriginalWeaponAssets(ResourceAccess& resources,PreparedTiles& shared,const Weapons& weapons){
    const auto& table=weapons.raw();
    for(std::size_t row=0;row<table.rowCount();++row){
        const auto field=[&](std::string_view key){return std::string(table.value(row,key).value_or(""));};
        const auto bullet=field("BltTile");if(bullet!="-")shared.prepare(resources,"Blt_"+bullet+".png");
        const auto effect=field("EffType");
        if(effect=="Trail"||effect=="Laser"||effect=="Rail"||effect=="Flame"||effect=="Shotgun")
            if(auto* tile=shared.prepare(resources,field("EffTile")+".png")){tile->columns=effect=="Rail"?4:1;tile->rows=1;}
        const auto explode=field("ExplodeEff");if(explode!="-")shared.prepare(resources,"Efx_"+explode+".png");
    }
}
std::vector<std::string> originalStartupSoundNames(const Weapons& weapons){
    std::vector<std::string> result;const auto& table=weapons.raw();result.reserve(table.rowCount()+22);
    for(std::size_t row=0;row<table.rowCount();++row)result.push_back(std::string(table.value(row,"FireSound").value_or(""))+".wav");
    // GlobalInfo.init0x15ef4 writes these SFX slots after openWeaponInfo.
    for(unsigned id:{0u,1u,2u,3u,4u,5u,6u,10u,11u,12u,13u,7u,8u,14u,9u,15u,16u,17u,18u,19u,20u,21u})result.emplace_back(audio::soundFilename(id));
    return result;
}
}

#include "Core/Presentation/SharedAssetPreparation.hpp"
#include "Core/Audio/SoundIDs.hpp"
#include <cassert>
using namespace hm2;
struct Files:ResourceAccess {
    std::vector<std::string> reads;
    bool exists(std::string_view)const override{return true;}
    Bytes read(std::string_view name)override{
        reads.emplace_back(name);
        return {137,80,78,71,13,10,26,10,0,0,0,13,'I','H','D','R',0,0,0,30,0,0,0,17,8,6,0,0,0,0,0,0,0};
    }
};
int main(){
    const auto weapons=Weapons::parse(DataTable::parseText(
        "Parts\tType\tDmg\tRange\tShtRng\tShtCnt\tShtDly\tSpeed\tAimRt\tRldDly\tBltTile\tEffType\tEffTile\tExplodeEff\tExplScl\tShell\tUiTile\tFireSound\n"
        "Z-last-key\tGun\t1\t1\t1\t1\t1\t1\t1\t1\tshared\tRail\trail\tx\t1\t0\t0\tZSound\n"
        "A-first-key\tGun\t1\t1\t1\t1\t1\t1\t1\t1\tshared\tLaser\tbeam\tx\t1\t0\t0\tASound\n"));
    Files files;PreparedTiles global("global");prepareOriginalWeaponAssets(files,global,weapons);
    const std::vector<std::string> expected={"Blt_shared.png","rail.png","Efx_x.png","beam.png"};
    assert(files.reads==expected&&global.all().size()==4);
    assert(global.lookup("rail.png")->columns==4&&global.lookup("beam.png")->columns==1);
    const auto revision=global.revision();prepareOriginalWeaponAssets(files,global,weapons);
    assert(files.reads==expected&&global.revision()==revision); // Shared cache reuse across World creation.
    const auto sounds=originalStartupSoundNames(weapons);assert(sounds.size()==24);
    assert(sounds[0]=="ZSound.wav"&&sounds[1]=="ASound.wav"); // Table order, not sorted Parts map.
    const unsigned ids[]={0,1,2,3,4,5,6,10,11,12,13,7,8,14,9,15,16,17,18,19,20,21};
    for(unsigned i=0;i<22;++i)assert(sounds[i+2]==audio::soundFilename(ids[i]));
}

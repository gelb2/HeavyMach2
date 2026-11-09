#pragma once
#include "Core/Data/DataTable.hpp"
#include <map>

namespace hm2 {
enum class WeaponKind:std::int16_t {gun,shotgun,laser,railgun,bomb,missile,sniper,nuclear,bullet};
enum class WeaponEffect:std::int16_t {none=0,flame=2,trail=4,laser=5,rail=6,shotgun=7};
struct WeaponInfo {
    std::string part;
    WeaponKind kind=WeaponKind::gun;
    bool recognizedKind=false;
    std::int16_t damage=0,damageRange=0,shotRange=0,shell=0;
    std::uint8_t shotCount=0,uiTile=0;
    float shotDelay=0,reloadDelay=0,speed=0,aimRate=0,explodeScale=0;
    WeaponEffect effect=WeaponEffect::none;
    unsigned effectColumns=1;
    std::string bulletImage,effectImage,explodeImage,fireSound;
    int attackPoint=0;
};
// GlobalInfo::openWeaponInfo0x14bc4 data consumer. Resource names are retained;
// preparing textures/OpenAL and ticking controllers are separate pending steps.
class Weapons {
    DataTable table_;
    std::map<std::string,WeaponInfo,std::less<>> weapons_;
public:
    static Weapons parse(DataTable);
    [[nodiscard]] const auto& all()const{return weapons_;}
    [[nodiscard]] const DataTable& raw()const{return table_;}
    [[nodiscard]] const WeaponInfo& weapon(std::string_view part)const;
};
}

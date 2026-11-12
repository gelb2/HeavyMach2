#include "Core/Data/Weapons.hpp"
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace hm2 {
namespace {
std::string string(const DataTable& t,std::size_t r,std::string_view key){return std::string(t.value(r,key).value_or(""));}
template<class Number>Number number(const DataTable& t,std::size_t r,std::string_view key){
    if constexpr(std::is_integral_v<Number>)return t.integer(r,key);
    else return t.real(r,key);
}
std::int16_t small(int n){return std::bit_cast<std::int16_t>(std::uint16_t(n));}
}
Weapons Weapons::parse(DataTable table){
    Weapons out;
    constexpr std::string_view types[]={"Gun","Shotgun","Laser","Railgun","Bomb","Missile","Sniper","Nuclear","Bullet"};
    for(std::size_t row=0;row<table.rowCount();++row){
        WeaponInfo w;w.part=string(table,row,"Parts");if(w.part.empty())throw std::runtime_error("Empty weapon part ID");
        const auto type=string(table,row,"Type");
        for(unsigned i=0;i<std::size(types);++i)if(type==types[i]){w.kind=WeaponKind(i);w.recognizedKind=true;break;}
        // Original unknown type leaves the ObjC zero-initialized field atGun.
        w.damage=small(number<int>(table,row,"Dmg"));w.damageRange=small(number<int>(table,row,"Range"));
        const float range=float(number<int>(table,row,"ShtRng"));const float doubled=range+range;
        if(!std::isfinite(doubled)||double(doubled)>std::numeric_limits<int>::max()||double(doubled)<std::numeric_limits<int>::min())throw std::runtime_error("Weapon range outside source integer domain");
        w.shotRange=small(int(doubled));w.shotCount=std::uint8_t(number<int>(table,row,"ShtCnt"));
        w.shotDelay=number<float>(table,row,"ShtDly");w.reloadDelay=number<float>(table,row,"RldDly");w.speed=number<float>(table,row,"Speed");w.aimRate=number<float>(table,row,"AimRt");
        const auto bullet=string(table,row,"BltTile");if(bullet!="-")w.bulletImage="Blt_"+bullet+".png";
        const auto effect=string(table,row,"EffType");
        if(effect=="Trail")w.effect=WeaponEffect::trail;else if(effect=="Laser")w.effect=WeaponEffect::laser;
        else if(effect=="Rail"){w.effect=WeaponEffect::rail;w.effectColumns=4;}
        else if(effect=="Flame")w.effect=WeaponEffect::flame;else if(effect=="Shotgun")w.effect=WeaponEffect::shotgun;
        if(w.effect!=WeaponEffect::none)w.effectImage=string(table,row,"EffTile")+".png";
        const auto explosion=string(table,row,"ExplodeEff");
        if(explosion!="-"){w.explodeImage="Efx_"+explosion+".png";w.explodeScale=number<float>(table,row,"ExplScl");}
        w.shell=small(number<int>(table,row,"Shell"));w.uiTile=std::uint8_t(number<int>(table,row,"UiTile"));w.fireSound=string(table,row,"FireSound")+".wav";
        const float duration=float(w.shotCount)*w.shotDelay+w.reloadDelay;
        const float score=(1.f/duration)*float(int(w.shotCount)*int(w.damage))+.5f;
        if(std::isfinite(score)&&double(score)<=std::numeric_limits<int>::max()&&double(score)>=std::numeric_limits<int>::min())w.attackPoint=int(score);
        else throw std::runtime_error("Weapon attack score outside source integer domain");
        out.weapons_.insert_or_assign(w.part,std::move(w));
    }
    out.table_=std::move(table);return out;
}
const WeaponInfo& Weapons::weapon(std::string_view part)const{const auto i=weapons_.find(part);if(i==weapons_.end())throw std::runtime_error("Unresolved weapon part: "+std::string(part));return i->second;}
}

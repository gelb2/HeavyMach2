#include "Core/Data/Weapons.hpp"
#include "Core/Data/Models.hpp"
#include <cassert>
#include <cmath>
#include <numbers>
using namespace hm2;
void same(float a,float b){assert(std::abs(a-b)<.0001f);}
int main(){
    const auto table=DataTable::parseText("Parts\tType\tDmg\tRange\tShtRng\tShtCnt\tShtDly\tRldDly\tSpeed\tAimRt\tBltTile\tEffType\tEffTile\tExplodeEff\tExplScl\tShell\tFireSound\tUiTile\nFixture\tGun\t7\t0\t200\t10\t0.16\t2.5\t1\t1\t-\t-\t-\t-\t-\t0\tWpn_Gun01\t0\nFixture\tMissile\t26\t60\t300\t2\t0.5\t2\t0.2\t1\tMissile01\tTrail\tEfx_Trail01\tExplodeGround\t1\t0\tWpn_Missile01\t0\nLargeRange\tGun\t7\t0\t16777217\t258\t0.25suffix\t1\t0.2\t1\t-\tRail\tEfx_Rail\t-\t-\t-2\tWpn_Gun01\t258\n");
    const auto weapons=Weapons::parse(table);assert(weapons.all().size()==2&&weapons.raw().decoded()==table.decoded());
    const auto& w=weapons.weapon("Fixture");assert(w.kind==WeaponKind::missile&&w.damage==26&&w.damageRange==60&&w.shotRange==600&&w.shotCount==2&&w.attackPoint==17);same(w.shotDelay,.5);same(w.reloadDelay,2);same(w.speed,.2);assert(w.bulletImage=="Blt_Missile01.png"&&w.effectImage=="Efx_Trail01.png"&&w.explodeImage=="Efx_ExplodeGround.png"&&w.fireSound=="Wpn_Missile01.wav");
    const auto& large=weapons.weapon("LargeRange");assert(large.shotRange==0&&large.shotCount==2&&large.uiTile==2&&large.shell==-2&&large.effectColumns==4&&large.effect==WeaponEffect::rail);same(large.shotDelay,.25);assert(large.bulletImage.empty());
    const auto comma=Weapons::parse(DataTable::parseText("Parts\tType\tDmg\tRange\tShtRng\tShtCnt\tShtDly\tRldDly\tSpeed\tAimRt\tBltTile\tEffType\tEffTile\tExplodeEff\tExplScl\tShell\tFireSound\tUiTile\nComma\tGun\t+1,000suffix\t-2,000suffix\t500\t+2suffix\t0.25suffix\t1\t1\t1\t-\t-\t-\t-\t-\t0\tWpn_Gun01\t0\n"));
    assert(comma.weapon("Comma").damage==1000&&comma.weapon("Comma").damageRange==-2000&&comma.weapon("Comma").shotCount==2);same(comma.weapon("Comma").shotDelay,.25f);
    const auto numbers=DataTable::parseText("I\tF\n +1,234tail\t -1,234.5tail\n-\t-\n +12\t0x1p2\n");
    assert(numbers.integer(0,"I")==1234);same(numbers.real(0,"F"),-1234.5f);assert(numbers.integer(1,"I")==0);same(numbers.real(1,"F"),0);same(numbers.real(2,"F"),4);
    assert(numbers.integer(100,"I")==0);same(numbers.real(100,"F"),0);same(numbers.real(0,"Missing"),0);
    for(const std::string text:{"2147483648","-2147483649","99999999999999999999999","123456789012345678901234"}){bool rejected=false;try{(void)DataTable::parseText("I\n"+text+"\n").integer(0,"I");}catch(...){rejected=true;}assert(rejected);}
    const auto limits=DataTable::parseText("I\n2147483647\n-2147483648\n");assert(limits.integer(0,"I")==2147483647&&limits.integer(1,"I")==(-2147483647-1));
    Part p{"Synthetic",4,1,{{-9,-3},{17,.5},{20,2}}};PartPose pose{&p,{12,21},0,3,0,false};auto muzzle=Models::muzzlePosition(pose,1);same(muzzle.x,38);same(muzzle.y,24.5);
    pose.reverseY=true;muzzle=Models::muzzlePosition(pose,2);same(muzzle.x,41);same(muzzle.y,16);
    pose.angle=std::numbers::pi_v<float>/2;muzzle=Models::muzzlePosition(pose,1);same(muzzle.x,15.5);same(muzzle.y,47);
    bool fail=false;try{(void)Models::muzzlePosition(pose,3);}catch(...){fail=true;}assert(fail);
}

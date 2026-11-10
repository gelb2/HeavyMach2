#include "Core/Simulation/WeaponController.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>
using namespace hm2;
namespace {
void near(float a,float b){assert(std::abs(a-b)<.00001f);}
// Only common-controller behavior is under test. No particles are allocated;
// actual attack, damage, audio output and pool RNG belong to separate tests.
struct Host:WeaponHost {
    WeaponTarget victim{1,{100,0},200};bool present=true;
    std::vector<std::uint32_t> values;std::size_t reads=0;
    std::vector<WeaponEmission> shots;unsigned sounds=0,ranges=0;
    const WeaponTarget* target()const override{return present?&victim:nullptr;}
    std::uint32_t random()override{++reads;return values.empty()?0x40000000u:values.at(reads-1);}
    void emit(const WeaponInfo&,const WeaponEmission& s)override{shots.push_back(s);}
    void outOfRange(bool left)override{assert(left);++ranges;}
    void fireSound(const WeaponInfo&,Vec2 p)override{near(p.x,0);near(p.y,0);++sounds;}
};
}
int main(){
    WeaponInfo w;w.kind=WeaponKind::gun;w.shotCount=10;w.shotDelay=.16f;w.reloadDelay=2.5f;w.shotRange=400;w.aimRate=1;
    Part part{"Fixture",4,1,{{-9,-3},{17,.5},{20,2}}};unsigned muzzle=1;
    float parent=0;
    WeaponTick input;input.pose={&part,{12,21},0,3,0,false};input.currentMuzzle=&muzzle;input.parentAddX=&parent;input.weaponReady=true;input.isPlayer=true;input.left=true;
    Host h;WeaponController c;
    input.time=2.499; c.tick(&w,input,h);assert(c.state().fireLeft==0&&h.shots.empty()&&h.reads==0);
    input.time=2.5;c.tick(&w,input,h);assert(h.shots.size()==1&&h.sounds==1&&c.state().fireLeft==9&&h.reads==3&&muzzle==2);
    near(h.shots[0].muzzle.x,38);near(h.shots[0].muzzle.y,24.5f);near(h.shots[0].destination.x,100);near(h.shots[0].destination.y,12.25f);
    near(c.state().fireBound,-4.5f);near(parent,-2.25f);near(c.state().fireTime,2.5f);
    h.present=false;const auto frozen=c.state();input.time=3;c.tick(&w,input,h);near(c.state().fireBound,frozen.fireBound);assert(h.reads==3);
    h.present=true;h.victim.position.x=400;input.time=4;const auto ammo=c.state().fireLeft;c.tick(&w,input,h);assert(c.state().fireLeft==ammo&&h.reads==3&&h.ranges==1&&muzzle==2);near(c.state().fireBound,-4.05f);
    h.victim.position.x=std::nextafter(400.f,0.f);input.weaponReady=false;c.tick(&w,input,h);assert(h.reads==3&&c.state().fireLeft==ammo);
    input.weaponReady=true;c.tick(&w,input,h);assert(h.shots.size()==2&&h.reads==6&&muzzle==1);
    // No-target explicit fire still reloads and emits, using two endpoint RNGs.
    Host free;free.present=false;WeaponController f;input.isFire=true;input.time=2.5;muzzle=1;f.tick(&w,input,free);
    assert(free.reads==3&&free.shots.size()==1);near(free.shots[0].destination.x,398);near(free.shots[0].destination.y,24.5f);
    // Target hp0 and fire=false skip even reload and recoil, not just damage.
    Host dead;dead.victim.hp=0;input.isFire=false;WeaponController d;d.tick(&w,input,dead);assert(d.state().fireLeft==0&&dead.reads==0);
    // Same-tick outer float elapsed can pass while inner rounded deadline fails.
    Host boundary;WeaponController b({1,100.f,0});input.time=100.16;muzzle=1;b.tick(&w,input,boundary);
    assert(boundary.shots.empty()&&boundary.sounds==1&&boundary.reads==2&&muzzle==2&&b.state().fireLeft==1);near(b.state().fireBound,-4.5f);
    // Random jitter must use world-time double + float delta, not a fixed timer.
    Host jitter;jitter.values={0,0,0};WeaponController j({1,0,0});input.time=2.5;j.tick(&w,input,jitter);near(j.state().fireTime,float(2.5+double(w.shotDelay*-.2f)));
    c.reload(10);assert(c.state().fireLeft==0);near(c.state().fireTime,10);
    c.tick(nullptr,input,h);input.pose.part=nullptr;c.tick(&w,input,h);assert(h.reads==6);
}

#include "Core/Simulation/ProjectileMotion.hpp"
#include <cassert>
#include <cmath>
using namespace hm2;
void near(float a,float b){assert(std::abs(a-b)<.00001f);}
int main(){
    WeaponInfo missile;missile.kind=WeaponKind::missile;missile.speed=.2f;
    ProjectileMotion p(missile,{0,0},{100,0},500,0x40000000u);
    near(p.state().angle,0);near(p.state().velocity.x,.06f);near(p.state().acceleration.x,.01f);assert(!p.state().collideCheck&&!p.state().toPlayer);
    assert(!p.tick());near(p.state().velocity.x,.07f);near(p.state().position.x,.07f/.0166f);
    const auto before=p.state();assert(!p.tick(true));near(p.state().position.x,before.position.x);
    bool collision=false;for(int i=0;i<100&&!collision;++i)collision=p.tick();assert(collision);p.remove();const auto terminal=p.state();assert(!p.tick());near(p.state().position.x,terminal.position.x);
    ProjectileMotion shortShot(missile,{}, {1,0},0,0x40000000u);assert(shortShot.tick());
    // Other original bullet kinds have initial velocity but no missile accel.
    for(auto kind:{WeaponKind::shotgun,WeaponKind::laser,WeaponKind::railgun,WeaponKind::bomb,WeaponKind::nuclear,WeaponKind::bullet}){
        WeaponInfo w;w.kind=kind;w.speed=.2f;ProjectileMotion b(w,{}, {100,0},0,0x40000000u,true);
        near(b.state().velocity.x,.2f);near(b.state().acceleration.x,0);assert(b.state().toPlayer);assert(!b.tick());near(b.state().position.x,.2f/.0166f);near(b.state().angle,kind==WeaponKind::railgun?.1f:0.f);
    }
    // Equality is not endpoint collision. Increased distance is checked first.
    WeaponInfo b;b.kind=WeaponKind::bullet;
    ProjectileState s;s.destination={2,0};s.velocity={.0166f,0};s.previousLengthSquared=4;s.shotLengthSquared=4;
    ProjectileMotion exact(b,s);assert(!exact.tick());near(exact.state().position.x,1);assert(exact.tick());
    s.show=false;ProjectileMotion hidden(b,s);assert(!hidden.tick());near(hidden.state().position.x,0);
    bool rejected=false;try{WeaponInfo gun;ProjectileMotion invalid(gun,{}, {1,0},0,0);}catch(...){rejected=true;}assert(rejected);
    ProjectileMotion temporary([]{WeaponInfo w;w.kind=WeaponKind::missile;w.speed=.2f;return w;}(),{}, {100,0},0,0x40000000u);assert(!temporary.tick());near(temporary.state().velocity.x,.07f);
}

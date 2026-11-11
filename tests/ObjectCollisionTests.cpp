#include "Core/Simulation/ObjectCollision.hpp"
#include <cassert>
using namespace hm2;
int main(){
    ObjectBody a{{0,0},30,MachineKind::vehicle},b{{50,0},30,MachineKind::vehicle};
    assert(resolveObjectCollision(a,b)&&a.position.x==-3&&b.position.x==50);
    a={{0,0},30,MachineKind::vehicle};b={{50,0},25,MachineKind::bipod};
    assert(resolveObjectCollision(a,b)&&a.position.x==0&&b.position.x==51.5f);
    a={{0,0},30,MachineKind::vehicle};b={{30,0},25,MachineKind::turret};
    assert(resolveObjectCollision(a,b)&&a.position.x==-7.5000005f&&b.position.x==30);
    a={{0,0},25,MachineKind::turret};b={{30,0},30,MachineKind::turret};
    assert(resolveObjectCollision(a,b)&&a.position.x==0&&b.position.x==37.5f);
    a={{0,0},30};b={{60,0},30};assert(!resolveObjectCollision(a,b));
    b.position={59.999f,0};assert(resolveObjectCollision(a,b));
    a={{0,0},30};b={{0,0},30};assert(!resolveObjectCollision(a,b)&&a.position.x==0&&b.position.x==0);
    b.position={59,59};assert(!resolveObjectCollision(a,b));
}

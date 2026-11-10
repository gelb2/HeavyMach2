#include "Core/Simulation/ObjectCollision.hpp"
#include <cmath>
namespace hm2 {
bool resolveObjectCollision(ObjectBody& self,ObjectBody& other){
    const float radius=float(self.radius+other.radius);
    Vec2 delta{self.position.x-other.position.x,self.position.y-other.position.y};
    if(radius<=std::abs(delta.x)||radius<=std::abs(delta.y))return false;
    const float squared=delta.x*delta.x+delta.y*delta.y;
    if(squared==0||squared>=radius*radius)return false;
    const float distance=std::sqrt(squared);delta.x/=distance;delta.y/=distance;
    const float push=(radius-distance)*.3f;delta.x*=push;delta.y*=push;
    if(self.kind==MachineKind::turret||(other.kind!=MachineKind::turret&&self.radius>other.radius)){
        other.position.x-=delta.x;other.position.y-=delta.y;
    }else{self.position.x+=delta.x;self.position.y+=delta.y;}
    return true;
}
}

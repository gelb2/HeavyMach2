#include "Core/Simulation/WorldNavigation.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace hm2 {
namespace {
float distance2(Vec2 a,Vec2 b){const float x=a.x-b.x,y=a.y-b.y;return x*x+y*y;}
float ease(float from,float to){return std::abs(from-to)>.5f?from+(to-from)/50:to;}
}
Vec2 WorldNavigation::point(unsigned i)const{if(i>=count_)throw std::out_of_range("Waypoint outside active path");return points_[(start_+i)%12];}
void WorldNavigation::append(Vec2 p,bool add,BipodMotion& actor){
    if(!add||count_==0){start_=0;count_=0;actor.setDestination(p);}
    else if(count_>11)count_=11;
    points_[(start_+count_)%12]=p;++count_;
}
bool WorldNavigation::input(PointerPhase phase,Vec2 screen,std::uintptr_t id,const MapCamera& camera,BipodMotion& actor){
    if(phase==PointerPhase::cancel)return inputWorld(phase,{},id,actor);
    if(!id||!std::isfinite(screen.x)||!std::isfinite(screen.y)||screen.y<100)return false;
    return inputWorld(phase,camera.toWorld(screen),id,actor);
}
bool WorldNavigation::inputWorld(PointerPhase phase,Vec2 world,std::uintptr_t id,BipodMotion& actor){
    if(phase==PointerPhase::cancel){const bool owned=pointer_==id;if(owned)pointer_=0;return owned;}
    if(!id||!std::isfinite(world.x)||!std::isfinite(world.y))return false;
    if(phase==PointerPhase::begin){
        if(pointer_)return false;pointer_=id;
        if(distance2(world,actor.state().position)<900)return true;
        int closest=-1;float best=900;
        for(unsigned i=0;i<count_;++i){const float d=distance2(world,point(i));if(d<best){best=d;closest=int(i);}}
        if(closest<=0)append(world,false,actor);
        else {const auto retained=point(unsigned(closest));count_=unsigned(closest);append(retained,true,actor);}
        return true;
    }
    if(phase==PointerPhase::end){
        const bool active=pointer_!=0;
        if(pointer_==id&&count_&&distance2(world,point(count_-1))>900)append(world,true,actor);
        // Original state2 clears the move pointer even for another touch ID.
        pointer_=0;return active;
    }
    if(pointer_!=id)return false;
    if(phase==PointerPhase::move){if(count_&&distance2(world,point(count_-1))>3600)append(world,true,actor);return true;}
    return false;
}
void WorldNavigation::process(BipodMotion& actor){
    if(!count_)return;const auto p=point(0),a=actor.state().position;
    if(std::abs(p.x-a.x)<16&&std::abs(p.y-a.y)<16){--count_;start_=(start_+1)%12;if(count_)actor.setDestination(point(0));}
}
void processCamera(MapCamera& camera,Vec2 player,const WorldNavigation& navigation,std::optional<Vec2> target){
    Vec2 wanted=player;
    if(target){wanted.x+=(target->x-player.x)*.5f;wanted.y+=(target->y-player.y)*.5f;}
    else if(navigation.count()){const auto p=navigation.point(navigation.count()-1);wanted.x+=(p.x-player.x)*.5f;wanted.y+=(p.y-player.y)*.5f;}
    camera.center.x=std::clamp(ease(camera.center.x,wanted.x),-128.f,128.f);
    camera.center.y=std::clamp(ease(camera.center.y,wanted.y),-94.f,0.f);
    camera.backgroundWorld={(camera.center.x+camera.offset.x)*.94f,(camera.center.y+camera.offset.y)*.94f};
}
}

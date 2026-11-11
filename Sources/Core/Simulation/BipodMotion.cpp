#include "Core/Simulation/BipodMotion.hpp"
#include <bit>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace hm2 {
namespace {
constexpr float tau=std::bit_cast<float>(0x40c90fdau);
void finite(Vec2 p){if(!std::isfinite(p.x)||!std::isfinite(p.y))throw std::runtime_error("Nonfinite bipod point");}
float shortest(float current,float wanted){const float d=wanted-current;if(double(d)>std::numbers::pi)current+=tau;else if(double(d)<-std::numbers::pi)current-=tau;return current;}
void turnFoot(float& value,float wanted,float divisor){value=shortest(value,wanted);value+=(wanted-value)/divisor;if(std::abs(value-wanted)<.01f)value=wanted;}
}
BipodMotion::BipodMotion(Vec2 p,float direction,float stepSize){finite(p);if(!std::isfinite(direction)||!std::isfinite(stepSize)||stepSize<=0)throw std::runtime_error("Invalid bipod initial values");s_.position=p;s_.angle=s_.bodyAngle=s_.left.angle=s_.right.angle=direction;s_.stepSize=stepSize;}
BipodMotion::BipodMotion(BipodState s):s_(s){finite(s.position);finite(s.destination);if(s.stepSize<=0||!std::isfinite(s.stepSize)||s.walkStep<0||s.walkStep>4||s.walkFoot<0||s.walkFoot>1||s.currentFoot< -1||s.currentFoot>1)throw std::runtime_error("Invalid bipod snapshot");}
void BipodMotion::setDestination(Vec2 p){finite(p);s_.destination=p;s_.state=1;if(s_.walkStep==0)s_.walkStep=1;}
void BipodMotion::applyContact(Vec2 p){finite(p);s_.position=p;}
void BipodMotion::footChange(){
    s_.walkFoot=s_.walkFoot?0:1;s_.currentFoot=s_.walkFoot;
    const auto& current=s_.currentFoot?s_.right:s_.left;
    s_.stepLen=current.addX;s_.moveLen=0;s_.walkStep=s_.blocked?4:3;
}
void BipodMotion::tick(const TerrainEdge* wall,std::optional<Vec2> target){
    if(!s_.inScreen)return;
    if(target)finite(*target);
    auto current=[&]()->BipodFoot*{return s_.currentFoot<0?nullptr:s_.currentFoot?&s_.right:&s_.left;};
    if(s_.state==1){
        const Vec2 d{s_.destination.x-s_.position.x,s_.destination.y-s_.position.y};
        if(std::abs(d.x)<=2&&std::abs(d.y)<=2){footChange();s_.state=0;}
        else {
            const int phase=s_.walkStep;
            if(phase==1){
                float desired=s_.angle;
                if(!wall||wall->normal.x*d.x+wall->normal.y*d.y<=0)desired=float(std::atan2(double(d.y),double(d.x)));
                else {const float a=float(std::atan2(double(wall->dir.y),double(wall->dir.x)));desired=wall->dir.x*d.x+wall->dir.y*d.y<0?float(double(a)+(std::numbers::pi+double(.1f))):a-.1f;}
                const float diff=desired-s_.angle;
                if(double(diff)>std::numbers::pi)s_.angle=float(double(s_.angle)+2*std::numbers::pi);
                else if(double(diff)<-std::numbers::pi)s_.angle=float(double(s_.angle)-2*std::numbers::pi);
                if(!s_.blocked||std::abs(s_.angle-desired)>1)s_.walkStep=2;
                if(s_.walkStep==2){s_.angle=desired;footChange();}
            }
            else if(phase==3){
                if(auto* foot=current())turnFoot(foot->angle,s_.angle,7);
                s_.stepLen+=(s_.stepSize-s_.stepLen)/5.8f;
                if(s_.stepSize-s_.stepLen<.5f){s_.stepLen=s_.stepSize;s_.walkStep=1;}
                const float phase=std::sin(float(double(s_.stepLen/s_.stepSize)*std::numbers::pi));
                if(auto* foot=current())foot->scaleState=1+.2f*phase;
                s_.bodyScaleState=1+.04f*phase;s_.bodyAddY=(s_.walkFoot==0?.1f:-.1f)*s_.stepLen;
            }else if(phase==4){
                if(auto* foot=current())turnFoot(foot->angle,s_.angle,8);
                s_.stepLen+=(0-s_.stepLen)/16;
                if(auto* foot=current())if(foot->angle==s_.angle)s_.walkStep=1;
            }
        }
    }else s_.stepLen+=(0-s_.stepLen)/6;
    const float old=s_.moveLen;s_.moveLen+=(s_.stepLen-s_.moveLen)/6;
    if(auto* foot=current()){foot->addX=s_.stepLen-s_.moveLen;auto& other=s_.currentFoot?s_.left:s_.right;other.addX=-s_.moveLen;}
    s_.speed=s_.moveLen-old;s_.position.x+=s_.speed*std::cos(s_.angle);s_.position.y+=s_.speed*std::sin(s_.angle);
    if(target){
        const Vec2 d{target->x-s_.position.x,target->y-s_.position.y};const float distance=d.x*d.x+d.y*d.y;
        const float aim=float(std::atan2(double(d.y),double(d.x)));
        s_.bodyAngle=shortest(s_.bodyAngle,aim);s_.bodyAngle+=(aim-s_.bodyAngle)/10;
        s_.leftWeaponAngle=s_.rightWeaponAngle=s_.bodyAngle;s_.weaponReady=std::abs(aim-s_.bodyAngle)<.1f;
        if(distance<20000){const float offset=.25f+distance/-80000.f;s_.leftWeaponAngle-=offset;s_.rightWeaponAngle+=offset;}
        s_.baseAngle+=(s_.angle-s_.baseAngle)/10;
    }else{
        s_.bodyAngle=shortest(s_.bodyAngle,s_.angle);s_.bodyAngle+=(s_.angle-s_.bodyAngle)/10;
        s_.leftWeaponAngle=s_.rightWeaponAngle=s_.baseAngle=s_.bodyAngle;
    }
}
PlayerPartAngles BipodMotion::angles()const{return {s_.baseAngle,s_.left.angle,s_.right.angle,s_.bodyAngle,s_.leftWeaponAngle,s_.rightWeaponAngle};}
PlayerPartOffsets BipodMotion::offsets()const{return {{s_.left.addX,0},{s_.right.addX,0},{0,s_.bodyAddY},{0,s_.bodyAddY},{0,s_.bodyAddY}};}
}

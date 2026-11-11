#include "Core/Simulation/EnemyActors.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace hm2 {
namespace {
void validTime(double t){if(!std::isfinite(t)||!std::isfinite(float(t)))throw std::runtime_error("Invalid original enemy time");}
PartPose& poseFor(std::vector<PartPose>& poses,std::size_t index){
    const auto p=std::find_if(poses.begin(),poses.end(),[&](const auto& pose){return pose.mountIndex==index;});
    if(p==poses.end())throw std::runtime_error("Missing original enemy mount pose");return *p;
}
struct VehicleBridge:EnemyVehicleHost {
    EnemyActor& actor;EnemyActorHost& host;
    VehicleBridge(EnemyActor& a,EnemyActorHost& h):actor(a),host(h){}
    std::uint32_t random()override{return host.random();}
    const WeaponTarget* player()const override{return host.player();}
    void objectContacts(EnemyVehicle&)override{host.objectContacts(actor);}
    void emit(const WeaponInfo& info,const WeaponEmission& emission,const EnemyVehicleEmission& context)override{
        host.emit(info,emission,{&actor,context.machinePosition,context.partCenter,context.mountIndex,context.hostile});
    }
    void fireSound(const WeaponInfo& info,Vec2 p)override{host.fireSound(info,p);}
};
struct WeaponBridge:WeaponHost {
    EnemyActor& actor;EnemyActorHost& host;std::size_t mount;Vec2 center;
    WeaponBridge(EnemyActor& a,EnemyActorHost& h,std::size_t m,Vec2 c):actor(a),host(h),mount(m),center(c){}
    std::uint32_t random()override{return host.random();}
    const WeaponTarget* target()const override{return actor.state().targetPlayer?host.player():nullptr;}
    void emit(const WeaponInfo& info,const WeaponEmission& shot)override{host.emit(info,shot,{&actor,actor.motion().position,center,mount,true});}
    void outOfRange(bool)override{throw std::runtime_error("Enemy requested Player range UI");}
    void fireSound(const WeaponInfo& info,Vec2 p)override{host.fireSound(info,p);}
};
}
EnemyActor::EnemyActor(const Models& models,const Weapons& weapons,const MachineInfo& info,Vec2 p,double time,WorldRandom& random,std::optional<Vec2> spawner,unsigned partBaseLayer)
:models_(models),info_(info),contact_{p,-1,-1,0}{
    validTime(time);if(!supports(info.kind))throw std::runtime_error("Enemy kind has no verified HD map-spawned consumer");
    if(!std::isfinite(p.x)||!std::isfinite(p.y))throw std::runtime_error("Invalid original enemy position");
    if(info.kind==MachineKind::vehicle){vehicle_=std::make_unique<EnemyVehicle>(models,weapons,info,p,time,random,spawner,partBaseLayer);return;}
    mounts_=models_.modelMounts(info.name);activeParts_.assign(mounts_.size(),true);
    for(std::size_t i=0;i<mounts_.size();++i){auto& mount=mounts_[i];mount.layer+=partBaseLayer;
        if(models_.part(mount.part).type==4){const auto w=weapons.all().find(mount.part);
            if(w!=weapons.all().end()){EnemyActorWeapon value;value.mountIndex=i;value.info=&w->second;weapons_.push_back(std::move(value));state_.attackable=true;}
        }
    }
    state_.hp=info.hp;state_.firstAttack=random.random()%15==0;state_.createTime=float(time);
    state_.stopDelay=(state_.createTime+randomUnit(random))+.5f;
    motion_.position=p;motion_.angle=randomUnit(random)*std::bit_cast<float>(0x40c90fdbu);
    poses_=models_.mountedPose(mounts_,0);atlasPoses_=poses_;
}
EnemyActor::~EnemyActor()=default;
EnemyActorState& EnemyActor::state(){return vehicle_?vehicle_->state():state_;}
const EnemyActorState& EnemyActor::state()const{return vehicle_?vehicle_->state():state_;}
const VehicleState& EnemyActor::motion()const{return vehicle_?vehicle_->motion():motion_;}
const TerrainContact& EnemyActor::contact()const{return vehicle_?vehicle_->contact():contact_;}
std::span<const PartMount> EnemyActor::mounts()const{return vehicle_?vehicle_->mounts():std::span<const PartMount>(mounts_);}
bool EnemyActor::partActive(std::size_t i)const{return vehicle_?vehicle_->partActive(i):activeParts_.at(i);}
std::span<const EnemyActorWeapon> EnemyActor::weapons()const{return vehicle_?vehicle_->weapons():std::span<const EnemyActorWeapon>(weapons_);}
std::span<const PartPose> EnemyActor::poses()const{return vehicle_?vehicle_->poses():std::span<const PartPose>(poses_);}
std::span<const PartPose> EnemyActor::atlasPoses()const{return vehicle_?vehicle_->atlasPoses():std::span<const PartPose>(atlasPoses_);}
void EnemyActor::setDestination(Vec2 p){
    if(!std::isfinite(p.x)||!std::isfinite(p.y))throw std::runtime_error("Invalid original enemy destination");
    if(vehicle_){vehicle_->setDestination(p);return;}motion_.destination=p;motion_.state=1;
}
void EnemyActor::applyContact(Vec2 p){
    if(!std::isfinite(p.x)||!std::isfinite(p.y))throw std::runtime_error("Invalid original enemy contact");
    if(vehicle_){vehicle_->applyContact(p);return;}motion_.position=p;
}
void EnemyActor::setMachineState(int s){if(vehicle_)vehicle_->setMachineState(s);else motion_.state=s;}
void EnemyActor::setMovementRates(float moveSpeed,float rotationSpeed){
    if(vehicle_){
        vehicle_->setMovementRates(moveSpeed,rotationSpeed);
        return;
    }
    motion_.moveSpeed=moveSpeed;
    motion_.rotationSpeed=rotationSpeed;
}
void EnemyActor::removePartsForCorpse(){
    if(vehicle_){vehicle_->removePartsForCorpse();return;}if(mounts_.empty())return;
    mounts_[0].layer=1;for(std::size_t i=1;i<activeParts_.size();++i)activeParts_[i]=false;
    for(auto& w:weapons_)if(!activeParts_[w.mountIndex])w.removed=true;
    for(auto* poses:{&poses_,&atlasPoses_}){std::erase_if(*poses,[&](const auto& p){return !activeParts_[p.mountIndex];});for(auto& p:*poses)if(p.mountIndex==0)p.layer=1;}
}
void EnemyActor::retargetAfterDamage(){auto& s=state();s.targetPlayer=true;if(info_.kind!=MachineKind::aircraft&&info_.kind!=MachineKind::train)s.moveToTarget=true;}
void EnemyActor::aircraftAI(double time,WorldRandom& random){
    // AirCraft0xa200: state0 bypasses the stopDelay deadline, including equality.
    if(time>double(state_.stopDelay)||motion_.state==0){
        const float delay=randomUnit(random)*5.f;state_.stopDelay=float((time+double(delay))+5.0);
        const float x=randomUnit(random)*1200.f-600.f,y=randomUnit(random)*1200.f-600.f;
        setDestination({x,y});
    }
}
void EnemyActor::moveAircraft(){
    if(motion_.state!=1)return;
    const Vec2 d{motion_.destination.x-motion_.position.x,motion_.destination.y-motion_.position.y};
    if(std::abs(d.x)<=4&&std::abs(d.y)<=4){motion_.state=0;return;} // Does not zero speed.
    const float wanted=float(std::atan2(double(d.y),double(d.x))),delta=wanted-motion_.angle;
    constexpr float tau=std::bit_cast<float>(0x40c90fdau);
    // ASM0xdbd4..0xdc06 converts FLOAT delta to double before the pi comparison.
    if(double(delta)>std::numbers::pi)motion_.angle+=tau;
    else if(double(delta)<-std::numbers::pi)motion_.angle-=tau;
    motion_.angle+=(wanted-motion_.angle)/motion_.rotationSpeed;
    if(d.x*d.x+d.y*d.y>900)motion_.speed+=(motion_.moveSpeed-motion_.speed)/100.f;
    else motion_.speed+=(.5f-motion_.speed)/60.f;
    motion_.velocity={std::cos(motion_.angle)*motion_.speed,std::sin(motion_.angle)*motion_.speed};
    motion_.position.x+=motion_.velocity.x;motion_.position.y+=motion_.velocity.y;
}
void EnemyActor::attackAI(double time,EnemyActorHost& host){
    if(state_.targetPlayer||(!state_.firstAttack&&!info_.attack)||!(double(state_.attackCheckTime+.5f)<time))return;
    if(const auto* p=host.player()){const Vec2 d{motion_.position.x-p->position.x,motion_.position.y-p->position.y};if(d.x*d.x+d.y*d.y<40000.f)state_.targetPlayer=true;}
}
void EnemyActor::followTarget(double time,EnemyActorHost& host){
    const auto* p=state_.targetPlayer?host.player():nullptr;
    if(p&&double(state_.targetCheckTime+.5f)<time){
        if(state_.moveToTarget){const Vec2 d{p->position.x-motion_.position.x,p->position.y-motion_.position.y};setDestination(d.x*d.x+d.y*d.y<40000.f?motion_.position:p->position);}
        state_.targetCheckTime=float(time);
    }
}
void EnemyActor::tickParts(double time,EnemyActorHost& host){
    poses_=models_.mountedPose(mounts_,motion_.angle);std::erase_if(poses_,[&](const auto& p){return !activeParts_[p.mountIndex];});atlasPoses_=poses_;
    for(std::size_t i=0;i<mounts_.size();++i){
        if(!activeParts_[i])continue;auto& mount=mounts_[i];const auto& part=models_.part(mount.part);
        auto w=std::find_if(weapons_.begin(),weapons_.end(),[&](const auto& weapon){return weapon.mountIndex==i;});
        if(w==weapons_.end()&&part.type!=7)mount.angle=mount.parent<0?motion_.angle:mounts_[std::size_t(mount.parent)].angle;
        auto fresh=models_.mountedPose(mounts_,motion_.angle);auto pose=poseFor(fresh,i);
        if(part.type==7){mount.angle+=.2f;constexpr float tau=std::bit_cast<float>(0x40c90fdbu);if(mount.angle>tau)mount.angle-=tau;pose.angle=mount.angle;}
        poseFor(atlasPoses_,i)=pose;
        if(w!=weapons_.end()&&!w->removed){
            const auto* target=state_.targetPlayer?host.player():nullptr;std::optional<Vec2> aim;if(target&&target->hp>0)aim=target->position;
            const auto ready=w->aim.tick(time,motion_.position,motion_.angle,aim,host,true,true,state_.hp);if(ready)state_.weaponReady=*ready;
            mount.angle=w->aim.state().angle;pose.angle=mount.angle;
            WeaponTick input;input.time=time;input.machinePosition=motion_.position;if(const auto* p=host.player())input.playerPosition=p->position;
            input.pose=pose;input.currentMuzzle=&w->currentMuzzle;input.weaponReady=state_.weaponReady;
            if(mount.parent>=0)input.parentAddX=&mounts_[std::size_t(mount.parent)].addPosition.x;
            WeaponBridge bridge(*this,host,i,pose.center);const auto* fireTarget=bridge.target();const bool enters=fireTarget&&fireTarget->hp>0;
            w->controller.tick(w->info,input,bridge);if(enters)mount.addPosition.x=w->controller.state().fireBound;
        }
        poseFor(poses_,i)=pose;
    }
}
void EnemyActor::tick(EnemyActorTick input,const TerrainCollision& terrain,EnemyActorHost& host){
    validTime(input.time);if(vehicle_){VehicleBridge bridge(*this,host);vehicle_->tick(input,terrain,bridge);return;}
    if(!input.paused&&state_.hp>0){if(info_.kind==MachineKind::aircraft)aircraftAI(input.time,host);attackAI(input.time,host);}
    if(!state_.show)return;
    if(!input.paused&&(motion_.state==0||motion_.state==1)&&(info_.kind==MachineKind::aircraft||input.inScreen)){
        if(info_.kind==MachineKind::aircraft)moveAircraft();
        else{contact_=terrain.resolve(motion_.position,contact_.previous);motion_.position=contact_.position;host.objectContacts(*this);}
        followTarget(input.time,host);
    }
    tickParts(input.time,host);
}
}

#include "Core/Simulation/EnemyVehicle.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace hm2 {
namespace {
void validTime(double time){
    if(!std::isfinite(time)||!std::isfinite(float(time)))throw std::runtime_error("Invalid original enemy time");
}
PartPose& poseFor(std::vector<PartPose>& poses,std::size_t mount){
    const auto it=std::find_if(poses.begin(),poses.end(),[&](const auto& p){return p.mountIndex==mount;});
    if(it==poses.end())throw std::runtime_error("Missing original enemy mount pose");return *it;
}
struct WeaponAdapter:WeaponHost {
    EnemyVehicle& actor;EnemyVehicleHost& host;std::size_t mount;Vec2 center;
    WeaponAdapter(EnemyVehicle& a,EnemyVehicleHost& h,std::size_t i,Vec2 p):actor(a),host(h),mount(i),center(p){}
    const WeaponTarget* target()const override{return actor.state().targetPlayer?host.player():nullptr;}
    std::uint32_t random()override{return host.random();}
    void emit(const WeaponInfo& info,const WeaponEmission& shot)override{
        host.emit(info,shot,{&actor,actor.motion().position,center,mount,true});
    }
    void outOfRange(bool)override{throw std::runtime_error("Enemy weapon must not request Player range UI");}
    void fireSound(const WeaponInfo& info,Vec2 position)override{host.fireSound(info,position);}
};
}
EnemyVehicle::EnemyVehicle(const Models& models,const Weapons& weapons,const MachineInfo& info,Vec2 position,double time,
                           WorldRandom& random,std::optional<Vec2> spawner,unsigned partBaseLayer)
:models_(models),info_(info),contact_{position,-1,-1,0},spawner_(spawner){
    validTime(time);
    if(info.kind!=MachineKind::vehicle)throw std::runtime_error("Unverified non-vehicle enemy consumer");
    if(!std::isfinite(position.x)||!std::isfinite(position.y))throw std::runtime_error("Invalid original enemy position");
    mounts_=models_.modelMounts(info.name);
    activeParts_.assign(mounts_.size(),true);
    for(std::size_t i=0;i<mounts_.size();++i){
        auto& mount=mounts_[i];mount.layer+=partBaseLayer;
        const auto& part=models_.part(mount.part);
        if(part.type==4){
            const auto found=weapons.all().find(part.name);
            if(found!=weapons.all().end()){
                // decodeChunk calls inherited initWithMach:Parts:, bypassing
                // WeaponAI::init and Player's setReload. Both clocks stay0.
                EnemyVehicleWeapon weapon;weapon.mountIndex=i;weapon.info=&found->second;
                weapons_.push_back(std::move(weapon));state_.attackable=true;
            }
        }
    }
    state_.hp=info.hp;
    state_.firstAttack=random.random()%15==0;
    state_.createTime=float(time);
    state_.stopDelay=(state_.createTime+randomUnit(random))+.5f;
    VehicleState vehicle;vehicle.position=position;
    constexpr float makeDirectionTau=std::bit_cast<float>(0x40c90fdbu);
    vehicle.angle=randomUnit(random)*makeDirectionTau;
    motion_=VehicleMotion(vehicle);
    // setDir does not fill the Player-only body/foot role pointers for MCB
    // enemies. Their initial own rotations remain zero until their first tick.
    poses_=models_.mountedPose(mounts_,0);atlasPoses_=poses_;
}
void EnemyVehicle::setMachineState(int value){auto s=motion_.state();s.state=value;motion_=VehicleMotion(s);}
void EnemyVehicle::removePartsForCorpse(){
    if(mounts_.empty())return;
    mounts_[0].layer=1;
    for(std::size_t i=1;i<activeParts_.size();++i)activeParts_[i]=false;
    for(auto& weapon:weapons_)if(!activeParts_[weapon.mountIndex])weapon.removed=true;
    for(auto* poses:{&poses_,&atlasPoses_}){
        std::erase_if(*poses,[&](const auto& pose){return !activeParts_[pose.mountIndex];});
        for(auto& pose:*poses)if(pose.mountIndex==0)pose.layer=1;
    }
}
void EnemyVehicle::vehicleAI(double time,EnemyVehicleHost& host){
    const auto* target=state_.targetPlayer?host.player():nullptr;
    if(time>double(state_.stopDelay)&&(!target||!state_.moveToTarget)){
        const float delay=randomUnit(host)*4.f;
        state_.stopDelay=float((time+double(delay))+2.0);
        Vec2 destination;
        if(spawner_){
            const float x=randomUnit(host)*800.f-400.f;
            destination.x=spawner_->x+x;
            const float y=randomUnit(host)*800.f-400.f;
            destination.y=spawner_->y+y;
        }else{
            destination.x=randomUnit(host)*2000.f-1000.f;
            destination.y=randomUnit(host)*2000.f-1000.f;
        }
        motion_.setDestination(destination);
    }
}
void EnemyVehicle::attackAI(double time,EnemyVehicleHost& host){
    if(state_.targetPlayer||(!state_.firstAttack&&!info_.attack)||!(double(state_.attackCheckTime+.5f)<time))return;
    if(const auto* player=host.player()){
        const Vec2 delta{motion_.state().position.x-player->position.x,motion_.state().position.y-player->position.y};
        if(delta.x*delta.x+delta.y*delta.y<40000.f)state_.targetPlayer=true;
    }
    // The original does not update attackCheckTime here.
}
void EnemyVehicle::followTarget(double time,EnemyVehicleHost& host){
    const auto* target=state_.targetPlayer?host.player():nullptr;
    if(target&&double(state_.targetCheckTime+.5f)<time){
        if(state_.moveToTarget){
            const auto position=motion_.state().position;
            const Vec2 delta{target->position.x-position.x,target->position.y-position.y};
            // ASM0xd9d8 passes self.x/y when near; 0xda10 passes target.x/y
            // when the squared distance is >=40000. Both call setDestPos.
            motion_.setDestination(delta.x*delta.x+delta.y*delta.y<40000.f?position:target->position);
        }
        state_.targetCheckTime=float(time);
    }
}
void EnemyVehicle::tickParts(double time,EnemyVehicleHost& host){
    // Preserve the common model sort while updating part state in original
    // insertion order. The connection graph is not the child tick hierarchy.
    poses_=models_.mountedPose(mounts_,motion_.state().angle);
    std::erase_if(poses_,[&](const auto& pose){return !activeParts_[pose.mountIndex];});
    atlasPoses_=poses_;
    for(std::size_t i=0;i<mounts_.size();++i){
        if(!activeParts_[i])continue;
        auto& mount=mounts_[i];const auto& part=models_.part(mount.part);
        auto weapon=std::find_if(weapons_.begin(),weapons_.end(),[&](const auto& w){return w.mountIndex==i;});
        if(weapon==weapons_.end())mount.angle=mount.parent<0?motion_.state().angle:mounts_[std::size_t(mount.parent)].angle;
        auto prepared=models_.mountedPose(mounts_,motion_.state().angle);
        PartPose pose=poseFor(prepared,i); // refreshRotationPos before child AI.
        if(part.type==7){
            mount.angle+=.2f;
            constexpr float tau=std::bit_cast<float>(0x40c90fdbu);
            if(mount.angle>tau)mount.angle-=tau;
            pose.angle=mount.angle;
        }
        poseFor(atlasPoses_,i)=pose;
        if(weapon!=weapons_.end()&&!weapon->removed){
            const auto* target=state_.targetPlayer?host.player():nullptr;
            std::optional<Vec2> livingTarget;
            if(target&&target->hp>0)livingTarget=target->position;
            const auto ready=weapon->aim.tick(time,motion_.state().position,motion_.state().angle,livingTarget,host,true,true,state_.hp);
            if(ready)state_.weaponReady=*ready;
            mount.angle=weapon->aim.state().angle;
            pose.angle=mount.angle; // center remains the pre-aim refresh result.
            WeaponTick input;input.time=time;input.machinePosition=motion_.state().position;
            if(const auto* player=host.player())input.playerPosition=player->position;
            input.pose=pose;input.currentMuzzle=&weapon->currentMuzzle;input.weaponReady=state_.weaponReady;
            if(mount.parent>=0)input.parentAddX=&mounts_[std::size_t(mount.parent)].addPosition.x;
            WeaponAdapter adapter(*this,host,i,pose.center);
            const auto* fireTarget=adapter.target();const bool enters=fireTarget&&fireTarget->hp>0;
            weapon->controller.tick(weapon->info,input,adapter);
            if(enters)mount.addPosition.x=weapon->controller.state().fireBound;
        }
        poseFor(poses_,i)=pose;
    }
}
void EnemyVehicle::tick(EnemyVehicleTick input,const TerrainCollision& terrain,EnemyVehicleHost& host){
    validTime(input.time);
    if(!input.paused&&state_.hp>0){vehicleAI(input.time,host);attackAI(input.time,host);}
    if(!state_.show)return;
    if(!input.paused&&(motion_.state().state==0||motion_.state().state==1)&&input.inScreen){
        const int wall=contact_.current;
        motion_.tick(host,wall<0?nullptr:&terrain.edges().at(std::size_t(wall)));
        contact_=terrain.resolve(motion_.state().position,contact_.previous);
        motion_.applyContact(contact_.position);
        host.objectContacts(*this);
        followTarget(input.time,host);
    }
    // Enemy/vehicle root pause guards do not suppress child aim/fire ticks.
    tickParts(input.time,host);
}
}

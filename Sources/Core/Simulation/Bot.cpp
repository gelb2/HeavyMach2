#include "Core/Simulation/Bot.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace hm2 {
namespace {
float unit(WorldRandom& random){return float(random.random())*0x1p-31f;}
float distance2(Vec2 a,Vec2 b){const float x=a.x-b.x,y=a.y-b.y;return x*x+y*y;}
Vec2 playerPosition(const BotHost& host){const auto* player=host.player();if(!player)throw std::logic_error("Bot requires the actual world Player");return player->position;}
PartPose& poseFor(std::vector<PartPose>& poses,std::size_t i){const auto p=std::find_if(poses.begin(),poses.end(),[&](const auto& p){return p.mountIndex==i;});if(p==poses.end())throw std::logic_error("Missing original Bot part");return *p;}
struct WeaponBridge:WeaponHost {
    Bot& actor;BotHost& host;std::size_t mount;Vec2 center;BotMetrics& metrics;
    WeaponBridge(Bot& a,BotHost& h,std::size_t m,Vec2 p,BotMetrics& x):actor(a),host(h),mount(m),center(p),metrics(x){}
    std::uint32_t random()override{return host.random();}
    const WeaponTarget* target()const override{return actor.state().target?host.target(actor.state().target):nullptr;}
    void emit(const WeaponInfo& info,const WeaponEmission& shot)override{++metrics.emissions;host.emit(info,shot,{&actor,actor.motion().position,center,mount,false,false});}
    void fireSound(const WeaponInfo& info,Vec2 p)override{host.fireSound(info,p);}
    void outOfRange(bool)override{throw std::logic_error("Bot requested Player-only range UI");}
};
}
Bot::Bot(const Models& models,const Weapons& weapons,const MachineInfo& info,Vec2 position,double time,WorldRandom& random):models_(models),info_(info){
    if(info.kind!=MachineKind::aircraft||info.hp<=0||!std::isfinite(time)||!std::isfinite(float(time))||!std::isfinite(position.x)||!std::isfinite(position.y))throw std::runtime_error("Unverified original Bot definition/start state");
    state_.createTime=float(time);state_.stopDelay=(state_.createTime+unit(random))+.5f;
    motion_.moveSpeed=2;motion_.rotationSpeed=15;motion_.position=position;
    mounts_=models_.modelMounts(info.name);
    for(std::size_t i=0;i<mounts_.size();++i){auto& m=mounts_[i];m.layer+=5;
        if(models_.part(m.part).type==4){const auto found=weapons.all().find(m.part);if(found!=weapons.all().end()){BotWeapon w;w.mountIndex=i;w.info=&found->second;weapons_.push_back(std::move(w));state_.attackable=true;}}
    }
    state_.hp=info.hp;motion_.angle=unit(random)*std::bit_cast<float>(0x40c90fdbu);
    // decodeChunk calls PartsController::initWithMach directly. Do not invoke
    // WeaponAI.init or Player.setReload here; both clocks remain allocation0.
    poses_=models_.mountedPose(mounts_,0);atlasPoses_=poses_;
}
void Bot::setSlotIndex(unsigned slot){if(slot>=3)throw std::out_of_range("Original Bot slot outside3");state_.slotIndex=slot;}
void Bot::setDestination(Vec2 p){if(!std::isfinite(p.x)||!std::isfinite(p.y))throw std::runtime_error("Nonfinite Bot destination");motion_.destination=p;motion_.state=1;}
void Bot::clearTargetIf(std::uint64_t id,BotHost& host){if(state_.target==id){if(id)host.unselectTarget(id);state_.target=0;state_.moveToTarget=false;}}
void Bot::pickedItem(std::uint64_t id){if(state_.pickItem==id){state_.pickItem=0;state_.jobCheckTime=0;}}
void Bot::refreshGaugeVisual(){const float ratio=float(state_.hp)/float(info_.hp);gauge_.scaleX=ratio*.5f;gauge_.x=-(ratio*-10.5f+10.5f);}
void Bot::refreshHPGauge(BotHost& host){host.writeSlotHP(state_.slotIndex,state_.hp);refreshGaugeVisual();}
void Bot::aircraftAI(double time,BotHost& host){
    if(state_.target&&state_.moveToTarget)return;
    if(!(double(state_.stopDelay)<time)&&motion_.state!=0)return;
    ++metrics_.wanderAttempts;
    const float playerX=playerPosition(host).x,x=playerX+(unit(host)*400.f-200.f);
    const float playerY=playerPosition(host).y,y=playerY+(unit(host)*400.f-200.f);
    if(distance2({x,y},motion_.position)>2500.f){
        setDestination({x,y});const float delay=unit(host)*.3f;
        state_.stopDelay=float(time+double(delay)+double(.3f));state_.jobCheckTime=0;++metrics_.wanderAccepted;
    }
}
void Bot::attackJob(BotHost& host){
    if(!state_.attackable)return;
    const auto* target=host.playerTarget();state_.target=target?target->id:0;
    if(state_.target&&!state_.moveToTarget)state_.moveToTarget=true;
}
void Bot::pickJob(double time,BotHost& host){
    if(state_.jobCheckTime!=0&&!(double(state_.jobCheckTime)<time))return;
    ++metrics_.itemSearches;float nearest=160000;const BotItemTarget* selected=nullptr;
    for(const auto& item:host.items())if(item.id){const float d=distance2(motion_.position,item.position);if(d<nearest){nearest=d;selected=&item;}}
    if(selected){state_.pickItem=selected->id;setDestination(selected->position);}
    const float delay=unit(host);state_.jobCheckTime=float(time+double(delay)+1.0);
    const float stop=unit(host);state_.stopDelay=float(time+double(stop)+.5);
}
void Bot::regenerate(double time,BotHost& host){
    if(state_.hp>0&&state_.hp<info_.hp&&double(state_.hpRefreshTime+5.f)<time){++state_.hp;state_.hpRefreshTime=float(time);++metrics_.regeneratedHP;refreshHPGauge(host);}
}
void Bot::moveAircraft(){
    if(motion_.state!=1)return;
    const Vec2 d{motion_.destination.x-motion_.position.x,motion_.destination.y-motion_.position.y};
    if(std::abs(d.x)<=4&&std::abs(d.y)<=4){motion_.state=0;return;}
    const float wanted=float(std::atan2(double(d.y),double(d.x))),delta=wanted-motion_.angle;
    constexpr float tau=std::bit_cast<float>(0x40c90fdau);
    if(double(delta)>std::numbers::pi)motion_.angle+=tau;else if(double(delta)<-std::numbers::pi)motion_.angle-=tau;
    motion_.angle+=(wanted-motion_.angle)/motion_.rotationSpeed;
    if(d.x*d.x+d.y*d.y>900)motion_.speed+=(motion_.moveSpeed-motion_.speed)/100.f;
    else motion_.speed+=(.5f-motion_.speed)/60.f;
    motion_.velocity={std::cos(motion_.angle)*motion_.speed,std::sin(motion_.angle)*motion_.speed};
    motion_.position.x+=motion_.velocity.x;motion_.position.y+=motion_.velocity.y;
}
void Bot::followTarget(double time,BotHost& host){
    const auto* target=state_.target?host.target(state_.target):nullptr;
    if(target&&double(state_.targetCheckTime+.5f)<time){if(state_.moveToTarget)setDestination(distance2(motion_.position,target->position)<40000.f?motion_.position:target->position);state_.targetCheckTime=float(time);}
}
void Bot::tickParts(double time,BotHost& host){
    poses_=models_.mountedPose(mounts_,motion_.angle);atlasPoses_=poses_;
    for(std::size_t i=0;i<mounts_.size();++i){auto& mount=mounts_[i];const auto& part=models_.part(mount.part);
        auto w=std::find_if(weapons_.begin(),weapons_.end(),[&](const auto& w){return w.mountIndex==i;});
        if(w==weapons_.end()&&part.type!=7)mount.angle=mount.parent<0?motion_.angle:mounts_[std::size_t(mount.parent)].angle;
        auto fresh=models_.mountedPose(mounts_,motion_.angle);auto pose=poseFor(fresh,i);
        if(part.type==7){mount.angle+=.2f;constexpr float tau=std::bit_cast<float>(0x40c90fdbu);if(mount.angle>tau)mount.angle-=tau;pose.angle=mount.angle;}
        poseFor(atlasPoses_,i)=pose;
        if(w!=weapons_.end()){
            const auto* target=state_.target?host.target(state_.target):nullptr;std::optional<Vec2> aim;if(target&&target->hp>0)aim=target->position;
            const auto ready=w->aim.tick(time,motion_.position,motion_.angle,aim,host,true,true,state_.hp);if(ready)state_.weaponReady=*ready;
            mount.angle=w->aim.state().angle;pose.angle=mount.angle;
            WeaponTick input;input.time=time;input.machinePosition=motion_.position;input.playerPosition=playerPosition(host);input.pose=pose;
            input.currentMuzzle=&w->currentMuzzle;input.weaponReady=state_.weaponReady;
            if(mount.parent>=0)input.parentAddX=&mounts_[std::size_t(mount.parent)].addPosition.x;
            WeaponBridge bridge(*this,host,i,pose.center,metrics_);const auto* fireTarget=bridge.target();const bool enters=fireTarget&&fireTarget->hp>0;
            w->controller.tick(w->info,input,bridge);if(enters)mount.addPosition.x=w->controller.state().fireBound;
        }
        poseFor(poses_,i)=pose;
    }
}
void Bot::tick(BotTick input,BotHost& host){
    if(state_.removed)return;if(!std::isfinite(input.time)||!std::isfinite(float(input.time)))throw std::runtime_error("Invalid Bot clock");++metrics_.ticks;
    if(!input.paused){aircraftAI(input.time,host);switch(info_.referenceType){case 0:attackJob(host);break;case 2:pickJob(input.time,host);break;default:break;}regenerate(input.time,host);}
    if(!state_.show)return;
    if(!input.paused&&(motion_.state==0||motion_.state==1)){moveAircraft();followTarget(input.time,host);}
    // Original parent tick continues visual/controller children during pause.
    host.beforeParts(*this);tickParts(input.time,host);
}
void Bot::crash(BotHost& host){
    host.baseCrash(*this);if(state_.target){host.unselectTarget(state_.target);state_.target=0;state_.moveToTarget=false;}motion_.state=2;
    host.clearSlotCode(state_.slotIndex);host.writeSlotHP(state_.slotIndex,0);state_.removed=true;++metrics_.deaths;
}
void Bot::onDamage(int amount,BotHost& host){
    ++metrics_.damageCalls;state_.hp=std::bit_cast<std::int32_t>(std::uint32_t(state_.hp)-std::uint32_t(amount));
    if(state_.hp<1){state_.hp=0;crash(host);}host.hitSound(*this);refreshHPGauge(host);
}
}

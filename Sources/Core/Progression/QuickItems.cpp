#include "Core/Progression/QuickItems.hpp"
#include <bit>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace hm2::progression {
namespace {
float distanceSquared(Vec2 a,Vec2 b){const float x=a.x-b.x,y=a.y-b.y;return x*x+y*y;}
int integer(float value){
    if(!std::isfinite(value)||double(value)<std::numeric_limits<int>::min()||double(value)>std::numeric_limits<int>::max())
        throw std::runtime_error("Unrepresentable original item parameter");
    return int(value);
}
int add(int a,int b){return std::bit_cast<std::int32_t>(std::uint32_t(a)+std::uint32_t(b));}
int multiply(int a,int b){return std::bit_cast<std::int32_t>(std::uint32_t(a)*std::uint32_t(b));}
}
QuickItems::QuickItems(Inventory& inventory,SlotView slot,const Weapons& weapons,QuickItemsHost& host)
    :inventory_(inventory),slot_(slot),weapons_(weapons),host_(host){}
void QuickItems::use(const Item* item){
    item_=item;
    if(!item){tapState_=0;host_.hideMessage();selected_=-1;host_.selectQuick(-1);return;}
    if(item->type==1){tapState_=1;host_.message("TapToRepairMach",0,false);}
    else if(item->type==2){tapState_=2;host_.message("TapToAttackPos",0,false);}
    // Original other types retain tapState, including a prior armed mode.
}
void QuickItems::clear(){use(nullptr);}
void QuickItems::clearAfterTap(){
    item_=nullptr;tapState_=0;selected_=-1;host_.selectQuick(-1);host_.hideMessage();
}
bool QuickItems::press(unsigned slot,int displayedCount,bool paused){
    if(paused||slot>=5)return false;
    const auto* item=inventory_.quickSlots()[slot];selected_=item&&displayedCount>0?int(slot):-1;
    host_.selectQuick(selected_);
    if(selected_>=0){host_.sound(0);use(item==item_?nullptr:item);}
    // An empty/zero-count quick press changes selection only; the original
    // handler does not call World.useItem(nil) on this branch.
    return true;
}
void QuickItems::repair(const Item& item,const RepairTarget& target,float renderTime){
    if(!target.player&&(target.botSlot<0||target.botSlot>=3))throw std::runtime_error("Repair requires the actual original BOT slot");
    // GobHvMach.useRepairItem runs before Player/BOT HP writes. A full pool
    // skips this visual only, and therefore skips the pool reset RNG too.
    if(auto* particle=host_.allocateParticle(renderTime)){
        auto& p=particle->state();p.life=1;p.tile=host_.prepareRepairEffect(item.iconImage());p.tileNumber=0;
        p.scale={.5f,.5f};p.scaleVelocity=.01f;p.position=target.position;particle->start(renderTime);
    }
    int amount=integer(item.param1);
    if(item.param2==1.f)amount=multiply(target.maximumHP,amount)/100;
    int hp=add(target.hp,amount);if(target.maximumHP<hp)hp=target.maximumHP;
    host_.setHP(target,hp);
    if(target.player)slot_.hp(hp);
    else slot_.u32(0x64c+unsigned(target.botSlot)*4,std::uint32_t(hp)); // Bot.refreshHPGauge0x8b7c.
    auto repaired=target;repaired.hp=hp;host_.refreshActorHP(repaired);if(target.player)host_.refreshPlayerHP();
}
bool QuickItems::input(PointerPhase phase,Vec2 screen,const MapCamera& camera,bool paused,float renderTime,double worldTime){
    const auto player=host_.player();
    if(paused||!player||player->hp<1)return false;
    if(tapState_==0)return false; // Preserve the normal navigation consumer.
    if(phase!=PointerPhase::begin||!item_)return true;
    if(!std::isfinite(screen.x)||!std::isfinite(screen.y)||!std::isfinite(renderTime)||!std::isfinite(worldTime))
        throw std::runtime_error("Nonfinite original quick-item input");
    // Original0x12284/0x11ff8 performs these float operations in this
    // order. Reassociating to screen+center-half+offset changes strict900.
    const Vec2 base{camera.center.x-camera.viewport.width*.5f,camera.center.y-camera.viewport.height*.5f};
    const Vec2 translated{base.x+camera.offset.x,base.y+camera.offset.y};
    const Vec2 world{translated.x+screen.x,translated.y+screen.y};const Item* item=item_;
    if(tapState_==1){
        std::optional<RepairTarget> target;
        if(distanceSquared(world,player->position)<900.f)target=*player;
        else{
            float nearest=900;
            for(const auto& bot:host_.bots()){const float d=distanceSquared(world,bot.position);if(d<nearest){nearest=d;target=bot;}}
        }
        if(target){
            repair(*item,*target,renderTime);
            inventory_.subtract(*item,1);host_.refreshQuickItem(*item,inventory_.count(*item));host_.sound(13);
        }
        clearAfterTap();return true;
    }
    if(tapState_==2){
        if(item->stringParameter.starts_with("AS_")){
            std::array<char,128> key{};const int count=std::snprintf(key.data(),key.size(),"%s_%02d",item->stringParameter.c_str(),integer(item->param1));
            if(count<0||count>=int(key.size()))throw std::runtime_error("Original airstrike key exceeds its field");
            const auto weapon=weapons_.all().find(key.data());const auto* info=weapon==weapons_.all().end()?nullptr:&weapon->second;
            host_.startAirStrike({item,info,key.data(),world,worldTime,renderTime,item->param2,info?unsigned(info->shotCount):0});
            inventory_.subtract(*item,1);host_.refreshQuickItem(*item,inventory_.count(*item));
        }
        clearAfterTap();return true;
    }
    return true;
}
}

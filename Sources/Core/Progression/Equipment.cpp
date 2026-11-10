#include "Core/Progression/Equipment.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace hm2::progression {
namespace {
int signed32(std::uint32_t v){return std::bit_cast<std::int32_t>(v);}
int multiply(int a,int b){return signed32(std::uint32_t(a)*std::uint32_t(b));}
std::int16_t shortCount(int v){return std::bit_cast<std::int16_t>(std::uint16_t(v));}
constexpr int slotTypes[]={11,12,10,10,13,14};
}
int EquipStats::playerDamage(int incoming,WorldRandom& random)const{
    const int defense=signed32(std::uint32_t(bodyDefense)+std::uint32_t(armorDefense));if(!defense)return incoming;
    const int r=int(random.random())%defense;
    const int reduction=signed32(std::uint32_t(multiply(r,incoming))+std::uint32_t(incoming))/100;
    return signed32(std::uint32_t(incoming)-std::uint32_t(reduction));
}
Equipment::Equipment(SlotView slot,Inventory& inventory,const Definitions& defs,const Weapons& weapons,const DataTable& table,Events& events,EquipmentEvents* ui,EquipStats initial):slot_(slot),inventory_(inventory),defs_(defs),weapons_(weapons),events_(events),ui_(ui),stats_(initial){
    for(std::size_t row=0;row<table.rowCount();++row)if(const auto* item=defs.findItem(table.value(row,"Code").value_or("")))itemOrder_.push_back(item);
}
void Equipment::publish(){if(ui_)ui_->statsChanged(stats_);}
void Equipment::refreshLevel(){
    const auto b=Experience::bounds(slot_.level());slot_.maxHP(b.baseMaxHP);stats_.baseHP=b.baseMaxHP;stats_.damageBonus=b.levelStat;
    // calcLevelUpExp0x16774 signed divide2048, clamp only above270.
    stats_.hpBarWidth=std::min(multiply(stats_.maximumHP(),270)/2048,270);publish();
}
void Equipment::apply(const Item& item,unsigned slot){
    switch(slot){
    case 0:
        stats_.bonusHP=int(item.param1);stats_.bodyDefense=int(item.param2);refreshLevel();if(ui_)ui_->refreshHPGauge();break;
    case 1:
        stats_.specialMaximum=item.param1;stats_.specialRegeneration=item.param2;stats_.stepSizeInteger=int(item.param3);stats_.stepSize=item.param3;
        stats_.specialBarWidth=int(stats_.specialMaximum*146.f/320.f);if(ui_)ui_->stepSize(item.param3);
        // Original0x1821c bottom branch clamps offset+8 (HP), not offset+C(SP).
        if(stats_.hpBarWidth>146)stats_.hpBarWidth=146;break;
    case 2:case 3:{
        if(ui_)ui_->weaponUI(item,slot);
        const auto found=weapons_.all().find(item.stringParameter);if(found!=weapons_.all().end()){
            if(slot==2)stats_.leftAttackPoint=found->second.attackPoint;else stats_.rightAttackPoint=found->second.attackPoint;
        }break;
    }
    case 4:stats_.armorDefense=int(item.param1);break;
    case 5:break; // Original PtEquip branch only changes the slot/flag.
    default:throw std::out_of_range("Original equipment slot outside6 slots");
    }publish();
}
void Equipment::restore(){
    for(unsigned i=0;i<6;++i){const auto* entry=inventory_.entry(inventory_.equipment()[i]);if(entry&&entry->item&&entry->count>0&&entry->item->type==slotTypes[i])apply(*entry->item,i);}
    stats_.specialCurrent=0;refreshLevel();
}
bool Equipment::equip(int position,unsigned slot,bool inField){
    failure_=EquipmentFailure::none;const auto* entry=inventory_.entry(position);
    if(!entry||!entry->item||entry->count<1||slot>=6){failure_=EquipmentFailure::invalidPosition;return false;}
    if(entry->flags){failure_=EquipmentFailure::alreadyEquipped;return false;}
    if(entry->item->type!=slotTypes[slot]){failure_=EquipmentFailure::wrongType;return false;}
    if(inField&&slot!=2&&slot!=3){
        failure_=EquipmentFailure::fieldRestricted;if(slot==0||slot==1)events_.alert("EquipParts");events_.sound(4);return false;
    }
    const auto* item=entry->item;const bool ok=inventory_.setEquip(position,slot,[&](const Item& equipped,unsigned s){apply(equipped,s);});
    if(ok){if(inField&&ui_)ui_->setParts(item->stringParameter,slot==2?5:6);events_.changed(Change::inventory);events_.sound(4);}return ok;
}
void Equipment::swap(int a,int b){if(!inventory_.entry(a)||!inventory_.entry(b))return;inventory_.swap(a,b);events_.changed(Change::inventory);}
void Equipment::arrange(){inventory_.arrange();events_.changed(Change::inventory);events_.sound(0);}
bool Equipment::registerQuick(unsigned slot,int position){
    failure_=EquipmentFailure::none;if(slot>=5){failure_=EquipmentFailure::invalidPosition;return false;}
    const Item* item=nullptr;if(position!=-1){const auto* e=inventory_.entry(position);if(!e||!e->item){failure_=EquipmentFailure::invalidPosition;return false;}item=e->item;}
    // DlgInven drag/commit does not test AttrQ here; it assigns the actual item.
    inventory_.quickSlot(slot,item);if(ui_)ui_->quickChanged(slot,item);events_.changed(Change::inventory);events_.sound(12);return true;
}
const Item* Equipment::bot(unsigned slot)const{if(slot>=3)throw std::out_of_range("Original bot slot outside3 slots");return defs_.findItem(slot_.string(0x63a+slot*6,6));}
void Equipment::setBot(unsigned slot,const Item* item){slot_.string(0x63a+slot*6,6,item?item->code:"");slot_.u32(0x64c+slot*4,0);if(ui_)ui_->botChanged(slot,item);events_.changed(Change::inventory);}
BotChange Equipment::registerBot(unsigned slot,int position,bool inField){
    failure_=EquipmentFailure::none;if(slot>=3){failure_=EquipmentFailure::invalidPosition;return {};}
    if(inField){failure_=EquipmentFailure::fieldRestricted;events_.alert("EquipParts");return {};}
    const auto* entry=inventory_.entry(position);if(!entry||!entry->item||entry->count<1){failure_=EquipmentFailure::invalidPosition;return {};}
    if(entry->item->type!=4){failure_=EquipmentFailure::wrongType;return {};}
    const Item* item=entry->item;const Item* old=bot(slot);events_.sound(4);setBot(slot,item);inventory_.subtractFrom(position,1);
    const bool returned=!old||inventory_.add(*old,1);return {true,returned};
}
BotChange Equipment::unregisterBot(unsigned slot){
    failure_=EquipmentFailure::none;if(slot>=3){failure_=EquipmentFailure::invalidPosition;return {};}
    const auto* old=bot(slot);bool returned=true;if(old){returned=inventory_.add(*old,1);setBot(slot,nullptr);}events_.sound(0);return {old!=nullptr,returned};
}
bool Equipment::category(const Item& item,unsigned category){switch(category){case 0:return item.type==10;case 1:return item.type>=11&&item.type<=14;case 2:return item.type==4;case 3:return item.type==2||item.type==3;case 4:return item.type==1||item.type==5||item.type==6;default:return false;}}
std::vector<const Item*> Equipment::shopItems(unsigned c)const{
    std::vector<const Item*> out;const auto level=slot_.level();for(const auto* i:itemOrder_)if(i->buy&&(i->level==0||(level-20<i->level&&i->level<=level+7))&&category(*i,c))out.push_back(i);return out;
}
int Equipment::shopPageCount(unsigned c)const{return int(shopItems(c).size()/24)+1;}
int Equipment::sellPrice(const Item& item){return item.type==5?item.price:item.price/2;}
TradeResult Equipment::buy(std::string_view code,int count){
    failure_=EquipmentFailure::none;const auto* item=defs_.findItem(code);
    if(count<1){failure_=EquipmentFailure::invalidQuantity;return {false,0,0,failure_};}
    const auto level=slot_.level();if(!item||!item->buy||(item->level!=0&&!(level-20<item->level&&item->level<=level+7))){failure_=EquipmentFailure::notAvailable;return {false,0,0,failure_};}
    const int total=multiply(item->price,count);if(signed32(slot_.credit())<total){failure_=EquipmentFailure::insufficientCredit;events_.alert("NotEnoughCr");events_.sound(0);return {false,0,0,failure_};}
    const int before=inventory_.count(*item);const bool added=inventory_.add(*item,shortCount(count));const int delta=inventory_.count(*item)-before;
    if(!added){failure_=EquipmentFailure::inventoryFull;events_.alert("TakeItemFail");events_.sound(0);return {false,delta,0,failure_};}
    slot_.credit(slot_.credit()-std::uint32_t(total));events_.changed(Change::credit);events_.sound(0);return {true,delta,signed32(0u-std::uint32_t(total)),EquipmentFailure::none};
}
TradeResult Equipment::sell(int position,int count){
    failure_=EquipmentFailure::none;const auto* entry=inventory_.entry(position);
    if(!entry||!entry->item||entry->count<1){failure_=EquipmentFailure::invalidPosition;return {false,0,0,failure_};}
    if(!entry->item->sell||entry->flags){failure_=EquipmentFailure::notAvailable;return {false,0,0,failure_};}
    if(count<1||count>entry->count){failure_=EquipmentFailure::invalidQuantity;return {false,0,0,failure_};}
    const auto* item=entry->item;const int total=multiply(sellPrice(*item),count);inventory_.subtractFrom(position,shortCount(count));slot_.credit(slot_.credit()+std::uint32_t(total));events_.changed(Change::credit);events_.sound(0);return {true,-count,total,EquipmentFailure::none};
}
EquipmentCodes Equipment::codes()const{EquipmentCodes out{};for(unsigned i=0;i<4;++i){const auto* e=inventory_.entry(inventory_.equipment()[i]);if(e&&e->item)out[i]=e->item->code;}return out;}
std::vector<PartMount> Equipment::mounts(const Models& models,const PlayerEquipment& parts,PlayerPartAngles angles)const{return parts.mounts(models,codes(),angles);}
void Equipment::tickSpecial(){if(stats_.specialCurrent<stats_.specialMaximum)stats_.specialCurrent+=stats_.specialRegeneration;if(stats_.specialCurrent>stats_.specialMaximum)stats_.specialCurrent=stats_.specialMaximum;}
void Equipment::specialCurrent(float value){if(!std::isfinite(value))throw std::runtime_error("Nonfinite SP state");stats_.specialCurrent=value;}
}

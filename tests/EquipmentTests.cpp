#include "Core/Progression/Equipment.hpp"
#include "Core/Data/SaveData.hpp"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace hm2;using namespace hm2::progression;
namespace {
struct Random:WorldRandom {std::uint32_t value=0;unsigned calls=0;std::uint32_t random()override{++calls;return value;}};
struct Log:Events,EquipmentEvents {
    std::vector<std::string> calls;std::function<void()> gaugeCheck;unsigned alertCount=0,partCalls=0;
    void changed(Change c)override{calls.push_back("dirty"+std::to_string(int(c)));}
    void sound(unsigned i)override{calls.push_back("sound"+std::to_string(i));}
    void alert(std::string_view key)override{calls.push_back(std::string(key));++alertCount;}
    void statsChanged(const EquipStats&)override{calls.push_back("stats");}
    void refreshHPGauge()override{calls.push_back("hpGauge");if(gaugeCheck)gaugeCheck();}
    void stepSize(float)override{calls.push_back("step");}
    void weaponUI(const Item&,unsigned slot)override{calls.push_back("weaponUI"+std::to_string(slot));}
    void setParts(std::string_view part,unsigned type)override{assert(!part.empty()&&(type==5||type==6));calls.push_back("parts"+std::to_string(type));++partCalls;}
};
struct Data {
    std::filesystem::path root;unsigned reads=0;DataTable items;Definitions defs;Weapons weapons;Models models;PlayerEquipment parts;
    Bytes read(std::string_view name){++reads;std::ifstream f(root/name,std::ios::binary);if(!f)throw std::runtime_error("Missing original resource "+std::string(name));return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
    DataTable table(std::string_view name){return DataTable::parse(read(name));}
    explicit Data(std::filesystem::path p):root(std::move(p)){
        items=table("ItemInfo.tble");defs=Definitions::parse(items,table("ItemRegen.tble"),table("MissionList.tble"),[&](auto name){return table(name);},"us");
        weapons=Weapons::parse(table("WeaponInfo.tble"));models=Models::parse(Chunk::parse(read("PartsInfo.mcb")),Chunk::parse(read("MachInfo.mcb")));parts=PlayerEquipment::parse(items);
    }
};
struct Fixture {
    SaveData save;SlotView slot;Log log;Progression progress;Equipment equipment;
    explicit Fixture(Data& d):slot(save.slot(0)),progress(slot,d.defs,log),equipment(slot,progress.inventory,d.defs,d.weapons,d.items,log,&log){save.initializeSlot(0,"EquipmentTest","us",0);progress.restore();equipment.restore();}
    int add(Data& d,std::string_view code,int count=1){const auto* item=d.defs.findItem(code);assert(item&&progress.inventory.add(*item,std::int16_t(count)));for(int i=0;i<progress.inventory.pages()*25;++i){const auto* e=progress.inventory.entry(i);if(e->item==item&&!e->flags&&e->count>0)return i;}assert(false);return -1;}
};
void originalRestoreAndEquip(Data& d){
    Fixture f(d);assert(f.progress.inventory.equipment()==(std::array<int,6>{0,1,2,3,-1,-1}));
    for(int i=0;i<4;++i)assert(f.progress.inventory.entry(i)->flags==1);
    const auto& stats=f.equipment.stats();assert(stats.maximumHP()==256&&stats.damageBonus==6&&stats.bodyDefense==0&&stats.armorDefense==0);assert(stats.hpBarWidth==33&&stats.specialBarWidth==82&&stats.specialMaximum==180&&stats.specialCurrent==0&&stats.specialRegeneration==1&&stats.stepSize==20);
    assert(stats.leftAttackPoint==d.weapons.weapon("P_Wpn_Gun_101").attackPoint&&stats.rightAttackPoint==d.weapons.weapon("P_Wpn_Msl_101").attackPoint);
    assert(f.slot.credit()==500&&f.progress.inventory.count(*d.defs.findItem("01001"))==5); // Restore never buys or consumes.
    EquipStats previous;previous.armorDefense=17;Equipment retained(f.slot,f.progress.inventory,d.defs,d.weapons,d.items,f.log,&f.log,previous);retained.restore();assert(retained.stats().armorDefense==17); // Original clearInven does not clear absent global armor.
    assert(!f.equipment.canUnequip(0)&&!f.progress.inventory.setEquip(-1,0));
    const int body=f.add(d,"11002");f.slot.hp(91);int actorHP=73;f.log.gaugeCheck=[&]{assert(f.progress.inventory.entry(body)->flags==0&&f.progress.inventory.entry(0)->flags==1);};
    assert(f.equipment.equip(body,0));f.log.gaugeCheck={};assert(f.equipment.stats().maximumHP()==276&&f.equipment.stats().bodyDefense==2&&f.slot.hp()==91&&actorHP==73);assert(f.progress.inventory.entry(0)->flags==0&&f.progress.inventory.entry(body)->flags==1);
    assert(!f.equipment.equip(body,0)&&f.equipment.lastFailure()==EquipmentFailure::alreadyEquipped);assert(!f.equipment.equip(5,2)&&f.equipment.lastFailure()==EquipmentFailure::wrongType);
    const int anotherBody=f.add(d,"11003");assert(!f.equipment.equip(anotherBody,0,true)&&f.equipment.lastFailure()==EquipmentFailure::fieldRestricted);
    const int weapon=f.add(d,"10012");const auto beforeParts=f.log.partCalls;assert(f.equipment.equip(weapon,3,true)&&f.log.partCalls==beforeParts+1);assert(f.log.calls.end()!=std::find(f.log.calls.begin(),f.log.calls.end(),"parts6"));
    // Owning an above-level drop is sufficient; raw/maintenance equip has no Lv gate.
    const int highBody=f.add(d,"11034");assert(f.equipment.equip(highBody,0));const int armor=f.add(d,"13006");assert(f.equipment.equip(armor,4));assert(f.equipment.stats().maximumHP()==1256&&f.equipment.stats().bodyDefense==70&&f.equipment.stats().armorDefense==32);
    Random random;random.value=101;assert(f.equipment.stats().playerDamage(100,random)==-2&&random.calls==1);EquipStats zero;const auto calls=random.calls;assert(zero.playerDamage(100,random)==100&&random.calls==calls);
    const int foot=f.add(d,"12002");assert(f.equipment.stats().hpBarWidth==165);assert(f.equipment.equip(foot,1)&&f.equipment.stats().hpBarWidth==146&&f.equipment.stats().specialBarWidth==86);f.equipment.refreshLevel();assert(f.equipment.stats().hpBarWidth==165);
    f.slot.level(99);f.equipment.refreshLevel();assert(f.equipment.stats().damageBonus==600&&f.equipment.stats().hpBarWidth==270);const int foot2=f.add(d,"12003");assert(f.equipment.equip(foot2,1)&&f.equipment.stats().hpBarWidth==146);f.equipment.refreshLevel();assert(f.equipment.stats().hpBarWidth==270&&f.equipment.stats().stepSize==22);
    f.equipment.specialCurrent(199.5f);f.equipment.tickSpecial();assert(f.equipment.stats().specialCurrent==200);f.equipment.restore();assert(f.equipment.stats().specialCurrent==0&&f.equipment.stats().hpBarWidth==270);
    std::cout<<"Original six-slot restore/type/flag/field gates/derived stats/HP separation PASS\n";
}
void swapArrangeAndQuick(Data& d){
    Fixture f(d);std::array<std::string,4> before;for(unsigned i=0;i<4;++i)before[i]=f.equipment.codes()[i];
    f.equipment.swap(2,24);assert(f.progress.inventory.equipment()[2]==24&&f.progress.inventory.entry(24)->flags==1);assert(f.equipment.codes()[2]==before[2]);
    f.equipment.arrange();for(unsigned i=0;i<4;++i)assert(f.equipment.codes()[i]==before[i]);
    const auto& entries=f.progress.inventory.entries();assert(entries[0].item->type==11&&entries[1].item->type==12&&entries[2].item->type==10&&entries[3].item->type==10&&entries[4].item->type==4&&entries[5].item->type==1);
    const int bolt=f.add(d,"05001",3);const auto count=f.progress.inventory.count(*d.defs.findItem("05001"));assert(!d.defs.findItem("05001")->quick);assert(f.equipment.registerQuick(4,bolt)&&f.progress.inventory.isQuick(*d.defs.findItem("05001")));assert(f.progress.inventory.count(*d.defs.findItem("05001"))==count);
    assert(!f.equipment.registerQuick(5,bolt));f.progress.prepareSave();assert(f.slot.string(0x61c+4*6,6)=="05001");assert(f.equipment.registerQuick(4,-1)&&!f.progress.inventory.isQuick(*d.defs.findItem("05001")));
    std::cout<<"Swap/arrange retain flags/equip indices; actual quick5 registration PASS\n";
}
void stockAndTrading(Data& d){
    Fixture f(d);const auto parts=f.equipment.shopItems(1);assert(parts.size()==8&&f.equipment.shopPageCount(1)==1);
    assert(f.equipment.buy("11001",1).failure==EquipmentFailure::notAvailable);assert(f.equipment.buy("11034",1).failure==EquipmentFailure::notAvailable);
    auto result=f.equipment.buy("01001",2);assert(result.complete&&result.itemDelta==2&&result.creditDelta==-20&&f.slot.credit()==480&&f.progress.inventory.count(*d.defs.findItem("01001"))==7);
    result=f.equipment.buy("11004",1);assert(!result.complete&&result.failure==EquipmentFailure::insufficientCredit&&f.slot.credit()==480&&f.log.calls.end()!=std::find(f.log.calls.begin(),f.log.calls.end(),"NotEnoughCr"));
    f.slot.credit(800);result=f.equipment.buy("11004",1);assert(result.complete&&f.slot.credit()==0); // Lv6 is within original +7 stock window at Lv0.
    assert(!f.equipment.sell(f.progress.inventory.equipment()[0],1).complete);result=f.equipment.sell(5,1);assert(result.complete&&result.creditDelta==5&&f.slot.credit()==5);
    const int bolt=f.add(d,"05001",3);result=f.equipment.sell(bolt,2);assert(result.complete&&result.creditDelta==4&&f.slot.credit()==9);assert(Equipment::sellPrice(*d.defs.findItem("05001"))==2&&Equipment::sellPrice(*d.defs.findItem("01001"))==5);
    assert(!f.equipment.sell(bolt,2).complete&&f.slot.credit()==9);
    f.slot.level(21);const auto stock=f.equipment.shopItems(1);auto contains=[&](auto id){return std::any_of(stock.begin(),stock.end(),[&](auto* item){return item->code==id;});};assert(!contains("11002")&&contains("11003")&&contains("11016")&&!contains("11017"));
    const auto pos=[&](auto id){return std::find_if(stock.begin(),stock.end(),[&](auto* item){return item->code==id;})-stock.begin();};assert(pos("11013")<pos("11010")); // Original rows, not code sorting.
    f.slot.level(17);
    assert(f.equipment.shopItems(1).size()==24&&f.equipment.shopPageCount(1)==2);
    f.slot.level(21);
    assert(f.equipment.shopItems(0).size()==48&&f.equipment.shopPageCount(0)==3);
    f.slot.level(99);
    const auto highLevelWeapons=f.equipment.shopItems(0);
    assert(highLevelWeapons.size()==1&&highLevelWeapons.front()->code=="10504");
    assert(f.equipment.shopPageCount(0)==1);
    Fixture partial(d);partial.progress.inventory.pages(1);for(int i=0;i<25;++i)partial.progress.inventory.setItem(i,i<24?d.defs.findItem("10011"):nullptr,i<24?1:0);partial.slot.credit(1000);
    result=partial.equipment.buy("11002",2);assert(!result.complete&&result.failure==EquipmentFailure::inventoryFull&&result.itemDelta==1&&result.creditDelta==0&&partial.slot.credit()==1000&&partial.progress.inventory.count(*d.defs.findItem("11002"))==1);
    std::cout<<"Original stock range/order, balances/prices, rejected/partial purchase and sales PASS\n";
}
void botRegistration(Data& d){
    Fixture f(d);assert(f.equipment.bot(0)->code=="04201");const auto originalCount=f.progress.inventory.count(*d.defs.findItem("04201"));assert(originalCount==1);
    const auto denied=f.equipment.registerBot(1,4,true);assert(!denied.changed&&f.equipment.bot(1)==nullptr&&f.progress.inventory.count(*d.defs.findItem("04201"))==1);
    const auto assigned=f.equipment.registerBot(1,4);assert(assigned.changed&&assigned.oldReturned&&f.equipment.bot(1)->code=="04201"&&f.progress.inventory.count(*d.defs.findItem("04201"))==0&&f.slot.u32(0x64c+4)==0);
    const int bot=f.add(d,"04001",2);f.slot.u32(0x64c,12345);auto swapped=f.equipment.registerBot(0,bot);assert(swapped.changed&&swapped.oldReturned&&f.equipment.bot(0)->code=="04001"&&f.slot.u32(0x64c)==0&&f.progress.inventory.count(*d.defs.findItem("04001"))==1&&f.progress.inventory.count(*d.defs.findItem("04201"))==1);
    auto removed=f.equipment.unregisterBot(0);assert(removed.changed&&removed.oldReturned&&f.equipment.bot(0)==nullptr&&f.progress.inventory.count(*d.defs.findItem("04001"))==2);
    Fixture full(d);full.progress.inventory.pages(1);for(int i=0;i<25;++i)full.progress.inventory.setItem(i,i?d.defs.findItem("10011"):d.defs.findItem("04001"),i?1:2);const auto alerts=full.log.alertCount;
    swapped=full.equipment.registerBot(0,0);assert(swapped.changed&&!swapped.oldReturned&&full.equipment.bot(0)->code=="04001"&&full.progress.inventory.count(*d.defs.findItem("04201"))==0&&full.log.alertCount==alerts);
    full.slot.string(0x63a,6,"04002");removed=full.equipment.unregisterBot(0);assert(removed.changed&&!removed.oldReturned&&full.equipment.bot(0)==nullptr&&full.log.alertCount==alerts);
    std::cout<<"Bot3 register/replace/base gate/return order and full-inventory original loss PASS\n";
}
void originalPartCaches(Data& d){
    const auto reads=d.reads;unsigned mounted=0;
    for(const auto& [id,item]:d.defs.items())if(item.type==10||item.type==11||item.type==12){
        Fixture f(d);const int pos=f.add(d,id);const unsigned slot=item.type==11?0:item.type==12?1:2;assert(f.equipment.equip(pos,slot,item.type==10));
        const auto mounts=f.equipment.mounts(d.models,d.parts);assert(mounts.size()==6);const auto pose=d.models.mountedPose(mounts);assert(pose.size()==6);
        assert(std::any_of(pose.begin(),pose.end(),[&](const auto& p){return p.part==&d.models.part(item.stringParameter);}));++mounted;
    }
    assert(d.reads==reads&&mounted>100);std::cout<<"Original item->strParam->common Models cache mounts="<<mounted<<" warmReads=0 PASS\n";
}
}
int main(int argc,char** argv){assert(argc==2);Data data(argv[1]);originalRestoreAndEquip(data);swapArrangeAndQuick(data);stockAndTrading(data);botRegistration(data);originalPartCaches(data);std::cout<<"HM2_EQUIPMENT originalItems="<<data.defs.items().size()<<" PASS (memory only; item use not implemented here)\n";}

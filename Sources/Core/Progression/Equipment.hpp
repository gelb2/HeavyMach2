#pragma once
#include "Core/Progression/Progression.hpp"
#include "Core/Data/PlayerEquipment.hpp"
#include "Core/Data/Weapons.hpp"

namespace hm2::progression {
struct EquipStats {
    // damageBonus is the original +0x20 PERCENT, not integer extra damage.
    // WeaponEffects/ProjectileImpact apply it to each original base damage.
    int baseHP=0,bonusHP=0,bodyDefense=0,armorDefense=0,damageBonus=0;
    int hpBarWidth=0,specialBarWidth=0,leftAttackPoint=0,rightAttackPoint=0,stepSizeInteger=0;
    float specialCurrent=0,specialMaximum=0,specialRegeneration=0,stepSize=0;
    int maximumHP()const{return baseHP+bonusHP;}
    // Original Player0xbf58. No invented minimum/percentage clamp.
    int playerDamage(int incoming,WorldRandom&)const;
};
struct EquipmentEvents {
    virtual ~EquipmentEvents()=default;
    virtual void statsChanged(const EquipStats&){}
    virtual void refreshHPGauge(){}
    virtual void stepSize(float){}
    virtual void weaponUI(const Item&,unsigned){}
    // Only field weapon replacement calls original Player.setParts types5/6.
    virtual void setParts(std::string_view,unsigned){}
    virtual void quickChanged(unsigned,const Item*){}
    virtual void botChanged(unsigned,const Item*){}
};
enum class EquipmentFailure {none,invalidPosition,wrongType,alreadyEquipped,fieldRestricted,notAvailable,insufficientCredit,inventoryFull,invalidQuantity};
struct TradeResult {bool complete=false;int itemDelta=0,creditDelta=0;EquipmentFailure failure=EquipmentFailure::none;};
struct BotChange {bool changed=false,oldReturned=true;};
// Source stock order comes from ItemInfo's rows, not map/code sorting.
class Equipment {
    SlotView slot_;Inventory& inventory_;const Definitions& defs_;const Weapons& weapons_;Events& events_;EquipmentEvents* ui_;
    EquipStats stats_;std::vector<const Item*> itemOrder_;EquipmentFailure failure_=EquipmentFailure::none;
    void apply(const Item&,unsigned);void publish();void setBot(unsigned,const Item*);
public:
    Equipment(SlotView,Inventory&,const Definitions&,const Weapons&,const DataTable& originalItems,Events&,EquipmentEvents* =nullptr,EquipStats initial={});
    Equipment(const Equipment&)=delete;Equipment& operator=(const Equipment&)=delete;
    // Run after Progression.restore. Replays validated equipment0..5, then the
    // final selectSlot calcLevel and SP=0 rule. It never purchases anything.
    void restore();void refreshLevel();
    const EquipStats& stats()const{return stats_;}
    EquipmentFailure lastFailure()const{return failure_;}
    bool equip(int inventoryPosition,unsigned slot,bool inField=false);
    // Raw setEquip(-1,slot) is false in HD. General body/weapon unequip is not
    // invented; the original Disarm action below is for bot registration.
    bool canUnequip(unsigned)const{return false;}
    void swap(int,int);void arrange();
    bool registerQuick(unsigned quickSlot,int inventoryPosition);
    BotChange registerBot(unsigned botSlot,int inventoryPosition,bool inField=false);
    BotChange unregisterBot(unsigned botSlot);
    const Item* bot(unsigned)const;
    static bool category(const Item&,unsigned);
    std::vector<const Item*> shopItems(unsigned category)const;
    int shopPageCount(unsigned category)const; // original count/24+1
    static int sellPrice(const Item&);
    TradeResult buy(std::string_view code,int count=1);
    TradeResult sell(int inventoryPosition,int count);
    EquipmentCodes codes()const;
    std::vector<PartMount> mounts(const Models&,const PlayerEquipment&,PlayerPartAngles={})const;
    // Player.tick's per-tick SP regeneration; actual dash/item use is separate.
    void tickSpecial();void specialCurrent(float);
};
}

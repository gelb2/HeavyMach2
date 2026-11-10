#include "Core/Session/GameSession.hpp"
#include "Core/Progression/Equipment.hpp"
#include "Core/Progression/QuickItems.hpp"
#include "Core/Presentation/PartSprites.hpp"
#include "Core/Presentation/Visibility.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <limits>
#include <stdexcept>

namespace hm2 {
namespace {
using Slot=progression::SlotView;
DataTable table(ResourceAccess& r,std::string_view name){return DataTable::parse(r.read(name));}
}
struct GameSession::Impl:progression::Events,progression::EquipmentEvents,CombatSceneHooks,progression::QuickItemsHost {
    ResourceAccess& resources;SessionHost& host;WorldRandom& random;
    SaveData data;std::array<std::uint8_t,SaveData::slotSize> working{};
    DataTable itemTable,descriptions;progression::Definitions definitions;
    Weapons weapons;Models models;PlayerEquipment parts;
    Maps maps;Machines machines;
    std::unique_ptr<progression::Progression> progress;
    std::unique_ptr<progression::Equipment> equipment;
    std::unique_ptr<CombatScene> scene;
    std::unique_ptr<progression::QuickItems> quickItems;
    PartSprites previewSprites;
    ui::ScreensState view;SessionMetrics metrics;
    GameClock clock;bool active=true,dirty=true,worldSaveAllowed=false;
    std::uint64_t presentationRevision=0;
    int loadedSlot=-1;std::string mapName,selectedMissionId;
    int detailInventory=-1,tradePosition=-1;const progression::Item* tradeItem=nullptr;
    const ui::ScreenAction* activeAction=nullptr;
    static bool screenOwnsSound(const ui::ScreenAction& a,unsigned sound){
        const auto id=a.button.key.id;const auto event=a.button.type;
        if(a.screen==ui::ScreenId::slots)return (event==ui::ButtonEventType::push&&id==1009&&sound==0)||
            (event==ui::ButtonEventType::pop&&(id==1001||id==1004||id==1010)&&sound==1);
        if(a.screen==ui::ScreenId::base)return event==ui::ButtonEventType::push&&id>=2001&&id<=2006&&sound==0;
        if(a.screen==ui::ScreenId::field&&a.panel==ui::Panel::root){
            if(event==ui::ButtonEventType::push&&id>=1022&&id<=1024&&sound==4)return true;
            if(event==ui::ButtonEventType::pop&&sound==0&&(id==1021||id==1025||id==1026||id==1027||id==1029||id==1030||id==1031||id==1032))return true;
        }return false;
    }
    void uiSound(unsigned id){if(!activeAction||!screenOwnsSound(*activeAction,id))host.soundID(id,{});}
    static int wrappedAdd(int a,int b){return std::bit_cast<std::int32_t>(std::uint32_t(a)+std::uint32_t(b));}
    static int wrappedMultiply(int a,int b){return std::bit_cast<std::int32_t>(std::uint32_t(a)*std::uint32_t(b));}
    static std::string localDate(std::uint32_t seconds){
        const std::time_t time=seconds;std::tm local{};if(!localtime_r(&time,&local))return {};
        char text[32];std::snprintf(text,sizeof(text),"%04d/%02d/%02d %02d:%02d:%02d",local.tm_year+1900,local.tm_mon+1,local.tm_mday,local.tm_hour,local.tm_min,local.tm_sec);return text;
    }
    struct Drop {progression::FieldItem item;VisibilityState root,icon;Vec2 pickupPlayer;};
    std::vector<Drop> drops;
    PreparedTiles* worldTiles=nullptr;

    Impl(ResourceAccess& r,SessionHost& h,WorldRandom& rng,std::optional<SaveData> initial)
    :resources(r),host(h),random(rng),data(initial?std::move(*initial):SaveData{}),itemTable(table(r,"ItemInfo.tble")),descriptions(table(r,"Description_us.tble")),
    definitions(progression::Definitions::parse(itemTable,table(r,"ItemRegen.tble"),table(r,"MissionList.tble"),[&r](std::string_view n){return table(r,n);},"us")),
    weapons(Weapons::parse(table(r,"WeaponInfo.tble"))),models(Models::parse(Chunk::parse(r.read("PartsInfo.mcb")),Chunk::parse(r.read("MachInfo.mcb")))),parts(PlayerEquipment::parse(itemTable)),maps(Maps::parse(Chunk::parse(r.read("MapInfo.mcb")))),machines(Machines::parse(table(r,"MachList.tble"))){}
    Slot slot(){return Slot(working);}
    std::string base() {return slot().string(0x55,16);}
    void changed(progression::Change change)override{
        // Outside screen7 Experience does not call Player.levelUp, but original
        // calcLevelUpExp still updates MainInfo's derived stats at a level gain.
        if(change==progression::Change::experience&&equipment&&equipment->stats().baseHP!=slot().maxHP())equipment->refreshLevel();
        if(change==progression::Change::inventory||change==progression::Change::mission)dirty=true;
    }
    void sound(unsigned id)override{uiSound(id);}
    void sound(std::string_view n,Vec2 p)override{host.sound(n,p);}
    void soundID(unsigned id,Vec2 p)override{host.soundID(id,p);}
    void alert(std::string_view key)override{host.alert(key);}
    void refreshHPGauge()override{} // HUD values consume the live slot in dynamicHUD.
    std::optional<progression::RepairTarget> player()const override{
        if(!scene)return std::nullopt;return progression::RepairTarget{1,true,scene->player().position,scene->playerHP(),playerMaximumHP(),-1};
    }
    std::vector<progression::RepairTarget> bots()const override{return {};} // No live BOT actor consumer yet.
    void setHP(const progression::RepairTarget& target,int hp)override{if(!scene||!target.player||target.id!=1)throw std::logic_error("Missing live repair actor");scene->setPlayerHP(hp);}
    void refreshActorHP(const progression::RepairTarget& target)override{if(!scene||!target.player)throw std::logic_error("Missing live repair gauge");scene->refreshPlayerHPGauge();}
    void refreshPlayerHP()override{} // Same live-slot HUD, no new persistence event.
    Particle* allocateParticle(float)override{return scene?scene->allocateParticle():nullptr;}
    const TileImage* preparedTile(std::string_view name)override{return scene?scene->preparedTile(name):nullptr;}
    void selectQuick(int selected)override{view.selectedQuick=selected;dirty=true;}
    void message(std::string_view key,int background,bool autoHide)override{view.messageVisible=true;view.messageBackground=background;view.messageAutoHide=autoHide;view.messageWorldTime=scene?scene->clock().worldTime():view.messageWorldTime;++view.messageRevision;view.message.clear();if(const auto row=descriptions.findRow(key))view.message=descriptions.value(*row,"Value").value_or("");dirty=true;}
    void hideMessage()override{view.messageVisible=false;view.messageWorldTime=scene?scene->clock().worldTime():view.messageWorldTime;++view.messageRevision;view.message.clear();dirty=true;}
    void refreshQuickItem(const progression::Item& item,int count)override{for(unsigned i=0;i<5;++i)if(progress->inventory.quickSlots()[i]==&item)view.quick[i].count=count;dirty=true;}
    void startAirStrike(const progression::AirStrikeRequest& request)override{if(!scene)throw std::logic_error("Missing live AirStrike world");scene->startAirStrike(request.weapon,request.fireDelay,request.position);}
    void setParts(std::string_view,unsigned partsType)override{if(scene&&equipment&&(partsType==5||partsType==6))scene->equipWeapon(equipment->codes(),partsType==5?2:3);}
    void statsChanged(const progression::EquipStats&)override{dirty=true;}
    void levelUp()override{
        if(equipment){equipment->refreshLevel();slot().hp(equipment->stats().maximumHP());}
        // This writes SLOT HP only. Player.levelUp does not heal its actor HP.
        dirty=true;
    }
    void playerHPChanged(int hp)override{slot().hp(hp);}
    void playerProgressTick()override{if(equipment)equipment->tickSpecial();}
    float playerSpecial()const override{return equipment?equipment->stats().specialCurrent:0;}
    void setPlayerSpecial(float value)override{if(!equipment)throw std::logic_error("Missing original Player equipment SP");equipment->specialCurrent(value);}
    void playerDied()override{dirty=true;}
    int playerDamage(int value,WorldRandom& rng)override{return equipment->stats().playerDamage(value,rng);}
    int damageBonus()const override{return equipment?equipment->stats().damageBonus:0;}
    int playerMaximumHP()const override{return equipment?equipment->stats().maximumHP():256;}
    void outOfRange(bool left)override{if(left)view.outOfRangeLeft=true;else view.outOfRangeRight=true;}
    void dropEnemyLoot(const MachineInfo& info,Vec2 p,double,float,WorldRandom& rng)override{
        progress->itemSpawner.drop(info,p,rng,[this](auto item){
            Drop drop;drop.item=std::move(item);drop.root.visual=drop.icon.visual=true;
            auto setBounds=[](VisibilityState& state,const TileImage* tile){if(tile)state.bounds={-tile->pixels.width*.5f,-tile->pixels.height*.5f,tile->pixels.width*.5f,tile->pixels.height*.5f};};
            setBounds(drop.root,worldTiles->lookup("Efx_Item2.png"));setBounds(drop.icon,worldTiles->lookup(drop.item.item->iconImage()));drops.push_back(std::move(drop));
        });
    }
    void rewardEnemyKill(const MachineInfo& info,Vec2,double,float,WorldRandom&)override{
        progress->killed(info.name,info.experience,mapName);
    }
    void prepareTiles(ResourceAccess& r,PreparedTiles& cache)override{
        worldTiles=&cache;cache.prepare(r,"Efx_Item2.png");
        for(const auto& [code,item]:definitions.items())cache.prepare(r,item.iconImage());
    }
    void tickItems(double worldTime,float,Vec2 player,WorldRandom& rng)override{
        std::erase_if(drops,[](const auto& drop){return drop.item.removed;});
        for(auto& drop:drops){drop.item.tick(worldTime,player,{},rng,scene?&scene->terrain():nullptr,[&](const auto& i,int value){progress->pickup(i,value,worldTime);});if(drop.item.removed)drop.pickupPlayer=player;}
    }
    void collectItems(VisualLayer& layer,const MapCamera& camera,const PreparedTiles& tiles)override{
        for(auto& drop:drops){const auto& item=drop.item;
            auto submit=[&](std::string_view name,VisibilityState& visibility,bool atlas,unsigned blend,float scale,float angle,float alpha,bool removed){
                const auto* tile=tiles.lookup(name);if(!tile)return;
                if(!atlas){visibility.removed=removed;refreshVisibility(visibility,item.position,&camera);if(removed)return;}
                VisualImage image;image.tile=tiles.visual(*tile);image.screenPosition=camera.toScreen(item.position);image.useAtlas=atlas;image.blendType=blend;image.scale={scale,scale};image.angle=angle;image.alpha=alpha;image.isInScreen=visibility.inScreen;layer.submitImage(image);
                visibility.removed=removed;refreshVisibility(visibility,item.position,&camera);
            };
            submit("Efx_Item2.png",drop.root,item.rootUsesAtlas,2,item.scale,item.angle,item.alpha,item.removed);
            // calcWorldPos only adds positions. Parent angle/alpha/scale do not
            // propagate into this independent original QobImage child.
            submit(item.item->iconImage(),drop.icon,true,0,item.item->type==0?1.f:.5f,0,1,false);
        }
    }
    void clearSelection(){view.selectedInventory=view.extInventory=view.selectedEquipment=view.extEquipment=view.selectedBot=view.extBot=view.selectedQuick=view.extQuick=-1;detailInventory=-1;}
    void screen(ui::ScreenId id){if(view.screen!=id){view.message.clear();view.messageVisible=false;++view.messageRevision;}view.screen=id;if(progress)progress->screen(int(id));dirty=true;
        if(id==ui::ScreenId::title&&scene){worldSaveAllowed=scene->isSave();quickItems.reset();scene.reset();drops.clear();worldTiles=nullptr;}
        if(id!=ui::ScreenId::field)closeDialogs();
        if(id==ui::ScreenId::slots){view.selectedSlot=-1;for(unsigned i=0;i<3;++i){Slot candidate(data.slot(i));if(candidate.byte(8)&&(view.selectedSlot<0||std::bit_cast<std::int32_t>(Slot(data.slot(unsigned(view.selectedSlot))).u32(4))<std::bit_cast<std::int32_t>(candidate.u32(4))))view.selectedSlot=int(i);}}
        clearSelection();selectedMissionId.clear();view.selectedMission=-1;view.missionBody.clear();view.selectedShop=-1;
        if(id==ui::ScreenId::shop){view.inventoryVisible=true;view.shopCategory=0;view.shopPage=0;view.inventoryPage=0;}
    }
    ui::ItemView itemView(const progression::Item* item,int count=0,int position=-1,bool equipped=false,int priceMode=0){
        ui::ItemView out;if(!item)return out;out.dataId=item->code;out.name=item->name;out.icon=item->iconImage();out.count=count;out.price=item->price;out.level=item->level;out.position=position;out.countable=item->countable;out.equipped=equipped;
        char large[64];std::snprintf(large,sizeof(large),"IconB%05d.png",item->icon);out.largeIcon=large;
        ui::ItemDescriptionValues values;values.type=item->type;values.price=item->price;values.param1=item->param1;values.param2=item->param2;values.param3=item->param3;
        if(const auto it=weapons.all().find(item->stringParameter);it!=weapons.all().end()){values.weapon=&it->second;out.attackPoint=it->second.attackPoint;}
        if(const auto machine=machines.all().find(item->stringParameter);machine!=machines.all().end()){values.machineResolved=true;values.botRefType=machine->second.referenceType;values.botHP=machine->second.hp;}
        if(equipment){const auto& s=equipment->stats();values.leftAttack=s.leftAttackPoint;values.rightAttack=s.rightAttackPoint;values.bodyHP=s.bonusHP;values.bodyDefense=s.bodyDefense;values.armorDefense=s.armorDefense;values.footSpeed=s.stepSizeInteger;values.boosterMaximum=s.specialMaximum;}
        out.description=ui::Screens::itemDescription(values,priceMode).value_or("");return out;
    }
    Frame preview(progression::Inventory& inventory){
        EquipmentCodes codes{};for(unsigned i=0;i<4;++i)if(const auto* e=inventory.entry(inventory.equipment()[i]);e&&e->item)codes[i]=e->item->code;
        auto pose=models.mountedPose(parts.mounts(models,codes));previewSprites.prepare(resources,pose);Frame out;previewSprites.append(out,pose,{});return out;
    }
    void rebuild(){
        for(unsigned i=0;i<3;++i){auto& v=view.slots[i];Slot s(data.slot(i));v={};v.used=s.byte(8)!=0;if(!v.used)continue;
            v.name=s.string(9,32);v.country=s.string(0x29,4);v.createdAt=localDate(s.u32(0));v.savedAt=localDate(s.u32(4));v.location=s.string(0x3d,24);v.missionPercent=definitions.missions().empty()?0:int(std::uint32_t(int(std::bit_cast<std::int16_t>(s.u16(0x65a)))*100)/definitions.missions().size());v.level=s.level()+1;v.credits=int(s.credit());v.experience=int(s.experience());v.experienceRate=s.experienceGauge();
            progression::Inventory inventory(definitions);inventory.importSlot(s);v.machPreview=preview(inventory);
        }
        if(progress&&equipment){auto s=slot();const auto& stats=equipment->stats();view.locale=s.string(0x29,4);view.baseId=base();
            view.credits=std::bit_cast<std::int32_t>(s.credit());view.level=s.level();view.attack=stats.damageBonus;view.defense=stats.bodyDefense+stats.armorDefense;view.speed=stats.stepSizeInteger;
            view.equipmentStats={stats.leftAttackPoint,stats.rightAttackPoint,stats.baseHP,stats.bonusHP,stats.bodyDefense,stats.armorDefense,stats.stepSizeInteger,stats.specialMaximum,stats.specialRegeneration};
            view.hpBarWidth=stats.hpBarWidth;view.hpBarFilled=stats.maximumHP()?int((std::int64_t(s.hp())*stats.hpBarWidth)/stats.maximumHP()):0;
            view.hpRate=stats.maximumHP()?float(s.hp())/float(stats.maximumHP()):0;view.spBarWidth=stats.specialBarWidth;
            view.spBarFilled=stats.specialMaximum?float(stats.specialBarWidth)*stats.specialCurrent/stats.specialMaximum:0;view.spRate=stats.specialMaximum?stats.specialCurrent/stats.specialMaximum:0;view.boosterReady=stats.specialCurrent>=100;
            view.experienceRate=s.experienceGauge();view.machPreview=preview(progress->inventory);view.inventory.clear();
            const auto& entries=progress->inventory.entries();for(std::size_t i=0;i<entries.size();++i){auto item=itemView(entries[i].item,entries[i].count,int(i),entries[i].flags!=0,view.screen==ui::ScreenId::shop?2:0);if(view.screen==ui::ScreenId::shop)item.enabled=entries[i].item&&entries[i].item->sell&&!entries[i].flags;view.inventory.push_back(std::move(item));}
            for(unsigned i=0;i<6;++i){const auto* e=progress->inventory.entry(progress->inventory.equipment()[i]);view.equipment[i]=itemView(e?e->item:nullptr,e?e->count:0,progress->inventory.equipment()[i],true);}
            for(unsigned i=0;i<3;++i)view.bots[i]=itemView(equipment->bot(i),1);
            for(unsigned i=0;i<5;++i){const auto* item=progress->inventory.quickSlots()[i];view.quick[i]=itemView(item,item?progress->inventory.count(*item):0);}
            view.shopItems.clear();for(const auto* item:equipment->shopItems(unsigned(view.shopCategory)))view.shopItems.push_back(itemView(item,1,-1,false,1));
            view.availableMissions.clear();view.acceptedMissions.clear();
            auto missionView=[&](const progression::MissionDefinition& d,bool accepted){
                const auto* state=progress->missions.state(d.id);ui::ItemView item;item.dataId=d.id;item.name=d.name;
                std::vector<ui::MissionRequirementText> requirements;std::uint16_t did=0,total=0;
                for(std::size_t i=0;i<d.requirements.size();++i){const auto& req=d.requirements[i];std::string name=req.object;
                    if(req.type==3){const auto* resolved=definitions.findItem(req.object);if(!resolved){item.description.clear();return item;}name=resolved->name;}
                    const int done=accepted&&state&&i<state->did.size()?state->did[i]:0;
                    requirements.push_back({req.type,std::move(name),done,req.count});did=std::uint16_t(did+done);total=std::uint16_t(total+req.count);
                }
                item.did=std::bit_cast<std::int16_t>(did);item.total=std::bit_cast<std::int16_t>(total);item.complete=state&&state->requireComplete;
                item.description=ui::Screens::missionDescription(d.comment,requirements,accepted).value_or("");return item;
            };
            for(const auto& d:definitions.missions())if(view.availableMissions.size()<5&&progress->missions.available(d.id,base(),s.level()))view.availableMissions.push_back(missionView(d,false));
            for(const auto* d:progress->missions.acceptedDefinitions())if(d)view.acceptedMissions.push_back(missionView(*d,true));
            view.selectedMission=-1;view.missionBody.clear();view.missionTitle.clear();
            auto select=[&](const std::vector<ui::ItemView>& list,int start){for(std::size_t i=0;i<list.size();++i)if(list[i].dataId==selectedMissionId){view.selectedMission=start+int(i);view.missionBody=list[i].description;view.missionTitle=list[i].name;}};
            if(view.screen==ui::ScreenId::command)select(view.availableMissions,0);
            select(view.acceptedMissions,view.screen==ui::ScreenId::command?int(view.availableMissions.size()):0);
            constexpr const char* airliftBases[]={"Base001","Base002","Base003","Base005","Base006","Base004"};
            constexpr int airliftPrices[]={50,300,1000,4000,16000,24000};
            for(unsigned i=0;i<6;++i){view.airlifts[i]={};if(s.u32(0x1b60+i*4)){view.airlifts[i].dataId=airliftBases[i];if(const auto map=maps.all().find(airliftBases[i]);map!=maps.all().end())view.airlifts[i].name=map->second.description;view.airlifts[i].price=airliftPrices[i];}}
            for(unsigned side=2;side<=3;++side){const auto* e=progress->inventory.entry(progress->inventory.equipment()[side]);std::string icon;
                if(e&&e->item){char name[64];std::snprintf(name,sizeof(name),"IconB%05d.png",e->item->icon);icon=name;}
                (side==2?view.selectedWeaponLeft:view.selectedWeaponRight)=std::move(icon);
            }
            view.weaponChoices.clear();if(view.weaponChoiceType==2||view.weaponChoiceType==3)for(std::size_t i=0;i<entries.size();++i){const auto& e=entries[i];if(e.item&&e.item->type==10&&e.count>0&&!e.flags){auto item=itemView(e.item,e.count,int(i),e.flags);item.attackDelta=item.attackPoint-(view.weaponChoiceType==2?stats.leftAttackPoint:stats.rightAttackPoint);view.weaponChoices.push_back(std::move(item));}}
            if(view.weaponChoiceType&&!view.weaponChoices.size())view.weaponChoiceType=0;
        }dirty=false;++presentationRevision;
    }
    void closeDialogs(){view.inventoryVisible=view.missionVisible=view.airliftVisible=view.systemVisible=view.tradeVisible=false;view.weaponChoiceType=0;if(scene)scene->setPaused(false);}
    int inventoryPosition(int cell)const{return view.inventoryPage*25+cell;}
    bool validIdentity(const ui::ButtonKey& key,const progression::Item* item)const{return key.dataId.empty()||(item&&key.dataId==item->code);}
    void drag(Vec2);
    bool inventoryAction(const ui::ScreenAction&);
    bool shopAction(const ui::ScreenAction&);
    void trade(bool buying){
        tradePosition=buying?-1:inventoryPosition(view.selectedInventory);tradeItem=nullptr;
        if(buying){const auto list=equipment->shopItems(unsigned(view.shopCategory));const auto position=view.shopPage*24+view.selectedShop;if(position>=0&&position<int(list.size()))tradeItem=list[std::size_t(position)];}
        else if(const auto* entry=progress->inventory.entry(tradePosition);entry&&entry->item&&entry->count>0&&!entry->flags&&entry->item->sell)tradeItem=entry->item;
        if(!tradeItem)return;
        view.tradeVisible=true;view.buyMode=buying;view.tradeItemId=tradeItem->code;
        view.tradeCount=buying?1:progress->inventory.entry(tradePosition)->count;
        view.tradePrice=wrappedMultiply(buying?tradeItem->price:progression::Equipment::sellPrice(*tradeItem),view.tradeCount);dirty=true;
    }
    void dynamicHUD(){
        if(scene)view.messageWorldTime=scene->clock().worldTime();
        if(!equipment)return;const auto s=slot();const auto& stats=equipment->stats();
        view.credits=std::bit_cast<std::int32_t>(s.credit());view.level=s.level();view.experienceRate=s.experienceGauge();
        view.enemyHpRate=scene?scene->selectedEnemyHpRate():0;
        view.hpBarWidth=stats.hpBarWidth;view.hpBarFilled=stats.maximumHP()?int((std::int64_t(s.hp())*stats.hpBarWidth)/stats.maximumHP()):0;
        view.hpRate=stats.maximumHP()?float(s.hp())/float(stats.maximumHP()):0;view.spBarWidth=stats.specialBarWidth;
        view.spBarFilled=stats.specialMaximum?float(stats.specialBarWidth)*stats.specialCurrent/stats.specialMaximum:0;view.spRate=stats.specialMaximum?stats.specialCurrent/stats.specialMaximum:0;view.boosterReady=stats.specialCurrent>=100;
    }
};
void GameSession::Impl::drag(Vec2 point){
    if(view.selectedInventory<0)return;
    const bool maintenance=view.screen==ui::ScreenId::maintenance;
    const Vec2 root{384,view.screen==ui::ScreenId::shop?312.f:512.f};
    const Vec2 inv{root.x+(maintenance?27.f:24.f),root.y+(maintenance?63.f:120.f)};
    const float x=(point.x-inv.x+30)/60,y=(point.y-inv.y-30)/-60;
    view.extInventory=x>=0&&x<5&&y>=0&&y<5?int(y)*5+int(x):-1;
    if(view.extInventory>=0){dirty=true;return;}
    if(view.screen==ui::ScreenId::field){const float qx=(point.x-511+28.5f)/57,qy=(point.y-29+28.5f)/57;
        view.extQuick=qx>=0&&qx<5&&qy>=0&&qy<1?int(qx):-1;
        if(view.extQuick>=0){dirty=true;return;}
    }
    const auto* entry=progress->inventory.entry(inventoryPosition(view.selectedInventory));
    const auto* item=entry?entry->item:nullptr;
    const Vec2 equip{root.x+(maintenance?-148.f:-196.f),root.y+(maintenance?52.f:76.f)};
    constexpr Vec2 positions[]={{0,0},{0,-74},{-56,-9},{56,-9},{56,-74},{-56,-74}};
    constexpr int types[]={11,12,10,10,13,14};view.extEquipment=-1;
    for(unsigned i=0;i<6;++i){const Vec2 p{equip.x+positions[i].x,equip.y+positions[i].y};
        if(point.x>p.x-20&&point.x<p.x+20&&point.y>p.y-20&&point.y<p.y+20&&item&&item->type==types[i]){view.extEquipment=int(i);dirty=true;return;}}
    const Vec2 bot{root.x+(maintenance?-47.f:-58.f),root.y+(maintenance?52.f:78.f)};
    const float bx=(point.x-bot.x+30)/60,by=(point.y-bot.y-30)/-60;
    view.extBot=item&&item->type==4&&bx>=0&&bx<=1&&by>=0&&by<3?int(by):-1;dirty=true;
}
bool GameSession::Impl::inventoryAction(const ui::ScreenAction& a){
    if(!progress||!equipment)return false;
    const bool maintenance=view.screen==ui::ScreenId::maintenance;
    const bool inField=view.screen==ui::ScreenId::field;
    if(!maintenance&&!(view.inventoryVisible&&(inField||view.screen==ui::ScreenId::shop)))return false;
    const auto id=a.button.key.id;const auto phase=a.button.type;
    if(id>=6000&&id<=6024){
        const int cell=int(id)-6000,position=inventoryPosition(cell);const auto* entry=progress->inventory.entry(position);
        if(!validIdentity(a.button.key,entry?entry->item:nullptr))return true;
        if(phase==ui::ButtonEventType::push){
            detailInventory=-1;view.selectedInventory=entry&&entry->count!=0?cell:-1;view.extInventory=-1;
            if(maintenance){view.selectedBot=view.extBot=-1;}
            dirty=true;uiSound(0);
            // GuiShop subscribes after the embedded DlgInven. Both original
            // handlers request SFX0; retain that observed pair, not a third.
            if(view.screen==ui::ScreenId::shop&&entry&&entry->item&&entry->count>0){view.selectedShop=-1;uiSound(0);}
            return true;
        }
        if(phase==ui::ButtonEventType::move){drag(a.button.position);return true;}
        if(phase==ui::ButtonEventType::pop){if(view.extInventory<0){detailInventory=view.selectedInventory;view.selectedBot=view.extBot=-1;}dirty=true;return true;}
        if(phase==ui::ButtonEventType::cancel){
            const int source=view.selectedInventory>=0?inventoryPosition(view.selectedInventory):-1;
            if(source>=0){
                if(view.extInventory>=0)equipment->swap(source,inventoryPosition(view.extInventory));
                else if(inField&&view.extQuick>=0)equipment->registerQuick(unsigned(view.extQuick),source);
                else if(view.extEquipment>=0)equipment->equip(source,unsigned(view.extEquipment),inField);
                else if(view.extBot>=0)equipment->registerBot(unsigned(view.extBot),source,inField);
            }
            clearSelection();dirty=true;return true;
        }return true;
    }
    if(phase==ui::ButtonEventType::push){
        if(id==6090||id==6091){view.inventoryPage=std::clamp(view.inventoryPage+(id==6090?-1:1),0,std::max(0,progress->inventory.pages()-1));view.selectedInventory=view.extInventory=-1;detailInventory=-1;dirty=true;uiSound(0);return true;}
        if(id==6093){clearSelection();equipment->arrange();dirty=true;return true;}
        if(id==2112){detailInventory=-1;if(view.screen==ui::ScreenId::shop)view.selectedShop=-1;dirty=true;uiSound(0);return true;}
        if(id==6092&&!maintenance&&inField){closeDialogs();dirty=true;uiSound(0);return true;}
        if(id>=6300&&id<=6302){
            if(inField)return true;
            const int index=int(id)-6300;const auto* item=equipment->bot(unsigned(index));if(!validIdentity(a.button.key,item))return true;
            view.selectedBot=index;view.selectedInventory=view.extInventory=-1;detailInventory=-1;if(view.screen==ui::ScreenId::shop)view.selectedShop=-1;dirty=true;uiSound(0);return true;
        }
    }
    if(phase==ui::ButtonEventType::pop&&id==6310){
        if(inField)return true;
        if(view.selectedBot>=0)equipment->unregisterBot(unsigned(view.selectedBot));clearSelection();dirty=true;return true;
    }
    if(id>=6100&&id<=6105)return true; // Targets only; no invented tap-equip.
    return false;
}
bool GameSession::Impl::shopAction(const ui::ScreenAction& a){
    if(view.screen!=ui::ScreenId::shop||!equipment)return false;
    const auto id=a.button.key.id;const auto phase=a.button.type;
    if(view.tradeVisible){
        if(phase!=ui::ButtonEventType::pop)return true; // Original trade gate.
        if(id==2107){view.tradeVisible=false;dirty=true;uiSound(0);return true;}
        if(id==2110||id==2111){
            if(!tradeItem)return true;
            const int unit=view.buyMode?tradeItem->price:progression::Equipment::sellPrice(*tradeItem);
            if(id==2110&&view.tradeCount>1)--view.tradeCount;
            else if(id==2111){
                if(view.buyMode){if(wrappedAdd(view.tradePrice,unit)<=std::bit_cast<std::int32_t>(slot().credit()))view.tradeCount=wrappedAdd(view.tradeCount,1);}
                else if(const auto* e=progress->inventory.entry(tradePosition);e&&view.tradeCount<e->count)++view.tradeCount;
            }
            view.tradePrice=wrappedMultiply(unit,view.tradeCount);dirty=true;uiSound(0);return true;
        }
        if(id==2108&&view.buyMode){if(tradeItem)equipment->buy(tradeItem->code,view.tradeCount);else uiSound(0);view.tradeVisible=false;dirty=true;return true;}
        if(id==2109&&!view.buyMode){if(tradeItem)equipment->sell(tradePosition,view.tradeCount);else uiSound(0);view.tradeVisible=false;view.selectedShop=-1;clearSelection();dirty=true;return true;}
        return true;
    }
    if(phase==ui::ButtonEventType::push){
        if(id==2104){const int category=a.button.key.intData;if(category<0||category>4)return true;view.shopCategory=category;view.shopPage=0;view.selectedShop=-1;dirty=true;uiSound(0);return true;}
        if(id==2105||id==2106){view.shopPage=std::clamp(view.shopPage+(id==2105?-1:1),0,std::max(0,equipment->shopPageCount(unsigned(view.shopCategory))-1));view.selectedShop=-1;dirty=true;uiSound(0);return true;}
        if(id>=2200&&id<=2223){const auto items=equipment->shopItems(unsigned(view.shopCategory));const int cell=int(id)-2200,index=view.shopPage*24+cell;
            if(index<int(items.size())&&validIdentity(a.button.key,items[std::size_t(index)])){view.selectedShop=cell;view.selectedInventory=-1;detailInventory=-1;dirty=true;uiSound(0);}return true;}
    }
    if(phase==ui::ButtonEventType::pop&&(id==2102||id==2103)){trade(id==2102);uiSound(0);return true;}
    return false;
}
GameSession::GameSession(ResourceAccess& r,SessionHost& h,WorldRandom& rng,std::optional<SaveData> data):impl_(std::make_unique<Impl>(r,h,rng,std::move(data))){}
GameSession::~GameSession()=default;
void GameSession::tick(){auto& p=*impl_;if(!p.active)return;p.clock.step();if(p.scene)p.scene->tick();}
void GameSession::setActive(bool active){
    auto& p=*impl_;p.active=active;
    if(!active){const bool hadTargets=p.view.extInventory!=-1||p.view.extEquipment!=-1||p.view.extBot!=-1||p.view.extQuick!=-1;
        p.view.extInventory=p.view.extEquipment=p.view.extBot=p.view.extQuick=-1;if(hadTargets)p.dirty=true;
    }
    if(p.scene)p.scene->setActive(active);
}
bool GameSession::createSlot(unsigned index,std::string_view name,std::string_view country){
    if(index>=3||name.empty())return false;auto& p=*impl_;p.data.initializeSlot(index,name,country,p.host.unixTime());p.dirty=true;return true;
}
bool GameSession::deleteSlot(unsigned index){if(index>=3)return false;auto& p=*impl_;auto bytes=p.data.slot(index);std::fill(bytes.begin(),bytes.end(),0);p.dirty=true;return true;}
bool GameSession::selectSlot(unsigned index){
    auto& p=*impl_;if(index>=3||!p.data.slot(index)[8])return false;
    const auto previousStats=p.equipment?p.equipment->stats():progression::EquipStats{};
    p.quickItems.reset();p.scene.reset();p.drops.clear();p.worldTiles=nullptr;p.equipment.reset();p.progress.reset();
    std::copy(p.data.slot(index).begin(),p.data.slot(index).end(),p.working.begin());p.loadedSlot=int(index);p.view.selectedSlot=int(index);
    p.progress=std::make_unique<progression::Progression>(p.slot(),p.definitions,p);p.progress->screen(int(p.view.screen));p.progress->restore();
    p.equipment=std::make_unique<progression::Equipment>(p.slot(),p.progress->inventory,p.definitions,p.weapons,p.itemTable,p,&p,previousStats);p.equipment->restore();
    p.slot().u32(4,p.host.unixTime());if(p.slot().hp()<1)p.slot().hp(p.slot().maxHP());p.progress->experience.add(0,int(p.view.screen));p.dirty=true;
    return save(SaveReason::selectSlot);
}
void GameSession::enterBase(){
    auto& p=*impl_;if(!p.progress)throw std::logic_error("No original working slot selected");if(p.scene)p.worldSaveAllowed=p.scene->isSave();p.quickItems.reset();p.scene.reset();p.drops.clear();p.worldTiles=nullptr;
    auto s=p.slot();auto current=s.string(0x2d,16);if(!current.starts_with("Base")){current="Base001";s.string(0x2d,16,current);}s.string(0x55,16,current);
    s.hp(p.equipment->stats().maximumHP());for(unsigned i=0;i<3;++i)s.u32(0x64c+i*4,0);
    if(const auto map=p.maps.all().find(current);map!=p.maps.all().end())s.string(0x3d,24,map->second.description);else s.string(0x3d,24,""); // Original nil map name; no invented Base004 definition.
    constexpr const char* visited[]={"Base001","Base002","Base003","Base005","Base006"};
    for(unsigned i=0;i<5;++i)if(current==visited[i])s.u32(0x1b60+i*4,1);
    p.view.selectedBaseButton=0;p.screen(ui::ScreenId::base);p.closeDialogs();save(SaveReason::enterBase);
}
bool GameSession::enterField(std::string_view map,int gate){
    auto& p=*impl_;if(!p.progress)return false;
    // World.setStage reseeds from mach_absolute_time before creating its pool.
    p.slot().string(0x2d,16,map);p.slot().u32(0x68,std::uint32_t(gate));
    if(p.scene)p.worldSaveAllowed=p.scene->isSave();
    p.quickItems.reset();p.random.reseed(p.host.absoluteTimeLow());p.scene.reset();p.drops.clear();p.worldTiles=nullptr;
    CombatSetup setup;setup.map=map;setup.gate=gate;setup.playerHP=p.slot().hp();setup.equipment=p.equipment->codes();setup.random=&p.random;setup.initialRenderTime=p.clock.renderTime();setup.offsetInsideGate=true;setup.isSave=p.worldSaveAllowed;
    auto scene=std::make_unique<CombatScene>(p.resources,p,setup);p.scene=std::move(scene);p.quickItems=std::make_unique<progression::QuickItems>(p.progress->inventory,p.slot(),p.weapons,p);p.mapName=map;p.screen(ui::ScreenId::field);p.closeDialogs();return true;
}
bool GameSession::acceptMission(std::string_view id){auto& p=*impl_;return p.progress&&p.progress->acceptMission(id,p.base());}
bool GameSession::claimMission(std::string_view id){auto& p=*impl_;return p.progress&&p.progress->claimMission(id,p.base());}
bool GameSession::equip(int position,unsigned index){auto& p=*impl_;return p.equipment&&p.equipment->equip(position,index,p.view.screen==ui::ScreenId::field);}
bool GameSession::save(SaveReason reason){
    auto& p=*impl_;if(p.progress){p.progress->prepareSave();std::copy(p.working.begin(),p.working.end(),p.data.slot(unsigned(p.loadedSlot)).begin());}
    const auto bytes=p.data.encode(p.random,p.host.absoluteTimeLow());++p.metrics.saveRequests;const bool ok=p.host.writeSave(bytes,reason);if(ok)++p.metrics.saveCompletions;return ok;
}
const ui::ScreensState& GameSession::state(){if(impl_->dirty)impl_->rebuild();impl_->dynamicHUD();return impl_->view;}
const SessionMetrics& GameSession::metrics()const{return impl_->metrics;}
const SaveData& GameSession::envelope()const{return impl_->data;}
std::span<const std::uint8_t> GameSession::workingSlot()const{return impl_->working;}
CombatScene* GameSession::battle(){return impl_->scene.get();}
float GameSession::renderTime()const{return impl_->clock.renderTime();}
std::uint64_t GameSession::presentationRevision()const{return impl_->presentationRevision;}
Frame GameSession::preparationFrame()const{return impl_->scene?impl_->scene->preparationFrame():Frame{};}
std::vector<FieldItemSnapshot> GameSession::fieldItems()const{
    std::vector<FieldItemSnapshot> result;result.reserve(impl_->drops.size());
    for(const auto& drop:impl_->drops){const auto& item=drop.item;result.push_back({item.item?item.item->code:"",item.item?item.item->type:-1,item.value,item.position,item.removed,drop.pickupPlayer});}return result;
}
Frame GameSession::drawWorld(){return impl_->scene?impl_->scene->drawFrame():Frame{};}
bool GameSession::inputWorld(PointerPhase phase,Vec2 point,std::uintptr_t id){auto& p=*impl_;if(!p.active||!p.scene||!id)return false;
    if(p.quickItems&&p.quickItems->input(phase,point,p.scene->camera(),p.scene->isPaused(),p.scene->clock().renderTime(),p.scene->clock().worldTime()))return true;
    return p.scene->input(phase,point,id);
}
// Action dispatch is defined beside the screen contracts; unknown IDs are
// retained in metrics until their original handlers have been restored.
void GameSession::action(const ui::ScreenAction& a){
    auto& p=*impl_;if(a.screen!=p.view.screen)return;
    struct ActionScope {Impl& impl;const ui::ScreenAction* previous;ActionScope(Impl& i,const ui::ScreenAction& a):impl(i),previous(i.activeAction){impl.activeAction=&a;}~ActionScope(){impl.activeAction=previous;}} scope(p,a);
    if(a.titleTap){if(a.titleReady)p.screen(ui::ScreenId::slots);return;}
    const auto id=a.button.key.id;const auto event=a.button.type;
    if(event==ui::ButtonEventType::release)return; // Not an original subscribed action.
    if(a.screen==ui::ScreenId::slots){
        auto start=[&]{const int selected=p.view.selectedSlot;if(selected>=0&&selected<3){if(p.data.slot(unsigned(selected))[8]){if(selectSlot(unsigned(selected)))enterBase();}else p.host.requestName(unsigned(selected));}};
        if(id==1009&&event==ui::ButtonEventType::push){const int selected=a.button.key.intData;if(selected<0||selected>=3)return;if(p.view.selectedSlot!=selected){p.view.selectedSlot=selected;p.dirty=true;}else start();return;}
        if(event==ui::ButtonEventType::pop&&id==1001){start();return;}
        if(event==ui::ButtonEventType::pop&&id==1010){if(p.view.selectedSlot>=0&&p.view.selectedSlot<3&&p.data.slot(unsigned(p.view.selectedSlot))[8])p.host.requestDelete(unsigned(p.view.selectedSlot));return;}
        if(event==ui::ButtonEventType::pop&&id==1004){p.screen(ui::ScreenId::title);return;}
    }
    if(a.screen==ui::ScreenId::base&&id>=2001&&id<=2006){
        if(event==ui::ButtonEventType::pop){const int button=int(id)-2000;if(p.view.selectedBaseButton!=button){p.view.selectedBaseButton=button;p.dirty=true;return;}
            if(button==1)p.screen(ui::ScreenId::command);else if(button==2)p.screen(ui::ScreenId::shop);else if(button==3)p.screen(ui::ScreenId::maintenance);else if(button==4)p.screen(ui::ScreenId::slots);
            else{
                const auto& raw=p.maps.raw().children;const auto found=std::find_if(raw.begin(),raw.end(),[&](const auto& c){return c.type==1&&c.text(0,24)==p.slot().string(0x2d,16);});
                const auto index=std::size_t(button-5);
                if(found!=raw.end()&&index<found->children.size()&&found->children[index].type==2){const auto& link=found->children[index];enterField(link.text(6,16),std::bit_cast<std::int8_t>(link.byte(22)));}
                else ++p.metrics.unsupportedActions;
            }return;}
        if(event==ui::ButtonEventType::cancel){p.view.selectedBaseButton=0;p.dirty=true;}return;
    }
    if(!p.progress||!p.equipment){
        const bool known=a.screen==ui::ScreenId::slots&&(id==1001||id==1004||id==1009||id==1010);
        if(!known&&(event==ui::ButtonEventType::push||event==ui::ButtonEventType::pop))++p.metrics.unsupportedActions;
        return;
    }
    // The original tradeBG blocks every underlying Push/Pop receiver.
    if(a.screen==ui::ScreenId::shop&&p.view.tradeVisible){p.shopAction(a);return;}
    if(p.inventoryAction(a))return;
    if(p.shopAction(a))return;
    if((a.screen==ui::ScreenId::command||a.screen==ui::ScreenId::maintenance||a.screen==ui::ScreenId::shop)&&event==ui::ButtonEventType::pop&&id==2100){enterBase();p.uiSound(0);return;}
    if((a.screen==ui::ScreenId::command||a.screen==ui::ScreenId::field)&&event==ui::ButtonEventType::pop){
        const bool command=a.screen==ui::ScreenId::command;
        if((command&&id>=5100&&id<=5104)||(id>=5200&&id<=5204&&(!command?p.view.missionVisible:true))){
            const auto& current=state();const auto& missions=id<5200?current.availableMissions:current.acceptedMissions;const auto index=std::size_t(id-(id<5200?5100:5200));
            if(index<missions.size()&&(a.button.key.dataId.empty()||a.button.key.dataId==missions[index].dataId)){p.selectedMissionId=missions[index].dataId;p.dirty=true;p.uiSound(0);}return;
        }
        if(command&&id==5004){const auto id=p.selectedMissionId;if(!id.empty()&&(a.button.key.dataId.empty()||a.button.key.dataId==id))acceptMission(id);p.selectedMissionId.clear();p.view.selectedMission=-1;p.dirty=true;p.uiSound(0);return;}
        if(!command&&id==5000&&p.view.missionVisible){p.closeDialogs();p.dirty=true;p.uiSound(0);return;}
        if(!command&&id==5006&&p.view.missionVisible){p.uiSound(0);return;} // Original sound-only branch.
    }
    if(a.screen==ui::ScreenId::field&&p.scene){
        if(event==ui::ButtonEventType::push&&(id==1022||id==1023)){p.view.weaponChoiceType=p.view.weaponChoiceType?0:(id==1022?2:3);p.dirty=true;p.uiSound(4);return;}
        if(event==ui::ButtonEventType::push&&id==1024){
            const auto* entry=p.progress->inventory.entry(a.button.key.intData);const auto chooser=p.view.weaponChoiceType;
            if((chooser==2||chooser==3)&&p.validIdentity(a.button.key,entry?entry->item:nullptr)){if(equip(a.button.key.intData,unsigned(chooser)))p.view.weaponChoiceType=0;else p.uiSound(4);p.dirty=true;}return;
        }
        if(event==ui::ButtonEventType::pop){
            if(id==1021||id==1025||id==1026){const bool opening=!(id==1021?p.view.systemVisible:id==1025?p.view.inventoryVisible:p.view.missionVisible);p.closeDialogs();
                if(opening){if(id==1021)p.view.systemVisible=true;else if(id==1025){p.view.inventoryVisible=true;p.clearSelection();}else{p.view.missionVisible=true;p.selectedMissionId.clear();}p.scene->setPaused(true);}p.dirty=true;p.uiSound(0);return;}
            if(id==1027){p.closeDialogs();p.scene->setPaused(true);p.view.airliftVisible=true;p.view.selectedAirlift=-1;p.dirty=true;p.uiSound(0);return;}
            if(id==1030){p.closeDialogs();p.dirty=true;p.uiSound(0);return;}
            if(id==1031){p.closeDialogs();p.scene->setPaused(true);p.uiSound(0);
                if(p.scene->isSave()){save(SaveReason::saveAndExit);const auto score=p.slot().u32(0x70);if(std::bit_cast<std::int32_t>(p.slot().u32(0x74))<std::bit_cast<std::int32_t>(score)){p.slot().u32(0x74,score);++p.metrics.unsupportedActions; /* reportScore platform consumer pending */}}
                p.screen(ui::ScreenId::title);return;}
            if(id==1032){p.closeDialogs();p.uiSound(0);p.host.alert("VisitIndiean");p.dirty=true;return;}
            if(p.view.airliftVisible&&id>=4002&&id<=4007){const auto& current=state();const int index=int(id)-4002;const auto& item=current.airlifts[std::size_t(index)];
                if(!item.dataId.empty()&&(a.button.key.dataId.empty()||a.button.key.dataId==item.dataId)){p.view.selectedAirlift=index;p.dirty=true;p.uiSound(0);}return;}
            if(p.view.airliftVisible&&id==4000){p.closeDialogs();p.dirty=true;p.uiSound(0);return;}
            if(p.view.airliftVisible&&id==4001){const auto current=state().airlifts;const int selected=p.view.selectedAirlift;
                if(selected<0||selected>=6||current[std::size_t(selected)].dataId.empty()||std::bit_cast<std::int32_t>(p.slot().credit())<current[std::size_t(selected)].price){p.host.alert("NotEnoughCr");p.closeDialogs();}
                else{const auto& destination=current[std::size_t(selected)];p.slot().credit(p.slot().credit()-std::uint32_t(destination.price));p.slot().string(0x2d,16,destination.dataId);enterBase();}
                p.view.selectedAirlift=-1;p.dirty=true;p.uiSound(0);return;}
        }
    }
    // Count missing actual consumers only at their original action phase.
    // Other phases on known controls are intentional original no-ops.
    if(a.screen==ui::ScreenId::command&&(id==5007||id==5009)&&event==ui::ButtonEventType::pop){++p.metrics.unsupportedActions;return;}
    if(a.screen==ui::ScreenId::field&&id==1029&&event==ui::ButtonEventType::pop){++p.metrics.unsupportedActions;return;}
    if(a.screen==ui::ScreenId::field&&id>=6200&&id<=6204&&event==ui::ButtonEventType::push){
        if(p.quickItems&&p.scene&&!p.scene->isPaused()&&!p.view.inventoryVisible&&!p.view.missionVisible&&!p.view.airliftVisible&&!p.view.systemVisible){
            const unsigned index=id-6200;const auto* item=p.progress->inventory.quickSlots()[index];
            if(item&&item->type!=1&&item->type!=2){++p.metrics.unsupportedActions;return;} // Other original item types remain unverified.
            const auto& current=state();p.quickItems->press(index,current.quick[index].count,false);
        }return;
    }
    const bool known=(a.screen==ui::ScreenId::slots&&(id==1001||id==1004||id==1009||id==1010))||
        (a.screen==ui::ScreenId::base&&id>=2001&&id<=2006)||
        (a.screen==ui::ScreenId::field&&((id>=1021&&id<=1032)||(id>=4000&&id<=4007)||(id>=5000&&id<=5006)||(id>=5200&&id<=5204)||(id>=6000&&id<=6024)||(id>=6090&&id<=6093)||(id>=6100&&id<=6105)||(id>=6200&&id<=6204)||(id>=6300&&id<=6302)||id==6310))||
        (a.screen==ui::ScreenId::command&&(id==2100||id==5004||id==5007||id==5009||(id>=5100&&id<=5104)||(id>=5200&&id<=5204)))||
        ((a.screen==ui::ScreenId::maintenance||a.screen==ui::ScreenId::shop)&&((id>=6000&&id<=6024)||(id>=6090&&id<=6093)||(id>=6100&&id<=6105)||(id>=6300&&id<=6302)||id==6310||id==2100||id==2112))||
        (a.screen==ui::ScreenId::shop&&((id>=2102&&id<=2111)||(id>=2200&&id<=2223)));
    if(!known&&(event==ui::ButtonEventType::push||event==ui::ButtonEventType::pop))++p.metrics.unsupportedActions;
}
}

#pragma once
#include "Core/Data/DataTable.hpp"
#include "Core/Data/Machines.hpp"
#include "Core/Simulation/TerrainCollision.hpp"
#include "Core/Simulation/WorldRandom.hpp"
#include <array>
#include <functional>
#include <map>
#include <span>

namespace hm2::progression {
// GlobalInfo.init0x15ef4 / sfxID0x14ab8, resolved from original CFStrings.
std::string_view originalSoundName(unsigned id);
// View over the ORIGINAL 0x2360-byte slot, not a replacement save format.
// Callers retain the original full-file envelope, unknown bytes and write gates.
class SlotView {
    std::span<std::uint8_t> bytes_;
public:
    static constexpr std::size_t size=0x2360;
    explicit SlotView(std::span<std::uint8_t>);
    std::uint32_t u32(std::size_t)const;std::uint16_t u16(std::size_t)const;
    void u32(std::size_t,std::uint32_t);void u16(std::size_t,std::uint16_t);
    std::uint8_t byte(std::size_t)const;void byte(std::size_t,std::uint8_t);
    std::string string(std::size_t,std::size_t)const;void string(std::size_t,std::size_t,std::string_view);
    int level()const;std::uint32_t experience()const;std::uint32_t credit()const;int hp()const;int maxHP()const;float experienceGauge()const;
    void level(int);void experience(std::uint32_t);void credit(std::uint32_t);void hp(int);void maxHP(int);void experienceGauge(float);
};
struct Item {
    std::string code,name,stringParameter;int icon=0,type=0,level=0,price=0;float param1=0,param2=0,param3=0;
    bool buy=false,sell=false,quick=false,countable=false;
    std::string iconImage()const;
};
struct Requirement {int type=0;std::string object,place;int count=0;bool matchMap=false;};
struct Action {
    static constexpr int placeSpawner=8;
    int type=0,value=0;
    std::string item,enemy,place;
    int x=0,y=0;
};
struct MissionDefinition {
    std::string id,name,owner,completePlace,previous,comment;int level=0;
    std::vector<Requirement> requirements;std::vector<Action> acceptActions,completeActions;
};
struct SpawnDefinition {int minimum=0,maximum=0,totalWeight=0;std::vector<std::pair<std::string,int>> cumulative;};
class Definitions {
    std::map<std::string,Item,std::less<>> items_;
    std::map<std::string,SpawnDefinition,std::less<>> spawns_;
    std::vector<MissionDefinition> missions_;
public:
    using MissionLoader=std::function<DataTable(std::string_view)>;
    static Definitions parse(DataTable items,DataTable regeneration,DataTable missions,const MissionLoader&,std::string_view locale);
    const Item* findItem(std::string_view)const;
    const SpawnDefinition* findSpawn(std::string_view)const;
    const auto& items()const{return items_;}const auto& missions()const{return missions_;}
};
enum class Change {experience,credit,inventory,mission,health};
struct MissionState {bool loaded=false,accepted=false,complete=false,requireComplete=false;std::vector<int> did;};
// Synchronous callbacks. changed only marks memory dirty; it MUST NOT write a
// DB/file per kill, hit or pickup. The platform save event owns actual writes.
struct Events {
    virtual ~Events()=default;
    virtual void changed(Change){}
    virtual void sound(unsigned /*original sfxID*/){}
    virtual void levelUp(){} // HD0xb1f8: particle, SLOT HP write, HP UI, bot setHp.
    virtual void experienceGauge(float){}
    virtual void missionUpdated(const MissionDefinition&,const MissionState&){}
    virtual void missionReward(const MissionDefinition&,const Action&){}
    virtual void pickupBegin(const Item&){} // Show icon/reset previous text.
    virtual void pickupText(const Item&,int,bool,double){} // Runs even if full.
    virtual void quickItemChanged(const Item&){}
    virtual void alert(std::string_view /*original description key*/){}
    virtual void alert(std::string_view key,std::string_view){alert(key);}
    // GuiCommandCenter creates its result window after SFX11, before applying
    // the ordered actions. Other Progression consumers need no presentation.
    virtual void missionCompletionStarted(const MissionDefinition&){}
};
struct ExperienceBounds {std::uint32_t previous=0,next=0;int baseMaxHP=0,levelStat=0;};
class Experience {
    SlotView slot_;Events& events_;ExperienceBounds cached_{};int cachedLevel_=-1;
public:
    Experience(SlotView slot,Events& events):slot_(slot),events_(events){}
    static ExperienceBounds bounds(int level);
    ExperienceBounds recalculate();
    bool add(std::int32_t amount,int screen,bool hasPlayer=true);
};
struct InventoryEntry {const Item* item=nullptr;std::int16_t count=0;std::uint8_t flags=0;};
class Inventory {
    const Definitions& defs_;std::array<InventoryEntry,200> entries_{};std::array<int,6> equipment_{};std::array<const Item*,5> quick_{};int pages_=3;
public:
    std::function<void(const Item&)> countChanged;
    explicit Inventory(const Definitions&);
    void importSlot(SlotView);void exportSlot(SlotView);
    int pages()const{return pages_;}void pages(int);
    const auto& entries()const{return entries_;}
    const auto& equipment()const{return equipment_;}
    const auto& quickSlots()const{return quick_;}
    bool isQuick(const Item&)const;void quickSlot(unsigned,const Item*);
    const InventoryEntry* entry(int)const;
    bool setItem(int,const Item*,std::int16_t);
    // Raw Inventory0x1821c. UI's field/flag gates belong to Equipment.
    bool setEquip(int,unsigned,const std::function<void(const Item&,unsigned)>& beforeAssign={});
    bool subtractFrom(int,std::int16_t);
    void swap(int,int);void arrange();
    int count(const Item&)const;int emptySlots()const;
    bool add(const Item&,std::int16_t);bool subtract(const Item&,std::int16_t);
};
class Missions {
    const Definitions& defs_;Inventory& inventory_;Events& events_;
    std::vector<MissionState> states_;std::vector<std::size_t> accepted_;
    bool check(std::size_t);void refresh(std::size_t,int screen);
public:
    Missions(const Definitions&,Inventory&,Events&);
    const MissionDefinition* definition(std::string_view)const;
    const MissionState* state(std::string_view)const;
    std::size_t acceptedCount()const{return accepted_.size();}
    std::vector<const MissionDefinition*> acceptedDefinitions()const;
    bool available(std::string_view,std::string_view base,int level)const;
    bool accept(std::string_view); // Original manager method, used by restore.
    bool finish(std::string_view);
    void kill(std::string_view enemy,int count,std::string_view place,int screen);
    void arrive(std::string_view place,int screen);
    void checkItemCount(const Item&,int screen);
    void importSlot(SlotView);void exportSlot(SlotView)const;
};
struct PickupBot {int referenceType=0;Vec2 position;std::uint64_t id=0;};
struct FieldItem {
    const Item* item=nullptr;int value=0;Vec2 position,velocity;float checkTime=0,speed=0,angle=0,scale=1,alpha=1;
    bool pickerBot=false,removed=false,rootUsesAtlas=false;
    static FieldItem create(const Item&,int value,Vec2,WorldRandom&,bool rootAtlas=false);
    // Root Efx_Item2.png blendType2; child uses original Icon%05d.png,
    // child scale1 for coins/.5 otherwise. Parent renders through common GPU.
    void tick(double time,Vec2 player,std::span<const PickupBot>,WorldRandom&,const TerrainCollision*,
        const std::function<void(const Item&,int)>& pickup,
        const std::function<void(std::size_t)>& picker={});
};
class ItemSpawner {
    const Definitions& defs_;
public:
    explicit ItemSpawner(const Definitions& defs):defs_(defs){}
    int spawnCount(std::string_view,WorldRandom&)const;
    const Item* spawnItem(std::string_view,WorldRandom&)const;
    // Call in enemy onCrash AFTER base/bot cleanup and BEFORE spawner-death /
    // target cleanup / mission.kill / addExp. create consumes original RNG2.
    void drop(const MachineInfo&,Vec2,WorldRandom&,const std::function<void(FieldItem)>&)const;
};
class Progression {
    SlotView slot_;const Definitions& defs_;Events& events_;int screen_=7;
public:
    Inventory inventory;Experience experience;Missions missions;ItemSpawner itemSpawner;
    Progression(SlotView,const Definitions&,Events&);
    Progression(const Progression&)=delete;Progression& operator=(const Progression&)=delete;
    void screen(int value){screen_=value;}
    void restore();void prepareSave(); // Memory conversions only, no disk IO.
    bool acceptMission(std::string_view,std::string_view base);
    bool claimMission(std::string_view,std::string_view base);
    void killed(std::string_view enemy,int exp,std::string_view place); // Mission before EXP.
    bool pickup(const Item&,int value,double time);
};
}

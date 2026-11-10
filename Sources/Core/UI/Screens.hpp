#pragma once
#include "Core/Data/Resources.hpp"
#include "Core/Data/Weapons.hpp"
#include "Core/Presentation/PreparedTiles.hpp"
#include "Core/Simulation/WorldRandom.hpp"
#include "Core/UI/Button.hpp"
#include "Core/UI/Text.hpp"
#include <array>
#include <memory>
#include <optional>

namespace hm2::ui {
enum class ScreenId : unsigned {title=1,slots=2,base=3,command=4,maintenance=5,shop=6,field=7};
enum class Panel : unsigned {root,inventory,equipment,bot,quick,mission,airlift,system,trade,weaponChoices};
struct ItemView {
    // Original ItemInfo Code / mission ID / slot item code. Empty means no item.
    std::string dataId,icon,name,description,largeIcon;
    unsigned iconTile=0,iconColumns=1,iconRows=1;
    int count=0,price=0,level=0,position=-1,did=0,total=0,attackPoint=0,attackDelta=0;
    bool enabled=true,selected=false,countable=false,equipped=false,complete=false;
};
struct SlotView {
    bool used=false;
    std::string name,country,createdAt,savedAt,location;
    int level=0,credits=0,experience=0,missionPercent=0;
    float experienceRate=0;
    Frame machPreview; // Actual assembly commands, relative to original (-170,34).
};
struct EquipmentStatsView {
    int attackLeft=0,attackRight=0,baseHP=0,bonusHP=0,bodyDefense=0,armorDefense=0,stepSize=0;
    float boosterMaximum=0,boosterRegen=0;
};
struct ScreensState {
    ScreenId screen=ScreenId::title;
    std::string locale="us",baseId="Base001";
    int selectedSlot=-1,selectedBaseButton=0;
    std::array<SlotView,3> slots;
    std::vector<ItemView> inventory,shopItems,availableMissions,acceptedMissions,weaponChoices;
    std::array<ItemView,6> equipment;
    EquipmentStatsView equipmentStats;
    std::array<ItemView,3> bots;
    std::array<ItemView,5> quick;
    std::array<ItemView,6> airlifts;
    int selectedInventory=-1,selectedEquipment=-1,selectedBot=-1,selectedQuick=-1;
    int extInventory=-1,extEquipment=-1,extBot=-1,extQuick=-1;
    int inventoryPage=0,shopPage=0,shopCategory=0,selectedShop=-1,selectedMission=-1,selectedAirlift=-1,weaponChoiceType=0;
    int credits=0,level=0,attack=0,defense=0,speed=0;
    float hpRate=0,spRate=0,experienceRate=0,enemyHpRate=0;
    int hpBarWidth=0,hpBarFilled=0,spBarWidth=0;
    float spBarFilled=0;
    bool boosterReady=false;
    bool outOfRangeLeft=false,outOfRangeRight=false;
    bool inventoryVisible=false,missionVisible=false,airliftVisible=false,systemVisible=false,tradeVisible=false;
    bool buyMode=false,pathMode=false;
    std::string selectedWeaponLeft,selectedWeaponRight,message,missionTitle,missionBody;
    std::string tradeItemId;
    int tradeCount=0,tradePrice=0;
    Frame machPreview; // Actual equipped Player assembly, no stand-in artwork.
};
struct PreparedScreenText {std::string image;std::shared_ptr<const TextBitmap> bitmap;};
struct ScreenAction {
    ScreenId screen;
    Panel panel;
    ButtonEvent button;
    // Title is an original full-screen receiver, with no QobButton ID.
    bool titleTap=false;
    bool titleReady=false;
};
struct ScreensHost {
    virtual ~ScreensHost()=default;
    // The host owns real slot/progression/equipment/mission transitions. It
    // returns their actual state through Screens.refresh(); no local fake save.
    virtual void action(const ScreenAction&)=0;
    virtual PreparedScreenText prepareText(const TextRasterRequest&)=0;
    virtual void bgm(std::string_view originalFile)=0;
    virtual void sound(std::string_view originalFile)=0;
    virtual void sfx(unsigned originalGlobalSfxId)=0;
    virtual void quake(float /*strength*/,unsigned /*count*/,float /*delay*/){}
};
struct ScreenCoverage {
    ScreenId screen;
    Panel panel;
    std::string feature,source;
    bool implemented=false;
};
struct ItemDescriptionValues {
    int type=0,price=0;
    float param1=0,param2=0,param3=0;
    const WeaponInfo* weapon=nullptr;
    bool machineResolved=false;
    int botRefType=-1,botHP=0;
    int leftAttack=0,rightAttack=0,bodyHP=0,bodyDefense=0,armorDefense=0,footSpeed=0;
    float boosterMaximum=0;
};
struct MissionRequirementText {int type=0;std::string name;int did=0,total=0;};
class Screens {
    struct Impl;
    ResourceAccess& resources_;
    PreparedTiles& tiles_;
    ScreensHost& host_;
    std::shared_ptr<Impl> impl_;
public:
    Screens(ResourceAccess&,PreparedTiles&,ScreensHost&);
    void refresh(const ScreensState&,float renderTime=0);
    // Original presentation tick (Title timeline/RNG), separate from draw.
    void tick(float renderTime,WorldRandom&);
    // Existing field HUD nodes only: no asset reads, text bake or button rebuild.
    // Call refresh for screen, inventory, selection or panel changes.
    bool updateHUD(const ScreensState&);
    bool input(PointerPhase,Vec2,std::uintptr_t,float renderTime);
    void discardInputs();
    void append(Frame&,float renderTime)const;
    [[nodiscard]] std::vector<ButtonKey> buttons()const;
    [[nodiscard]] std::shared_ptr<Button> button(ButtonKey)const;
    [[nodiscard]] static std::vector<ScreenCoverage> coverage();
    // nullopt identifies an original uninitialized-buffer / unresolved lookup
    // branch; no description is invented for those branches.
    [[nodiscard]] static std::optional<std::string> itemDescription(const ItemDescriptionValues&,int priceMode);
    [[nodiscard]] static std::optional<std::string> missionDescription(std::string comment,
        const std::vector<MissionRequirementText>&,bool accepted);
};
}

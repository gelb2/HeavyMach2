#pragma once
#include "Core/Session/GameSession.hpp"
#include <cstdint>
#include <memory>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hm2::validation {
enum class ScenarioRole {combat,shop,mission,saveRestart,movementLoot};
struct ScenarioDescription {std::string label;ScenarioRole role=ScenarioRole::combat;std::vector<std::string> weapons;};
struct NavigationObservation {std::uint64_t frame=0;Vec2 player,world,screen;std::string lootCode;bool accepted=false,blocked=false;};
struct PickupObservation {std::uint64_t frame=0;std::string code;int type=0,value=0;Vec2 player,item;int creditDelta=0;};
struct ScenarioObservation {
    std::uint64_t ticks=0,firstFrame=0,lastFrame=0,draws=0,sounds=0,saves=0;
    std::uint64_t buttonRequests=0,buttonFailures=0;
    std::uint64_t emissions=0,playerEmissions=0,damage=0,kills=0;
    std::vector<std::string> emittedWeaponParts;
    int initialLevel=0,level=0,credit=0,playerHP=0;std::uint32_t experience=0;
    std::uint64_t aliveTicks=0,firstEmissionFrame=0,lastEmissionFrame=0;
    std::map<std::string,std::uint64_t,std::less<>> weaponEmissions;
    bool missionAccepted=false,missionComplete=false,shopPurchased=false,insufficientRejected=false,restarted=false;
    Vec2 startPosition,lastPosition;float distanceMoved=0,maxDisplacement=0;
    std::uint32_t initialExperience=0;int initialCredit=0,initialHP=0;
    std::uint64_t navigationRequests=0,navigationRejected=0,movementFrames=0,blockedFrames=0,maxObservedDrops=0;
    std::uint64_t pickupEvents=0,coinPickups=0,itemPickups=0,injuryFrames=0,injuryPoints=0;
    int pickupCreditGain=0,inventoryGain=0;
    std::vector<NavigationObservation> navigation;
    std::vector<PickupObservation> pickups;
};
struct ButtonActivity {unsigned scenario=0,id=0;int intData=0;bool delivered=false;};
struct FrameActivity {
    std::uint64_t frame=0,tickedScenarios=0,emittingScenarios=0,emissions=0,damage=0,kills=0;
    std::vector<std::string> emittedWeaponParts;
    std::vector<ButtonActivity> buttons;
    std::vector<WeaponKind> emittedWeaponKinds;
    std::uint64_t activeProjectiles=0,combatActiveProjectiles=0,combatEmittingScenarios=0,combatDamage=0;
    bool saveRestart=false,movementLootMoving=false;
    std::uint64_t movementLootEmissions=0,movementLootPickups=0,movementLootInjury=0;
    int movementLootCreditGain=0;
};
struct ConcurrentMetrics {
    std::uint64_t frames=0,drawFrames=0,underlyingReads=0,underlyingExists=0,warmReadMisses=0,warmExistsMisses=0;
    std::size_t activeScenarios=0,combatScenarios=0,activeWeaponKinds=0,activeWeaponDefinitions=0;
    bool commonFrameInterval=false;
    std::uint64_t maxConcurrentEmittingScenarios=0,maxConcurrentWeaponKinds=0,maxConcurrentWeaponDefinitions=0;
    std::uint64_t framesWithFiveWeaponDefinitions=0,framesWithFiveWeaponKinds=0;
    std::uint64_t menuActionFrames=0,menuFramesWithCombatActivity=0;
    std::uint64_t movementFramesWithCombatActivity=0,pickupFramesWithCombatActivity=0,injuryFramesWithCombatActivity=0;
    std::uint64_t movementEmissionFramesWithCombatActivity=0;
    std::vector<std::string> warmReadNames,warmExistsNames;
};
struct ValidationHost {
    virtual ~ValidationHost()=default;
    virtual void sound(unsigned scenario,std::string_view file,Vec2)=0;
    virtual void soundID(unsigned scenario,unsigned id,Vec2)=0;
    virtual bool writeSave(unsigned scenario,std::span<const std::uint8_t>,SaveReason)=0;
    virtual std::uint32_t absoluteTimeLow(unsigned scenario)=0;
    virtual std::uint32_t unixTime()=0;
    // Called once on the source frame, before pane coordinate conversion.
    virtual void appendScenarioUI(unsigned,GameSession&,Frame&){}
    virtual void tickScenarioUI(unsigned,GameSession&,WorldRandom&){}
    virtual void sessionRestarted(unsigned,GameSession&){}
    // Native host performs real Screens begin/end. CPU hosts may use the same
    // ScreenAction consumer; return false when the requested button is absent.
    virtual bool pressButton(unsigned scenario,unsigned id,int intData=0)=0;
};
// Validation start conditions are encoded in separate original-format saves.
// All sessions remain independent and tick on the same monotonically numbered
// frame; this class owns no OS audio, filesystem writer or worker threads.
class ConcurrentValidation {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    ConcurrentValidation(ResourceAccess&,ValidationHost&);
    ~ConcurrentValidation();
    ConcurrentValidation(const ConcurrentValidation&)=delete;
    ConcurrentValidation& operator=(const ConcurrentValidation&)=delete;
    void tick();
    void setActive(bool);
    // Each session's drawWorld is consumed at most once per tick. A repeated
    // request returns the cached composed frame and never consumes VFX twice.
    Frame drawFrame(Size viewport={1920,1080});
    GameSession& session(unsigned);
    ResourceAccess& cachedResources();
    [[nodiscard]] std::size_t scenarioCount()const;
    [[nodiscard]] const std::vector<ScenarioDescription>& scenarios()const;
    [[nodiscard]] const std::vector<ScenarioObservation>& observations()const;
    [[nodiscard]] ConcurrentMetrics metrics()const;
    [[nodiscard]] const std::vector<FrameActivity>& frameActivity()const;
    [[nodiscard]] std::vector<std::string> supported()const;
    [[nodiscard]] std::vector<std::string> missingFeatures()const;
};
}

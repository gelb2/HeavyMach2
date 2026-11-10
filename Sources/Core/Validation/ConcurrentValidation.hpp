#pragma once
#include "Core/Session/GameSession.hpp"
#include <cstdint>
#include <array>
#include <memory>
#include <stdexcept>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hm2::validation {
enum class ScenarioRole {combat,shop,mission,saveRestart,movementLoot,aidKit,airStrike,worldAudit,dash,botAttack,botPicker,help,gameOver,rankingUnavailable};
struct ScenarioDescription {std::string label;ScenarioRole role=ScenarioRole::combat;std::vector<std::string> weapons;std::string map{};};
struct NavigationObservation {std::uint64_t frame=0;Vec2 player,world,screen;std::string lootCode;bool accepted=false,blocked=false;};
struct PickupObservation {std::uint64_t frame=0;std::string code;int type=0,value=0;Vec2 player,item;int creditDelta=0;};
struct BotObservation {
    std::string code,machine;unsigned slot=0;std::uint64_t id=0;
    bool spawned=false,removed=false;std::uint64_t lastObservedFrame=0;int initialHP=0,hp=0,maximumHP=0;
    std::uint64_t ticks=0,rotorFrames=0,emissions=0,enemyHPRemoved=0,itemCaptures=0,damageCalls=0,hpRemoved=0,regeneratedHP=0;
    float lastRotor=0;
    std::uint64_t repairFrame=0,repairParticles=0,repairSounds=0;int repairBefore=0,repairAfter=0,repairCountBefore=0,repairCountAfter=0;bool repairAccepted=false;
};
struct HelpInputObservation {
    std::uint64_t frame=0;float x=0;int beforePage=-1,afterPage=-1;
    bool accepted=false,beforeGuide=false,afterGuide=false,closed=false,paused=false;
};
struct HelpObservation {
    std::uint32_t initialCounter=0,resultingCounter=0;
    bool initialPresent=false,initialPaused=false,initialGuide=false,initialChangePages=false;
    int initialPage=-1;unsigned initialLayer=0;
    bool menuOpened=false,menuPaused=false,menuGuide=false;int menuPage=-1;unsigned menuLayer=0;
    std::uint64_t pausedFrames=0,runningFrames=0,closedFrame=0,drawFrames=0,guideDrawFrames=0;
    double timeAtMenu=0,timeAtClose=0,lastWorldTime=0;
    std::string lastImage;std::vector<HelpInputObservation> inputs;
};
struct WeaponSwapObservation {
    std::string code,part,oldCode;unsigned inventoryPosition=0;
    std::uint64_t chooserFrame=0,equipFrame=0,firstEmissionFrame=0,savesBefore=0,savesAfter=0;
    int countBefore=0,countAfter=0,oldCountAfter=0;
    bool ownedBefore=false,equippedBefore=false,ownedAfter=false,equippedAfter=false,oldEquippedAfter=false,chooserShown=false,equipped=false,pausedBefore=false,pausedAfter=false;
};
struct GameOverObservation {
    std::uint64_t deathFrame=0,requestFrame=0,confirmFrame=0,baseFrame=0,token=0,requests=0;
    std::uint64_t savesBefore=0,savesAtRequest=0,savesAfterConfirm=0,savesAtBase=0,enemyEmissions=0,damageCalls=0;
    int initialHP=0,deathHP=-1,deathPlayerState=-1,requestHP=-1,requestPlayerState=-1,requestWorldState=-1,confirmDoorPhase=-1,baseDoorPhase=-1,baseHP=0;
    std::array<std::uint32_t,3> savedBotHP{};
    float requestStateTime=0,requestRenderTime=0;
    bool targetInputAccepted=false,requestPaused=false,waitAdvanced=false,confirmAccepted=false,pendingCleared=false,duplicateRejected=false,savedPayloadValid=false;
    double requestWorldTime=0,confirmWorldTime=0;
    std::string baseId,locationAtConfirm;
};
struct RankingObservation {
    std::uint64_t frame=0,notices=0,savesBefore=0,savesAfter=0;
    std::string title,message;std::vector<std::string> events;
    bool slotUnchanged=false,savedBytesUnchanged=false,screenUnchanged=false,transitionUnchanged=false,unsupportedUnchanged=false;
};
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
    std::string quickItemCode;
    std::uint64_t quickUseFrame=0,repairSounds=0,quickRepairSounds=0;
    int quickCountBefore=0,quickCountAfter=0,quickHPBefore=0,quickHPAfter=0,quickMaximumHP=0,quickSlotHPBefore=0,quickSlotHPAfter=0,quickSelectionAfter=-2;
    unsigned long quickParticleAllocations=0;
    bool quickTapAccepted=false;
    std::string strikeItemCode;
    std::uint64_t strikeUseFrame=0,strikeRequests=0,strikeProjectiles=0,strikeDamageCalls=0,strikeEnemyHPRemoved=0,strikeTicks=0;
    int strikeCountBefore=0,strikeCountAfter=0;
    std::size_t activeStrikes=0;
    Vec2 strikeTarget;
    bool strikeTapAccepted=false,strikeTargetVisibleEnemy=false;
    // The first tick is the real spawner pass, before newly appended actors tick.
    // Later totals can include legitimate respawns and must not replace it.
    std::array<std::uint64_t,3> firstSpawns{},enemySpawns{},enemyEmissions{},enemyDamage{},enemyKills{},enemyRemovals{};
    std::uint64_t bossSpawns=0,worldQuickUses=0;
    std::uint64_t dashStarts=0,dashMoveTicks=0,dashParticles=0,dashRandomCalls=0,dashSounds=0,dashInputFrame=0;
    float dashSPBefore=0,dashSPAfter=0,dashMaxDisplacement=0;
    Vec2 dashStartPosition;
    bool dashInputAccepted=false;
    std::array<WeaponSwapObservation,2> botWeaponSwaps{};
    std::array<BotObservation,3> bots{};std::string botThreatName;std::uint64_t botThreatEmissions=0;HelpObservation help;GameOverObservation gameOver;RankingObservation ranking;
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
    std::uint64_t aidKitUses=0,airStrikeUses=0,botEmissions=0,botItemContacts=0,botRewardArrivals=0,botHPRemoved=0,botRepairs=0,helpInputRequests=0,gameOverEvents=0,rankingNotices=0;
};
struct ConcurrentMetrics {
    std::uint64_t frames=0,drawFrames=0,underlyingReads=0,underlyingExists=0,warmReadMisses=0,warmExistsMisses=0;
    std::size_t activeScenarios=0,combatScenarios=0,activeWeaponKinds=0,activeWeaponDefinitions=0;
    bool commonFrameInterval=false;
    std::uint64_t maxConcurrentEmittingScenarios=0,maxConcurrentWeaponKinds=0,maxConcurrentWeaponDefinitions=0;
    std::uint64_t framesWithFiveWeaponDefinitions=0,framesWithFiveWeaponKinds=0;
    std::uint64_t menuActionFrames=0,menuFramesWithCombatActivity=0;
    std::uint64_t movementFramesWithCombatActivity=0,pickupFramesWithCombatActivity=0,injuryFramesWithCombatActivity=0;
    std::uint64_t movementEmissionFramesWithCombatActivity=0,aidKitFramesWithCombatActivity=0,airStrikeFramesWithCombatActivity=0;
    std::uint64_t botFramesWithCombatActivity=0,helpFramesWithCombatActivity=0,gameOverFramesWithCombatActivity=0,rankingFramesWithCombatActivity=0;
    std::vector<std::string> warmReadNames,warmExistsNames;
};
struct ValidationHost {
    virtual ~ValidationHost()=default;
    virtual void sound(unsigned scenario,std::string_view file,Vec2)=0;
    virtual void soundID(unsigned scenario,unsigned id,Vec2)=0;
    virtual bool writeSave(unsigned scenario,std::span<const std::uint8_t>,SaveReason)=0;
    virtual ui::PreparedScreenText prepareWorldText(unsigned,const ui::TextRasterRequest&){throw std::logic_error("Missing validation world text host");}
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
    // Diagnostic copy after drawFrame, without another world/UI/VFX draw.
    [[nodiscard]] Frame scenarioFrame(unsigned)const;
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

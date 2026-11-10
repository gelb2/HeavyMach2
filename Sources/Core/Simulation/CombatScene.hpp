#pragma once
#include "Core/Simulation/BipodMotion.hpp"
#include "Core/Simulation/GameClock.hpp"
#include "Core/Simulation/WorldNavigation.hpp"
#include "Core/Simulation/WorldRandom.hpp"
#include "Core/Data/Machines.hpp"
#include "Core/Data/Resources.hpp"
#include <memory>
#include <map>

namespace hm2 {
class PreparedTiles;
class VisualLayer;
struct WeaponInfo;
class Particle;
struct TileImage;
struct CombatEnemySnapshot {std::uint64_t id=0;std::string name;Vec2 position;int hp=0;bool targetingPlayer=false,corpse=false;std::size_t activeParts=0;MachineKind kind=MachineKind::bipod;bool boss=false,moveToTarget=false;};
struct CombatPlayerWeaponSnapshot {
    unsigned inventoryEquipSlot=0,currentMuzzle=1,parentSocket=0;
    std::size_t insertionIndex=0;
    std::string part;
    bool reverseY=false;
    Vec2 addPosition;
    int fireLeft=0;
    float fireTime=0,fireBound=0;
};
struct CombatDashSnapshot {Vec2 velocity,lastEffectPosition;float tapTime=0,special=0;std::uintptr_t movePointer=0;unsigned pathCount=0;int currentTerrain=-1;};
struct CombatMetrics {
    std::uint64_t dashStarts=0,dashMoveTicks=0,dashParticleAllocations=0,dashRandomCalls=0;
    std::uint64_t airStrikeRequests=0,airStrikeProjectiles=0,airStrikeDamageCalls=0,airStrikeTicks=0,airStrikeEnemyHPRemoved=0;
    std::uint64_t randomCalls=0,emissions=0,damageCalls=0,projectilesCreated=0,projectilesRemoved=0,soundRequests=0;
    std::uint64_t playerEmissions=0,enemyEmissions=0,enemyTicks=0,projectileTicks=0,drawCalls=0,kills=0;
    unsigned long particleAllocations=0;
    std::map<std::string,std::uint64_t,std::less<>> playerWeaponEmissions,enemyWeaponEmissions;
    // Observations at actual spawn/emission/damage/death/removal consumers.
    std::map<MachineKind,std::uint64_t> enemySpawnsByKind,enemyEmissionsByKind,enemyDamageCallsByKind,enemyKillsByKind,enemyRemovalsByKind;
};
struct CombatSceneHooks {
    virtual ~CombatSceneHooks()=default;
    virtual void sound(std::string_view filename,Vec2 relativePosition)=0;
    virtual void soundID(unsigned originalID,Vec2 relativePosition)=0;
    // Synchronous onCrash continuation. Scene removes parts and updates the
    // spawner/selection between these calls. A drop is not a credit award.
    virtual void dropEnemyLoot(const MachineInfo&,Vec2,double worldTime,float renderTime,WorldRandom&)=0;
    virtual void rewardEnemyKill(const MachineInfo&,Vec2,double worldTime,float renderTime,WorldRandom&)=0;
    virtual int playerDamage(int originalDamage,WorldRandom&)=0;
    virtual int damageBonus()const=0;
    virtual int playerMaximumHP()const{return 256;}
    // Player.tick0xc028: after HP regeneration, before GobHvMach.tick.
    virtual void playerProgressTick(){}
    virtual float playerSpecial()const{return 0;}
    virtual void setPlayerSpecial(float); // Unsupported host fails instead of fake consumption.
    virtual void playerHPChanged(int){}
    virtual void playerDied(){}
    virtual void outOfRange(bool /* left */){}
    // Original listItem slot precedes pool/spawners/Player. External progression
    // may synchronously pick up items here using the same world stream.
    virtual void tickItems(double,float,Vec2,WorldRandom&){}
    virtual void prepareTiles(ResourceAccess&,PreparedTiles&){}
    virtual void collectItems(VisualLayer&,const MapCamera&,const PreparedTiles&){}
};
struct CombatSetup {
    std::string map="Stage01_01";
    int gate=0,playerHP=256;
    bool isSave=false; // GobWorld byte state; caller transfers link-gate state.
    // GobWorld.setStage_LinkGate0x13778 offsets the Player50 units inward.
    // False retains the existing isolated gate-position comparison fixture.
    bool offsetInsideGate=false;
    EquipmentCodes equipment=PlayerEquipment::startingCodes;
    std::uint32_t randomSeed=1;
    float initialRenderTime=0;
    WorldRandom* random=nullptr; // If supplied, must outlive this scene.
};
// Restored HD battle consumer. Platform display adaptation, menus, persistence
// and cheat setup remain separate consumers of this same game state.
class CombatScene {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    CombatScene(ResourceAccess&,CombatSceneHooks&,CombatSetup={});
    ~CombatScene();
    CombatScene(const CombatScene&)=delete;
    CombatScene& operator=(const CombatScene&)=delete;
    void tick();
    bool input(PointerPhase,Vec2 logical,std::uintptr_t identifier);
    bool selectEnemy(std::uint64_t,bool append=false);
    void setFiring(bool left,bool right);
    void equipWeapon(EquipmentCodes,unsigned inventoryEquipSlot); // Only2/3.
    void refreshPlayerHPGauge();
    Particle* allocateParticle(); // Existing world pool/reset/RNG only.
    [[nodiscard]] const TileImage* preparedTile(std::string_view)const;
    [[nodiscard]] bool isPaused()const;
    void startAirStrike(const WeaponInfo*,float fireDelay,Vec2 target);
    [[nodiscard]] std::size_t activeAirStrikes()const;
    void setPlayerHP(int); // Original setHp: field writer; no damage/death event.
    void setIsSave(bool); // Original0xfea0 field writer, independent of HP.
    [[nodiscard]] bool isSave()const;
    void setPaused(bool);
    void setActive(bool);
    [[nodiscard]] const GameClock& clock()const;
    [[nodiscard]] const BipodState& player()const;
    [[nodiscard]] int playerHP()const;
    [[nodiscard]] const MapCamera& camera()const;
    [[nodiscard]] const TerrainCollision& terrain()const;
    [[nodiscard]] const CombatMetrics& metrics()const;
    [[nodiscard]] CombatDashSnapshot dashState()const;
    [[nodiscard]] std::vector<CombatEnemySnapshot> enemies()const;
    [[nodiscard]] std::vector<CombatPlayerWeaponSnapshot> playerWeapons()const;
    [[nodiscard]] std::uint64_t selectedEnemy()const;
    [[nodiscard]] float selectedEnemyHpRate()const;
    [[nodiscard]] std::size_t spawnerCount()const;
    [[nodiscard]] std::size_t activeProjectiles()const;
    [[nodiscard]] std::size_t visibleParticles()const;
    [[nodiscard]] Frame preparationFrame()const;
    // Draw-time trail/effect emission is intentional. Call once per rendered
    // scene, not as a getter from telemetry or tests that merely inspect state.
    Frame drawFrame();
};
}

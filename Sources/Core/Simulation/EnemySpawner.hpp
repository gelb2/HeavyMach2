#pragma once
#include "Core/Data/Maps.hpp"
#include "Core/Simulation/WorldRandom.hpp"

namespace hm2 {
struct EnemySpawnerHost:WorldRandom {
    virtual bool inCamera(Vec2 position)const=0;
    virtual bool spawn(const MapSpawner&,bool boss)=0;
};
struct EnemySpawnerState {int regenCount=0;float checkTime=-1,spawnTime=0,regenTime=0;};
class EnemySpawner {
    MapSpawner definition_;
    bool attack_=false,boss_=false;
    EnemySpawnerState state_;
public:
    EnemySpawner(MapSpawner definition,bool attack,EnemySpawnerState state={});
    void tick(double worldTime,Vec2 player,EnemySpawnerHost&);
    void enemyDead(double worldTime,WorldRandom&);
    [[nodiscard]] const auto& definition()const{return definition_;}
    [[nodiscard]] const auto& state()const{return state_;}
    [[nodiscard]] bool boss()const{return boss_;}
};
}

#include "Core/Simulation/EnemySpawner.hpp"
#include <cmath>
#include <stdexcept>

namespace hm2 {
EnemySpawner::EnemySpawner(MapSpawner definition,bool attack,EnemySpawnerState state)
:definition_(std::move(definition)),attack_(attack),boss_(definition_.model.starts_with("Boss")),state_(state){
    if(definition_.model.empty())throw std::runtime_error("Empty original spawner model");
}
void EnemySpawner::tick(double time,Vec2 player,EnemySpawnerHost& host){
    if(boss_&&state_.regenTime!=0)return;
    if(state_.regenCount!=0)return;
    if(double(float(state_.checkTime+.5f))>=time)return;
    state_.checkTime=float(time);
    if(double(float(state_.spawnTime+state_.regenTime))>=time)return;
    if(std::abs(definition_.position.x-player.x)>=1024.f||std::abs(definition_.position.y-player.y)>=1024.f)return;
    if(state_.regenTime!=0&&host.inCamera(definition_.position))return;
    (void)host.spawn(definition_,boss_);
    ++state_.regenCount; // Original count also advances after a nil result.
}
void EnemySpawner::enemyDead(double time,WorldRandom& random){
    state_.spawnTime=float(time);state_.regenTime=randomUnit(random)*30.f+30.f;
    if(attack_)state_.regenTime+=50.f;
}
}

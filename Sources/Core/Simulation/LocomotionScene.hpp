#pragma once
#include "Core/Simulation/WorldNavigation.hpp"
#include "Core/Presentation/PlayerMapPreview.hpp"

namespace hm2 {
struct LocomotionActor {
    EquipmentCodes equipment=PlayerEquipment::startingCodes;
    Vec2 position;
    float direction=0;
    std::optional<Vec2> aimTarget;
};
// Current restored game slice: equipped bipod, terrain, ordinary touch path,
// camera and pause. No save/audio/XP/shots or invented enemy simulation.
class LocomotionScene {
    PlayerMapPreview presentation_;
    TerrainCollision terrain_;
    std::vector<LocomotionActor> definitions_;
    std::vector<BipodMotion> actors_;
    std::vector<TerrainContact> contacts_;
    WorldNavigation navigation_;
    MapCamera camera_;
    double time_=0;
    unsigned long ticks_=0;
    bool paused_=false;
    static std::vector<PlayerAssemblyCase> prepare(std::span<const LocomotionActor>);
public:
    LocomotionScene(ResourceAccess&,std::string_view map,Size viewport,std::span<const LocomotionActor>);
    void tick();
    bool input(PointerPhase,Vec2 logical,std::uintptr_t id);
    void setPaused(bool paused){paused_=paused;if(paused)navigation_.cancel();}
    [[nodiscard]] bool paused()const{return paused_;}
    [[nodiscard]] double time()const{return time_;}
    [[nodiscard]] unsigned long ticks()const{return ticks_;}
    [[nodiscard]] const MapCamera& camera()const{return camera_;}
    [[nodiscard]] const auto& actors()const{return actors_;}
    [[nodiscard]] const auto& contacts()const{return contacts_;}
    [[nodiscard]] const auto& terrain()const{return terrain_;}
    [[nodiscard]] const auto& navigation()const{return navigation_;}
    [[nodiscard]] Frame frame()const;
};
}

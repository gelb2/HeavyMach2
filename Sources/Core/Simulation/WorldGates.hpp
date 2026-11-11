#pragma once
#include "Core/Data/DataTable.hpp"
#include "Core/Data/Maps.hpp"
#include "Core/Presentation/PreparedTiles.hpp"
#include "Core/Presentation/Visibility.hpp"
#include "Core/UI/Text.hpp"
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <span>

namespace hm2::ui {struct PreparedScreenText;}
namespace hm2 {
// GlobalInfo.openMapInfo0x154a4/getMapName0x15464. MapInfo.description is
// deliberately separate: the original gate labels query MapList.tble.
class WorldGateNames {
    DataTable table_;
    std::map<std::string,std::string,std::less<>> names_;
    explicit WorldGateNames(DataTable);
public:
    explicit WorldGateNames(ResourceAccess&);
    static WorldGateNames parse(DataTable);
    [[nodiscard]] std::optional<std::string_view> find(std::string_view map)const;
    [[nodiscard]] const auto& all()const{return names_;}
    [[nodiscard]] const DataTable& raw()const{return table_;}
};
struct WorldGateSnapshot {
    MapGate definition;
    std::string destinationName;
    float angle=0;
    VisibilityState gate;
    std::array<VisibilityState,2> labels;
    // QobBase registered visuals during the last tick, in parent/white/black
    // order. Camera clipping does not suppress a child's own tick.
    std::array<bool,3> registered{};
};
class WorldGates {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    static constexpr unsigned layer=1;
    using PrepareText=std::function<ui::PreparedScreenText(const ui::TextRasterRequest&)>;
    // openEventGate is the actual accepted-mission/openMap result, evaluated
    // once at map load. Keep Map.gates' raw order; no mission/XP policy here.
    WorldGates(ResourceAccess&,PreparedTiles& sharedTiles,std::span<const MapGate>,
               bool openEventGate,const WorldGateNames&,PrepareText);
    ~WorldGates();
    WorldGates(const WorldGates&)=delete;
    WorldGates& operator=(const WorldGates&)=delete;
    WorldGates(WorldGates&&)noexcept;
    WorldGates& operator=(WorldGates&&)noexcept;
    // Call at each gate's objectBase phase, before the Player's own movement.
    // Every gate reads pause anew; onLink must request a deferred transition,
    // not destroy this module/Scene during its tick. Rotation precedes pause.
    void tick(Vec2 player,const std::function<bool()>& paused,
              const std::function<void(const MapGate&)>& onLink,const MapCamera* camera=nullptr);
    // Actual layer1, non-atlas LinkGate + QobText path. collect advances no
    // state and performs no resource/font access. Visibility is last-tick
    // registration; the original non-atlas gate uses CURRENT isInCam at tick.
    void collect(VisualLayer& layer1,const MapCamera* camera=nullptr)const;
    void collect(std::span<VisualLayer> layers,const MapCamera* camera=nullptr)const;
    [[nodiscard]] Frame preparationFrame()const;
    [[nodiscard]] std::span<const WorldGateSnapshot> snapshots()const;
};
}

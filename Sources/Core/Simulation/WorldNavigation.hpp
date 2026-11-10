#pragma once
#include "Core/Presentation/MapPreview.hpp"
#include "Core/Simulation/BipodMotion.hpp"
#include <array>
#include <cstdint>

namespace hm2 {
enum class PointerPhase {begin,move,end,cancel};
// EasyMove's unoccupied-ground waypoint path. Enemy/item/Dash consumers run
// beside it at their original input branches.
class WorldNavigation {
    std::array<Vec2,12> points_{};
    unsigned start_=0,count_=0;
    std::uintptr_t pointer_=0;
    void append(Vec2,bool add,BipodMotion&);
public:
    [[nodiscard]] unsigned count()const{return count_;}
    [[nodiscard]] Vec2 point(unsigned logicalIndex)const;
    [[nodiscard]] std::uintptr_t pointer()const{return pointer_;}
    bool input(PointerPhase,Vec2 logical,std::uintptr_t,const MapCamera&,BipodMotion&);
    // Scene EasyMove already applied its original camera arithmetic/gates.
    bool inputWorld(PointerPhase,Vec2 world,std::uintptr_t,BipodMotion&);
    void process(BipodMotion&);
    void cancel(){pointer_=0;}
};
// HD processCamera: movement path/target midpoint, axis ease50/snap.5,
// fixed HD limits. Expanded display/other devices do not change game limits.
void processCamera(MapCamera&,Vec2 player,const WorldNavigation&,std::optional<Vec2> target=std::nullopt);
}

#pragma once
#include "Core/Presentation/PreparedTiles.hpp"
#include <functional>

namespace hm2 {
struct MapLoadingState {int step=0;float alpha=0;bool removed=false;};
struct MapLoadingCallbacks {
    std::function<void()> clearObjects,clearTiles,setStage;
};
// GuiMapLoading0x2da14/0x2d730, separate from GameMain's loading doors.
class MapLoading {
    PreparedTiles tiles_{"GuiMapLoading"};
    MapLoadingState state_;
public:
    explicit MapLoading(ResourceAccess&);
    void tick(const MapLoadingCallbacks&);
    void append(Frame&)const;
    [[nodiscard]] Frame preparationFrame()const;
    [[nodiscard]] const MapLoadingState& state()const{return state_;}
};
}

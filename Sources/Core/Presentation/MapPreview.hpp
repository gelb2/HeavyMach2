#pragma once
#include "Core/Data/Maps.hpp"
#include "Core/Data/Resources.hpp"
#include <optional>

namespace hm2 {
// Original camera translation, without shake or game-follow interpolation.
// Diagnostic display only; no camera clamping or combat visibility is invented.
struct MapCamera {
    Vec2 center,offset;
    Size viewport{768,1024};
    float inputMinimumY=100;
    Vec2 backgroundWorld;
    // Configured surfaces adapt camera framing, never world geometry/radii.
    bool fitWorld=false,keepFocusVisible=false;
    float lowerOverscan=94,focusMinimumY=100,focusRadius=25;
    [[nodiscard]] Vec2 toScreen(Vec2 world)const;
    [[nodiscard]] Vec2 toWorld(Vec2 screen)const;
};
class MapPreview {
    Maps maps_;
    std::string mapName_,foreground_,background_;
    Size foregroundSize_,backgroundSize_;
public:
    MapPreview(ResourceAccess&,std::string_view mapName);
    [[nodiscard]] const Maps& maps()const{return maps_;}
    [[nodiscard]] const Map& map()const{return maps_.map(mapName_);}
    [[nodiscard]] Frame compose(const MapCamera&)const;
};
}

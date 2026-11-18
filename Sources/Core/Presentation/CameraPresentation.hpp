#pragma once
#include "Core/Presentation/DisplayLayout.hpp"

namespace hm2 {
enum class CameraPolicy {legacyHD,fixedSurface,compactFocus};
struct CameraFocus {
    Vec2 player;
    std::optional<Vec2> target,lastPath;
};
struct CameraPresentationState {
    CameraPolicy policy=CameraPolicy::legacyHD;
    float idleScale=1,currentZoom=1,targetZoom=1,minimumScale=1,worldScale=1;
    Size viewport{768,1024};
    DisplayTransform worldToCanvas;
    bool fillSurface=false,edgeOverscan=false;
};
// Per-Scene presentation state. No game clock/RNG, resource or OS dependencies.
class CameraPresentation {
    DisplayConfiguration display_;
    struct DistanceZoom {float current=1,target=1;} distance_;
    std::optional<float> previousPlayerY_;
    CameraPresentationState state_;
    void project();
public:
    void configure(DisplayConfiguration);
    void advance(CameraFocus);
    [[nodiscard]] const CameraPresentationState& snapshot()const{return state_;}
};
}

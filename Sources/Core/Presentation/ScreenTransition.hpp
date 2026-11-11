#pragma once
#include "Core/Presentation/PreparedTiles.hpp"
#include <array>
#include <functional>
#include <span>
#include <string_view>

namespace hm2 {
struct ScreenTransitionState {
    int phase=8,requestedScreen=1,previousScreen=1;
    bool visible=false;
    Vec2 top{},bottom{},left{},right{};
    Vec2 centerScale{1,1};
    float centerAngle=0;
};
struct ScreenTransitionCallbacks {
    // Synchronous refreshScreen. It observes phase4. Exceptions preserve that
    // phase and propagate without issuing Open; the host handles its failure.
    std::function<void(int targetScreen)> replaceScreen;
    // Names identify resources already prepared in the common SoundManager.
    std::function<void(std::string_view)> playSound;
};
// HD GameMain.init/start/changeScreen/tick (0x53b0/0x4f0c/0x3e18/0x45e8).
// Owns the common loading-door presentation only. World pause, Session/save,
// input delivery, RNG, and the separate GuiMapLoading remain host concerns.
class ScreenTransition {
    PreparedTiles tiles_{"GameMain:Loading"};
    ScreenTransitionState state_;
public:
    static constexpr unsigned layer=13;
    static constexpr std::array<std::string_view,2> soundResources={
        "Loading_Open.wav","Loading_Close.wav"};
    explicit ScreenTransition(ResourceAccess&,int initialCurrentScreen=1);
    // Original gate is phase>6 and ID1..8. Same-screen requests are accepted.
    [[nodiscard]] bool request(int targetScreen);
    // Processes exactly one entry phase. Host callbacks must keep this module
    // alive and must not recursively tick it. No whole-Session tick is owned.
    void tick(const ScreenTransitionCallbacks&);
    void append(VisualLayer&)const;
    void append(std::span<VisualLayer>)const;
    void append(Frame&)const;
    [[nodiscard]] Frame preparationFrame()const;
    [[nodiscard]] const ScreenTransitionState& state()const{return state_;}
};
}

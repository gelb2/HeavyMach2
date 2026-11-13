#pragma once
#include "Core/Presentation/PreparedTiles.hpp"
#include "Core/Simulation/WorldNavigation.hpp"
#include <array>
#include <functional>
#include <span>
#include <string>

namespace hm2::ui {
struct HelpState {
    int page=0;
    unsigned layer=13;
    bool changePages=false,guidanceVisible=false,removed=false;
};
// GuiHelp0x2b2c4: its own original tile manager, three prepared locale pages,
// and an optional HelpHelp image. No World/Session/save ownership lives here.
class Help {
    PreparedTiles tiles_;
    std::string locale_;
    std::array<std::string,3> pages_;
    HelpState state_;
    Vec2 center_{384,512};
    DisplayConfiguration display_;float scroll_=0,scrollOrigin_=0;Vec2 pointerStart_;std::uintptr_t pointer_=0;bool dragged_=false;
    [[nodiscard]] DisplayTransform presentation()const;
    std::function<void()> onClose_;
    void close();
public:
    // Exactly page==-1 enables navigation/guidance. Other values select a
    // fixed page through the source clamp. Locale is used verbatim, no fallback.
    Help(ResourceAccess&,std::string locale,int page,std::function<void()> onClose={});
    Help(const Help&)=delete;
    Help& operator=(const Help&)=delete;
    Help(Help&&)=delete;
    Help& operator=(Help&&)=delete;
    void setPage(int);
    void discardInputs(){pointer_=0;dragged_=false;}
    void setDisplay(DisplayConfiguration display){display_=display;scroll_=0;discardInputs();}
    void setCenter(Vec2 center){center_=center;}
    void setLayer(unsigned value){state_.layer=value;} // QobBase propagation.
    // All events are consumed, including an event which closes the receiver.
    // Original GuiHelp changes pages on end/state2 only. Display adaptation
    // additionally owns vertical scrolling and discards its pointer on suspend.
    bool input(PointerPhase,Vec2 logical,std::uintptr_t identifier=0);
    // The caller chooses this receiver's original state.layer. The span
    // overload resolves that layer explicitly; it never merges into world5.
    void append(VisualLayer&)const;
    void append(std::span<VisualLayer>)const;
    void append(Frame&)const;
    [[nodiscard]] Frame preparationFrame()const;
    [[nodiscard]] const PreparedTiles& tiles()const{return tiles_;}
    [[nodiscard]] const HelpState& state()const{return state_;}
    [[nodiscard]] const std::string& locale()const{return locale_;}
};
}

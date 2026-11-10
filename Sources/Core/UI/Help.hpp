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
    void setLayer(unsigned value){state_.layer=value;} // QobBase propagation.
    // All events are consumed, including an event which closes the receiver.
    // Only end/state2 changes it. Identifier/Y are unused by original GuiHelp.
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

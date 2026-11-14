#pragma once
#include "Core/Validation/ConcurrentValidation.hpp"

namespace hm2::validation {
// Driven on the same frame clock as every other validation session. Hosts must
// recreate Screens before frame340 to test its real stored-profile read path.
inline void exerciseHUDLayoutFrame(ui::Screens& screens,GameSession& game,HUDLayoutObservation& o,std::uint64_t frame){
    const auto buttonPoint=[&](unsigned id)->std::optional<Vec2>{for(const auto& key:screens.buttons())if(key.id==id)if(const auto button=screens.button(key);button&&button->visible&&button->active)return screens.buttonScreenPosition(key);return {};};
    const auto press=[&](unsigned id){if(const auto point=buttonPoint(id)){constexpr std::uintptr_t pointer=0x48554454;++o.inputs;screens.input(PointerPhase::begin,*point,pointer,game.renderTime());screens.input(PointerPhase::end,*point,pointer,game.renderTime());return true;}return false;};
    if(frame==300){press(1021);o.menuOpened=game.state().systemVisible&&game.battle()&&game.battle()->isPaused();}
    if(frame==310){press(9100);o.editorOpened=screens.hudEditing();}
    if(frame==320&&screens.hudEditing())if(const auto point=buttonPoint(1021)){
        const DisplayLayout layout(game.display());const auto group=hud::visualBounds(screens.hudConfiguration(),hud::controls,layout),area=hud::safe(layout);
        // Original768-wide HD canvas has no horizontal travel at its right
        // edge. Move above the weapon hit rectangles there; wider surfaces
        // have genuine space to move right without covering another button.
        o.dragDelta=area.x+area.width-group.x-group.width>=40?Vec2{40,20}:Vec2{0,140*layout.ui(DisplayRegion::bottom).scale};
        o.menuBefore=*point;o.dragBegin=*point;++o.inputs;screens.input(PointerPhase::begin,*point,0x48554444,game.renderTime());
    }
    if(frame==321&&screens.hudEditing()){++o.inputs;screens.input(PointerPhase::move,{o.dragBegin.x+o.dragDelta.x,o.dragBegin.y+o.dragDelta.y},0x48554444,game.renderTime());}
    if(frame==322&&screens.hudEditing()){++o.inputs;screens.input(PointerPhase::end,{o.dragBegin.x+o.dragDelta.x,o.dragBegin.y+o.dragDelta.y},0x48554444,game.renderTime());if(const auto point=buttonPoint(1021)){o.menuAfter=*point;o.dragMoved=point->x!=o.menuBefore.x||point->y!=o.menuBefore.y;}}
    if(frame==330){press(9103);o.applied=!screens.hudEditing()&&screens.hudConfiguration().customized();if(o.applied)o.appliedSettings=hud::encode(screens.hudConfiguration());}
    if(frame==340)o.reloaded=o.applied&&!screens.hudEditing()&&hud::encode(screens.hudConfiguration())==o.appliedSettings;
    if(frame==350){press(1030);o.resumed=game.battle()&&!game.battle()->isPaused()&&!game.state().systemVisible;}
}
}

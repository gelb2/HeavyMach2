#include "Core/Simulation/LocomotionScene.hpp"
#include <bit>
#include <stdexcept>

namespace hm2 {
std::vector<PlayerAssemblyCase> LocomotionScene::prepare(std::span<const LocomotionActor> definitions){
    std::vector<PlayerAssemblyCase> result;for(const auto& d:definitions)result.push_back({d.equipment,d.position,d.direction,{0,d.direction,d.direction,d.direction,0,0},{}});return result;
}
LocomotionScene::LocomotionScene(ResourceAccess& r,std::string_view map,Size viewport,std::span<const LocomotionActor> definitions)
:presentation_(r,map,prepare(definitions)),terrain_(presentation_.map().map()),definitions_(definitions.begin(),definitions.end()){
    if(definitions.empty())throw std::runtime_error("Locomotion scene needs a Player");
    camera_.viewport=viewport;(void)camera_.toScreen({});
    for(const auto& d:definitions){
        // Reuse the exact Code dictionary already prepared for equipment.
        const float size=presentation_.equipment().item(d.equipment[1]).parameter3;
        actors_.emplace_back(d.position,d.direction,size);contacts_.push_back({d.position,-1,-1,0});
    }
    camera_.center={definitions.front().position.x+140,definitions.front().position.y};
    processCamera(camera_,actors_.front().state().position,navigation_,definitions_.front().aimTarget);
}
void LocomotionScene::tick(){
    if(paused_)return;
    constexpr float increment=std::bit_cast<float>(0x3c87fcb9u);time_+=double(increment);++ticks_;
    // Original world tick: processMyMach -> processCamera -> child movement.
    navigation_.process(actors_.front());
    processCamera(camera_,actors_.front().state().position,navigation_,definitions_.front().aimTarget);
    for(std::size_t i=0;i<actors_.size();++i){
        if(!actors_[i].state().inScreen)continue;
        const auto current=contacts_[i].current;
        actors_[i].tick(current<0?nullptr:&terrain_.edges().at(std::size_t(current)),definitions_[i].aimTarget);
        contacts_[i]=terrain_.resolve(actors_[i].state().position,contacts_[i].previous);
        actors_[i].applyContact(contacts_[i].position);
    }
}
bool LocomotionScene::input(PointerPhase phase,Vec2 p,std::uintptr_t id){return !paused_&&navigation_.input(phase,p,id,camera_,actors_.front());}
Frame LocomotionScene::frame()const{
    std::vector<PlayerAssemblyCase> cases;cases.reserve(actors_.size());
    for(std::size_t i=0;i<actors_.size();++i){const auto& a=actors_[i];cases.push_back({definitions_[i].equipment,a.state().position,a.state().angle,a.angles(),a.offsets()});}
    return presentation_.compose(camera_,cases);
}
}

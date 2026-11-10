#include "Core/UI/Help.hpp"
#include <algorithm>
#include <atomic>
#include <stdexcept>
#include <utility>

namespace hm2::ui {
namespace {
std::string nextManagerIdentity(){static std::atomic<std::uint64_t> next{0};return "GuiHelp:"+std::to_string(next.fetch_add(1,std::memory_order_relaxed));}
}
Help::Help(ResourceAccess& resources,std::string locale,int page,std::function<void()> onClose)
    :tiles_(nextManagerIdentity()),locale_(std::move(locale)),onClose_(std::move(onClose)){
    for(unsigned i=0;i<pages_.size();++i){pages_[i]="HelpScr0"+std::to_string(i+1)+"_"+locale_+".png";tiles_.prepare(resources,pages_[i]);}
    if(page==-1){tiles_.prepare(resources,"HelpHelp.png");state_.changePages=state_.guidanceVisible=true;page=0;}
    setPage(page);
}
void Help::setPage(int page){state_.page=std::clamp(page,0,2);}
void Help::close(){
    if(state_.removed)return;state_.removed=true;
    // Session removes this child and restores World.pause. Move first so a
    // callback may safely reenter input or destroy its owning Help instance.
    auto callback=std::move(onClose_);if(callback)callback();
}
bool Help::input(PointerPhase phase,Vec2 logical,std::uintptr_t){
    if(state_.removed||phase!=PointerPhase::end)return true;
    if(state_.guidanceVisible){state_.guidanceVisible=false;return true;}
    if(!state_.changePages){close();return true;}
    if(logical.x<204.f)setPage(state_.page-1);
    else if(logical.x<=564.f)close();
    else setPage(state_.page+1);
    return true;
}
void Help::append(VisualLayer& layer)const{
    if(state_.removed)return;
    const auto submit=[&](std::string_view name){const auto* tile=tiles_.lookup(name);if(!tile)return;
        VisualImage image;image.tile=tiles_.visual(*tile);image.screenPosition={384,512};image.tileNo=0;image.useAtlas=false;layer.submitImage(image);
    };
    submit(pages_[std::size_t(state_.page)]);
    if(state_.guidanceVisible)submit("HelpHelp.png");
}
void Help::append(std::span<VisualLayer> layers)const{
    if(state_.removed)return;if(state_.layer>=layers.size())throw std::out_of_range("GuiHelp original layer is not available");append(layers[state_.layer]);
}
void Help::append(Frame& frame)const{VisualLayer layer;append(layer);std::move(layer.drain()).appendBatches(frame);}
Frame Help::preparationFrame()const{Frame frame;tiles_.appendPreparation(frame);return frame;}
}

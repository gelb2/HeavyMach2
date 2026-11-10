#include "Core/Presentation/MapLoading.hpp"
#include <stdexcept>
namespace hm2 {
MapLoading::MapLoading(ResourceAccess& resources){tiles_.prepare(resources,"StageClearBG.png");}
void MapLoading::tick(const MapLoadingCallbacks& host){
    if(state_.removed)return;
    switch(state_.step){
    case 0:state_.alpha+=(1.f-state_.alpha)/20.f;if(state_.alpha>.99f){state_.alpha=1;state_.step=1;}break;
    case 1:if(!host.clearObjects)throw std::logic_error("Missing original clearAllObject consumer");host.clearObjects();state_.step=2;break;
    case 2:if(!host.clearTiles)throw std::logic_error("Missing original clearTileResMgr consumer");host.clearTiles();state_.step=4;break;
    case 4:if(!host.setStage)throw std::logic_error("Missing original setStage consumer");host.setStage();state_.step=5;break;
    case 5:state_.alpha+=(0.f-state_.alpha)/20.f;if(state_.alpha<.01f){state_.alpha=0;state_.step=6;}break;
    case 6:state_.removed=true;break;
    default:throw std::logic_error("Unrestored GuiMapLoading state");
    }
}
void MapLoading::append(Frame& frame)const{
    if(state_.removed)return;const auto* tile=tiles_.lookup("StageClearBG.png");if(!tile)return;
    VisualImage image;image.tile=tiles_.visual(*tile);image.screenPosition={384,512};image.alpha=state_.alpha;image.useAtlas=false;
    VisualLayer layer;layer.submitImage(image);auto result=layer.drain();for(auto& sprite:result.nonAtlas)sprite.visualLayer=13;std::move(result).appendBatches(frame);
}
Frame MapLoading::preparationFrame()const{Frame result;tiles_.appendPreparation(result);return result;}
}

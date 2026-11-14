#include "Core/Presentation/MapLoading.hpp"
#include "Core/Presentation/MenuArtwork.hpp"
#include <stdexcept>
namespace hm2 {
MapLoading::MapLoading(ResourceAccess& resources,PreparedTiles* shared,bool phoneArtwork):tiles_(shared?shared:&standaloneTiles_),phoneArtwork_(phoneArtwork),imageName_(phoneArtwork?menu::phoneAsset("StageClearBG.png"):"StageClearBG.png"){
    // GuiMapLoading.init0x2da14 uses ResMgr_Tile.sharedMgr. Ending a fade
    // releases its image node; it does not discard this shared tile.
    tiles_->prepare(resources,imageName_);
}
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
    if(state_.removed)return;const auto* tile=tiles_->lookup(imageName_);if(!tile)return;
    VisualImage image;image.tile=tiles_->visual(*tile);const auto view=artworkViewport();image.screenPosition={view.width*.5f,view.height*.5f};image.alpha=state_.alpha;image.useAtlas=false;
    VisualLayer layer;layer.submitImage(image);auto result=layer.drain();for(auto& sprite:result.nonAtlas)sprite.visualLayer=13;std::move(result).appendBatches(frame);
}
Frame MapLoading::preparationFrame()const{
    Frame result;if(const auto* tile=tiles_->lookup(imageName_)){Sprite image;image.image=tile->name;image.size=tile->pixels;image.textureSource=tile->textureSource;result.sprites.push_back(std::move(image));}return result;
}
}

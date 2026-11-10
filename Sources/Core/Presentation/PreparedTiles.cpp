#include "Core/Presentation/PreparedTiles.hpp"
#include "Core/Presentation/AssemblyPreview.hpp"
#include <stdexcept>
#include <utility>
namespace hm2 {
TileImage* PreparedTiles::prepare(ResourceAccess& resources,std::string_view name){
    if(name.empty())return nullptr;
    if(name.find('/')!=name.npos||name.find('\\')!=name.npos)throw std::runtime_error("Invalid original texture resource name");
    auto it=tiles_.find(name);
    if(it==tiles_.end()){
        std::optional<TileImage> prepared;
        if(resources.exists(name)){TileImage image;image.name=name;image.pixels=AssemblyPreview::pngSize(resources.read(name));prepared=std::move(image);}
        it=tiles_.emplace(std::string(name),std::move(prepared)).first;
    }
    return it->second?&*it->second:nullptr;
}
TileImage* PreparedTiles::lookup(std::string_view name){
    return const_cast<TileImage*>(std::as_const(*this).lookup(name));
}
const TileImage* PreparedTiles::lookup(std::string_view name)const{
    if(name.empty())return nullptr;
    const auto it=tiles_.find(name);if(it==tiles_.end())throw std::runtime_error("Texture was not prepared: "+std::string(name));
    return it->second?&*it->second:nullptr;
}
VisualTile PreparedTiles::visual(const TileImage& image)const{
    VisualTile out;out.identity=identity_+":"+image.name;out.image=image.name;out.content=image.pixels;
    out.storage={1,1};while(out.storage.width<out.content.width)out.storage.width*=2;while(out.storage.height<out.content.height)out.storage.height*=2;
    out.columns=image.columns;out.rows=image.rows;out.origin=image.origin;out.forRetina=image.forRetina;out.pixelFormat=image.pixelFormat;return out;
}
void PreparedTiles::prepareWorldEffects(ResourceAccess& resources){
    struct Split{const char* name;unsigned x,y;float originY;};
    // GobWorld::init0x11208, filename references and explicit split/setOrigin.
    constexpr Split splits[]={{"Efx_Explode02.png",6,4,-44},{"Efx_ExplodeGround.png",4,5,0},
        {"Efx_ExplodeBoss.png",4,4,0},{"Efx_ExplodeNPC.png",4,4,0},{"Efx_Fire.png",4,4,0},
        {"Efx_Damage.png",6,1,0},{"Efx_Smoke.png",4,4,0},{"Efx_RailgunEx.png",4,4,0}};
    for(const auto& split:splits)if(auto* image=prepare(resources,split.name)){
        image->columns=split.x;image->rows=split.y;image->origin.y=split.originY;
    }
}
void PreparedTiles::appendPreparation(Frame& frame)const{
    for(const auto& [name,tile]:tiles_)if(tile){Sprite s;s.image=name;s.size=tile->pixels;frame.sprites.push_back(std::move(s));}
}
}

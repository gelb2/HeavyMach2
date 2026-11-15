#include "Core/Presentation/PreparedTiles.hpp"
#include "Assets/Image.hpp"
#include "Core/Presentation/AssemblyPreview.hpp"
#include <algorithm>
#include "Core/Data/ResourceName.hpp"
#include <stdexcept>
#include <utility>
namespace hm2 {
TileImage* PreparedTiles::prepareShadow(ResourceAccess& resources,std::string_view name){
    if(name.empty())return nullptr;
    const auto key=std::string(name)+"~shd";
    if(auto it=tiles_.find(key);it!=tiles_.end())return it->second?&*it->second:nullptr;
    const auto* original=prepare(resources,name);std::optional<TileImage> shadow;
    if(original){shadow=*original;shadow->name=key;shadow->pixelFormat=3;
        shadow->textureSource={std::string(name),TextureVariant::originalHalfAlpha};shadow->color={0,0,0,1};shadow->origin={};shadow->forRetina=false;}
    const auto it=tiles_.emplace(key,std::move(shadow)).first;++revision_;
    return it->second?&*it->second:nullptr;
}
const TileImage* PreparedTiles::lookupShadow(std::string_view name)const{
    if(name.empty())return nullptr;
    return lookup(std::string(name)+"~shd");
}
TileImage* PreparedTiles::prepare(ResourceAccess& resources,std::string_view name){
    if(name.empty())return nullptr;
    if(!validResourceName(name))throw std::runtime_error("Invalid original texture resource name");
    auto it=tiles_.find(name);
    if(it==tiles_.end()){
        std::optional<TileImage> prepared;
        if(resources.exists(name)){TileImage image;image.name=name;const auto bytes=resources.read(name);constexpr std::uint8_t png[]={137,80,78,71,13,10,26,10};if(bytes.size()>=8&&std::equal(std::begin(png),std::end(png),bytes.begin()))image.pixels=AssemblyPreview::pngSize(bytes);else{const auto info=imageInfo(bytes);image.pixels={float(info.width),float(info.height)};}prepared=std::move(image);}
        it=tiles_.emplace(std::string(name),std::move(prepared)).first;++revision_;
    }
    return it->second?&*it->second:nullptr;
}
TileImage* PreparedTiles::lookup(std::string_view name){
    return const_cast<TileImage*>(std::as_const(*this).lookup(name));
}
const TileImage* PreparedTiles::lookup(std::string_view name)const{
    if(name.empty())return nullptr;
    const auto it=tiles_.find(name);if(it==tiles_.end()){
        if(fallback_)return fallback_->lookup(name);
        throw std::runtime_error("Texture was not prepared: "+std::string(name));
    }
    return it->second?&*it->second:nullptr;
}
VisualTile PreparedTiles::visual(const TileImage& image)const{
    VisualTile out;out.identity=identity_+":"+image.name;out.image=image.name;out.content=image.pixels;
    out.storage={1,1};while(out.storage.width<out.content.width)out.storage.width*=2;while(out.storage.height<out.content.height)out.storage.height*=2;
    out.columns=image.columns;out.rows=image.rows;out.origin=image.origin;out.forRetina=image.forRetina;out.pixelFormat=image.pixelFormat;
    out.textureSource=image.textureSource;out.color=image.color;return out;
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
    for(const auto& [name,tile]:tiles_)if(tile){Sprite s;s.image=name;s.size=tile->pixels;s.textureSource=tile->textureSource;frame.sprites.push_back(std::move(s));}
}
}

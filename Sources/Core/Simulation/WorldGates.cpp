#include "Core/Simulation/WorldGates.hpp"
#include "Core/UI/Screens.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace hm2 {
namespace {
Vec2 whiteWorld(Vec2 gate){return {0.f+gate.x,6.f+gate.y};}
Vec2 blackWorld(Vec2 gate){const auto white=whiteWorld(gate);return {1.f+white.x,-1.f+white.y};}
Vec2 screen(Vec2 world,const MapCamera* camera){return camera?camera->toScreen(world):world;}
void finite(Vec2 p){if(!std::isfinite(p.x)||!std::isfinite(p.y))throw std::runtime_error("Nonfinite world gate position");}
}
WorldGateNames::WorldGateNames(DataTable table):table_(std::move(table)){
    for(std::size_t row=0;row<table_.rowCount();++row){
        const auto key=table_.value(row,"MapName"),value=table_.value(row,"MapDesc");
        // NSDictionary's insertion cannot accept nil. Diagnose a malformed
        // reconstruction input rather than inventing MapInfo/fallback labels.
        if(!key||!value)throw std::runtime_error("Missing original MapList name/description");
        names_.insert_or_assign(std::string(*key),std::string(*value));
    }
}
WorldGateNames::WorldGateNames(ResourceAccess& resources):WorldGateNames(DataTable::parse(resources.read("MapList.tble"))){}
WorldGateNames WorldGateNames::parse(DataTable table){return WorldGateNames(std::move(table));}
std::optional<std::string_view> WorldGateNames::find(std::string_view name)const{
    const auto i=names_.find(name);if(i==names_.end())return {};return i->second;
}
struct WorldGates::Impl {
    VisualTile image;
    std::vector<WorldGateSnapshot> states;
    std::vector<std::array<ui::Text,2>> text;
    Impl(ResourceAccess& resources,PreparedTiles& cache,std::span<const MapGate> gates,
         bool openEventGate,const WorldGateNames& names,const PrepareText& prepare){
        // GobWorld.addLinkGate0x10ac4 prepares the shared tile on each actual
        // creation. A map with every event gate filtered out prepares none.
        bool prepared=false;
        for(const auto& gate:gates){
            if(gate.eventOnly&&!openEventGate)continue;
            finite(gate.position);
            if(!prepared){auto* tile=cache.prepare(resources,"LinkGate.png");if(!tile)throw std::runtime_error("Missing original LinkGate.png");
                tile->columns=tile->rows=1;image=cache.visual(*tile);prepared=true;}
            const auto name=names.find(gate.linkMap);if(!name)throw std::runtime_error("Unresolved original MapList destination: "+gate.linkMap);
            if(!prepare)throw std::runtime_error("Original world gate text preparer is absent");
            WorldGateSnapshot state;state.definition=gate;state.destinationName=*name;
            state.gate.visual=true;state.gate.bounds={-image.content.width*.5f,-image.content.height*.5f,image.content.width*.5f,image.content.height*.5f};
            std::array<std::shared_ptr<const ui::TextBitmap>,2> bitmaps;
            std::array<std::string,2> images;
            for(unsigned n=0;n<2;++n){
                ui::TextRasterRequest request{"GobLinkGate:"+gate.linkMap+(n?":black":":white"),state.destinationName,"Trebuchet-BoldItalic",{64,48},13,ui::TextAlign::center,false,0};
                auto result=prepare(request);
                if(!result.bitmap||!(result.bitmap->request==request))throw std::runtime_error("World gate text preparation changed the original request");
                images[n]=std::move(result.image);bitmaps[n]=std::move(result.bitmap);
                // QobText.init uses Texture2D.width/height (POT storage),
                // unlike Tile2D's content-sized draw quad (64x48).
                const float w=float(bitmaps[n]->layout.width),h=float(bitmaps[n]->layout.height);
                state.labels[n].visual=true;state.labels[n].bounds={-w*.5f,-h*.5f,w*.5f,h*.5f};
            }
            states.push_back(std::move(state));
            text.push_back({ui::Text{std::move(images[0]),std::move(bitmaps[0])},ui::Text{std::move(images[1]),std::move(bitmaps[1])}});
            text.back()[0].setColor(255,255,255);text.back()[1].setColor(0,0,0);
        }
    }
};
WorldGates::WorldGates(ResourceAccess& resources,PreparedTiles& tiles,std::span<const MapGate> gates,
                     bool openEventGate,const WorldGateNames& names,PrepareText prepare)
    :impl_(std::make_unique<Impl>(resources,tiles,gates,openEventGate,names,prepare)){}
WorldGates::~WorldGates()=default;
WorldGates::WorldGates(WorldGates&&)noexcept=default;
WorldGates& WorldGates::operator=(WorldGates&&)noexcept=default;
void WorldGates::tick(Vec2 player,const std::function<bool()>& paused,
                      const std::function<void(const MapGate&)>& onLink,const MapCamera* camera){
    finite(player);if(!paused)throw std::runtime_error("World gate pause getter is absent");
    for(auto& state:impl_->states){
        state.angle+=.01f;
        if(!paused()&&std::abs(player.x-state.definition.position.x)<30.f&&
           std::abs(player.y-state.definition.position.y)<30.f){if(onLink)onLink(state.definition);}
        // Non-atlas QobImage.tick0x38e50 has no pre-registration using cached
        // isInScreen. QobBase.tick0x36d98 refreshes its current visibility first
        // and visits child Texts even when the parent's camera overlap is false.
        refreshVisibility(state.gate,state.definition.position,camera);
        state.registered[0]=state.gate.show&&!state.gate.removed&&state.gate.inScreen;
        refreshVisibility(state.labels[0],whiteWorld(state.definition.position),camera);
        state.registered[1]=state.gate.show&&state.labels[0].show&&!state.labels[0].removed&&state.labels[0].inScreen;
        refreshVisibility(state.labels[1],blackWorld(state.definition.position),camera);
        state.registered[2]=state.gate.show&&state.labels[0].show&&state.labels[1].show&&!state.labels[1].removed&&state.labels[1].inScreen;
    }
}
void WorldGates::collect(VisualLayer& layer,const MapCamera* camera)const{
    for(std::size_t index=0;index<impl_->states.size();++index){
        const auto& state=impl_->states[index];
        if(state.registered[0]){VisualImage gate;gate.tile=impl_->image;gate.screenPosition=screen(state.definition.position,camera);gate.angle=state.angle;gate.blendType=2;gate.useAtlas=false;layer.submitImage(gate);}
        // Labels are separate visual children with translation-only world pos.
        // Copying presentation values never invokes prepareText or font lookup.
        for(unsigned n=0;n<2;++n)if(state.registered[n+1]){
            auto label=impl_->text[index][n];label.position=screen(n?blackWorld(state.definition.position):whiteWorld(state.definition.position),camera);
            Frame f;label.append(f);for(auto& sprite:f.sprites)layer.submitNonAtlas(std::move(sprite));
        }
    }
}
void WorldGates::collect(std::span<VisualLayer> layers,const MapCamera* camera)const{
    if(layers.size()<=layer)throw std::out_of_range("Original world gate layer1 is absent");
    collect(layers[layer],camera);
}
Frame WorldGates::preparationFrame()const{
    Frame out;
    if(!impl_->states.empty()){Sprite tile;tile.image=impl_->image.image;tile.size=impl_->image.content;tile.textureSource=impl_->image.textureSource;out.sprites.push_back(std::move(tile));}
    for(const auto& pair:impl_->text)for(const auto& text:pair)text.append(out);
    return out;
}
std::span<const WorldGateSnapshot> WorldGates::snapshots()const{return impl_->states;}
}

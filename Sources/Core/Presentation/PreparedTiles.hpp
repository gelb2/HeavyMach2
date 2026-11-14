#pragma once
#include "Core/Data/Resources.hpp"
#include "Core/Presentation/TileImage.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include <map>
#include <optional>
namespace hm2 {
// Resource manager preparation and immutable frame-time cache lookup. A failed
// original resource remains nil; no replacement art or per-unit path exists.
class PreparedTiles {
    std::string identity_;
    std::map<std::string,std::optional<TileImage>,std::less<>> tiles_;
    PreparedTiles* fallback_=nullptr;
    std::uint64_t revision_=0;
public:
    // A World may read global/shared resources, but preparation always writes
    // its own scope. Per-World splits/origins never mutate a shared Tile2D.
    explicit PreparedTiles(std::string identity="shared",PreparedTiles* fallback=nullptr):identity_(std::move(identity)),fallback_(fallback){}
    TileImage* prepare(ResourceAccess&,std::string_view);
    TileImage* prepareShadow(ResourceAccess&,std::string_view originalImage);
    const TileImage* lookupShadow(std::string_view originalImage)const;
    TileImage* lookup(std::string_view);
    const TileImage* lookup(std::string_view)const;
    [[nodiscard]] VisualTile visual(const TileImage&)const;
    void prepareWorldEffects(ResourceAccess&);
    void appendPreparation(Frame&)const;
    [[nodiscard]] const auto& all()const{return tiles_;}
    [[nodiscard]] const std::string& identity()const{return identity_;}
    [[nodiscard]] std::uint64_t revision()const{return revision_+(fallback_?fallback_->revision():0);}
};
}

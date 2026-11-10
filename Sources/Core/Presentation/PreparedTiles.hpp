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
public:
    explicit PreparedTiles(std::string identity="shared"):identity_(std::move(identity)){}
    TileImage* prepare(ResourceAccess&,std::string_view);
    TileImage* lookup(std::string_view);
    const TileImage* lookup(std::string_view)const;
    [[nodiscard]] VisualTile visual(const TileImage&)const;
    void prepareWorldEffects(ResourceAccess&);
    void appendPreparation(Frame&)const;
    [[nodiscard]] const auto& all()const{return tiles_;}
};
}

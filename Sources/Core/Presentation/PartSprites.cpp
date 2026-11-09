#include "Core/Presentation/PartSprites.hpp"
#include "Core/Presentation/AssemblyPreview.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace hm2 {
namespace {std::string image(const Part& p){return p.tileCount?p.name+".png":"PartsDummy.png";}}
void PartSprites::prepare(ResourceAccess& r,std::span<const PartPose> poses){
    for(const auto& pose:poses){const auto& p=*pose.part;const auto name=image(p);if(sizes_.contains(name))continue;
        // Verified original DummyParts root: getTile returns nil; atlas skips
        // nil and fallback sends no GL draw. Do not hide other missing assets.
        if(p.name=="DummyParts"&&!r.exists(name)){sizes_.emplace(name,std::nullopt);continue;}
        sizes_.emplace(name,AssemblyPreview::pngSize(r.read(name)));
    }
}
void PartSprites::append(Frame& frame,std::span<const PartPose> poses,Vec2 position)const{
    if(!std::isfinite(position.x)||!std::isfinite(position.y))throw std::runtime_error("Nonfinite part placement");
    for(const auto& pose:poses){const auto& p=*pose.part;const auto name=image(p);const auto i=sizes_.find(name);
        if(i==sizes_.end())throw std::runtime_error("Part texture was not prepared: "+name);
        if(!i->second)continue;
        const auto size=*i->second;const auto columns=std::max(1u,p.tileCount);
        if(std::uint32_t(size.width)%columns)throw std::runtime_error("Original tile grid does not divide image");
        frame.sprites.push_back({name,{position.x+pose.center.x,position.y+pose.center.y},{size.width/float(columns),size.height},pose.angle,0,columns,pose.reverseY});
    }
}
}

#include "Core/Presentation/AssemblyPreview.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace hm2 {
namespace {
std::uint32_t bigWord(std::span<const std::uint8_t> b,std::size_t i){return std::uint32_t(b[i])<<24|std::uint32_t(b[i+1])<<16|std::uint32_t(b[i+2])<<8|b[i+3];}
}
Size AssemblyPreview::pngSize(std::span<const std::uint8_t> bytes) {
    constexpr std::uint8_t signature[]={137,80,78,71,13,10,26,10};
    if(bytes.size()<8||!std::equal(std::begin(signature),std::end(signature),bytes.begin()))throw std::runtime_error("Not a PNG resource");
    std::size_t pos=8;
    while(pos<=bytes.size()&&bytes.size()-pos>=12){
        const auto n=bigWord(bytes,pos);
        if(n>bytes.size()-pos-12)throw std::runtime_error("PNG chunk outside resource");
        if(bytes[pos+4]=='I'&&bytes[pos+5]=='H'&&bytes[pos+6]=='D'&&bytes[pos+7]=='R'){
            if(n!=13)throw std::runtime_error("Invalid PNG IHDR length");
            const auto w=bigWord(bytes,pos+8),h=bigWord(bytes,pos+12);
            if(!w||!h||w>16384||h>16384)throw std::runtime_error("Invalid PNG dimensions");
            return {float(w),float(h)};
        }
        pos+=12+n;
    }
    throw std::runtime_error("PNG has no IHDR");
}
AssemblyPreview::AssemblyPreview(ResourceAccess& r,std::span<const AssemblyCase> cases,Size viewport)
:models_(Models::parse(Chunk::parse(r.read("PartsInfo.mcb")),Chunk::parse(r.read("MachInfo.mcb")))) {
    for(const auto& c:cases)for(const auto& pose:models_.staticPose(c.model,c.machineAngle,c.weaponAngle)){
        const auto& p=*pose.part;const auto name=p.tileCount?p.name+".png":"PartsDummy.png";
        if(!imageSizes_.contains(name))imageSizes_.emplace(name,pngSize(r.read(name)));
    }
    frame_=compose(cases,viewport);
}
Frame AssemblyPreview::compose(std::span<const AssemblyCase> cases,Size viewport)const {
    if(!std::isfinite(viewport.width)||!std::isfinite(viewport.height)||viewport.width<=0||viewport.height<=0)throw std::runtime_error("Invalid preview viewport");
    Frame frame;frame.width=viewport.width;frame.height=viewport.height;
    for(const auto& c:cases){
        if(!std::isfinite(c.position.x)||!std::isfinite(c.position.y))throw std::runtime_error("Invalid preview position");
        for(const auto& pose:models_.staticPose(c.model,c.machineAngle,c.weaponAngle)){
            const auto& p=*pose.part;
            const auto name=p.tileCount?p.name+".png":"PartsDummy.png";
            const auto size=imageSizes_.find(name);
            if(size==imageSizes_.end())throw std::runtime_error("Part texture was not prepared before frame composition: "+name);
            const unsigned columns=std::max(1u,p.tileCount);
            if(std::uint32_t(size->second.width)%columns)throw std::runtime_error("Original tile grid does not divide image");
            frame.sprites.push_back({name,{c.position.x+pose.center.x,c.position.y+pose.center.y},
                {size->second.width/float(columns),size->second.height},pose.angle,0,columns,false});
        }
    }
    return frame;
}
}

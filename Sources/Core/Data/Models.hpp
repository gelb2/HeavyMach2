#pragma once
#include "Core/Data/Chunk.hpp"
#include "Core/Types.hpp"
#include <map>
#include <string_view>
#include <span>

namespace hm2 {
struct Part {
    std::string name;
    unsigned type=0,tileCount=0;
    std::vector<Vec2> sockets;
    [[nodiscard]] Vec2 socket(unsigned i)const{return i<sockets.size()?sockets[i]:Vec2{};}
};
struct Assembly {
    std::string part;
    unsigned parentSocket=0,zOrder=0;
    std::vector<Assembly> children;
};
struct Model {
    std::string name;
    bool completeOriginalSlice=false;
    std::vector<Assembly> parts;
};
struct PartPose {
    const Part* part=nullptr;
    Vec2 center;
    float angle=0;
    unsigned layer=0,zOrder=0;
    bool reverseY=false;
};
// Prepared by the original model decoder or equipment mounting rules. Angles
// are absolute; controller-produced animation/addPosition/scale are not implied.
struct PartMount {
    std::string part;
    int parent=-1;
    unsigned parentSocket=0,layer=0,zOrder=0;
    float angle=0;
    bool reverseY=false;
};
class Models {
    std::map<std::string,Part,std::less<>> parts_;
    std::map<std::string,Model,std::less<>> models_;
public:
    static Models parse(const Chunk& parts,const Chunk& machines);
    [[nodiscard]] const auto& parts()const{return parts_;}
    [[nodiscard]] const auto& models()const{return models_;}
    [[nodiscard]] const Part& part(std::string_view)const;
    [[nodiscard]] const Model& model(std::string_view)const;
    // Verified static pose only: scale1/addPosition0/reverse0, no controller tick.
    // Machine and weapon directions can coexist in a single assembled model.
    [[nodiscard]] std::vector<PartPose> staticPose(std::string_view,float machineAngle=0,float weaponAngle=0)const;
    [[nodiscard]] std::vector<PartPose> mountedPose(std::span<const PartMount>,float machineAngle=0)const;
};
}

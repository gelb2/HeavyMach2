#include "Core/Data/Models.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace hm2 {
namespace {
Assembly assembly(const Chunk& c) {
    if(c.type!=4||c.data.size()!=26)throw std::runtime_error("Invalid HM2 assembly payload");
    Assembly a{c.text(0,24),c.byte(24),c.byte(25),{}};
    if(a.part.empty())throw std::runtime_error("Empty original part reference");
    for(const auto& child:c.children){if(child.type!=4)throw std::runtime_error("Unknown assembly node type");a.children.push_back(assembly(child));}
    return a;
}
Vec2 add(Vec2 a,Vec2 b){return {a.x+b.x,a.y+b.y};}
Vec2 subtract(Vec2 a,Vec2 b){return {a.x-b.x,a.y-b.y};}
Vec2 rotate(Vec2 p,float angle){const float c=std::cos(angle),s=std::sin(angle);return {p.x*c-p.y*s,p.x*s+p.y*c};}
}
Models Models::parse(const Chunk& parts,const Chunk& machines) {
    if(parts.type!=0||!parts.data.empty()||machines.type!=2||!machines.data.empty())throw std::runtime_error("Unexpected HM2 catalog root");
    Models out;
    for(const auto& c:parts.children){
        if(c.type!=1||c.data.size()<27||!c.children.empty())throw std::runtime_error("Invalid HM2 part record");
        Part p;p.name=c.text(0,24);p.type=c.byte(24);p.tileCount=c.byte(25);const auto n=c.byte(26);
        if(p.name.empty()||c.data.size()!=27+std::size_t(n)*8)throw std::runtime_error("Invalid HM2 sockets");
        for(unsigned i=0;i<n;++i){Vec2 v{c.real32(27+i*8),c.real32(31+i*8)};if(!std::isfinite(v.x)||!std::isfinite(v.y))throw std::runtime_error("Nonfinite original socket");p.sockets.push_back(v);}
        if(!out.parts_.emplace(p.name,std::move(p)).second)throw std::runtime_error("Duplicate HM2 part ID");
    }
    for(const auto& c:machines.children){
        if(c.type!=3||c.data.size()!=33)throw std::runtime_error("Invalid HM2 model record");
        Model m;m.name=c.text(0,24);m.completeOriginalSlice=c.declaredSize==c.consumedSize;
        if(m.name.empty())throw std::runtime_error("Empty HM2 model ID");
        for(const auto& child:c.children)m.parts.push_back(assembly(child));
        if(!out.models_.emplace(m.name,std::move(m)).second)throw std::runtime_error("Duplicate HM2 model ID");
    }
    for(const auto& [name,m]:out.models_){
        std::function<void(const Assembly&)> check=[&](const Assembly& a){(void)out.part(a.part);for(const auto& child:a.children)check(child);};
        for(const auto& a:m.parts)check(a);
    }
    return out;
}
const Part& Models::part(std::string_view name)const {auto i=parts_.find(name);if(i==parts_.end())throw std::runtime_error("Unresolved HM2 part: "+std::string(name));return i->second;}
const Model& Models::model(std::string_view name)const {auto i=models_.find(name);if(i==models_.end())throw std::runtime_error("Unresolved HM2 model: "+std::string(name));return i->second;}
std::vector<PartPose> Models::staticPose(std::string_view name,float machineAngle,float weaponAngle)const {
    if(!std::isfinite(machineAngle)||!std::isfinite(weaponAngle))throw std::runtime_error("Nonfinite pose angle");
    const auto& m=model(name);
    if(!m.completeOriginalSlice)throw std::runtime_error("Original selected model slice is incomplete: "+m.name);
    std::vector<PartPose> result;
    // Every part is a direct machine child; connection ancestry is a separate
    // socket graph. GobMachParts setBaseParts/getSocketPos/refreshRotationPos.
    std::function<void(const Assembly&,Vec2,float,bool)> visit=[&](const Assembly& a,Vec2 base,float parentAngle,bool connected){
        const auto& p=part(a.part);const Vec2 anchor=p.socket(0);
        const float ownAngle=p.type==4?weaponAngle:p.type==7?0.f:parentAngle;
        const Vec2 center=subtract(rotate(base,parentAngle),rotate(anchor,ownAngle));
        result.push_back({&p,center,ownAngle,connected?1u:0u,a.zOrder});
        for(const auto& child:a.children){
            // Missing nonzero socket returns the inherited base (0x1c7e4).
            Vec2 socket=child.parentSocket==0?subtract(base,anchor):
                child.parentSocket<p.sockets.size()?add(base,subtract(p.socket(child.parentSocket),anchor)):base;
            visit(child,socket,ownAngle,true);
        }
    };
    for(const auto& a:m.parts)visit(a,{},machineAngle,false);
    std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.layer==b.layer?a.zOrder<b.zOrder:a.layer<b.layer;});
    return result;
}
}

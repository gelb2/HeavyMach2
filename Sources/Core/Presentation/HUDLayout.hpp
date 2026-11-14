#pragma once
#include "Core/Data/Resources.hpp"
#include "Core/Presentation/DisplayLayout.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <span>
#include <stdexcept>

namespace hm2::hud {
// Restoration setting; not part of the original progress-save envelope.
enum Group : unsigned {gauges,combat,controls,count};
// The original 768x256 image has transparent top padding. These extents are
// its decoded alpha bounds unioned with each group's visible widgets and hits.
// Quick-slot hits extend left of the controls artwork; range labels extend
// right of the combat artwork. Placement must retain both.
inline constexpr std::array<ClipRect,count> bounds{{{0,0,288,124},{288,0,224,138},{487,0,281,133}}};
// Artwork ownership never follows the wider widget/hit placement extents:
// every original background pixel belongs to exactly one fixed source slice.
inline constexpr std::array<ClipRect,count> backgroundSlices{{{0,0,288,256},{288,0,212,256},{500,0,268,256}}};
struct Placement {float x=0,y=0;bool operator==(const Placement&)const=default;};
struct Configuration {
    std::array<std::optional<Placement>,count> positions{};
    bool legacySafeArea=false;
    bool operator==(const Configuration&)const=default;
    bool customized()const{for(const auto& p:positions)if(p)return true;return false;}
};
inline ClipRect placementArea(const DisplayLayout& layout){
    const auto& c=layout.configuration();return c.enabled?ClipRect{0,0,c.canvas.width,c.canvas.height}:ClipRect{0,0,768,1024};
}
inline ClipRect legacyPlacementArea(const DisplayLayout& layout){
    const auto& c=layout.configuration();const auto a=c.enabled?c.content():ClipRect{0,0,768,1024};
    return {a.x,a.y,a.width,std::min(a.height,std::max(138*layout.ui(DisplayRegion::bottom).scale,a.height*.5f))};
}
inline DisplayTransform transform(const Configuration& config,unsigned group,const DisplayLayout& layout){
    auto t=layout.ui(DisplayRegion::bottom);if(group>=count||!config.positions[group])return t;
    const auto p=*config.positions[group];if(!std::isfinite(p.x)||!std::isfinite(p.y)||p.x<0||p.x>1||p.y<0||p.y>1)throw std::invalid_argument("Invalid HUD placement");
    const auto a=config.legacySafeArea?legacyPlacementArea(layout):placementArea(layout);const auto b=bounds[group];const float width=b.width*t.scale,height=b.height*t.scale;
    const float x=a.x+std::max(0.f,a.width-width)*p.x,y=a.y+std::max(0.f,a.height-height)*p.y;
    t.offset={x-b.x*t.scale,y-b.y*t.scale};return t;
}
inline ClipRect visualBounds(const Configuration& config,unsigned group,const DisplayLayout& layout){const auto t=transform(config,group,layout);const auto b=bounds.at(group);const auto p=t.map({b.x,b.y});return {p.x,p.y,b.width*t.scale,b.height*t.scale};}
inline ClipRect intersect(ClipRect a,ClipRect b){const float x=std::max(a.x,b.x),y=std::max(a.y,b.y);return {x,y,std::max(0.f,std::min(a.x+a.width,b.x+b.width)-x),std::max(0.f,std::min(a.y+a.height,b.y+b.height)-y)};}
inline ClipRect originalClip(const Configuration& config,unsigned group,const DisplayLayout& layout){
    const auto& c=layout.configuration();const auto a=(config.customized()||(c.enabled&&c.compact))?placementArea(layout):(c.enabled?c.content():ClipRect{0,0,768,1024});
    const auto t=transform(config,group,layout);const auto left=t.map({0,0});
    // Sliding gauges were clipped by the original screen edge. Move that edge
    // together with the group so a drag cannot expose its hidden source pixels.
    return intersect(a,{left.x,a.y,768*t.scale,a.height});
}
inline ClipRect backgroundClip(const Configuration& config,unsigned group,const DisplayLayout& layout){
    const auto t=transform(config,group,layout);const auto b=backgroundSlices.at(group);const auto p=t.map({b.x,b.y});
    return intersect(originalClip(config,group,layout),{p.x,p.y,b.width*t.scale,b.height*t.scale});
}
inline Placement draggedOrigin(unsigned group,const DisplayLayout& layout,Vec2 origin){
    const auto a=placementArea(layout),b=bounds.at(group);const auto t=layout.ui(DisplayRegion::bottom);
    auto axis=[](float p,float start,float travel){return travel>0?std::clamp((p-start)/travel,0.f,1.f):.5f;};
    return {axis(origin.x,a.x,a.width-b.width*t.scale),axis(origin.y,a.y,a.height-b.height*t.scale)};
}
inline Configuration upgrade(Configuration config,const DisplayLayout& layout){
    if(!config.legacySafeArea)return config;
    Configuration result;
    for(unsigned i=0;i<count;++i)if(config.positions[i]){const auto t=transform(config,i,layout);const auto b=bounds[i];result.positions[i]=draggedOrigin(i,layout,t.map({b.x,b.y}));}
    return result;
}
inline Bytes encode(const Configuration& config){
    std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(9)<<"{\"version\":"<<(config.legacySafeArea?1:2)<<",\"positions\":[";
    for(unsigned i=0;i<count;++i){if(i)s<<',';if(!config.positions[i])s<<"null";else{const auto p=*config.positions[i];if(!std::isfinite(p.x)||!std::isfinite(p.y)||p.x<0||p.x>1||p.y<0||p.y>1)throw std::invalid_argument("Invalid HUD placement");s<<'['<<p.x<<','<<p.y<<']';}}
    s<<"]}\n";const auto text=s.str();return {text.begin(),text.end()};
}
inline Configuration decode(std::span<const std::uint8_t> bytes){
    if(bytes.size()>2048)throw std::invalid_argument("HUD settings too large");
    std::istringstream s(std::string(bytes.begin(),bytes.end()));s.imbue(std::locale::classic());
    auto literal=[&](std::string_view expected){bool quoted=false;for(std::size_t i=0;i<expected.size();++i){const char c=expected[i];if(!quoted&&(i==0||c=='{'||c=='}'||c=='['||c==']'||c==','||c==':'||c=='"'||expected[i-1]==':'))s>>std::ws;if(s.get()!=c)throw std::invalid_argument("Invalid HUD settings");if(c=='"')quoted=!quoted;}};
    literal("{\"version\":");int version=0;if(!(s>>version)||(version!=1&&version!=2))throw std::invalid_argument("Unsupported HUD settings version");literal(",\"positions\":[");Configuration result;result.legacySafeArea=version==1;
    for(unsigned i=0;i<count;++i){if(i)literal(",");s>>std::ws;if(s.peek()=='n')literal("null");else{literal("[");Placement p;if(!(s>>p.x))throw std::invalid_argument("Invalid HUD coordinate");literal(",");if(!(s>>p.y))throw std::invalid_argument("Invalid HUD coordinate");literal("]");if(!std::isfinite(p.x)||!std::isfinite(p.y)||p.x<0||p.x>1||p.y<0||p.y>1)throw std::invalid_argument("Invalid HUD coordinate");result.positions[i]=p;}}
    literal("]}");s>>std::ws;if(s.peek()!=std::char_traits<char>::eof())throw std::invalid_argument("Trailing HUD settings");return result;
}
}

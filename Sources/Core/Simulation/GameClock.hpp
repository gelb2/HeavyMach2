#pragma once
#include <bit>
#include <cstdint>
namespace hm2 {
class GameClock {
    double world_=0;
    float render_=0;
    std::uint64_t frames_=0,ticks_=0;
public:
    explicit GameClock(float initialRenderTime=0,double initialWorldTime=0):world_(initialWorldTime),render_(initialRenderTime){}
    static constexpr float increment=std::bit_cast<float>(0x3c87fcb9u);
    bool step(bool worldPaused=false,bool appActive=true){
        if(!appActive)return false;
        render_+=increment;++frames_;
        if(!worldPaused){world_+=double(increment);++ticks_;}
        return true;
    }
    [[nodiscard]] double worldTime()const{return world_;}
    [[nodiscard]] float renderTime()const{return render_;}
    [[nodiscard]] std::uint64_t frames()const{return frames_;}
    [[nodiscard]] std::uint64_t ticks()const{return ticks_;}
};
}

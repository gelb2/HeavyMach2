#pragma once
#include "Core/Simulation/WorldRandom.hpp"
#include "Core/Data/DarwinRandom.hpp"
namespace hm2 {
class DarwinWorldRandom final:public WorldRandom {
    DarwinRandom generator_;
    std::uint64_t calls_=0,seeds_=0;
public:
    explicit DarwinWorldRandom(std::uint32_t seed=1):generator_(seed){}
    std::uint32_t random()override{++calls_;return generator_.next();}
    void reseed(std::uint32_t seed)override{generator_=DarwinRandom(seed);++seeds_;}
    [[nodiscard]] std::uint64_t calls()const{return calls_;}
    [[nodiscard]] std::uint64_t reseeds()const{return seeds_;}
};
}

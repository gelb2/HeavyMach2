#pragma once
#include <cstdint>
#include <stdexcept>
namespace hm2 {
// One owning game instance supplies the stream to every game consumer. Data
// decoding, save reseeding and normal startup ownership remain explicit callers.
struct WorldRandom {
    virtual ~WorldRandom()=default;
    virtual std::uint32_t random()=0;
    virtual void reseed(std::uint32_t){throw std::runtime_error("World RNG does not support original save reseeding");}
};
inline float randomUnit(WorldRandom& source){return float(source.random())*0x1p-31f;}
}

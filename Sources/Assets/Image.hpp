#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace hm2 {
struct DecodedImage {
    unsigned width=0,height=0,sourceChannels=0;
    // Top row first, premultiplied RGBA. Same stb decode/options as HM1/MD.
    std::vector<std::uint8_t> rgba;
};
DecodedImage decodeImage(std::span<const std::uint8_t> encoded);
}

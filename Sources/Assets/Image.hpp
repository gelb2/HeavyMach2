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
struct PreparedAlphaImage {
    unsigned width=0,height=0,storageWidth=0,storageHeight=0;
    std::vector<std::uint8_t> alpha;
};
// HD initAlphaWithImage0x43118: clear POT, copy each source A >> 1.
// Current original part PNGs are <=1024. Do not improvise CoreGraphics'
// oversized-image resampling for other inputs.
PreparedAlphaImage prepareOriginalPartAlpha(const DecodedImage&);
}

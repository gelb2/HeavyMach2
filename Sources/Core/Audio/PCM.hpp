#pragma once
#include <cstdint>
#include <span>
#include <vector>
namespace hm2::audio {
// PCM value type adapted from MachDefense 0aa03aa7, Sources/Audio/PCM.hpp.
// MIT licence: ThirdParty/MachDefense/LICENSE.
// Original AudioFile payload. No resampling, channel folding or 8->16 conversion.
struct PCM {
    std::vector<std::uint8_t> bytes;
    unsigned sampleRate=0,channels=0,bitsPerSample=0;
};
// RIFF PCM reader for the shared core/tests. Preserves payload bytes and sample
// format. Apple also verifies its AudioFile decoder against this reader.
PCM decodeWave(std::span<const std::uint8_t>);
}

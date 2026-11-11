#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace hm2::audio {
struct MusicBufferPolicy {std::uint32_t bytes=0,packets=0,buffers=0;bool wholeFile=false;};
// HD1.0.3 0x3ff8c/0x41a20. This is not a new streaming policy:
// half-second byte estimate truncates, then clamps at 16..64KiB (unless one
// packet itself is larger). A file smaller than three buffers uses one buffer.
inline MusicBufferPolicy musicBufferPolicy(double sampleRate,std::uint32_t framesPerPacket,
    std::uint32_t maxPacket,std::uint64_t fileBytes,std::uint64_t filePackets){
    if(!(sampleRate>0&&sampleRate<=192000)||!maxPacket||maxPacket>1024*1024||!fileBytes||!filePackets)
        throw std::runtime_error("Invalid music packet metadata");
    const double estimate=framesPerPacket?maxPacket*(sampleRate/framesPerPacket)*.5:std::max(maxPacket,65536u);
    if(estimate>std::numeric_limits<std::uint32_t>::max())throw std::runtime_error("Music packet estimate overflow");
    auto bytes=std::uint32_t(estimate);
    if(bytes>65536&&bytes>maxPacket)bytes=65536;
    else if(bytes<16384)bytes=16384;
    if(fileBytes<=std::numeric_limits<std::uint32_t>::max()&&fileBytes<std::uint64_t(bytes)*3){
        if(filePackets>std::numeric_limits<std::uint32_t>::max())throw std::runtime_error("Music packet count overflow");
        return {std::uint32_t(fileBytes),std::uint32_t(filePackets),1,true};
    }
    if(!bytes||bytes/maxPacket==0)throw std::runtime_error("Music packet exceeds buffer");
    return {bytes,bytes/maxPacket,3,false};
}
}

#include "Core/Audio/PCM.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace hm2::audio {
namespace {
unsigned u16(std::span<const std::uint8_t> b,std::size_t p){return unsigned(b[p])|(unsigned(b[p+1])<<8);}
std::uint32_t u32(std::span<const std::uint8_t> b,std::size_t p){return u16(b,p)|(std::uint32_t(u16(b,p+2))<<16);}
bool tag(std::span<const std::uint8_t> b,std::size_t p,const char* t){return std::memcmp(b.data()+p,t,4)==0;}
}
PCM decodeWave(std::span<const std::uint8_t> bytes){
    if(bytes.size()<12||!tag(bytes,0,"RIFF")||!tag(bytes,8,"WAVE"))throw std::runtime_error("Expected original RIFF/WAVE PCM");
    const std::uint64_t end=std::uint64_t(u32(bytes,4))+8;
    if(end>bytes.size()||end<12)throw std::runtime_error("Truncated RIFF payload");
    PCM pcm;bool haveFormat=false,haveData=false;std::size_t data=0,length=0;unsigned align=0;
    for(std::size_t p=12;p<end;){
        if(end-p<8)throw std::runtime_error("Truncated WAVE chunk header");
        const auto size=u32(bytes,p+4);const auto start=p+8;
        if(size>end-start)throw std::runtime_error("Truncated WAVE chunk payload");
        if(tag(bytes,p,"fmt ")){
            if(haveFormat||size<16||u16(bytes,start)!=1)throw std::runtime_error("Unsupported original PCM format");
            pcm.channels=u16(bytes,start+2);pcm.sampleRate=u32(bytes,start+4);align=u16(bytes,start+12);pcm.bitsPerSample=u16(bytes,start+14);
            if(pcm.channels<1||pcm.channels>2||(pcm.bitsPerSample!=8&&pcm.bitsPerSample!=16)||pcm.sampleRate<1||pcm.sampleRate>192000||align!=pcm.channels*(pcm.bitsPerSample/8)||u32(bytes,start+8)!=pcm.sampleRate*align)
                throw std::runtime_error("Inconsistent original PCM format");
            haveFormat=true;
        }else if(tag(bytes,p,"data")){
            if(haveData)throw std::runtime_error("Ambiguous WAVE data chunks");
            data=start;length=size;haveData=true;
        }
        const std::uint64_t next=std::uint64_t(start)+size+(size&1);
        if(next>end)throw std::runtime_error("Missing odd WAVE chunk padding");
        p=std::size_t(next);
    }
    if(!haveFormat||!haveData||!length||length>64*1024*1024||length%align)throw std::runtime_error("Invalid original PCM samples");
    pcm.bytes.assign(bytes.begin()+data,bytes.begin()+data+length);return pcm;
}
}

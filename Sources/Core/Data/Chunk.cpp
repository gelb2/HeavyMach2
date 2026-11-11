#include "Core/Data/Chunk.hpp"
#include <bit>
#include <limits>
#include <stdexcept>

namespace hm2 {
namespace {
std::uint32_t word(std::span<const std::uint8_t> bytes,std::size_t offset) {
    if(offset>bytes.size() || bytes.size()-offset<4)throw std::runtime_error("Truncated QChunk word");
    return std::uint32_t(bytes[offset]) | std::uint32_t(bytes[offset+1])<<8 |
           std::uint32_t(bytes[offset+2])<<16 | std::uint32_t(bytes[offset+3])<<24;
}
Chunk read(std::span<const std::uint8_t> bytes,std::size_t& pos,unsigned depth) {
    if(depth>128)throw std::runtime_error("QChunk nesting exceeds 128");
    if(pos>bytes.size() || bytes.size()-pos<16)throw std::runtime_error("Truncated QChunk header");
    Chunk c;c.sourceOffset=pos;c.declaredSize=word(bytes,pos);c.type=word(bytes,pos+4);
    const auto length=word(bytes,pos+8);pos+=12;
    if(length>bytes.size()-pos || bytes.size()-pos-length<4)throw std::runtime_error("Truncated QChunk payload");
    c.data.assign(bytes.begin()+pos,bytes.begin()+pos+length);pos+=length;
    const auto count=word(bytes,pos);pos+=4;
    if(count>(bytes.size()-pos)/16)throw std::runtime_error("Truncated QChunk children");
    c.children.reserve(count);
    for(std::uint32_t i=0;i<count;++i)c.children.push_back(read(bytes,pos,depth+1));
    c.consumedSize=pos-c.sourceOffset;
    // The original traverses childCount; declaredSize is not a seek offset.
    return c;
}
void appendWord(std::vector<std::uint8_t>& out,std::uint32_t value) {
    for(unsigned shift=0;shift<32;shift+=8)out.push_back(std::uint8_t(value>>shift));
}
void append(const Chunk& c,std::vector<std::uint8_t>& out,unsigned depth) {
    if(depth>128 || c.data.size()>std::numeric_limits<std::uint32_t>::max() ||
       c.children.size()>std::numeric_limits<std::uint32_t>::max())throw std::runtime_error("QChunk exceeds format limits");
    appendWord(out,c.declaredSize);appendWord(out,c.type);appendWord(out,std::uint32_t(c.data.size()));
    out.insert(out.end(),c.data.begin(),c.data.end());appendWord(out,std::uint32_t(c.children.size()));
    for(const auto& child:c.children)append(child,out,depth+1);
}
}
Chunk Chunk::parse(std::span<const std::uint8_t> bytes) {
    std::size_t pos=0;auto root=read(bytes,pos,0);
    if(pos!=bytes.size())throw std::runtime_error("Trailing bytes after QChunk root");
    return root;
}
std::vector<std::uint8_t> Chunk::encode() const {std::vector<std::uint8_t> out;append(*this,out,0);return out;}
std::string Chunk::text(std::size_t offset,std::size_t capacity) const {
    if(offset>data.size() || capacity>data.size()-offset)throw std::runtime_error("QChunk string outside data");
    if(capacity==0)return {};
    std::size_t n=0;while(n<capacity&&data[offset+n])++n;
    return {reinterpret_cast<const char*>(data.data()+offset),n};
}
std::uint8_t Chunk::byte(std::size_t offset)const{return data.at(offset);}
std::uint32_t Chunk::uint32(std::size_t offset)const{return word(data,offset);}
float Chunk::real32(std::size_t offset)const{return std::bit_cast<float>(uint32(offset));}
}

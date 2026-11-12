#include "Core/Data/SaveData.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>
namespace hm2 {
namespace {
std::uint32_t word(std::span<const std::uint8_t> b,std::size_t p){return std::uint32_t(b[p])|(std::uint32_t(b[p+1])<<8)|(std::uint32_t(b[p+2])<<16)|(std::uint32_t(b[p+3])<<24);}
void word(std::span<std::uint8_t> b,std::size_t p,std::uint32_t v){for(unsigned i=0;i<4;++i)b[p+i]=std::uint8_t(v>>(i*8));}
void text(std::span<std::uint8_t> b,std::size_t p,std::string_view value){std::copy(value.begin(),value.end(),b.begin()+p);b[p+value.size()]=0;}
}
SaveData SaveData::decode(std::span<const std::uint8_t> encoded,WorldRandom& random,bool countOpen){
    // The original accepts arbitrary NSData length and can overrun its fixed
    // struct. Reject malformed length without changing the retained user data.
    if(encoded.size()!=byteCount)throw std::runtime_error("Expected original HD save length 28204");
    SaveData result;std::copy(encoded.begin(),encoded.end(),result.bytes_.begin());
    random.reseed(word(encoded,0));std::int32_t checksum=0;
    for(std::size_t i=4;i<byteCount-4;++i){
        auto& byte=result.bytes_[i];byte^=std::uint8_t(random.random());checksum+=std::bit_cast<std::int8_t>(byte);
    }
    if(word(encoded,byteCount-4)!=std::uint32_t(checksum))throw std::runtime_error("Original HD save checksum mismatch");
    if(countOpen){auto n=word(result.bytes_,4);if(std::bit_cast<std::int32_t>(n)<0)n=0;word(result.bytes_,4,n+1u);}
    return result;
}
Bytes SaveData::encode(WorldRandom& random,std::uint32_t absoluteTimeLow){
    // GlobalInfo::saveDataFile0x17580 reseeds the SAME global game stream.
    random.reseed(absoluteTimeLow);const auto seed=random.random()%0xffffu;word(bytes_,0,seed);
    Bytes encoded(bytes_.begin(),bytes_.end());random.reseed(seed);std::int32_t checksum=0;
    for(std::size_t i=4;i<byteCount-4;++i){checksum+=std::bit_cast<std::int8_t>(bytes_[i]);encoded[i]^=std::uint8_t(random.random());}
    word(encoded,byteCount-4,std::uint32_t(checksum));return encoded;
}
std::span<std::uint8_t> SaveData::slot(unsigned index){if(index>=slotCount)throw std::out_of_range("Original HD slot index");return std::span(bytes_).subspan(slotOffset+index*slotSize,slotSize);}
std::span<const std::uint8_t> SaveData::slot(unsigned index)const{if(index>=slotCount)throw std::out_of_range("Original HD slot index");return std::span(bytes_).subspan(slotOffset+index*slotSize,slotSize);}
void SaveData::initializeSlot(unsigned index,std::string_view name,std::string_view country,std::uint32_t unixTime){
    if(name.size()>31||country.size()>3||name.find('\0')!=name.npos||country.find('\0')!=country.npos)throw std::runtime_error("Original slot name/country exceeds its field");
    auto b=slot(index);std::fill(b.begin(),b.end(),0);word(b,0,unixTime);b[8]=1;text(b,9,name);text(b,0x29,country);text(b,0x2d,"Base001");
    word(b,0x80,256);word(b,0x7c,256);word(b,0x88,3);word(b,0x78,500);
    if(name=="GimmeCr"){word(b,0x78,100000000);word(b,0x70,100000000);word(b,0x6c,50);}
    constexpr std::string_view items[]={"11001","12001","10011","10111","04201","01001","02004","02005"};
    for(unsigned i=0;i<std::size(items);++i){text(b,0x8c+i*6,items[i]);b[0x53c+i]=i<5?1:5;}
    for(unsigned i=0;i<4;++i)word(b,0x604+i*4,i);
    text(b,0x61c,"01001");text(b,0x622,"02004");text(b,0x628,"02005");text(b,0x63a,"04201");
}
}

// Offline comparison only. Uses the game's existing decoder and writes metadata.
#include "Assets/Image.hpp"
#include <CommonCrypto/CommonDigest.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

int main() {
    for (;;) {
        std::array<unsigned char,8> header{};
        std::cin.read(reinterpret_cast<char*>(header.data()),header.size());
        if (std::cin.gcount()==0 && std::cin.eof()) return 0;
        if (std::cin.gcount()!=8) return 2;
        std::uint64_t length=0;
        for(unsigned i=0;i<8;++i) length|=std::uint64_t(header[i])<<(8*i);
        if (!length || length>128u*1024u*1024u) return 2;
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
        if (!std::cin.read(reinterpret_cast<char*>(bytes.data()),bytes.size())) return 2;
        try {
            const auto image=hm2::decodeImage(bytes);
            if(image.rgba.size()>std::numeric_limits<CC_LONG>::max()) return 2;
            std::array<unsigned char,CC_SHA256_DIGEST_LENGTH> digest{};
            CC_SHA256(image.rgba.data(),static_cast<CC_LONG>(image.rgba.size()),digest.data());
            constexpr char hex[]="0123456789abcdef";
            std::array<char,CC_SHA256_DIGEST_LENGTH*2+1> encoded{};
            for(std::size_t i=0;i<digest.size();++i){encoded[i*2]=hex[digest[i]>>4];encoded[i*2+1]=hex[digest[i]&15];}
            unsigned minX=image.width,minY=image.height,maxX=0,maxY=0;
            std::uint64_t occupied=0,partial=0;
            for(unsigned y=0;y<image.height;++y)for(unsigned x=0;x<image.width;++x){
                const auto alpha=image.rgba[(std::size_t(y)*image.width+x)*4+3];
                if(!alpha)continue;
                ++occupied;if(alpha<255)++partial;
                minX=std::min(minX,x);minY=std::min(minY,y);
                maxX=std::max(maxX,x);maxY=std::max(maxY,y);
            }
            std::cout<<"{\"ok\":true,\"width\":"<<image.width<<",\"height\":"<<image.height
                     <<",\"source_channels\":"<<image.sourceChannels<<",\"premultiplied_rgba_sha256\":\""<<encoded.data()
                     <<"\",\"alpha_nonzero_pixels\":"<<occupied<<",\"alpha_partial_pixels\":"<<partial<<",\"alpha_bounds_xyxy\":";
            if(occupied)std::cout<<'['<<minX<<','<<minY<<','<<maxX+1<<','<<maxY+1<<']';else std::cout<<"null";
            std::cout<<'}'<<std::endl;
        }catch(const std::exception&){std::cout<<"{\"ok\":false,\"error\":\"shared_decoder_rejected_input\"}"<<std::endl;}
    }
}

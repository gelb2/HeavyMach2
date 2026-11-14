#include "Assets/Image.hpp"
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace hm2 {
PreparedAlphaImage prepareOriginalPartAlpha(const DecodedImage& image){
    if(!image.width||!image.height||image.width>1024||image.height>1024||
       image.rgba.size()!=std::size_t(image.width)*image.height*4)
        throw std::runtime_error("Unsupported original part alpha image dimensions");
    PreparedAlphaImage out{image.width,image.height,1,1,{}};
    while(out.storageWidth<out.width)out.storageWidth*=2;
    while(out.storageHeight<out.height)out.storageHeight*=2;
    out.alpha.resize(std::size_t(out.storageWidth)*out.storageHeight,0);
    for(unsigned y=0;y<out.height;++y)for(unsigned x=0;x<out.width;++x)
        out.alpha[std::size_t(y)*out.storageWidth+x]=image.rgba[(std::size_t(y)*out.width+x)*4+3]>>1;
    return out;
}
EncodedImageInfo imageInfo(std::span<const std::uint8_t> encoded){
    if(encoded.empty()||encoded.size()>std::size_t(std::numeric_limits<int>::max()))throw std::runtime_error("Invalid encoded image size");
    int width=0,height=0,channels=0;
    if(!stbi_info_from_memory(encoded.data(),int(encoded.size()),&width,&height,&channels)||width<1||height<1||width>16384||height>16384)
        throw std::runtime_error("Invalid original image header");
    return {unsigned(width),unsigned(height),unsigned(channels)};
}
DecodedImage decodeImage(std::span<const std::uint8_t> encoded) {
    const auto info=imageInfo(encoded);const int bytes=int(encoded.size());int width=int(info.width),height=int(info.height),channels=int(info.sourceChannels);
    stbi_convert_iphone_png_to_rgb_thread(1);stbi_set_unpremultiply_on_load_thread(1);
    std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> pixels(stbi_load_from_memory(encoded.data(),bytes,&width,&height,&channels,4),stbi_image_free);
    if(!pixels)throw std::runtime_error(std::string("Cannot decode original image: ")+stbi_failure_reason());
    DecodedImage result{unsigned(width),unsigned(height),unsigned(channels),{}};
    result.rgba.assign(pixels.get(),pixels.get()+std::size_t(width)*height*4);
    // Existing HM1/MD image preparation: preserve float multiply/truncation.
    for(std::size_t i=0;i<result.rgba.size();i+=4){const float a=float(result.rgba[i+3])/255.f;
        for(int c=0;c<3;++c)result.rgba[i+c]=static_cast<std::uint8_t>(float(result.rgba[i+c])*a);}
    return result;
}
}

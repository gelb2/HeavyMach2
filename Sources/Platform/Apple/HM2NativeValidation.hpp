#pragma once
#import <UIKit/UIKit.h>
#include "Platform/Apple/HM2GameRuntime.hpp"
#include <algorithm>

namespace hm2::apple {
// Validation-only Apple entry point. Both suites drive the shared runtime.
class NativeValidationScenario {
public:
    virtual ~NativeValidationScenario()=default;
    virtual bool step(CAEAGLLayer*)=0;
    virtual void setDisplay(DisplayConfiguration)=0;
    virtual void setActive(bool)=0;
    virtual void setAudioActive(bool)=0;
};
inline void captureValidationFrame(GameRuntime& runtime,CAEAGLLayer* layer,NSURL* output){
    const auto require=[](bool condition,const char* message){
        if(!condition)throw std::runtime_error(message);
    };
    runtime.captureNextFrame();
    runtime.draw(layer);
    NSData* rgba=runtime.pixels();
    const CGSize size=runtime.drawableSize();
    const auto width=std::size_t(size.width),height=std::size_t(size.height),stride=width*4;
    require(width&&height&&rgba.length==height*stride,"No White Mountain GPU framebuffer");
    NSMutableData* topDown=[NSMutableData dataWithLength:rgba.length];
    const auto* begin=static_cast<const std::uint8_t*>(rgba.bytes);
    auto* end=static_cast<std::uint8_t*>(topDown.mutableBytes);
    for(std::size_t y=0;y<height;++y)std::copy_n(begin+(height-1-y)*stride,stride,end+y*stride);
    CGDataProviderRef provider=CGDataProviderCreateWithCFData((__bridge CFDataRef)topDown);
    CGColorSpaceRef colors=CGColorSpaceCreateDeviceRGB();
    CGImageRef image=CGImageCreate(width,height,8,32,stride,colors,kCGImageAlphaNoneSkipLast|kCGBitmapByteOrderDefault,provider,nullptr,false,kCGRenderingIntentDefault);
    NSData* png=UIImagePNGRepresentation([UIImage imageWithCGImage:image]);
    CGImageRelease(image);CGColorSpaceRelease(colors);CGDataProviderRelease(provider);
    require(png&&[png writeToURL:output atomically:YES],
            "Cannot export White Mountain GPU capture");
}
}

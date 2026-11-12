// Offline Apple icon packaging only. The game never links this utility.
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>
#import <ImageIO/ImageIO.h>
#include <cstdio>
#include <cstring>
#include <exception>
#include "Assets/Image.hpp"

int main(int argc,char** argv){@autoreleasepool{
    if(argc!=4||(std::strcmp(argv[3],"normal")&&std::strcmp(argv[3],"validation"))){
        std::fprintf(stderr,"usage: render_app_icon input output normal|validation\n");return 2;
    }
    NSData* input=[NSData dataWithContentsOfFile:[NSString stringWithUTF8String:argv[1]]];
    if(!input){std::fprintf(stderr,"Cannot open icon artwork\n");return 1;}
    hm2::DecodedImage decoded;
    try{
        decoded=hm2::decodeImage({static_cast<const std::uint8_t*>(input.bytes),input.length});
    }catch(const std::exception& error){
        std::fprintf(stderr,"Cannot decode title illustration: %s\n",error.what());return 1;
    }
    if(decoded.width!=768||decoded.height!=1024){
        std::fprintf(stderr,"Expected original 768x1024 Main_BG.png\n");return 1;
    }
    CGColorSpaceRef space=CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGDataProviderRef provider=CGDataProviderCreateWithData(nullptr,decoded.rgba.data(),decoded.rgba.size(),nullptr);
    CGImageRef original=CGImageCreate(decoded.width,decoded.height,8,32,decoded.width*4,space,
        CGBitmapInfo(kCGImageAlphaPremultipliedLast)|CGBitmapInfo(kCGBitmapByteOrder32Big),
        provider,nullptr,false,kCGRenderingIntentDefault);
    CGDataProviderRelease(provider);
    // Crop the portrait title illustration without stretching or repainting it.
    CGImageRef image=original?CGImageCreateWithImageInRect(original,CGRectMake(0,128,768,768)):nullptr;
    if(original)CGImageRelease(original);
    if(!image){CGColorSpaceRelease(space);std::fprintf(stderr,"Cannot crop title illustration\n");return 1;}
    const CGBitmapInfo format=CGBitmapInfo(kCGImageAlphaNoneSkipLast)|CGBitmapInfo(kCGBitmapByteOrder32Big);
    CGContextRef context=CGBitmapContextCreate(nullptr,1024,1024,8,1024*4,space,format);
    CGColorSpaceRelease(space);
    if(!context){CGImageRelease(image);std::fprintf(stderr,"Cannot allocate icon bitmap\n");return 1;}
    CGContextSetRGBFillColor(context,0,0,0,1);
    CGContextFillRect(context,CGRectMake(0,0,1024,1024));
    CGContextSetInterpolationQuality(context,kCGInterpolationHigh);
    CGContextDrawImage(context,CGRectMake(0,0,1024,1024),image);CGImageRelease(image);
    if(!std::strcmp(argv[3],"validation")){
        // Only the cheat build gets a label. In-game artwork stays unchanged.
        CGContextSetRGBFillColor(context,0.09,0.10,0.12,1);
        CGContextFillRect(context,CGRectMake(0,0,1024,160));
        CGContextSetRGBFillColor(context,0.94,0.65,0.12,1);
        CGContextFillRect(context,CGRectMake(0,160,1024,8));
        CTFontRef font=CTFontCreateWithName(CFSTR("HelveticaNeue-Bold"),112,nullptr);
        CGColorRef white=CGColorCreateGenericRGB(1,1,1,1);
        NSDictionary* attributes=@{(__bridge id)kCTFontAttributeName:(__bridge id)font,
                                   (__bridge id)kCTForegroundColorAttributeName:(__bridge id)white};
        NSAttributedString* text=[[NSAttributedString alloc]initWithString:@"TEST" attributes:attributes];
        CTLineRef line=CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)text);
        CGRect bounds=CTLineGetBoundsWithOptions(line,kCTLineBoundsUseGlyphPathBounds);
        CGContextSetTextPosition(context,(1024-bounds.size.width)/2-bounds.origin.x,
                                 (160-bounds.size.height)/2-bounds.origin.y);
        CTLineDraw(line,context);CFRelease(line);CGColorRelease(white);CFRelease(font);
    }
    CGImageRef result=CGBitmapContextCreateImage(context);CGContextRelease(context);
    NSURL* output=[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[2]]];
    CGImageDestinationRef destination=CGImageDestinationCreateWithURL((__bridge CFURLRef)output,CFSTR("public.png"),1,nullptr);
    if(!destination){CGImageRelease(result);std::fprintf(stderr,"Cannot create output PNG\n");return 1;}
    CGImageDestinationAddImage(destination,result,nullptr);CGImageRelease(result);
    const bool saved=CGImageDestinationFinalize(destination);CFRelease(destination);
    return saved?0:1;
}}

#include "Platform/Apple/HM2TextRasterizer.h"
#import <UIKit/UIKit.h>
#import <CoreText/CoreText.h>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace hm2::apple {
namespace {
std::string utf8(NSString* string){return string?std::string(string.UTF8String):std::string();}
NSString* originalString(const std::string& value){
    NSString* result=[[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];
    if(!result)throw std::runtime_error("Original text is not valid UTF-8");
    return result;
}
struct ContextRelease {void operator()(CGContextRef c)const{if(c)CGContextRelease(c);}};
struct ColorSpaceRelease {void operator()(CGColorSpaceRef c)const{if(c)CGColorSpaceRelease(c);}};
class AppleTextRasterizer final:public ui::TextRasterizer {
public:
    ui::TextBitmap rasterize(const ui::TextRasterRequest& request)override{
        @autoreleasepool {
            ui::TextBitmap result;result.request=request;result.layout=ui::textRasterLayout(request);
            NSString* text=originalString(request.text);
            NSString* name=originalString(request.fontName);
            UIFont* font=[UIFont fontWithName:name size:result.layout.fontSize];
            if(!font)throw std::runtime_error("Original UIFont is unavailable: "+request.fontName);
            result.metrics.resolvedFontName=utf8(font.fontName);
            result.metrics.osVersion=utf8(NSProcessInfo.processInfo.operatingSystemVersionString);
            result.metrics.ascender=float(font.ascender);result.metrics.descender=float(font.descender);
            result.metrics.leading=float(font.leading);result.metrics.lineHeight=float(font.lineHeight);
            result.alpha.resize(std::size_t(result.layout.width)*result.layout.height,0);
            std::unique_ptr<CGColorSpace,ColorSpaceRelease> gray(CGColorSpaceCreateDeviceGray());
            if(!gray)throw std::runtime_error("Cannot create original DeviceGray text color space");
            std::unique_ptr<CGContext,ContextRelease> context(CGBitmapContextCreate(result.alpha.data(),
                result.layout.width,result.layout.height,8,result.layout.width,gray.get(),kCGImageAlphaNone));
            if(!context)throw std::runtime_error("Cannot create original A8 text bitmap");
            CGContextSetGrayFillColor(context.get(),1,1);
            CGContextTranslateCTM(context.get(),0,result.layout.height);
            CGContextScaleCTM(context.get(),1,-1);
            UIGraphicsPushContext(context.get());
            // Original Texture2D(Text) 0x42fa4. Keep the deprecated NSString
            // path instead of replacing its wrapping/metrics with drawWithRect:.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            const CGSize drawn=[text drawInRect:CGRectMake(0,0,result.layout.content.width,result.layout.content.height)
                withFont:font lineBreakMode:static_cast<NSLineBreakMode>(0)
                alignment:static_cast<NSTextAlignment>(request.alignment)];
#pragma clang diagnostic pop
            UIGraphicsPopContext();
            result.metrics.drawnSize={float(drawn.width),float(drawn.height)};
            // Diagnose the current OS cascade without selecting a substitute
            // ourselves. Actual pixels above still come from the original API.
            CTFontRef base=CTFontCreateWithName((__bridge CFStringRef)font.fontName,font.pointSize,nullptr);
            if(base){
                for(NSUInteger i=0;i<text.length;){
                        const NSRange range=[text rangeOfComposedCharacterSequenceAtIndex:i];
                        CTFontRef selected=CTFontCreateForString(base,(__bridge CFStringRef)text,
                            CFRangeMake(CFIndex(range.location),CFIndex(range.length)));
                        if(selected){
                            NSString* selectedName=CFBridgingRelease(CTFontCopyPostScriptName(selected));
                            const auto selectedUTF8=utf8(selectedName);
                            if(std::find(result.metrics.cascadeFontNames.begin(),result.metrics.cascadeFontNames.end(),selectedUTF8)
                               ==result.metrics.cascadeFontNames.end())result.metrics.cascadeFontNames.push_back(selectedUTF8);
                            CFRelease(selected);
                        }
                        i=NSMaxRange(range);
                }
                CFRelease(base);
            }
            ui::measureTextInk(result);
            return result;
        }
    }
};
}
std::unique_ptr<ui::TextRasterizer> makeTextRasterizer(){return std::make_unique<AppleTextRasterizer>();}
}

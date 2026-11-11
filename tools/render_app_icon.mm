// Offline Apple icon packaging only. The game never links this utility.
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>
#import <ImageIO/ImageIO.h>
#include <cstdio>
#include <cstring>

int main(int argc,char** argv){@autoreleasepool{
    if(argc!=4||(std::strcmp(argv[3],"normal")&&std::strcmp(argv[3],"validation"))){
        std::fprintf(stderr,"usage: render_app_icon input output normal|validation\n");return 2;
    }
    NSURL* input=[NSURL fileURLWithPath:[NSString stringWithUTF8String:argv[1]]];
    CGImageSourceRef source=CGImageSourceCreateWithURL((__bridge CFURLRef)input,nullptr);
    if(!source){std::fprintf(stderr,"Cannot open icon artwork\n");return 1;}
    CGImageRef image=CGImageSourceCreateImageAtIndex(source,0,nullptr);CFRelease(source);
    if(!image||CGImageGetWidth(image)!=512||CGImageGetHeight(image)!=512){
        if(image)CGImageRelease(image);std::fprintf(stderr,"Expected original 512x512 artwork\n");return 1;
    }
    CGColorSpaceRef space=CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    const CGBitmapInfo format=CGBitmapInfo(kCGImageAlphaNoneSkipLast)|CGBitmapInfo(kCGBitmapByteOrder32Big);
    CGContextRef context=CGBitmapContextCreate(nullptr,1024,1024,8,1024*4,space,format);
    CGColorSpaceRelease(space);
    if(!context){CGImageRelease(image);std::fprintf(stderr,"Cannot allocate icon bitmap\n");return 1;}
    CGContextSetInterpolationQuality(context,kCGInterpolationHigh);
    CGContextDrawImage(context,CGRectMake(0,0,1024,1024),image);CGImageRelease(image);
    if(!std::strcmp(argv[3],"validation")){
        // A packaging label replaces the original HD footer, without redrawing
        // the mech or modifying any in-game asset.
        CGContextSetRGBFillColor(context,0.09,0.10,0.12,1);
        CGContextFillRect(context,CGRectMake(0,0,1024,304));
        CGContextSetRGBFillColor(context,0.94,0.65,0.12,1);
        CGContextFillRect(context,CGRectMake(0,304,1024,10));
        CTFontRef font=CTFontCreateWithName(CFSTR("HelveticaNeue-Bold"),184,nullptr);
        CGColorRef white=CGColorCreateGenericRGB(1,1,1,1);
        NSDictionary* attributes=@{(__bridge id)kCTFontAttributeName:(__bridge id)font,
                                   (__bridge id)kCTForegroundColorAttributeName:(__bridge id)white};
        NSAttributedString* text=[[NSAttributedString alloc]initWithString:@"TEST" attributes:attributes];
        CTLineRef line=CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)text);
        CGRect bounds=CTLineGetBoundsWithOptions(line,kCTLineBoundsUseGlyphPathBounds);
        CGContextSetTextPosition(context,(1024-bounds.size.width)/2-bounds.origin.x,
                                 (304-bounds.size.height)/2-bounds.origin.y);
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

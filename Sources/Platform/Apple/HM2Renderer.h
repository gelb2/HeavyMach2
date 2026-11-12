#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#include "Core/Types.hpp"
#include "Core/UI/Text.hpp"

@interface HM2Renderer:NSObject
- (instancetype)initWithResourceRoot:(NSURL*)root;
- (void)prepareFrame:(const hm2::Frame&)frame;
- (void)prepareText:(const hm2::ui::TextBitmap&)bitmap identifier:(const std::string&)identifier;
- (void)drawFrame:(const hm2::Frame&)frame layer:(CAEAGLLayer*)layer;
- (BOOL)logicalPoint:(CGPoint)point result:(hm2::Vec2*)result;
- (void)captureNextFrame;
- (NSData*)readRGBA;
@property(nonatomic,readonly) NSUInteger textureUploads;
@property(nonatomic,readonly) NSUInteger imageTextureUploads;
@property(nonatomic,readonly) NSUInteger textTextureUploads;
@property(nonatomic,readonly) NSUInteger alphaTextureUploads;
@property(nonatomic,readonly) NSUInteger imageResourceReads;
@property(nonatomic,readonly) NSUInteger imageDecodeCalls;
@property(nonatomic,readonly) NSUInteger presentedFrames;
@property(nonatomic,readonly) NSUInteger drawCalls;
@property(nonatomic,readonly) NSUInteger atlasDrawCalls;
@property(nonatomic,readonly) NSUInteger contextIdentity;
@property(nonatomic,readonly) CGSize drawableSize;
@end

#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#include "Core/Types.hpp"

@interface HM2Renderer:NSObject
- (instancetype)initWithResourceRoot:(NSURL*)root;
- (void)prepareFrame:(const hm2::Frame&)frame;
- (void)drawFrame:(const hm2::Frame&)frame layer:(CAEAGLLayer*)layer;
- (BOOL)logicalPoint:(CGPoint)point result:(hm2::Vec2*)result;
- (void)captureNextFrame;
- (NSData*)readRGBA;
@property(nonatomic,readonly) NSUInteger textureUploads;
@property(nonatomic,readonly) NSUInteger presentedFrames;
@property(nonatomic,readonly) NSUInteger contextIdentity;
@property(nonatomic,readonly) CGSize drawableSize;
@end

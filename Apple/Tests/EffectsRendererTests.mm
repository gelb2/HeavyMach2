#import <XCTest/XCTest.h>
#import <UIKit/UIKit.h>
#import "Platform/Apple/HM2Renderer.h"
#include "Platform/Apple/HM2TextRasterizer.h"
#include "Assets/Image.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include <algorithm>

@interface HM2EffectsSurface:UIView @end
@implementation HM2EffectsSurface
+ (Class)layerClass{return CAEAGLLayer.class;}
@end
@interface EffectsRendererTests:XCTestCase @end
@implementation EffectsRendererTests
- (void)testOriginalAtlasBatchUsesOneIndexedDraw {
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];NSData* encoded=[NSData dataWithContentsOfURL:[root URLByAppendingPathComponent:@"Efx_Fire.png"]];XCTAssertNotNil(encoded);if(!encoded)return;
    const auto image=hm2::decodeImage({static_cast<const std::uint8_t*>(encoded.bytes),encoded.length});
    UIWindow* window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];UIViewController* controller=[UIViewController new];HM2EffectsSurface* surface=[[HM2EffectsSurface alloc]initWithFrame:window.bounds];surface.contentScaleFactor=UIScreen.mainScreen.scale;
    CAEAGLLayer* layer=(CAEAGLLayer*)surface.layer;layer.opaque=YES;layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};controller.view=surface;window.rootViewController=controller;[window makeKeyAndVisible];[surface layoutIfNeeded];
    HM2Renderer* renderer=[[HM2Renderer alloc]initWithResourceRoot:root];hm2::Frame baseline;[renderer drawFrame:baseline layer:layer];const auto extent=renderer.drawableSize;baseline.width=extent.width;baseline.height=extent.height;
    unsigned sw=1,sh=1;while(sw<image.width)sw*=2;while(sh<image.height)sh*=2;
    hm2::VisualTile tile;tile.identity="original:shared:Efx_Fire";tile.image="Efx_Fire.png";tile.content={float(image.width),float(image.height)};tile.storage={float(sw),float(sh)};tile.columns=tile.rows=4;
    hm2::VisualLayer visual;
    for(unsigned i=0;i<40;++i){
        const hm2::Vec2 p{60+float(i%8)*36,60+float(i/8)*36};const unsigned n=i%12;
        hm2::VisualImage v;v.tile=tile;v.useAtlas=true;v.tileNo=n;v.blendType=2;v.screenPosition=p;v.alpha=.5f;visual.submitImage(v);
        hm2::Sprite s;s.image=tile.image;s.center=p;s.size={float(image.width)/4,float(image.height)/4};s.tile=n;s.columns=s.rows=4;s.color.a=127.f/255.f;s.blend={hm2::BlendFactor::sourceAlpha,hm2::BlendFactor::one};baseline.sprites.push_back(s);
    }
    [renderer prepareFrame:baseline];[renderer captureNextFrame];[renderer drawFrame:baseline layer:layer];NSData* expected=renderer.readRGBA;
    auto grouped=visual.drain();XCTAssertEqual(grouped.atlases.size(),1u);XCTAssertEqual(grouped.atlases.front().capacity,64u);
    hm2::Frame batch;batch.width=baseline.width;batch.height=baseline.height;grouped.appendBatches(batch);XCTAssertEqual(batch.sprites.size(),1u);
    const auto before=renderer.drawCalls,atlasBefore=renderer.atlasDrawCalls,uploads=renderer.textureUploads;
    [renderer captureNextFrame];[renderer drawFrame:batch layer:layer];NSData* actual=renderer.readRGBA;
    XCTAssertEqual(renderer.drawCalls-before,1u);XCTAssertEqual(renderer.atlasDrawCalls-atlasBefore,1u);XCTAssertEqual(renderer.textureUploads,uploads);
    NSUInteger differences=0;const auto* a=static_cast<const unsigned char*>(expected.bytes);const auto* b=static_cast<const unsigned char*>(actual.bytes);
    XCTAssertEqual(expected.length,actual.length);if(expected.length==actual.length)for(NSUInteger i=0;i<actual.length;i+=4)for(unsigned c=0;c<3;++c)if(std::abs(int(a[i+c])-int(b[i+c]))>1)++differences;
    XCTAssertEqual(differences,0u);NSLog(@"HM2_ATLAS_GPU quads=40 indexedDraws=1 capacity=64 pixelMismatches=%lu device=%@",(unsigned long)differences,UIDevice.currentDevice.model);
    window.hidden=YES;window.rootViewController=nil;
}
- (void)testA8TextStripColorAndBlendRemainPrepared {
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    UIWindow* window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];UIViewController* controller=[UIViewController new];
    HM2EffectsSurface* surface=[[HM2EffectsSurface alloc]initWithFrame:window.bounds];surface.contentScaleFactor=UIScreen.mainScreen.scale;
    CAEAGLLayer* layer=(CAEAGLLayer*)surface.layer;layer.opaque=YES;layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};controller.view=surface;window.rootViewController=controller;[window makeKeyAndVisible];[surface layoutIfNeeded];
    HM2Renderer* renderer=[[HM2Renderer alloc]initWithResourceRoot:root];hm2::Frame frame;[renderer drawFrame:frame layer:layer];
    const auto extent=renderer.drawableSize;frame.width=extent.width;frame.height=extent.height;
    auto bitmap=std::make_shared<hm2::ui::TextBitmap>();bitmap->request={"gpu-fixture","x","Arial",{14,10},8};bitmap->layout=hm2::ui::textRasterLayout(bitmap->request);bitmap->alpha.assign(std::size_t(bitmap->layout.width)*bitmap->layout.height,128);
    const std::string fixture="@text/constant128";[renderer prepareText:*bitmap identifier:fixture];
    hm2::ui::Text text(fixture,bitmap);text.position={40,40};text.append(frame);
    [renderer captureNextFrame];[renderer drawFrame:frame layer:layer];NSData* first=renderer.readRGBA;
    auto pixel=[&](NSData* data,unsigned x,unsigned y,unsigned c){return static_cast<const unsigned char*>(data.bytes)[(std::size_t(y)*unsigned(extent.width)+x)*4+c];};
    for(unsigned c=0;c<3;++c)XCTAssertEqualWithAccuracy(pixel(first,40,40,c),128,1);
    XCTAssertEqual(pixel(first,31,40,0),0);XCTAssertEqual(pixel(first,48,40,0),0);
    frame.sprites[0].strip[0].color={.5f,.25f,1,.5f};
    for(auto& vertex:frame.sprites[0].strip)vertex.color={.5f,.25f,1,.5f};
    [renderer captureNextFrame];[renderer drawFrame:frame layer:layer];NSData* colored=renderer.readRGBA;
    XCTAssertEqualWithAccuracy(pixel(colored,40,40,0),32,1);XCTAssertEqualWithAccuracy(pixel(colored,40,40,1),16,1);XCTAssertEqualWithAccuracy(pixel(colored,40,40,2),64,1);
    // Same strip, additive destination. Drawing two instances must add actual
    // framebuffer pixels, not merely count two submitted sprites.
    frame.sprites[0].blend={hm2::BlendFactor::sourceAlpha,hm2::BlendFactor::one};frame.sprites.push_back(frame.sprites[0]);
    [renderer captureNextFrame];[renderer drawFrame:frame layer:layer];NSData* additive=renderer.readRGBA;
    XCTAssertEqualWithAccuracy(pixel(additive,40,40,0),64,2);XCTAssertEqualWithAccuracy(pixel(additive,40,40,1),32,2);XCTAssertEqualWithAccuracy(pixel(additive,40,40,2),128,2);
    auto rasterizer=hm2::apple::makeTextRasterizer();hm2::ui::TextPreparationCache cache;
    const hm2::ui::TextRasterRequest request{"gpu-font","HeavyMach 2\n0123456789","Arial",{180,60},18,hm2::ui::TextAlign::left};
    const auto prepared=cache.prepare(*rasterizer,request);XCTAssertEqual(cache.prepare(*rasterizer,request).get(),prepared.get());XCTAssertEqual(cache.size(),1u);XCTAssertFalse(prepared->metrics.inkBands.empty());
    const std::string fontKey="@text/Arial";[renderer prepareText:*prepared identifier:fontKey];
    frame.sprites.clear();hm2::ui::Text original(fontKey,prepared);original.position={160,120};original.append(frame);
    const auto uploads=renderer.textureUploads,context=renderer.contextIdentity;
    [renderer captureNextFrame];[renderer drawFrame:frame layer:layer];NSData* fontPixels=renderer.readRGBA;NSUInteger nonblack=0;
    for(NSUInteger i=0;i<fontPixels.length;i+=4)if(static_cast<const unsigned char*>(fontPixels.bytes)[i])++nonblack;
    XCTAssertGreaterThan(nonblack,100u);
    for(unsigned i=0;i<120;++i){[renderer drawFrame:frame layer:layer];XCTAssertEqual(renderer.textureUploads,uploads);XCTAssertEqual(renderer.contextIdentity,context);}
    XCTAssertEqual(surface.subviews.count,0u);
    NSLog(@"HM2_EFFECTS_GPU A8=128 stripRGB=32,16,64 additiveRGB=64,32,128 fontPixels=%lu preparedTextures=%lu frames=%lu device=%@",(unsigned long)nonblack,(unsigned long)uploads,(unsigned long)renderer.presentedFrames,UIDevice.currentDevice.model);
    window.hidden=YES;window.rootViewController=nil;
}
- (void)testOriginalFourByFourEffectTilePixels {
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];NSData* encoded=[NSData dataWithContentsOfURL:[root URLByAppendingPathComponent:@"Efx_Fire.png"]];XCTAssertNotNil(encoded);if(!encoded)return;
    const auto image=hm2::decodeImage({static_cast<const std::uint8_t*>(encoded.bytes),encoded.length});XCTAssertEqual(image.width%4,0u);XCTAssertEqual(image.height%4,0u);
    UIWindow* window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];UIViewController* controller=[UIViewController new];HM2EffectsSurface* surface=[[HM2EffectsSurface alloc]initWithFrame:window.bounds];surface.contentScaleFactor=UIScreen.mainScreen.scale;
    CAEAGLLayer* layer=(CAEAGLLayer*)surface.layer;layer.opaque=YES;layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};controller.view=surface;window.rootViewController=controller;[window makeKeyAndVisible];[surface layoutIfNeeded];
    HM2Renderer* renderer=[[HM2Renderer alloc]initWithResourceRoot:root];hm2::Frame frame;[renderer drawFrame:frame layer:layer];const auto extent=renderer.drawableSize;frame.width=extent.width;frame.height=extent.height;
    const unsigned w=image.width/4,h=image.height/4,tile=9;hm2::Sprite sprite;sprite.image="Efx_Fire.png";sprite.center={32+float(w)/2,32+float(h)/2};sprite.size={float(w),float(h)};sprite.columns=sprite.rows=4;sprite.tile=tile;frame.sprites.push_back(sprite);[renderer prepareFrame:frame];[renderer captureNextFrame];[renderer drawFrame:frame layer:layer];
    NSData* data=renderer.readRGBA;const auto* actual=static_cast<const unsigned char*>(data.bytes);NSUInteger mismatch=0;
    // Pixel centers at 1:1; skip tile boundary filtering footprint only.
    for(unsigned y=1;y+1<h;++y)for(unsigned x=1;x+1<w;++x)for(unsigned c=0;c<3;++c){const auto expected=image.rgba[((tile/4*h+h-1-y)*image.width+tile%4*w+x)*4+c];const auto found=actual[((std::size_t(y)+32)*unsigned(extent.width)+x+32)*4+c];if(std::abs(int(found)-int(expected))>1)++mismatch;}
    XCTAssertEqual(mismatch,0u);NSLog(@"HM2_EFFECT_TILE_GPU tile=9 grid=4x4 pixelMismatches=%lu device=%@",(unsigned long)mismatch,UIDevice.currentDevice.model);window.hidden=YES;window.rootViewController=nil;
}
@end

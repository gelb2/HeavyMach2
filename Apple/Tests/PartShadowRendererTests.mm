#import <XCTest/XCTest.h>
#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>
#import "Platform/Apple/HM2Renderer.h"
#include "Platform/Apple/HM2Resources.hpp"
#include "Core/Presentation/PartSprites.hpp"
#include "Assets/Image.hpp"
#include <cmath>
#include <numbers>

@interface HM2PartShadowSurface:UIView @end
@implementation HM2PartShadowSurface
+ (Class)layerClass{return CAEAGLLayer.class;}
@end

@interface PartShadowRendererTests:XCTestCase @end
@implementation PartShadowRendererTests
- (void)testOriginalA8PartShadowPixelsAndWarmResources {
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    hm2::apple::Files files(root.fileSystemRepresentation);
    const auto models=hm2::Models::parse(hm2::Chunk::parse(files.read("PartsInfo.mcb")),hm2::Chunk::parse(files.read("MachInfo.mcb")));
    const auto& part=models.part("P_Wpn_Gun_101");const auto name=part.name+".png";
    const auto encoded=files.read(name),encodedCopy=encoded;const auto source=hm2::decodeImage(encoded);
    // Independent byte oracle: do not use prepareOriginalPartAlpha to build
    // the comparison texture. It uses the same original GL_ALPHA uploader as
    // text, but no text rasterization, image/color replacement or disk output.
    unsigned sw=1,sh=1;while(sw<source.width)sw*=2;while(sh<source.height)sh*=2;
    hm2::ui::TextBitmap oracle;oracle.layout.width=sw;oracle.layout.height=sh;oracle.alpha.resize(std::size_t(sw)*sh,0);
    for(unsigned y=0;y<source.height;++y)for(unsigned x=0;x<source.width;++x)
        oracle.alpha[std::size_t(y)*sw+x]=source.rgba[(std::size_t(y)*source.width+x)*4+3]>>1;
    hm2::ui::TextBitmap white;white.layout.width=white.layout.height=1;white.alpha={255};
    hm2::PreparedTiles tiles("apple-part-shadow");tiles.prepare(files,name);tiles.prepareShadow(files,name);
    hm2::Frame actual,expected;actual.width=expected.width=768;actual.height=expected.height=320;
    hm2::Sprite background;background.image="@text/shadow-test-background";background.center={384,160};background.size={768,320};background.color={.5f,.75f,1,1};
    actual.sprites.push_back(background);expected.sprites.push_back(background);
    hm2::VisualLayer layer;
    hm2::AtlasBatch reference;reference.image="@text/shadow-test-oracle";reference.tileIdentity=reference.image;
    reference.pixelFormat=3;reference.content={float(source.width),float(source.height)};reference.storage={float(sw),float(sh)};
    reference.blend={hm2::BlendFactor::sourceAlpha,hm2::BlendFactor::oneMinusSourceAlpha};
    for(unsigned i=0;i<4;++i){
        const float angle=i==0?0.f:i==1?.371f:i==2?std::numbers::pi_v<float>/2:std::numbers::pi_v<float>;
        hm2::PartPose pose;pose.part=&part;pose.angle=angle;pose.reverseY=i&1;pose.mountIndex=i;
        const hm2::Vec2 center{100.f+180.f*i,160};hm2::VisibilityState visibility;
        hm2::collectOriginalPartShadow(layer,tiles,pose,center,nullptr,visibility);
        // Original local child offset remains unrotated. Independent raw quad
        // geometry also catches a shared helper accidentally rotating it.
        const hm2::Vec2 origin{center.x+4,center.y-4};const float w=float(source.width)/2,h=float(source.height)/2*(pose.reverseY?-1.f:1.f);
        const hm2::Vec2 local[]={{-w,h},{-w,-h},{w,h},{w,-h}};
        const float u=float(source.width)/sw,v=float(source.height)/sh;
        const hm2::Vec2 uv[]={{0,0},{0,v},{u,0},{u,v}};hm2::AtlasQuad quad;
        for(unsigned k=0;k<4;++k)quad.vertices[k]={{origin.x+(local[k].x*std::cos(angle)-local[k].y*std::sin(angle)),origin.y+(local[k].x*std::sin(angle)+local[k].y*std::cos(angle))},{0,0,0,255},uv[k]};
        reference.quads.push_back(quad);for(unsigned n:{0,1,2,3,2,1})reference.indices.push_back(std::uint16_t(i*4+n));
    }
    layer.drain().appendBatches(actual);hm2::Sprite oracleDraw;oracleDraw.image=reference.image;oracleDraw.atlasBatch=std::make_shared<const hm2::AtlasBatch>(reference);expected.sprites.push_back(oracleDraw);
    UIWindow* window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];UIViewController* controller=[UIViewController new];HM2PartShadowSurface* surface=[[HM2PartShadowSurface alloc]initWithFrame:window.bounds];surface.contentScaleFactor=UIScreen.mainScreen.scale;
    CAEAGLLayer* drawable=(CAEAGLLayer*)surface.layer;drawable.opaque=YES;drawable.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};controller.view=surface;window.rootViewController=controller;[window makeKeyAndVisible];[surface layoutIfNeeded];
    HM2Renderer* renderer=[[HM2Renderer alloc]initWithResourceRoot:root];
    [renderer prepareText:white identifier:background.image];[renderer prepareText:oracle identifier:reference.image];[renderer prepareFrame:actual];[renderer prepareFrame:expected];
    XCTAssertEqual(renderer.alphaTextureUploads,1u);XCTAssertEqual(renderer.imageDecodeCalls,1u);XCTAssertEqual(renderer.imageResourceReads,1u);XCTAssertTrue(encoded==encodedCopy);
    const auto uploads=renderer.textureUploads,reads=renderer.imageResourceReads,decodes=renderer.imageDecodeCalls,context=renderer.contextIdentity;const auto sourceReads=files.reads;
    [renderer captureNextFrame];[renderer drawFrame:expected layer:drawable];NSData* referencePixels=renderer.readRGBA;
    [renderer captureNextFrame];[renderer drawFrame:actual layer:drawable];NSData* actualPixels=renderer.readRGBA;
    XCTAssertEqual(actualPixels.length,referencePixels.length);NSUInteger changed=0;
    if(actualPixels.length==referencePixels.length){const auto* a=static_cast<const std::uint8_t*>(actualPixels.bytes);const auto* b=static_cast<const std::uint8_t*>(referencePixels.bytes);for(NSUInteger i=0;i<actualPixels.length;++i)changed+=a[i]!=b[i];}
    XCTAssertGreaterThan(actualPixels.length,0u);XCTAssertEqual(changed,0u); // Same geometry/texture GL path; no relaxed threshold.
    for(unsigned i=0;i<120;++i){[renderer prepareFrame:actual];[renderer drawFrame:actual layer:drawable];}
    XCTAssertEqual(renderer.textureUploads,uploads);XCTAssertEqual(renderer.imageResourceReads,reads);XCTAssertEqual(renderer.imageDecodeCalls,decodes);XCTAssertEqual(renderer.contextIdentity,context);XCTAssertTrue(files.reads==sourceReads);
    XCTAttachment* attachment=[XCTAttachment attachmentWithData:actualPixels uniformTypeIdentifier:@"public.data"];attachment.name=@"part-shadow-actual-rgba";attachment.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:attachment];
    NSLog(@"HM2_PART_SHADOW_GPU bytes=%lu differentBytes=%lu alphaTextures=%lu warmReads=0 warmDecode=0 warmUploads=0 context=%lu device=%@",(unsigned long)actualPixels.length,(unsigned long)changed,(unsigned long)renderer.alphaTextureUploads,(unsigned long)context,UIDevice.currentDevice.model);
    window.hidden=YES;window.rootViewController=nil;
}
@end

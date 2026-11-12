#import <XCTest/XCTest.h>
#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>
#import <ImageIO/ImageIO.h>
#import "Platform/Apple/HM2Renderer.h"
#include "Platform/Apple/HM2Resources.hpp"
#include "Core/Presentation/PlayerMapPreview.hpp"
#include "Core/Simulation/LocomotionScene.hpp"
#include "Core/Data/Weapons.hpp"
#include <chrono>
#include <numbers>
#include <stdexcept>

namespace {
hm2::Frame bodyFrame(const hm2::Frame& source,std::size_t expectedShadows){
    hm2::Frame bodies;bodies.width=source.width;bodies.height=source.height;std::size_t shadows=0;
    for(const auto& sprite:source.sprites){
        if(sprite.textureSource.variant==hm2::TextureVariant::rgba){bodies.sprites.push_back(sprite);continue;}
        if(sprite.textureSource.variant!=hm2::TextureVariant::originalHalfAlpha||sprite.visualLayer!=0||sprite.image!=sprite.textureSource.image+"~shd"||!sprite.atlasBatch||sprite.atlasBatch->pixelFormat!=3||sprite.atlasBatch->textureSource!=sprite.textureSource)throw std::runtime_error("Invalid original shadow descriptor");
        shadows+=sprite.atlasBatch->quads.size();
    }
    if(shadows!=expectedShadows)throw std::runtime_error("Missing original shadow quads");return bodies;
}
}


@interface HM2TestSurface:UIView @end
@implementation HM2TestSurface
+ (Class)layerClass{return CAEAGLLayer.class;}
@end

@interface AssemblyRendererTests:XCTestCase @end
@implementation AssemblyRendererTests
- (void)testOriginalWeaponDataAndMuzzleOnApple {
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    hm2::apple::Files files(root.fileSystemRepresentation);
    const auto weapons=hm2::Weapons::parse(hm2::DataTable::parse(files.read("WeaponInfo.tble")));XCTAssertEqual(weapons.all().size(),213u);
    const auto& gun=weapons.weapon("P_Wpn_Gun_101");const auto& missile=weapons.weapon("P_Wpn_Msl_101");XCTAssertEqual(gun.shotRange,400);XCTAssertEqual(missile.shotRange,600);XCTAssertEqual(gun.shotCount,10);XCTAssertEqual(missile.shotCount,2);
    const auto models=hm2::Models::parse(hm2::Chunk::parse(files.read("PartsInfo.mcb")),hm2::Chunk::parse(files.read("MachInfo.mcb")));const auto equipment=hm2::PlayerEquipment::parse(hm2::DataTable::parse(files.read("ItemInfo.tble")));const auto pose=models.mountedPose(equipment.mounts(models));
    const auto mg=hm2::Models::muzzlePosition(pose[4],1),ml=hm2::Models::muzzlePosition(pose[5],1);XCTAssertEqualWithAccuracy(mg.x,38,.0001);XCTAssertEqualWithAccuracy(mg.y,24.5,.0001);XCTAssertEqualWithAccuracy(ml.x,27,.0001);XCTAssertEqualWithAccuracy(ml.y,-26.5,.0001);
    NSLog(@"HM2_WEAPON_DATA records=213 mgRange=400 mlRange=600 mgMuzzle=38,24.5 mlMuzzle=27,-26.5 device=%@",UIDevice.currentDevice.model);
}
- (void)testWalkingAndAimingUsePreparedGPUResources {
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    hm2::apple::Files files(root.fileSystemRepresentation);
    hm2::LocomotionActor definitions[]={{hm2::PlayerEquipment::startingCodes,{-80,0},0,std::nullopt},{{"11002","12002","10012","10112"},{80,0},0,hm2::Vec2{80,200}}};
    hm2::LocomotionScene scene(files,"Stage01_01",{480,320},definitions);
    UIWindow* window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];UIViewController* controller=[UIViewController new];HM2TestSurface* surface=[[HM2TestSurface alloc]initWithFrame:window.bounds];surface.contentScaleFactor=UIScreen.mainScreen.scale;
    CAEAGLLayer* layer=(CAEAGLLayer*)surface.layer;layer.opaque=YES;layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};controller.view=surface;window.rootViewController=controller;[window makeKeyAndVisible];[surface layoutIfNeeded];
    HM2Renderer* renderer=[[HM2Renderer alloc]initWithResourceRoot:root];auto frame=scene.frame();[renderer prepareFrame:frame];
    const auto uploads=renderer.textureUploads,context=renderer.contextIdentity;const auto before=files.reads;
    [renderer captureNextFrame];[renderer drawFrame:frame layer:layer];NSData* first=renderer.readRGBA;
    XCTAssertTrue(scene.input(hm2::PointerPhase::begin,scene.camera().toScreen({80,0}),1));scene.input(hm2::PointerPhase::end,scene.camera().toScreen({80,0}),1);
    for(int i=0;i<180;++i){@autoreleasepool{scene.tick();frame=scene.frame();[renderer drawFrame:frame layer:layer];XCTAssertEqual(renderer.textureUploads,uploads);XCTAssertEqual(renderer.contextIdentity,context);}}
    XCTAssertGreaterThan(scene.actors().front().state().position.x,-75.f);XCTAssertTrue(scene.actors()[1].state().weaponReady);XCTAssertEqual(bodyFrame(frame,10).sprites.size(),11u);
    [renderer captureNextFrame];[renderer drawFrame:frame layer:layer];NSData* after=renderer.readRGBA;XCTAssertEqual(first.length,after.length);
    NSUInteger changes=0;const auto* a=static_cast<const unsigned char*>(first.bytes);const auto* b=static_cast<const unsigned char*>(after.bytes);if(first.length==after.length)for(NSUInteger i=0;i<first.length;i+=4)if(a[i]!=b[i]||a[i+1]!=b[i+1]||a[i+2]!=b[i+2])++changes;
    XCTAssertGreaterThan(changes,100u);const auto time=scene.time();const auto ticks=scene.ticks();scene.setPaused(true);
    for(int i=0;i<10;++i){scene.tick();[renderer drawFrame:scene.frame() layer:layer];}XCTAssertEqual(scene.time(),time);XCTAssertEqual(scene.ticks(),ticks);XCTAssertTrue(files.reads==before);
    XCTAssertEqual(uploads,17u);XCTAssertEqual(renderer.presentedFrames,192u);XCTAssertEqual(surface.subviews.count,0u);
    NSLog(@"HM2_LOCOMOTION_GPU ticks=%lu frames=%lu textures=%lu changedPixels=%lu playerX=%.6f playerY=%.6f device=%@",scene.ticks(),(unsigned long)renderer.presentedFrames,(unsigned long)uploads,(unsigned long)changes,scene.actors().front().state().position.x,scene.actors().front().state().position.y,UIDevice.currentDevice.model);
    window.hidden=YES;window.rootViewController=nil;
}
- (void)testMapAndTwoEquippedPlayersRenderWithoutWarmFrameIO {
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    hm2::apple::Files files(root.fileSystemRepresentation);
    const float q=std::numbers::pi_v<float>/2;
    hm2::PlayerAssemblyCase cases[]={{hm2::PlayerEquipment::startingCodes,{-80,0},0,{}},{{"11002","12002","10012","10112"},{80,0},q,{q,q,q,q,0,0}}};
    const hm2::PlayerMapPreview preview(files,"Stage01_01",cases);const hm2::MapCamera camera{{},{},{480,320}};
    UIWindow* window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];UIViewController* controller=[UIViewController new];
    HM2TestSurface* surface=[[HM2TestSurface alloc]initWithFrame:window.bounds];surface.contentScaleFactor=UIScreen.mainScreen.scale;
    CAEAGLLayer* layer=(CAEAGLLayer*)surface.layer;layer.opaque=YES;layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};
    controller.view=surface;window.rootViewController=controller;[window makeKeyAndVisible];[surface layoutIfNeeded];
    HM2Renderer* renderer=[[HM2Renderer alloc]initWithResourceRoot:root];
    auto frame=preview.compose(camera,cases);XCTAssertEqual(bodyFrame(frame,10).sprites.size(),11u);
    [renderer prepareFrame:frame];const auto uploads=renderer.textureUploads,context=renderer.contextIdentity;
    XCTAssertEqual(uploads,17u);const auto bodies=bodyFrame(frame,10);XCTAssertTrue(bodies.sprites[5].reverseY);XCTAssertTrue(bodies.sprites[10].reverseY);
    const auto terrain=preview.map().compose(camera);[renderer captureNextFrame];[renderer drawFrame:terrain layer:layer];NSData* baseline=renderer.readRGBA;
    [renderer captureNextFrame];[renderer drawFrame:frame layer:layer];NSData* mixed=renderer.readRGBA;
    XCTAssertEqual(mixed.length,baseline.length);if(!mixed.length||mixed.length!=baseline.length)return;
    const auto size=renderer.drawableSize;const auto* a=static_cast<const unsigned char*>(baseline.bytes);const auto* b=static_cast<const unsigned char*>(mixed.bytes);
    NSUInteger changed[2]={};
    for(NSUInteger i=0;i<mixed.length;i+=4)if(std::abs(int(a[i])-int(b[i]))>2||std::abs(int(a[i+1])-int(b[i+1]))>2||std::abs(int(a[i+2])-int(b[i+2]))>2)++changed[(i/4)%NSUInteger(size.width)>=NSUInteger(size.width)/2?1:0];
    XCTAssertGreaterThan(changed[0],100u);XCTAssertGreaterThan(changed[1],100u);
    // Terrain is a real original JPEG, not a black placeholder beneath parts.
    NSUInteger terrainPixels=0;for(NSUInteger i=0;i<baseline.length;i+=4)if(a[i]||a[i+1]||a[i+2])++terrainPixels;
    XCTAssertGreaterThan(terrainPixels,NSUInteger(size.width*size.height)/4);
    const auto before=files.reads;
    for(int i=0;i<120;++i){@autoreleasepool{
        cases[1].angles.weaponLeft=float(i)*.01f;cases[1].angles.weaponRight=-float(i)*.02f;
        frame=preview.compose({{float(i)/10,float(-i)/20},{},camera.viewport},cases);[renderer drawFrame:frame layer:layer];
        XCTAssertEqual(bodyFrame(frame,10).sprites.size(),11u);XCTAssertEqual(renderer.textureUploads,uploads);XCTAssertEqual(renderer.contextIdentity,context);
    }}
    XCTAssertTrue(files.reads==before);XCTAssertEqual(renderer.presentedFrames,122u);XCTAssertEqual(surface.subviews.count,0u);
    NSLog(@"HM2_MAP_PLAYER_GPU frames=%lu textures=%lu terrainPixels=%lu leftChanged=%lu rightChanged=%lu device=%@",(unsigned long)renderer.presentedFrames,(unsigned long)uploads,(unsigned long)terrainPixels,(unsigned long)changed[0],(unsigned long)changed[1],UIDevice.currentDevice.model);
    window.hidden=YES;window.rootViewController=nil;
}
- (void)testOriginalPartsRenderTogetherWithoutWarmFrameIO {
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    hm2::apple::Files files(root.fileSystemRepresentation);
    hm2::AssemblyCase cases[]={{"E_Tank_01",{80,80},0,0},{"E_Tank_01",{240,80},std::numbers::pi_v<float>/2,0},
        {"E_Tank_01",{80,240},0,std::numbers::pi_v<float>/2},{"E_Tank_01",{240,240},.7f,-.4f}};
    const hm2::AssemblyPreview preview(files,cases,{320,320});
    XCTAssertEqual(preview.models().parts().size(),338u);XCTAssertEqual(bodyFrame(preview.frame(),8).sprites.size(),8u);
    UIWindow* window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];
    UIViewController* controller=[UIViewController new];HM2TestSurface* surface=[[HM2TestSurface alloc]initWithFrame:window.bounds];
    surface.contentScaleFactor=UIScreen.mainScreen.scale;
    CAEAGLLayer* layer=(CAEAGLLayer*)surface.layer;layer.opaque=YES;
    layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};
    controller.view=surface;window.rootViewController=controller;[window makeKeyAndVisible];[surface layoutIfNeeded];
    HM2Renderer* renderer=[[HM2Renderer alloc]initWithResourceRoot:root];[renderer prepareFrame:preview.frame()];
    const auto uploads=renderer.textureUploads,context=renderer.contextIdentity;
    XCTAssertEqual(uploads,4u);XCTAssertNotEqual(context,0u);
    [renderer captureNextFrame];[renderer drawFrame:preview.frame() layer:layer];
    NSData* rgba=renderer.readRGBA;const auto size=renderer.drawableSize;
    XCTAssertEqual(rgba.length,NSUInteger(size.width*size.height*4));
    const auto* p=static_cast<const unsigned char*>(rgba.bytes);NSUInteger nonblack=0,quadrants[4]={};
    for(NSUInteger i=0;i<rgba.length;i+=4)if(p[i]||p[i+1]||p[i+2]){++nonblack;const auto pixel=i/4;const unsigned q=(pixel%NSUInteger(size.width)>=NSUInteger(size.width)/2?1:0)+(pixel/NSUInteger(size.width)>=NSUInteger(size.height)/2?2:0);++quadrants[q];}
    XCTAssertGreaterThan(nonblack,1000u); // Actual GPU pixels, not only draw count.
    for(unsigned q=0;q<4;++q)XCTAssertGreaterThan(quadrants[q],1000u,@"A simultaneous model case was not visible");
    hm2::Vec2 point{};XCTAssertTrue([renderer logicalPoint:CGPointMake(surface.bounds.size.width/2,surface.bounds.size.height/2) result:&point]);
    XCTAssertEqualWithAccuracy(point.x,160.f,1.f);XCTAssertEqualWithAccuracy(point.y,160.f,1.f);
    // Compare a known unrotated source rectangle at a 1:1 physical pixel offset.
    // A flipped UV or translated quad can have the same nonblack pixel count.
    NSURL* imageURL=[root URLByAppendingPathComponent:@"E_Tank_Body_01.png"];
    CGImageSourceRef source=CGImageSourceCreateWithURL((__bridge CFURLRef)imageURL,nullptr);
    CGImageRef image=source?CGImageSourceCreateImageAtIndex(source,0,nullptr):nullptr;
    if(source)CFRelease(source);XCTAssertNotEqual(image,nullptr);if(!image)return;
    const auto iw=CGImageGetWidth(image),ih=CGImageGetHeight(image);std::vector<unsigned char> expected(iw*ih*4);
    CGColorSpaceRef color=CGColorSpaceCreateDeviceRGB();CGContextRef bitmap=CGBitmapContextCreate(expected.data(),iw,ih,8,iw*4,color,kCGBitmapByteOrder32Big|kCGImageAlphaPremultipliedLast);CGColorSpaceRelease(color);
    XCTAssertNotEqual(bitmap,nullptr);if(!bitmap){CGImageRelease(image);return;}
    CGContextDrawImage(bitmap,CGRectMake(0,0,iw,ih),image);CGContextRelease(bitmap);CGImageRelease(image);
    hm2::Frame pixelFrame;pixelFrame.width=size.width;pixelFrame.height=size.height;
    pixelFrame.sprites.push_back({"E_Tank_Body_01.png",{32+float(iw)/2,32+float(ih)/2},{float(iw),float(ih)}});
    NSUInteger mismatches=0;
    for(bool reverse:{false,true}){
        pixelFrame.sprites[0].reverseY=reverse;
        [renderer captureNextFrame];[renderer drawFrame:pixelFrame layer:layer];
        NSData* pixelCapture=renderer.readRGBA;const auto* actual=static_cast<const unsigned char*>(pixelCapture.bytes);
        XCTAssertEqual(pixelCapture.length,NSUInteger(size.width*size.height*4));if(pixelCapture.length!=NSUInteger(size.width*size.height*4))return;
        // Image decoders return top row first; GL readback begins at the bottom.
        // Original glOrthof0x277a / Tile2D UV0x445f2-0x44670: bottom=contentV,
        // top=0. Reverse-Y inverts this mapping. Do not relax RGB tolerance.
        for(std::size_t y=0;y<ih;++y)for(std::size_t x=0;x<iw;++x)for(int c=0;c<3;++c){const auto a=actual[((y+32)*NSUInteger(size.width)+x+32)*4+c],e=expected[((reverse?y:ih-1-y)*iw+x)*4+c];if(std::abs(int(a)-int(e))>1)++mismatches;}
    }
    XCTAssertEqual(mismatches,0u,@"Original texture UV/origin/alpha differs at 1:1 pixel mapping");
    const auto before=files.reads;
    double worst=0,total=0;
    for(int i=0;i<120;++i){@autoreleasepool{
        cases[1].machineAngle=float(i)*.01f;cases[2].weaponAngle=-float(i)*.02f;
        const auto start=std::chrono::steady_clock::now();
        const auto frame=preview.compose(cases,{320,320});[renderer drawFrame:frame layer:layer];
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();worst=std::max(worst,ms);total+=ms;
        XCTAssertEqual(bodyFrame(frame,8).sprites.size(),8u);XCTAssertEqual(renderer.contextIdentity,context);XCTAssertEqual(renderer.textureUploads,uploads);
    }}
    XCTAssertTrue(files.reads==before);XCTAssertEqual(renderer.presentedFrames,123u);
    // No platform controls exist in this first common GPU assembly surface.
    XCTAssertEqual(surface.subviews.count,0u);
    NSLog(@"HM2_ASSEMBLY_GPU bodySprites=8 shadowQuads=8 frames=%lu textures=%lu coloredPixels=%lu pixelMismatches=%lu meanMs=%.3f worstMs=%.3f device=%@ bounds=%@",(unsigned long)renderer.presentedFrames,(unsigned long)uploads,(unsigned long)nonblack,(unsigned long)mismatches,total/120,worst,UIDevice.currentDevice.model,NSStringFromCGRect(surface.bounds));
    window.hidden=YES;window.rootViewController=nil;
}
@end

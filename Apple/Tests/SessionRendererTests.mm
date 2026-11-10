#import <XCTest/XCTest.h>
#import <UIKit/UIKit.h>
#include "Platform/Apple/HM2GameRuntime.hpp"
#import "Platform/Apple/HM2Renderer.h"
#include "Assets/Image.hpp"
#include <algorithm>

@interface HM2SessionSurface:UIView @end
@implementation HM2SessionSurface
+ (Class)layerClass{return CAEAGLLayer.class;}
@end
namespace {
bool tap(hm2::apple::GameRuntime& runtime,unsigned id){
    for(const auto& key:runtime.screens().buttons())if(key.id==id){
        const auto button=runtime.screens().button(key);if(!button||!button->visible||!button->active)continue;
        const auto point=button->screenPosition();runtime.input(hm2::PointerPhase::begin,point,100);runtime.input(hm2::PointerPhase::end,point,100);return true;
    }return false;
}
UIImage* image(NSData* rgba,CGSize size){
    // glReadPixels starts at the bottom row; PNG/UIImage row0 is the top.
    const auto stride=std::size_t(size.width)*4,height=std::size_t(size.height);NSMutableData* topDown=[NSMutableData dataWithLength:rgba.length];
    const auto* source=static_cast<const std::uint8_t*>(rgba.bytes);auto* destination=static_cast<std::uint8_t*>(topDown.mutableBytes);
    for(std::size_t y=0;y<height;++y)std::copy_n(source+(height-1-y)*stride,stride,destination+y*stride);
    CGDataProviderRef provider=CGDataProviderCreateWithCFData((__bridge CFDataRef)topDown);CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
    CGImageRef cg=CGImageCreate(std::size_t(size.width),std::size_t(size.height),8,32,std::size_t(size.width)*4,space,CGBitmapInfo(kCGImageAlphaLast)|CGBitmapInfo(kCGBitmapByteOrderDefault),provider,nullptr,false,kCGRenderingIntentDefault);
    UIImage* result=cg?[UIImage imageWithCGImage:cg]:nil;if(cg)CGImageRelease(cg);CGColorSpaceRelease(space);CGDataProviderRelease(provider);return result;
}
}
@interface SessionRendererTests:XCTestCase @end
@implementation SessionRendererTests
- (void)testOriginalNameLengthCountryAndNoFallback {
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    NSURL* directory=[[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES] URLByAppendingPathComponent:NSUUID.UUID.UUIDString isDirectory:YES];NSError* error=nil;
    XCTAssertTrue([NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error]);NSURL* save=[directory URLByAppendingPathComponent:@"HeavyMach2.sav"];
    std::vector<hm2::apple::NativeRequest> requests;
    try{
        auto runtime=std::make_unique<hm2::apple::GameRuntime>(root,save,[&](const auto& r){requests.push_back(r);});
        for(unsigned i=0;i<2;++i){runtime->input(hm2::PointerPhase::begin,{384,512},1);runtime->input(hm2::PointerPhase::end,{384,512},1);}
        XCTAssertEqual(runtime->session().state().screen,hm2::ui::ScreenId::slots);
        XCTAssertTrue(runtime->provideName(0," ","zz"));XCTAssertEqual(runtime->session().state().slots[0].name,std::string(" "));XCTAssertEqual(runtime->session().state().slots[0].country,std::string("zz"));
        NSString* five=@"😀😀😀😀😀";XCTAssertEqual(five.length,10u);XCTAssertTrue(runtime->provideName(1,five.UTF8String,"kr"));
        XCTAssertEqual(runtime->session().state().slots[1].name,std::string(five.UTF8String));XCTAssertEqual(runtime->session().state().slots[1].country,std::string("kr"));
        const auto writes=runtime->session().metrics().saveRequests;NSData* before=[NSData dataWithContentsOfURL:save];
        NSString* six=@"😀😀😀😀😀😀";XCTAssertEqual(six.length,12u);XCTAssertFalse(runtime->provideName(2,six.UTF8String,"us"));XCTAssertFalse(runtime->provideName(2,"","us"));XCTAssertFalse(runtime->provideName(2,std::string_view("\xff",1),"us"));
        XCTAssertEqual(runtime->session().metrics().saveRequests,writes);XCTAssertFalse(runtime->session().state().slots[2].used);XCTAssertEqualObjects([NSData dataWithContentsOfURL:save],before);
        XCTAssertEqual(requests.size(),3u);for(const auto& r:requests){XCTAssertEqual(r.kind,hm2::apple::NativeRequest::Kind::alert);XCTAssertEqual(r.text,std::string("IncorrectName"));XCTAssertEqual(r.title,std::string("Incorrect Name"));}
        XCTAssertTrue(runtime->nativeText("MissingDescription").empty());
        NSLog(@"HM2_NAME_RULES UTF16space1/fiveEmoji10accepted sixEmoji12/empty/invalidUTF8rejected countryZZpreserved noExtraSave=1");runtime.reset();
    }catch(const std::exception& e){XCTFail(@"Original native name rules failed: %s",e.what());}
    XCTAssertTrue([NSFileManager.defaultManager removeItemAtURL:directory error:&error]);
}
- (void)testGPUMenuBattleAndAtomicSaveRestart {
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    NSURL* directory=[[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES] URLByAppendingPathComponent:NSUUID.UUID.UUIDString isDirectory:YES];NSError* error=nil;
    XCTAssertTrue([NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error]);
    NSURL* save=[directory URLByAppendingPathComponent:@"HeavyMach2.sav"];
    UIWindow* window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];UIViewController* controller=[UIViewController new];HM2SessionSurface* surface=[[HM2SessionSurface alloc]initWithFrame:window.bounds];surface.contentScaleFactor=UIScreen.mainScreen.scale;
    CAEAGLLayer* layer=(CAEAGLLayer*)surface.layer;layer.opaque=YES;layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};controller.view=surface;window.rootViewController=controller;[window makeKeyAndVisible];[surface layoutIfNeeded];
    unsigned requestedNames=0;std::vector<hm2::apple::NativeRequest> requests;
    try{
        auto runtime=std::make_unique<hm2::apple::GameRuntime>(root,save,[&](const auto& request){requests.push_back(request);if(request.kind==hm2::apple::NativeRequest::Kind::name)++requestedNames;});
        runtime->draw(layer);runtime->input(hm2::PointerPhase::begin,{384,512},1);runtime->input(hm2::PointerPhase::end,{384,512},1);runtime->tick();
        runtime->input(hm2::PointerPhase::begin,{384,512},2);runtime->input(hm2::PointerPhase::end,{384,512},2);runtime->tick();
        XCTAssertEqual(runtime->session().state().screen,hm2::ui::ScreenId::slots);
        for(const auto& key:runtime->screens().buttons())if(key.id==1001){
            const auto held=runtime->screens().button(key);const auto point=held->screenPosition();runtime->input(hm2::PointerPhase::begin,point,900);
            XCTAssertEqual(held->capturedPointer(),900u);runtime->setActive(false);runtime->input(hm2::PointerPhase::cancel,point,900);
            XCTAssertEqual(held->capturedPointer(),0u);XCTAssertFalse(held->pressed());runtime->setActive(true);break;
        }
        XCTAssertTrue(tap(*runtime,1009));XCTAssertTrue(tap(*runtime,1001));XCTAssertEqual(requestedNames,1u);
        XCTAssertFalse(runtime->provideName(0,"01234567890","us"));XCTAssertTrue(runtime->provideName(0,"MACH","us"));XCTAssertTrue(tap(*runtime,1001));runtime->tick();runtime->draw(layer);
        XCTAssertEqual(runtime->session().state().screen,hm2::ui::ScreenId::base);XCTAssertEqual(runtime->session().metrics().saveCompletions,3u);
        NSData* saved=[NSData dataWithContentsOfURL:save];XCTAssertEqual(saved.length,hm2::SaveData::byteCount);
        XCTAssertTrue(tap(*runtime,2005));XCTAssertTrue(tap(*runtime,2005));runtime->tick();runtime->draw(layer);
        XCTAssertEqual(runtime->session().state().screen,hm2::ui::ScreenId::field);XCTAssertNotEqual(runtime->session().battle(),nullptr);
        if(!runtime->session().battle())return;
        for(const auto& enemy:runtime->session().battle()->enemies())if(enemy.hp>0){
            const auto position=runtime->session().battle()->camera().toScreen(enemy.position);
            if(position.x>30&&position.x<738&&position.y>240&&position.y<940){runtime->input(hm2::PointerPhase::begin,position,7);runtime->input(hm2::PointerPhase::end,position,7);if(runtime->session().battle()->selectedEnemy())break;}
        }
        XCTAssertNotEqual(runtime->session().battle()->selectedEnemy(),0u);
        const auto reads=runtime->resourceReads();const auto prepared=runtime->preparedTextCount();const auto uploads=runtime->textureUploads();NSDictionary* audioBefore=runtime->audioMetrics();
        const auto saves=runtime->session().metrics().saveRequests;
        for(unsigned i=0;i<180;++i){runtime->tick();runtime->draw(layer);}
        XCTAssertGreaterThan(runtime->session().battle()->metrics().playerEmissions,0u);
        XCTAssertEqual(runtime->resourceReads(),reads);XCTAssertEqual(runtime->preparedTextCount(),prepared);XCTAssertEqual(runtime->textureUploads(),uploads);XCTAssertEqual(runtime->session().metrics().saveRequests,saves);
        NSDictionary* audioAfter=runtime->audioMetrics();for(NSString* key in @[@"contextsCreated",@"sourcesCreated",@"buffersCreated",@"effectFileReads",@"effectDecodes"])XCTAssertEqualObjects(audioBefore[key],audioAfter[key]);
        XCTAssertEqualObjects([NSData dataWithContentsOfURL:save],saved);
        runtime->captureNextFrame();runtime->draw(layer);NSData* pixels=runtime->pixels();const auto extent=runtime->drawableSize();XCTAssertEqual(pixels.length,std::size_t(extent.width)*std::size_t(extent.height)*4);
        unsigned colored=0;const auto* bytes=static_cast<const std::uint8_t*>(pixels.bytes);for(NSUInteger i=0;i<pixels.length;i+=4)if(bytes[i]||bytes[i+1]||bytes[i+2])++colored;XCTAssertGreaterThan(colored,1000u);
        // Compare opaque original icon pixels against an independent direct
        // sprite draw. This catches a nil filename or a covered UI icon even
        // when the surrounding HUD and count glyphs still render correctly.
        // Each EAGL context owns a separate drawable. A second context cannot
        // allocate storage from the live game's already-owned layer.
        HM2SessionSurface* referenceSurface=[[HM2SessionSurface alloc]initWithFrame:surface.bounds];referenceSurface.contentScaleFactor=surface.contentScaleFactor;
        CAEAGLLayer* referenceLayer=(CAEAGLLayer*)referenceSurface.layer;referenceLayer.opaque=YES;referenceLayer.drawableProperties=layer.drawableProperties;[surface addSubview:referenceSurface];[referenceSurface layoutIfNeeded];
        HM2Renderer* reference=[[HM2Renderer alloc]initWithResourceRoot:root];unsigned iconPixels=0,iconDifferences=0;
        const float scale=std::min(float(extent.width)/768.f,float(extent.height)/1024.f);
        const hm2::Vec2 offset{(float(extent.width)-768*scale)*.5f,(float(extent.height)-1024*scale)*.5f};
        for(unsigned slot=0;slot<3;++slot){
            const auto name=runtime->session().state().quick[slot].icon;NSData* encoded=[NSData dataWithContentsOfURL:[root URLByAppendingPathComponent:[NSString stringWithUTF8String:name.c_str()]]];XCTAssertNotNil(encoded);if(!encoded)continue;
            const auto source=hm2::decodeImage({static_cast<const std::uint8_t*>(encoded.bytes),encoded.length});const hm2::Vec2 center{511.f+57.f*slot,29};hm2::Frame expected;
            expected.sprites.push_back({name,center,{float(source.width),float(source.height)}});[reference prepareFrame:expected];[reference captureNextFrame];[reference drawFrame:expected layer:referenceLayer];NSData* rendered=reference.readRGBA;
            for(unsigned y=2;y<source.height*3/5;++y)for(unsigned x=2;x<source.width*3/5;++x){
                bool opaque=true;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)if(source.rgba[(std::size_t(int(y)+dy)*source.width+unsigned(int(x)+dx))*4+3]!=255)opaque=false;if(!opaque)continue;
                const unsigned px=unsigned(offset.x+(center.x-float(source.width)*.5f+float(x)+.5f)*scale),py=unsigned(offset.y+(center.y+float(source.height)*.5f-float(y)-.5f)*scale);
                const auto index=(std::size_t(py)*std::size_t(extent.width)+px)*4;if(index+3>=pixels.length||index+3>=rendered.length)continue;
                const auto* baseline=static_cast<const std::uint8_t*>(rendered.bytes);++iconPixels;for(unsigned c=0;c<3;++c)if(std::abs(int(bytes[index+c])-int(baseline[index+c]))>8)++iconDifferences;
            }
        }
        [referenceSurface removeFromSuperview];reference=nil;
        XCTAssertGreaterThan(iconPixels,20u);XCTAssertEqual(iconDifferences,0u);
        UIImage* capture=image(pixels,extent);XCTAssertNotNil(capture);if(capture){XCTAttachment* attachment=[XCTAttachment attachmentWithImage:capture];attachment.name=@"HM2 HD canvas GPU session";attachment.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:attachment];}
        // A second GPU render at an explicit384x512 report surface. This is a
        // preview output, not a resize/crop of the full-resolution evidence.
        surface.frame=CGRectMake(0,0,384,512);surface.contentScaleFactor=1;
        runtime->captureNextFrame();runtime->draw(layer);XCTAssertEqual(runtime->drawableSize().width,384);XCTAssertEqual(runtime->drawableSize().height,512);
        UIImage* preview=image(runtime->pixels(),runtime->drawableSize());if(preview){XCTAttachment* attachment=[XCTAttachment attachmentWithImage:preview];attachment.name=@"HM2 HD GPU preview384x512";attachment.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:attachment];}
        XCTAssertTrue(runtime->session().save(hm2::SaveReason::saveAndExit));const auto written=[NSData dataWithContentsOfURL:save];runtime.reset();
        auto restarted=std::make_unique<hm2::apple::GameRuntime>(root,save,[&](const auto& request){requests.push_back(request);});
        XCTAssertTrue(restarted->session().state().slots[0].used);XCTAssertEqual(restarted->session().state().slots[0].name,std::string("MACH"));XCTAssertEqualObjects([NSData dataWithContentsOfURL:save],written);
        // Read failure/corruption cannot silently initialize or overwrite data.
        restarted.reset();NSData* corrupt=[@"preserve-corrupt-save" dataUsingEncoding:NSUTF8StringEncoding];XCTAssertTrue([corrupt writeToURL:save options:NSDataWritingAtomic error:&error]);bool rejected=false;
        try{hm2::apple::GameRuntime bad(root,save,{});}catch(const std::exception&){rejected=true;}
        XCTAssertTrue(rejected);XCTAssertEqualObjects([NSData dataWithContentsOfURL:save],corrupt);
        NSLog(@"HM2_SESSION_GPU titleSlotsBaseField=1 warmResourceReads=0 warmTextureUploads=0 warmAudioAllocations=0 atomicSaveRestart=1 corruptPreserved=1 quickIconPixels=%u quickIconDifferences=%u coloredPixels=%u device=%@",iconPixels,iconDifferences,colored,UIDevice.currentDevice.model);
    }catch(const std::exception& e){XCTFail(@"Restored GPU session failed: %s",e.what());}
    window.hidden=YES;window.rootViewController=nil;XCTAssertTrue([NSFileManager.defaultManager removeItemAtURL:directory error:&error]);
}
@end

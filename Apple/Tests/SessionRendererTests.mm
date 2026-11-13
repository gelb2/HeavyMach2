#import <XCTest/XCTest.h>
#import <UIKit/UIKit.h>
#include "Platform/Apple/HM2GameRuntime.hpp"
#import "Platform/Apple/HM2Renderer.h"
#include "Assets/Image.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include <algorithm>

@interface HM2SessionSurface:UIView @end
@implementation HM2SessionSurface
+ (Class)layerClass{return CAEAGLLayer.class;}
@end
namespace {
void settle(hm2::apple::GameRuntime& runtime,CAEAGLLayer* layer=nullptr){unsigned ticks=0;while(runtime.session().screenTransition().phase!=8&&ticks++<200){runtime.tick();if(layer)runtime.draw(layer);}if(runtime.session().screenTransition().phase!=8)throw std::runtime_error("Original loading doors did not finish");}
bool tap(hm2::apple::GameRuntime& runtime,unsigned id,CAEAGLLayer* layer=nullptr){
    for(const auto& key:runtime.screens().buttons())if(key.id==id){
        const auto button=runtime.screens().button(key);if(!button||!button->visible||!button->active)continue;
        const auto point=button->screenPosition();runtime.input(hm2::PointerPhase::begin,point,100);runtime.input(hm2::PointerPhase::end,point,100);settle(runtime,layer);return true;
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
        for(unsigned i=0;i<2;++i){runtime->input(hm2::PointerPhase::begin,{384,512},1);runtime->input(hm2::PointerPhase::end,{384,512},1);}settle(*runtime);
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
        runtime->input(hm2::PointerPhase::begin,{384,512},2);runtime->input(hm2::PointerPhase::end,{384,512},2);runtime->tick();settle(*runtime,layer);
        XCTAssertEqual(runtime->session().state().screen,hm2::ui::ScreenId::slots);
        for(const auto& key:runtime->screens().buttons())if(key.id==1001){
            const auto held=runtime->screens().button(key);const auto point=held->screenPosition();runtime->input(hm2::PointerPhase::begin,point,900);
            XCTAssertEqual(held->capturedPointer(),900u);runtime->setActive(false);runtime->input(hm2::PointerPhase::cancel,point,900);
            XCTAssertEqual(held->capturedPointer(),0u);XCTAssertFalse(held->pressed());runtime->setActive(true);break;
        }
        XCTAssertTrue(tap(*runtime,1009,layer));XCTAssertTrue(tap(*runtime,1001,layer));XCTAssertEqual(requestedNames,1u);
        XCTAssertFalse(runtime->provideName(0,"01234567890","us"));XCTAssertTrue(runtime->provideName(0,"MACH","us"));XCTAssertTrue(tap(*runtime,1001,layer));runtime->tick();runtime->draw(layer);
        XCTAssertEqual(runtime->session().state().screen,hm2::ui::ScreenId::base);XCTAssertEqual(runtime->session().metrics().saveCompletions,3u);
        XCTAssertTrue(runtime->session().help()&&runtime->session().help()->state().page==0);
        runtime->input(hm2::PointerPhase::begin,{384,512},101);runtime->input(hm2::PointerPhase::end,{384,512},101);
        XCTAssertEqual(runtime->session().help(),nullptr);
        NSData* saved=[NSData dataWithContentsOfURL:save];XCTAssertEqual(saved.length,hm2::SaveData::byteCount);
        XCTAssertTrue(tap(*runtime,2005,layer));XCTAssertTrue(tap(*runtime,2005,layer));if(runtime->session().help()){runtime->input(hm2::PointerPhase::begin,{700,512},101);runtime->input(hm2::PointerPhase::end,{700,512},101);}runtime->tick();runtime->draw(layer);
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
- (void)testOriginalSlotPreviewShadowParentTranslation {
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    NSURL* directory=[[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES] URLByAppendingPathComponent:NSUUID.UUID.UUIDString isDirectory:YES];NSError* error=nil;
    XCTAssertTrue([NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error]);
    UIWindow* window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];UIViewController* controller=[UIViewController new];
    HM2SessionSurface* surface=[[HM2SessionSurface alloc]initWithFrame:CGRectMake(0,0,384,512)];surface.contentScaleFactor=1;
    CAEAGLLayer* layer=(CAEAGLLayer*)surface.layer;layer.opaque=YES;layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};
    controller.view=surface;window.rootViewController=controller;[window makeKeyAndVisible];surface.frame=CGRectMake(0,0,384,512);[surface layoutIfNeeded];
    try{
        hm2::apple::GameRuntime runtime(root,[directory URLByAppendingPathComponent:@"HeavyMach2.sav"],{});
        for(unsigned n=0;n<2;++n){runtime.input(hm2::PointerPhase::begin,{384,512},1);runtime.input(hm2::PointerPhase::end,{384,512},1);}settle(runtime,layer);
        for(unsigned slot=0;slot<3;++slot)XCTAssertTrue(runtime.provideName(slot,"MACH"+std::to_string(slot),"kr"));
        XCTAssertEqual(runtime.session().state().screen,hm2::ui::ScreenId::slots);
        // The actual Session producer supplies local original part frames.
        // Build an independent expected parent translation from those inputs,
        // rather than moving the already emitted (possibly incorrect) quads.
        std::vector<hm2::Sprite> oracleShadows;std::vector<std::string> bodyImages;
        for(unsigned slot=0;slot<3;++slot){
            const auto& source=runtime.session().state().slots[slot].machPreview;XCTAssertFalse(source.sprites.empty());
            const hm2::Vec2 origin{214,794.f-240.f*slot};unsigned count=0;
            for(const auto& sprite:source.sprites)if(sprite.textureSource.variant==hm2::TextureVariant::originalHalfAlpha){
                XCTAssertEqual(sprite.visualLayer.value_or(99),0u);
                XCTAssertTrue(bool(sprite.atlasBatch));if(!sprite.atlasBatch)continue;
                hm2::Sprite expected=sprite;auto batch=std::make_shared<hm2::AtlasBatch>(*sprite.atlasBatch);
                for(auto& quad:batch->quads)for(auto& vertex:quad.vertices){vertex.position.x+=origin.x;vertex.position.y+=origin.y;}
                expected.atlasBatch=std::move(batch);oracleShadows.push_back(std::move(expected));++count;
            }else{XCTAssertFalse(bool(sprite.atlasBatch));XCTAssertEqual(sprite.visualLayer.value_or(99),3u);bodyImages.push_back(sprite.image);}
            XCTAssertGreaterThan(count,0u);
        }
        hm2::Frame commands;runtime.screens().append(commands,runtime.session().renderTime());
        hm2::Frame actual;for(const auto& sprite:commands.sprites)if(!sprite.image.starts_with("@text/"))actual.sprites.push_back(sprite);
        // Keep every original PNG-backed slot command. Dynamic OS text is
        // excluded from both pixel-oracle frames; the full real slot screen is
        // captured separately below, including its actual prepared text.
        std::size_t index=0;
        std::vector<const hm2::AtlasBatch*> stable;
        for(const auto& sprite:actual.sprites)if(sprite.textureSource.variant==hm2::TextureVariant::originalHalfAlpha){
            XCTAssertLessThan(index,oracleShadows.size());if(index>=oracleShadows.size())continue;
            const auto& oracle=oracleShadows[index++];XCTAssertEqual(sprite.image,oracle.image);XCTAssertTrue(bool(sprite.atlasBatch));
            if(sprite.atlasBatch){stable.push_back(sprite.atlasBatch.get());XCTAssertEqual(sprite.atlasBatch->quads.size(),oracle.atlasBatch->quads.size());
                for(std::size_t q=0;q<std::min(sprite.atlasBatch->quads.size(),oracle.atlasBatch->quads.size());++q)for(unsigned v=0;v<4;++v){
                    XCTAssertEqual(sprite.atlasBatch->quads[q].vertices[v].position.x,oracle.atlasBatch->quads[q].vertices[v].position.x);
                    XCTAssertEqual(sprite.atlasBatch->quads[q].vertices[v].position.y,oracle.atlasBatch->quads[q].vertices[v].position.y);
                }}
        }
        XCTAssertEqual(index,oracleShadows.size());XCTAssertGreaterThan(index,0u);
        // Independent original global order: explicit shadow0, then opaque
        // SelSlot_BG on inherited3, followed by type0 direct body visuals3.
        // The former visible-shadow>0 assertion was wrong for this UI: the
        // original full-screen background intentionally covers those shadows.
        hm2::Frame expected;expected.sprites=oracleShadows;
        for(const auto& sprite:actual.sprites)if(sprite.textureSource.variant!=hm2::TextureVariant::originalHalfAlpha)expected.sprites.push_back(sprite);
        NSData* backgroundData=[NSData dataWithContentsOfURL:[root URLByAppendingPathComponent:@"SelSlot_BG.png"]];XCTAssertNotNil(backgroundData);
        if(backgroundData){const auto bg=hm2::decodeImage({static_cast<const std::uint8_t*>(backgroundData.bytes),backgroundData.length});XCTAssertEqual(bg.width,768u);XCTAssertEqual(bg.height,1024u);
            bool opaque=true;for(std::size_t i=3;i<bg.rgba.size();i+=4)opaque&=bg.rgba[i]==255;XCTAssertTrue(opaque);}
        runtime.captureNextFrame();runtime.draw(layer);UIImage* full=image(runtime.pixels(),runtime.drawableSize());XCTAssertNotNil(full);
        if(full){XCTAttachment* a=[XCTAttachment attachmentWithImage:full];a.name=@"original-slot-three-previews-with-parent-shadows";a.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:a];}
        HM2SessionSurface* referenceSurface=[[HM2SessionSurface alloc]initWithFrame:surface.bounds];referenceSurface.contentScaleFactor=1;
        CAEAGLLayer* referenceLayer=(CAEAGLLayer*)referenceSurface.layer;referenceLayer.opaque=YES;referenceLayer.drawableProperties=layer.drawableProperties;[surface addSubview:referenceSurface];[referenceSurface layoutIfNeeded];
        HM2Renderer* renderer=[[HM2Renderer alloc]initWithResourceRoot:root];[renderer prepareFrame:expected];
        // Observe prepared output separately from the recorded cold-frame
        // variation. No pixel tolerance or production GL state is changed.
        hm2::Frame warmNoShadow=expected;std::erase_if(warmNoShadow.sprites,[](const auto& sprite){return sprite.textureSource.variant==hm2::TextureVariant::originalHalfAlpha;});
        hm2::Frame warmNoBody=expected;std::erase_if(warmNoBody.sprites,[&](const auto& sprite){return std::find(bodyImages.begin(),bodyImages.end(),sprite.image)!=bodyImages.end();});
        for(unsigned warm=0;warm<30;++warm)for(const auto* prepared:{&actual,&expected,&warmNoShadow,&warmNoBody})[renderer drawFrame:*prepared layer:referenceLayer];
        [renderer captureNextFrame];[renderer drawFrame:actual layer:referenceLayer];NSData* preparedBaseline=renderer.readRGBA;
        NSMutableArray* preparedRepeat=[NSMutableArray array];
        for(unsigned replay=0;replay<16;++replay){[renderer captureNextFrame];[renderer drawFrame:actual layer:referenceLayer];NSData* pixels=renderer.readRGBA;
            XCTAssertEqual(pixels.length,preparedBaseline.length);NSUInteger differences=0;const auto* left=static_cast<const std::uint8_t*>(preparedBaseline.bytes);const auto* right=static_cast<const std::uint8_t*>(pixels.bytes);
            for(NSUInteger i=0;i<std::min(pixels.length,preparedBaseline.length);++i)differences+=left[i]!=right[i];
            [preparedRepeat addObject:@(differences)];XCTAssertEqual(differences,0u);
        }
        NSData* diagnostic=[NSJSONSerialization dataWithJSONObject:@{@"warmDraws":@120,@"preparedIdenticalFrameDifferenceBytes":preparedRepeat,@"pixelTolerance":@0} options:NSJSONWritingPrettyPrinted error:&error];
        if(diagnostic){XCTAttachment* attachment=[XCTAttachment attachmentWithData:diagnostic uniformTypeIdentifier:@"public.json"];attachment.name=@"slot-gpu-prepared-repeatability";attachment.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:attachment];}
        [renderer captureNextFrame];[renderer drawFrame:actual layer:referenceLayer];NSData* actualPixels=renderer.readRGBA;
        [renderer captureNextFrame];[renderer drawFrame:expected layer:referenceLayer];NSData* expectedPixels=renderer.readRGBA;
        XCTAssertEqual(renderer.drawableSize.width,384);XCTAssertEqual(renderer.drawableSize.height,512);
        XCTAssertEqual(actualPixels.length,expectedPixels.length);std::size_t differences=0;
        const auto* a=static_cast<const std::uint8_t*>(actualPixels.bytes);const auto* b=static_cast<const std::uint8_t*>(expectedPixels.bytes);
        for(NSUInteger i=0;i<std::min(actualPixels.length,expectedPixels.length);++i)if(a[i]!=b[i])++differences;
        XCTAssertEqual(differences,0u);
        hm2::Frame noShadow=expected;std::erase_if(noShadow.sprites,[](const auto& s){return s.textureSource.variant==hm2::TextureVariant::originalHalfAlpha;});
        [renderer captureNextFrame];[renderer drawFrame:noShadow layer:referenceLayer];NSData* noShadowPixels=renderer.readRGBA;
        std::size_t occludedShadowDifferences=0;const auto* clear=static_cast<const std::uint8_t*>(noShadowPixels.bytes);
        XCTAssertEqual(noShadowPixels.length,expectedPixels.length);
        for(NSUInteger i=0;i<std::min(noShadowPixels.length,expectedPixels.length);++i)if(b[i]!=clear[i])++occludedShadowDifferences;
        XCTAssertEqual(occludedShadowDifferences,0u);
        hm2::Frame noBody=expected;std::erase_if(noBody.sprites,[&](const auto& s){return std::find(bodyImages.begin(),bodyImages.end(),s.image)!=bodyImages.end();});
        [renderer captureNextFrame];[renderer drawFrame:noBody layer:referenceLayer];NSData* noBodyPixels=renderer.readRGBA;
        unsigned visibleBodies[3]={};const auto* noBodyBytes=static_cast<const std::uint8_t*>(noBodyPixels.bytes);XCTAssertEqual(noBodyPixels.length,expectedPixels.length);
        if(noBodyPixels.length==expectedPixels.length)for(unsigned slot=0;slot<3;++slot){
            const unsigned centerY=397-120*slot;for(unsigned y=centerY-55;y<centerY+55;++y)for(unsigned x=60;x<155;++x){
                const auto i=(std::size_t(y)*384+x)*4;if(i+3>=expectedPixels.length)continue;
                if(b[i]!=noBodyBytes[i]||b[i+1]!=noBodyBytes[i+1]||b[i+2]!=noBodyBytes[i+2])++visibleBodies[slot];
            }XCTAssertGreaterThan(visibleBodies[slot],0u);
        }
        [referenceSurface removeFromSuperview];renderer=nil;
        const auto reads=runtime.resourceReads();const auto uploads=runtime.textureUploads();const auto texts=runtime.preparedTextCount();
        for(unsigned frame=0;frame<120;++frame){runtime.draw(layer);hm2::Frame emitted;runtime.screens().append(emitted,runtime.session().renderTime());std::size_t next=0;
            for(const auto& sprite:emitted.sprites)if(sprite.textureSource.variant==hm2::TextureVariant::originalHalfAlpha){XCTAssertLessThan(next,stable.size());if(next<stable.size())XCTAssertEqual(sprite.atlasBatch.get(),stable[next]);++next;}
            XCTAssertEqual(next,stable.size());
        }
        XCTAssertEqual(runtime.resourceReads(),reads);XCTAssertEqual(runtime.textureUploads(),uploads);XCTAssertEqual(runtime.preparedTextCount(),texts);
        NSLog(@"HM2_SLOT_PREVIEW_GPU slots=3 translatedShadowBatches=%zu comparedBytes=%lu differentBytes=%zu occludedShadowDifferenceBytes=%zu visibleBodyPixels=%u/%u/%u stablePayload120=1 warmReads0Upload0Text0=1",index,(unsigned long)actualPixels.length,differences,occludedShadowDifferences,visibleBodies[0],visibleBodies[1],visibleBodies[2]);
    }catch(const std::exception& e){XCTFail(@"Original slot preview shadow failed: %s",e.what());}
    window.hidden=YES;window.rootViewController=nil;XCTAssertTrue([NSFileManager.defaultManager removeItemAtURL:directory error:&error]);
}
- (void)testOriginalShopItemDetailsAndSellPosition {
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    NSURL* directory=[[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES] URLByAppendingPathComponent:NSUUID.UUID.UUIDString isDirectory:YES];NSError* error=nil;
    XCTAssertTrue([NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error]);
    UIWindow* window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];UIViewController* controller=[UIViewController new];
    HM2SessionSurface* surface=[[HM2SessionSurface alloc]initWithFrame:CGRectMake(0,0,384,512)];surface.contentScaleFactor=1;
    CAEAGLLayer* layer=(CAEAGLLayer*)surface.layer;layer.opaque=YES;layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};
    controller.view=surface;window.rootViewController=controller;[window makeKeyAndVisible];surface.frame=CGRectMake(0,0,384,512);[surface layoutIfNeeded];
    try{
        hm2::apple::GameRuntime runtime(root,[directory URLByAppendingPathComponent:@"HeavyMach2.sav"],{});
        for(unsigned n=0;n<2;++n){runtime.input(hm2::PointerPhase::begin,{384,512},1);runtime.input(hm2::PointerPhase::end,{384,512},1);}settle(runtime,layer);
        XCTAssertTrue(runtime.provideName(0,"MACH","kr"));XCTAssertTrue(tap(runtime,1001,layer));
        if(runtime.session().help()){runtime.input(hm2::PointerPhase::begin,{384,512},101);runtime.input(hm2::PointerPhase::end,{384,512},101);XCTAssertFalse(runtime.session().help());}
        XCTAssertTrue(tap(runtime,2002,layer));XCTAssertTrue(tap(runtime,2002,layer));XCTAssertEqual(runtime.session().state().screen,hm2::ui::ScreenId::shop);
        if(runtime.session().help()){XCTAssertEqual(runtime.session().help()->state().page,1);runtime.input(hm2::PointerPhase::begin,{384,512},102);runtime.input(hm2::PointerPhase::end,{384,512},102);XCTAssertFalse(runtime.session().help());}
        const auto writes=runtime.session().metrics().saveRequests;
        auto inspect=[&](NSString* label,const std::string& largeIcon,bool popup){
            for(unsigned n=0;n<90;++n){runtime.tick();runtime.draw(layer);}
            hm2::Frame commands;runtime.screens().append(commands,runtime.session().renderTime());
            const auto large=std::find_if(commands.sprites.begin(),commands.sprites.end(),[&](const auto& v){return v.image==largeIcon;});
            XCTAssertTrue(large!=commands.sprites.end());if(large!=commands.sprites.end()){XCTAssertEqual(large->angle,0.f);XCTAssertFalse(large->strip.empty());}
            XCTAssertEqual(std::any_of(commands.sprites.begin(),commands.sprites.end(),[](const auto& v){return v.image=="ItemDesc.png";}),popup);
            const auto reads=runtime.resourceReads();const auto texts=runtime.preparedTextCount();const auto uploads=runtime.textureUploads();const auto audio=runtime.audioMetrics();
            for(unsigned n=0;n<60;++n){runtime.tick();runtime.draw(layer);}XCTAssertEqual(runtime.resourceReads(),reads);XCTAssertEqual(runtime.preparedTextCount(),texts);XCTAssertEqual(runtime.textureUploads(),uploads);XCTAssertEqual(runtime.session().metrics().saveRequests,writes);
            const auto after=runtime.audioMetrics();for(NSString* key in @[@"contextsCreated",@"sourcesCreated",@"buffersCreated",@"effectFileReads",@"effectDecodes"])XCTAssertEqualObjects(audio[key],after[key]);
            runtime.captureNextFrame();runtime.draw(layer);UIImage* capture=image(runtime.pixels(),runtime.drawableSize());XCTAssertNotNil(capture);
            if(capture){XCTAttachment* a=[XCTAttachment attachmentWithImage:capture];a.name=label;a.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:a];}
        };
        XCTAssertTrue(tap(runtime,2200,layer));XCTAssertTrue(runtime.session().state().shopDetailVisible);XCTAssertEqual(runtime.session().state().selectedShop,0);inspect(@"shop-original-weapon-large-detail",runtime.session().state().shopItems[0].largeIcon,false);
        XCTAssertTrue(tap(runtime,6002,layer));XCTAssertEqual(runtime.session().state().detailInventory,2);XCTAssertFalse(runtime.session().state().shopDetailVisible);inspect(@"shop-original-inventory-popup",runtime.session().state().detailItem.largeIcon,true);
        XCTAssertTrue(tap(runtime,2112,layer));XCTAssertEqual(runtime.session().state().detailInventory,-1);
        XCTAssertTrue(tap(runtime,6005,layer));bool foundSell=false;
        for(const auto& key:runtime.screens().buttons())if(key.id==2103){const auto b=runtime.screens().button(key);XCTAssertTrue(bool(b));if(b){XCTAssertEqual(b->screenPosition().x,384.f);XCTAssertEqual(b->screenPosition().y,552.f);foundSell=true;}}
        XCTAssertTrue(foundSell);XCTAssertEqual(runtime.session().metrics().unsupportedActions,0u);
        NSLog(@"HM2_SHOP_GPU actualButtons/KoreanSession/largeIcon/detailPopup/Sell384,552/warm60IO0Texture0AudioAlloc0/save0");
    }catch(const std::exception& e){XCTFail(@"Original shop detail failed: %s",e.what());}
    window.hidden=YES;window.rootViewController=nil;XCTAssertTrue([NSFileManager.defaultManager removeItemAtURL:directory error:&error]);
}
@end

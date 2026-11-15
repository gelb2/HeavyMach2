#import <XCTest/XCTest.h>
#import <UIKit/UIKit.h>
#import <CommonCrypto/CommonDigest.h>
#include "Core/Presentation/MenuArtwork.hpp"
#include "Platform/Apple/HM2GameRuntime.hpp"
#import "Platform/Apple/HM2Renderer.h"
#include "Platform/Apple/HM2TextRasterizer.h"
#include "Platform/Apple/HM2Resources.hpp"
#include "Core/Presentation/HUDLayout.hpp"
#include "Core/Progression/Progression.hpp"
#include "Assets/Image.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include <algorithm>
#include <map>
#include <set>

@interface HM2SessionSurface:UIView @end
@implementation HM2SessionSurface
+ (Class)layerClass{return CAEAGLLayer.class;}
@end
namespace {
struct HUDIdentityHost final:hm2::SessionHost,hm2::ui::ScreensHost {
    HM2Renderer* renderer;std::unique_ptr<hm2::ui::TextRasterizer> rasterizer=hm2::apple::makeTextRasterizer();
    hm2::ui::TextPreparationCache texts;std::map<const hm2::ui::TextBitmap*,std::string> names;
    std::optional<hm2::Bytes> savedLayout;std::vector<hm2::Bytes> progressWrites;
    explicit HUDIdentityHost(HM2Renderer* value):renderer(value){}
    hm2::ui::PreparedScreenText prepareText(const hm2::ui::TextRasterRequest& request)override{
        const auto bitmap=texts.prepare(*rasterizer,request);auto found=names.find(bitmap.get());
        if(found==names.end()){const auto name="@text/hud-identity/"+std::to_string(names.size());[renderer prepareText:*bitmap identifier:name];found=names.emplace(bitmap.get(),name).first;}
        return {found->second,bitmap};
    }
    hm2::ui::PreparedScreenText prepareWorldText(const hm2::ui::TextRasterRequest& request)override{return prepareText(request);}
    std::optional<hm2::Bytes> loadHUDLayout(std::string_view)override{return savedLayout;}
    bool saveHUDLayout(std::string_view,std::span<const std::uint8_t> bytes)override{savedLayout=hm2::Bytes(bytes.begin(),bytes.end());return true;}
    void action(const hm2::ui::ScreenAction&)override{}
    void sound(std::string_view,hm2::Vec2)override{}
    void sound(std::string_view)override{}
    void soundID(unsigned,hm2::Vec2)override{}
    void sfx(unsigned)override{}
    void bgm(std::string_view)override{}
    void alert(std::string_view)override{}
    void requestName(unsigned)override{}
    void requestDelete(unsigned)override{}
    bool writeSave(std::span<const std::uint8_t> bytes,hm2::SaveReason)override{progressWrites.emplace_back(bytes.begin(),bytes.end());return true;}
    std::uint32_t absoluteTimeLow()override{return 1;}
    std::uint32_t unixTime()override{return 1794268800;}
};
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
- (void)testMenuBackgroundsForDevice {
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    const bool phone=UIDevice.currentDevice.userInterfaceIdiom==UIUserInterfaceIdiomPhone;
    const CGSize extent=phone?CGSizeMake(912,420):CGSizeMake(1024,1366);
    UIWindow* window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];UIViewController* controller=[UIViewController new];
    HM2SessionSurface* surface=[[HM2SessionSurface alloc]initWithFrame:CGRectMake(0,0,extent.width,extent.height)];surface.contentScaleFactor=1;
    controller.view=surface;window.rootViewController=controller;[window makeKeyAndVisible];surface.frame=CGRectMake(0,0,extent.width,extent.height);
    CAEAGLLayer* layer=(CAEAGLLayer*)surface.layer;layer.opaque=YES;layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};
    auto display=hm2::DisplayConfiguration::surface({float(extent.width),float(extent.height)});display.device=phone?hm2::DisplayDevice::phone:hm2::DisplayDevice::tablet;
    auto narrow=hm2::DisplayConfiguration::surface({420,600});narrow.device=hm2::DisplayDevice::tablet;
    XCTAssertEqual(hm2::menu::background(narrow,"ShopBG.png"),std::string("ShopBG.png"));
    narrow.device=hm2::DisplayDevice::phone;XCTAssertEqual(hm2::menu::background(narrow,"ShopBG.png"),std::string("PhoneMenu/ShopBG.jpg"));
    HM2Renderer* renderer=[[HM2Renderer alloc]initWithResourceRoot:root];HUDIdentityHost host(renderer);hm2::apple::Files files(root.fileSystemRepresentation);
    hm2::PreparedTiles tiles("menu-background-check");hm2::ui::Screens ui(files,tiles,host);ui.setDisplay(display);hm2::DarwinWorldRandom random(1);
    NSMutableArray* rows=[NSMutableArray new];
    const auto sha=[](NSData* data){unsigned char hash[CC_SHA256_DIGEST_LENGTH];CC_SHA256(data.bytes,CC_LONG(data.length),hash);NSMutableString* text=[NSMutableString new];for(auto b:hash)[text appendFormat:@"%02x",b];return text;};
    NSString* binary=sha([NSData dataWithContentsOfFile:NSBundle.mainBundle.executablePath]);
    NSString* game=sha([NSData dataWithContentsOfFile:[NSBundle.mainBundle.executablePath stringByAppendingString:@".debug.dylib"]]);
    for(unsigned base=1;base<=6;++base)for(auto screen:{hm2::ui::ScreenId::base,hm2::ui::ScreenId::shop,hm2::ui::ScreenId::command}){@autoreleasepool{
        hm2::ui::ScreensState state;state.screen=screen;state.baseId="Base00"+std::to_string(base);state.locale="kr";state.credits=12345;
        if(screen==hm2::ui::ScreenId::shop){state.inventoryVisible=true;for(unsigned n=0;n<25;++n){hm2::ui::ItemView item;item.dataId="fixture"+std::to_string(n);item.icon="Icon10011.png";item.count=1;state.inventory.push_back(item);if(n<24)state.shopItems.push_back(item);}}
        ui.refresh(state,0);for(unsigned tick=0;tick<180;++tick)ui.tick(float(tick)/60,random);hm2::Frame frame;ui.append(frame,3);
        hm2::Frame preparation;ui.appendPreparation(preparation);[renderer prepareFrame:preparation];[renderer prepareFrame:frame];[renderer drawFrame:frame layer:layer];[renderer captureNextFrame];[renderer drawFrame:frame layer:layer];
        XCTAssertEqual(renderer.readRGBA.length,std::size_t(extent.width*extent.height)*4);if(renderer.readRGBA.length!=std::size_t(extent.width*extent.height)*4)continue;
        NSString* name=[NSString stringWithFormat:@"menu-%@-Base00%u-%s",phone?@"iphone":@"ipad",base,screen==hm2::ui::ScreenId::base?"hub":screen==hm2::ui::ScreenId::shop?"shop":"command"];
        auto* shot=[XCTAttachment attachmentWithImage:image(renderer.readRGBA,extent)];shot.name=name;shot.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:shot];
        NSMutableArray* backgrounds=[NSMutableArray new];
        for(const auto& sprite:frame.sprites){const auto& path=sprite.image;if(path.find("Base00")==std::string::npos&&path.find("ShopBG.")==std::string::npos&&path.find("CmdCenterBG.")==std::string::npos)continue;
            NSString* relative=[NSString stringWithUTF8String:path.c_str()];NSData* bytes=[NSData dataWithContentsOfURL:[root URLByAppendingPathComponent:relative]];XCTAssertNotNil(bytes);
            [backgrounds addObject:@{@"image":relative,@"sha256":sha(bytes)}];}
        [rows addObject:@{@"device":phone?@"iphone":@"ipad",@"base":[NSString stringWithUTF8String:state.baseId.c_str()],@"screen":screen==hm2::ui::ScreenId::base?@"hub":screen==hm2::ui::ScreenId::shop?@"shop":@"command",@"capture":name,@"backgrounds":backgrounds}];
    }}
    XCTAssertEqual(rows.count,18u);NSData* data=[NSJSONSerialization dataWithJSONObject:@{@"binarySHA256":binary,@"rows":rows} options:NSJSONWritingPrettyPrinted error:nil];
    NSMutableDictionary* observed=[[NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingMutableContainers error:nil] mutableCopy];observed[@"gameSHA256"]=game;data=[NSJSONSerialization dataWithJSONObject:observed options:NSJSONWritingPrettyPrinted error:nil];
    auto* record=[XCTAttachment attachmentWithData:data uniformTypeIdentifier:@"public.json"];record.name=phone?@"menu-assets-iphone":@"menu-assets-ipad";record.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:record];[window setHidden:YES];
}
- (void)testPhoneBottomHUDAndOriginalPortalRoutes {
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    UIWindow* window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];UIViewController* controller=[UIViewController new];
    HM2SessionSurface* surface=[[HM2SessionSurface alloc]initWithFrame:CGRectMake(0,0,912,420)];surface.contentScaleFactor=1;controller.view=surface;window.rootViewController=controller;[window makeKeyAndVisible];surface.frame=CGRectMake(0,0,912,420);
    CAEAGLLayer* layer=(CAEAGLLayer*)surface.layer;layer.opaque=YES;layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};
    struct Case {const char* map;int gate;bool path;};
    for(const auto test:{Case{"Stage01_01",0,false},Case{"Stage01_03",1,true}}){@autoreleasepool{
        NSURL* directory=[[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES] URLByAppendingPathComponent:NSUUID.UUID.UUIDString isDirectory:YES];NSError* error=nil;
        XCTAssertTrue([NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error]);
        NSURL* save=[directory URLByAppendingPathComponent:@"HeavyMach2.sav"];hm2::SaveData fixture;fixture.initializeSlot(0,"PORTAL","kr",1794268800);fixture.slot(0)[0x1b5c]=5;
        hm2::DarwinWorldRandom random(1);const auto bytes=fixture.encode(random,1);XCTAssertTrue([[NSData dataWithBytes:bytes.data() length:bytes.size()] writeToURL:save atomically:YES]);
        try{
            hm2::apple::GameRuntime runtime(root,save,{},[]{return 1u;});runtime.setDisplay(hm2::DisplayConfiguration::surface({912,420},{68,20,68,0}));
            XCTAssertTrue(runtime.session().selectSlot(0));XCTAssertTrue(runtime.session().enterField(test.map,test.gate));runtime.synchronize();settle(runtime,layer);
            auto* scene=runtime.session().battle();XCTAssertNotEqual(scene,nullptr);if(!scene)continue;
            scene->setPaused(true);for(unsigned i=0;i<600;++i)runtime.tick();scene->setPaused(false);runtime.synchronize();
            // The actual HUD quick-slot hit extends below the20pt safe inset.
            for(const auto& key:runtime.screens().buttons())if(key.id==6200){auto point=runtime.screens().buttonScreenPosition(key);point.y-=10;XCTAssertGreaterThan(point.y,0);XCTAssertLessThan(point.y,20);
                XCTAssertTrue(runtime.input(hm2::PointerPhase::begin,point,9200));XCTAssertTrue(runtime.inputHistory().back().uiConsumed);XCTAssertFalse(runtime.inputHistory().back().worldConsumed);
                runtime.input(hm2::PointerPhase::cancel,point,9200);
                // This real quick-slot press arms Repair. Toggle the same
                // original item off before testing ground navigation.
                if(runtime.session().state().selectedQuick==0){runtime.input(hm2::PointerPhase::begin,point,9202);runtime.input(hm2::PointerPhase::cancel,point,9202);}
                XCTAssertEqual(runtime.session().state().selectedQuick,-1);break;}
            const auto gates=scene->gates();const auto at=std::find_if(gates.begin(),gates.end(),[&](const auto& g){return g.definition.number==test.gate;});XCTAssertTrue(at!=gates.end());if(at==gates.end())continue;
            const auto gate=at->definition;const auto end=runtime.session().worldCanvasPoint(scene->camera().toScreen(gate.position));
            const auto start=test.path?runtime.session().worldCanvasPoint(scene->camera().toScreen({gate.position.x,gate.position.y+150})):end;
            XCTAssertTrue(runtime.input(hm2::PointerPhase::begin,start,9201));XCTAssertFalse(runtime.inputHistory().back().uiConsumed);XCTAssertTrue(runtime.inputHistory().back().worldConsumed);XCTAssertEqual(scene->dashState().movePointer,9201u);XCTAssertGreaterThan(scene->dashState().pathCount,0u);
            if(test.path){XCTAssertTrue(runtime.input(hm2::PointerPhase::move,end,9201));XCTAssertFalse(runtime.inputHistory().back().uiConsumed);}
            XCTAssertTrue(runtime.input(hm2::PointerPhase::end,end,9201));XCTAssertFalse(runtime.inputHistory().back().uiConsumed);XCTAssertEqual(scene->dashState().movePointer,0u);
            runtime.captureNextFrame();runtime.draw(layer);NSData* pixels=runtime.pixels();XCTAssertEqual(pixels.length,912u*420u*4u);if(pixels.length){XCTAttachment* attachment=[XCTAttachment attachmentWithImage:image(pixels,CGSizeMake(912,420))];attachment.name=test.path?@"Phone-original-path-portal":@"Phone-bottom-portal";attachment.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:attachment];}
            unsigned ticks=0;while(!runtime.session().mapLoading()&&runtime.session().screenTransition().phase==8&&ticks<600){runtime.tick();runtime.draw(layer);++ticks;}
            XCTAssertLessThan(ticks,600u);XCTAssertTrue(runtime.session().mapLoading()!=nullptr||runtime.session().screenTransition().phase!=8);
            auto live=hm2::Bytes(runtime.session().workingSlot().begin(),runtime.session().workingSlot().end());hm2::progression::SlotView slot(live);XCTAssertTrue(slot.string(0x2d,16)==gate.linkMap);
            // Finish the real loader/doors and decode the actual atomic save.
            const bool base=gate.linkMap.starts_with("Base");ticks=0;
            while((runtime.session().mapLoading()||runtime.session().screenTransition().phase!=8||runtime.session().state().screen!=(base?hm2::ui::ScreenId::base:hm2::ui::ScreenId::field))&&ticks<600){runtime.tick();runtime.draw(layer);++ticks;}
            XCTAssertLessThan(ticks,600u);XCTAssertTrue(runtime.session().mapLoading()==nullptr);XCTAssertEqual(runtime.session().screenTransition().phase,8);
            if(base)XCTAssertTrue(runtime.session().battle()==nullptr);else{XCTAssertNotEqual(runtime.session().battle(),nullptr);if(runtime.session().battle()){XCTAssertFalse(runtime.session().battle()->isPaused());XCTAssertGreaterThan(runtime.session().battle()->playerHP(),0);}}
            NSData* persisted=[NSData dataWithContentsOfURL:save];XCTAssertEqual(persisted.length,hm2::SaveData::byteCount);
            if(persisted.length==hm2::SaveData::byteCount){hm2::DarwinWorldRandom check(99);auto decoded=hm2::SaveData::decode({static_cast<const std::uint8_t*>(persisted.bytes),persisted.length},check,false);hm2::progression::SlotView disk(decoded.slot(0));XCTAssertTrue(disk.string(0x2d,16)==gate.linkMap);XCTAssertEqual(disk.u32(0x68),unsigned(gate.linkGate));}

        }catch(const std::exception& e){XCTFail(@"Phone bottom HUD/portal regression failed: %s",e.what());}
        XCTAssertTrue([NSFileManager.defaultManager removeItemAtURL:directory error:&error]);
    }}window.hidden=YES;
}
- (void)testPhoneLoweredCameraOriginalLayerCoverage {
    // Investigation only: manually supplied camera snapshots do not change
    // Scene/CameraPresentation, map data, gates, viewport, UVs or source sizes.
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    UIWindow* window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];UIViewController* controller=[UIViewController new];
    HM2SessionSurface* surface=[[HM2SessionSurface alloc]initWithFrame:CGRectMake(0,0,912,420)];surface.contentScaleFactor=1;
    CAEAGLLayer* layer=(CAEAGLLayer*)surface.layer;layer.opaque=YES;layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};controller.view=surface;window.rootViewController=controller;[window makeKeyAndVisible];surface.frame=CGRectMake(0,0,912,420);[surface layoutIfNeeded];
    try {
        hm2::apple::Files files(root.fileSystemRepresentation);const auto maps=hm2::Maps::parse(hm2::Chunk::parse(files.read("MapInfo.mcb")));
        HM2Renderer* renderer=[[HM2Renderer alloc]initWithResourceRoot:root];HUDIdentityHost host(renderer);
        hm2::PreparedTiles tiles("lower-camera-coverage");hm2::ui::Screens ui(files,tiles,host);const auto display=hm2::DisplayConfiguration::surface({912,420},{68,20,68,0});ui.setDisplay(display);
        hm2::ui::ScreensState state;state.screen=hm2::ui::ScreenId::field;state.locale="kr";ui.refresh(state,0);hm2::Frame preparation;ui.appendPreparation(preparation);[renderer prepareFrame:preparation];
        hm2::ui::TextBitmap marker;marker.layout.width=marker.layout.height=1;marker.layout.content={1,1};marker.alpha={255};const std::string markerName="@text/camera-coverage-marker";[renderer prepareText:marker identifier:markerName];
        hm2::Sprite markerSprite;markerSprite.image=markerName;markerSprite.center={456,210};markerSprite.size={912,420};markerSprite.color={1,0,1,1};
        const auto capture=[&](const hm2::Frame& frame){[renderer prepareFrame:frame];[renderer drawFrame:frame layer:layer];[renderer captureNextFrame];[renderer drawFrame:frame layer:layer];NSData* bytes=[renderer.readRGBA copy];XCTAssertEqual(bytes.length,912u*420u*4u);return bytes;};
        NSMutableArray* observations=[NSMutableArray new];unsigned mapCount=0,backgrounds=0,loweredLeaking=0,loweredHUDLeaking=0,extraBackgroundScales=0,originalSizeLoweredGaps=0,originalSizeLoweredHUDGaps=0;
        for(const auto& [name,map]:maps.all())if(name.starts_with("Stage")) {@autoreleasepool {
            (void)map;++mapCount;hm2::MapPreview preview(files,name);const bool hasBG=files.exists(name+"BG.jpg");backgrounds+=hasBG;
            for(unsigned backdropMode=0;backdropMode<(hasBG?2u:1u);++backdropMode)for(float drop:{0.f,94.f})for(bool drawHUD:{false,true}) {
                hm2::MapCamera camera;camera.viewport={912,420};camera.center={0,-301-drop};camera.fillSurface=true;camera.backgroundWorld={camera.center.x*.94f,camera.center.y*.94f};
                auto frame=preview.compose(camera);
                if(backdropMode==1){auto originalCamera=camera;originalCamera.fillSurface=false;const auto original=preview.compose(originalCamera);frame.sprites.front()=original.sprites.front();const auto info=hm2::imageInfo(files.read(name+"BG.jpg"));hm2::VisualImage visual;visual.tile.image=name+"BG.jpg";visual.tile.content={float(info.width),float(info.height)};visual.tile.storage={1,1};while(visual.tile.storage.width<info.width)visual.tile.storage.width*=2;while(visual.tile.storage.height<info.height)visual.tile.storage.height*=2;visual.screenPosition=frame.sprites.front().center;visual.scale={2,2};frame.sprites.front().strip=hm2::originalTileSprite(visual).strip;}
                const auto groundInfo=hm2::imageInfo(files.read(name+(files.exists(name+".jpg")?".jpg":".png")));
                XCTAssertEqual(groundInfo.width,1024u);XCTAssertEqual(groundInfo.height,1024u);const auto& ground=frame.sprites.back();XCTAssertEqual(ground.size.width,1024.f);XCTAssertEqual(ground.size.height,1024.f);XCTAssertEqual(ground.strip.size(),4u);
                for(const auto& sprite:frame.sprites)for(const auto& vertex:sprite.strip){XCTAssertGreaterThanOrEqual(vertex.uv.x,0.f);XCTAssertLessThanOrEqual(vertex.uv.x,1.f);XCTAssertGreaterThanOrEqual(vertex.uv.y,0.f);XCTAssertLessThanOrEqual(vertex.uv.y,1.f);}
                float bgScale=0;if(hasBG){const auto info=hm2::imageInfo(files.read(name+"BG.jpg"));bgScale=frame.sprites.front().size.width/(float(info.width)*2);XCTAssertEqual(frame.sprites.front().size.height/(float(info.height)*2),bgScale);if(bgScale!=1)++extraBackgroundScales;}
                if(drawHUD)ui.append(frame,15);NSData* normal=capture(frame);auto marked=frame;marked.sprites.insert(marked.sprites.begin(),markerSprite);NSData* withMarker=capture(marked);
                const auto* a=static_cast<const unsigned char*>(normal.bytes);const auto* b=static_cast<const unsigned char*>(withMarker.bytes);unsigned exposed=0,blackExposed=0;unsigned minimum=420,maximum=0,minX=912,maxX=0;
                for(unsigned y=0;y<420;++y)for(unsigned x=0;x<912;++x){const auto at=(y*912+x)*4;const bool changes=std::abs(int(a[at])-int(b[at]))>1||std::abs(int(a[at+1])-int(b[at+1]))>1||std::abs(int(a[at+2])-int(b[at+2]))>1;if(changes){++exposed;minimum=std::min(minimum,y);maximum=std::max(maximum,y);minX=std::min(minX,x);maxX=std::max(maxX,x);if(unsigned(a[at])+a[at+1]+a[at+2]<=3)++blackExposed;}}
                // Baseline coverage and intentional lower-camera gaps are
                // assertions of the experiment, not relaxed release criteria.
                // This test verifies the measured investigation outcome. The strict
                // candidate criterion stays exposed==0 AND no extra BG scale;
                // a passing investigation is not a passing camera candidate.
                if(backdropMode==0){if(hasBG){XCTAssertEqual(blackExposed,0u);XCTAssertGreaterThan(exposed,0u);XCTAssertEqual(minX,911u);XCTAssertEqual(maxX,911u);}else if(drop==0)XCTAssertEqual(exposed,0u,@"Baseline opaque terrain %s",name.c_str());else {XCTAssertGreaterThan(exposed,30000u,@"Missing backdrop must remain observable %s",name.c_str());if(drawHUD)++loweredHUDLeaking;else++loweredLeaking;}}
                else {XCTAssertEqual(bgScale,1.f);if(drop==94){XCTAssertGreaterThan(exposed,0u,@"Original768-wide backdrop cannot fill912-wide lower gap %s",name.c_str());if(drawHUD)++originalSizeLoweredHUDGaps;else++originalSizeLoweredGaps;}}
                [observations addObject:@{@"map":@(name.c_str()),@"drop":@(drop),@"HUD":@(drawHUD),@"hasOriginalBackdrop":@(hasBG),@"additionalBackgroundScale":@(bgScale),@"originalBackdropSizeMode":@(backdropMode==1),@"strictNoExpansionCandidatePass":@(exposed==0&&bgScale<=1),@"exposedPixels":@(exposed),@"firstExposedColumn":@(minX),@"lastExposedColumn":@(maxX),@"blackExposedPixels":@(blackExposed),@"firstExposedRow":@(minimum),@"lastExposedRow":@(maximum),@"terrainWidth":@(groundInfo.width),@"terrainHeight":@(groundInfo.height)}];
                NSLog(@"HM2_LOWER_CAMERA map=%s drop=%.0f HUD=%d BG=%d mode=%u extraBGScale=%.5f exposed=%u black=%u rows=%u..%u columns=%u..%u",name.c_str(),drop,drawHUD,hasBG,backdropMode,bgScale,exposed,blackExposed,minimum,maximum,minX,maxX);
                if((name=="Stage01_01"||name=="Stage02_06")&&drawHUD){XCTAttachment* shot=[XCTAttachment attachmentWithImage:image(normal,renderer.drawableSize)];shot.name=[NSString stringWithFormat:@"camera-%s-drop%.0f-backdropMode%u-HUD",name.c_str(),drop,backdropMode];shot.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:shot];}
                [renderer retainPreparedTextures:marked];
            }
        }}
        XCTAssertEqual(mapCount,38u);XCTAssertEqual(backgrounds,5u);XCTAssertEqual(loweredLeaking,33u);XCTAssertEqual(loweredHUDLeaking,33u);XCTAssertEqual(extraBackgroundScales,20u);XCTAssertEqual(originalSizeLoweredGaps,5u);XCTAssertEqual(originalSizeLoweredHUDGaps,5u);
        NSDictionary* report=@{@"maps":@(mapCount),@"originalBackdrops":@(backgrounds),@"loweredMapsExposingGap":@(loweredLeaking),@"loweredMapsExposingGapWithHUD":@(loweredHUDLeaking),@"viewportWidth":@912,@"viewportHeight":@420,@"cameraDrop":@94,@"additionalBackgroundScales":@(extraBackgroundScales),@"originalSizeBackdropMapsExposingGap":@(originalSizeLoweredGaps),@"originalSizeBackdropMapsExposingGapWithHUD":@(originalSizeLoweredHUDGaps),@"observations":observations};
        NSData* data=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingSortedKeys error:nullptr];XCTAttachment* receipt=[XCTAttachment attachmentWithData:data uniformTypeIdentifier:@"public.json"];receipt.name=@"original-layer-lowered-camera-coverage-report";receipt.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:receipt];
    }catch(const std::exception& e){XCTFail(@"Lower camera resource investigation: %s",e.what());}
    window.hidden=YES;window.rootViewController=nil;
}
- (void)testCustomHUDIdentityMatchesDefaultPixelsAtFractionalBoundaries{
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    struct Case{hm2::DisplayConfiguration display;CGFloat drawableScale;};
    const Case cases[]={
        {hm2::DisplayConfiguration::surface({912,420},{68,20,68,0}),1},
        {hm2::DisplayConfiguration::surface({857,393},{59,17,60,0}),1},
        {hm2::DisplayConfiguration::surface({667,375}),1.5}
    };
    for(const auto& test:cases){@autoreleasepool{
        const auto canvas=test.display.canvas;const hm2::DisplayLayout layout(test.display);const auto base=layout.ui(hm2::DisplayRegion::bottom);
        UIWindow* window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];UIViewController* controller=[UIViewController new];
        HM2SessionSurface* surface=[[HM2SessionSurface alloc]initWithFrame:CGRectMake(0,0,canvas.width,canvas.height)];surface.contentScaleFactor=test.drawableScale;
        CAEAGLLayer* layer=(CAEAGLLayer*)surface.layer;layer.opaque=YES;layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};
        controller.view=surface;window.rootViewController=controller;[window makeKeyAndVisible];surface.frame=CGRectMake(0,0,canvas.width,canvas.height);[surface layoutIfNeeded];
        try{
            HM2Renderer* renderer=[[HM2Renderer alloc]initWithResourceRoot:root];HUDIdentityHost host(renderer);hm2::apple::Files files(root.fileSystemRepresentation);hm2::DarwinWorldRandom random(1);
            hm2::SaveData fixture;fixture.initializeSlot(0,"HUD","kr",1794268800);fixture.slot(0)[0x1b5c]=5;
            hm2::GameSession game(files,host,random,std::move(fixture));XCTAssertTrue(game.selectSlot(0));XCTAssertTrue(game.enterField("Stage01_01",0));
            auto state=game.state();XCTAssertFalse(state.systemVisible); // No editor tools or modal pixels.
            hm2::PreparedTiles defaultTiles("hud-default"),customTiles("hud-custom");hm2::ui::Screens original(files,defaultTiles,host);original.setDisplay(test.display);original.refresh(state,0);
            hm2::hud::Configuration identity;for(unsigned group=0;group<hm2::hud::count;++group){const auto b=hm2::hud::bounds[group];identity.positions[group]=hm2::hud::draggedOrigin(group,layout,base.map({b.x,b.y}));}
            host.savedLayout=hm2::hud::encode(identity);hm2::ui::Screens custom(files,customTiles,host);custom.setDisplay(test.display);custom.refresh(state,0);
            XCTAssertFalse(original.hudEditing());XCTAssertFalse(custom.hudEditing());XCTAssertTrue(custom.hudConfiguration().customized());
            for(const auto& key:custom.buttons())XCTAssertFalse(key.id>=9100&&key.id<=9103);
            hm2::Frame expected,actual;expected.width=actual.width=canvas.width;expected.height=actual.height=canvas.height;original.append(expected,0);custom.append(actual,0);
            hm2::Frame preparation;original.appendPreparation(preparation);custom.appendPreparation(preparation);[renderer prepareFrame:preparation];
            // Same context, immutable frames and real prepared resources. The
            // warm-up matches the existing first-use GPU stabilization checks.
            for(unsigned warm=0;warm<60;++warm){[renderer drawFrame:expected layer:layer];[renderer drawFrame:actual layer:layer];}
            const auto capture=[&](const hm2::Frame& frame)->NSData*{[renderer captureNextFrame];[renderer drawFrame:frame layer:layer];return [renderer.readRGBA copy];};
            NSData* reference=capture(expected);NSData* repeated=capture(expected);NSData* candidate=capture(actual);
            XCTAssertEqual(reference.length,repeated.length);XCTAssertEqual(reference.length,candidate.length);
            if(!reference.length||reference.length!=candidate.length||reference.length!=repeated.length)throw std::runtime_error("HUD identity capture sizes do not match");
            const auto* a=static_cast<const std::uint8_t*>(reference.bytes);const auto* b=static_cast<const std::uint8_t*>(candidate.bytes);const auto* r=static_cast<const std::uint8_t*>(repeated.bytes);
            std::uint64_t repeatDifferences=0,rgbPixels=0,alphaPixels=0;unsigned maximum=0;
            for(std::size_t i=0;i<reference.length;i+=4){bool rgb=false;for(unsigned channel=0;channel<4;++channel){repeatDifferences+=a[i+channel]!=r[i+channel];const auto delta=unsigned(std::abs(int(a[i+channel])-int(b[i+channel])));maximum=std::max(maximum,delta);if(channel<3)rgb|=delta!=0;else alphaPixels+=delta!=0;}rgbPixels+=rgb;}
            XCTAssertEqual(repeatDifferences,0u,@"Default frame itself was unstable at %.0fx%.0f scale%.2f",canvas.width,canvas.height,test.drawableScale);
            XCTAssertEqual(rgbPixels,0u,@"HUD identity RGB seam/overblend at %.0fx%.0f scale%.2f",canvas.width,canvas.height,test.drawableScale);
            XCTAssertEqual(alphaPixels,0u,@"HUD identity alpha overblend at %.0fx%.0f scale%.2f",canvas.width,canvas.height,test.drawableScale);
            NSLog(@"HM2_HUD_IDENTITY_PIXELS canvas=%.0fx%.0f drawable=%@ scale=%.3f defaultSprites=%zu customSprites=%zu repeatByteDifferences=%llu rgbDifferentPixels=%llu alphaDifferentPixels=%llu maxDelta=%u",canvas.width,canvas.height,NSStringFromCGSize(renderer.drawableSize),test.drawableScale,expected.sprites.size(),actual.sprites.size(),repeatDifferences,rgbPixels,alphaPixels,maximum);
            if(rgbPixels||alphaPixels||repeatDifferences){
                for(const auto& entry:{std::pair{reference,@"default"},std::pair{candidate,@"custom-identity"}}){XCTAttachment* attachment=[XCTAttachment attachmentWithImage:image(entry.first,renderer.drawableSize)];attachment.name=[NSString stringWithFormat:@"HUD-%.0fx%.0f-scale%.2f-%@",canvas.width,canvas.height,test.drawableScale,entry.second];attachment.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:attachment];}
            }
        }catch(const std::exception& e){XCTFail(@"HUD identity GPU regression failed: %s",e.what());}
        window.hidden=YES;window.rootViewController=nil;
    }}
}
- (void)testHelpOwnerBoundariesPrepareAndReleaseOnlyHelpTextures{
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    NSURL* directory=[[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES] URLByAppendingPathComponent:NSUUID.UUID.UUIDString isDirectory:YES];NSError* error=nil;
    XCTAssertTrue([NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error]);
    NSURL* save=[directory URLByAppendingPathComponent:@"HeavyMach2.sav"];hm2::SaveData fixture;fixture.initializeSlot(0,"HELP","kr",1794268800);fixture.slot(0)[0x1b5c]=5;
    hm2::DarwinWorldRandom random(1);const auto bytes=fixture.encode(random,1);XCTAssertTrue([[NSData dataWithBytes:bytes.data() length:bytes.size()] writeToURL:save atomically:YES]);
    try{
        hm2::apple::GameRuntime runtime(root,save,{});XCTAssertTrue(runtime.session().selectSlot(0));XCTAssertTrue(runtime.session().enterField("Stage01_01",0));runtime.synchronize();settle(runtime);
        XCTAssertEqual(runtime.session().help(),nullptr);
        // Warm the real system menu once. Its PNGs belong to the persistent
        // GuiGame manager, while its five live text bitmaps end on closing.
        XCTAssertTrue(tap(runtime,1021));XCTAssertTrue(tap(runtime,1030));
        const auto baseline=runtime.resourceMetrics();
        const auto resident=[baseline[@"residentTextures"] unsignedIntegerValue],uploads=runtime.textureUploads();
        const auto imageUploads=[baseline[@"imageTextureUploads"] unsignedIntegerValue],baseText=[baseline[@"textBitmaps"] unsignedIntegerValue];
        const auto saves=runtime.session().metrics().saveRequests;std::string previousOwner;
        for(unsigned cycle=0;cycle<8;++cycle){
            const auto before=runtime.session().resourceRevision();XCTAssertTrue(tap(runtime,1021));
            const auto menu=runtime.resourceMetrics();const auto menuText=[menu[@"textBitmaps"] unsignedIntegerValue]-baseText;
            XCTAssertGreaterThan(menuText,0u);XCTAssertEqual([menu[@"residentTextures"] unsignedIntegerValue],resident+menuText);
            XCTAssertTrue(tap(runtime,1029));
            const auto* help=runtime.session().help();XCTAssertNotEqual(help,nullptr);if(!help)throw std::runtime_error("Actual system Help button did not create GuiHelp");
            const auto owner=help->tiles().identity();XCTAssertNotEqual(owner,previousOwner);previousOwner=owner;
            const auto opened=runtime.session().resourceRevision();XCTAssertGreaterThan(opened,before);
            XCTAssertEqual(help->tiles().all().size(),4u); // Three locale pages + original HelpHelp guidance.
            XCTAssertEqual([runtime.resourceMetrics()[@"residentTextures"] unsignedIntegerValue],resident+4);
            XCTAssertEqual([runtime.resourceMetrics()[@"imageTextureUploads"] unsignedIntegerValue],imageUploads+(cycle+1)*4);
            for(unsigned press=0;press<2;++press){runtime.input(hm2::PointerPhase::begin,{384,512},700+press);runtime.input(hm2::PointerPhase::end,{384,512},700+press);}
            XCTAssertEqual(runtime.session().help(),nullptr);XCTAssertGreaterThan(runtime.session().resourceRevision(),opened);
            XCTAssertEqual([runtime.resourceMetrics()[@"residentTextures"] unsignedIntegerValue],resident);
            XCTAssertEqual([runtime.resourceMetrics()[@"textBitmaps"] unsignedIntegerValue],baseText);
            XCTAssertEqual([runtime.resourceMetrics()[@"textureDeletes"] unsignedIntegerValue],[menu[@"textureDeletes"] unsignedIntegerValue]+menuText+4);
            const auto stableUploads=runtime.textureUploads(),stableDeletes=[runtime.resourceMetrics()[@"textureDeletes"] unsignedIntegerValue];runtime.synchronize();runtime.synchronize();
            XCTAssertEqual(runtime.textureUploads(),stableUploads);XCTAssertEqual([runtime.resourceMetrics()[@"textureDeletes"] unsignedIntegerValue],stableDeletes);
        }
        XCTAssertEqual(runtime.session().metrics().saveRequests,saves);
        NSLog(@"HM2_HELP_OWNER_LIFETIME cycles=8 texturesPerOwner=4 uploads=%lu deletes=%@ baselineResident=%lu finalResident=%@",(unsigned long)(runtime.textureUploads()-uploads),runtime.resourceMetrics()[@"textureDeletes"],(unsigned long)resident,runtime.resourceMetrics()[@"residentTextures"]);
    }catch(const std::exception& e){XCTFail(@"Actual Help resource owner boundary failed: %s",e.what());}
    XCTAssertTrue([NSFileManager.defaultManager removeItemAtURL:directory error:&error]);
}
- (void)testNormalStartupPreparesOriginalGlobalAndTitleSoundReferences{
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    NSURL* directory=[[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES] URLByAppendingPathComponent:NSUUID.UUID.UUIDString isDirectory:YES];NSError* error=nil;
    XCTAssertTrue([NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error]);
    try{
        hm2::apple::GameRuntime runtime(root,[directory URLByAppendingPathComponent:@"HeavyMach2.sav"],{});
        const auto ordered=runtime.session().orderedStartupSounds();XCTAssertEqual(ordered.size(),237u);
        std::set<std::string> global(ordered.begin(),ordered.end());XCTAssertEqual(global.size(),48u);
        for(const auto* name:{"Title_TitleIn.wav","Title_Bang.wav","Title_TapToStart.wav"})global.emplace(name);
        XCTAssertEqual(global.size(),50u);NSDictionary* before=runtime.audioMetrics();
        XCTAssertEqual([before[@"loadedSounds"] unsignedIntegerValue],global.size());
        XCTAssertEqual([before[@"effectFileReads"] unsignedIntegerValue],global.size());
        XCTAssertEqual([before[@"contextsCreated"] unsignedIntegerValue],1u);
        XCTAssertEqual([before[@"sourcesCreated"] unsignedIntegerValue],global.size());
        XCTAssertEqual([before[@"effectPlayRequests"] unsignedIntegerValue],0u); // Preparing is not playback.
        runtime.synchronize();runtime.synchronize();NSDictionary* after=runtime.audioMetrics();
        for(NSString* key in @[@"loadedSounds",@"effectFileReads",@"effectDecodes",@"sourcesCreated",@"buffersCreated",@"contextsCreated"])
            XCTAssertEqualObjects(before[key],after[key],@"Scalar title refresh repeated original sound preparation: %@",key);
    }catch(const std::exception& e){XCTFail(@"Original startup sound references failed: %s",e.what());}
    XCTAssertTrue([NSFileManager.defaultManager removeItemAtURL:directory error:&error]);
}
- (void)testEnemyLabelsAndNestedMenuCallbacksReleaseOnlyDeadText{
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    NSURL* directory=[[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES] URLByAppendingPathComponent:NSUUID.UUID.UUIDString isDirectory:YES];NSError* error=nil;
    XCTAssertTrue([NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error]);const auto save=[directory URLByAppendingPathComponent:@"HeavyMach2.sav"];
    hm2::SaveData fixture;fixture.initializeSlot(0,"TEXT","kr",1794268800);fixture.slot(0)[0x1b5c]=5;hm2::DarwinWorldRandom random(1);const auto bytes=fixture.encode(random,1);
    XCTAssertTrue([[NSData dataWithBytes:bytes.data() length:bytes.size()] writeToURL:save atomically:YES]);
    try{
        hm2::apple::GameRuntime runtime(root,save,{});XCTAssertTrue(runtime.session().selectSlot(0));XCTAssertTrue(runtime.session().enterField("Stage02_01",0));runtime.synchronize();settle(runtime);runtime.tick();runtime.tick();
        auto* scene=runtime.session().battle();XCTAssertNotEqual(scene,nullptr);if(!scene)return;
        std::map<std::string,std::uint64_t> targets;for(const auto& enemy:scene->enemies())if(enemy.hp>0&&!enemy.corpse)targets.emplace(enemy.name,enemy.id);
        XCTAssertGreaterThanOrEqual(targets.size(),2u);if(targets.size()<2)return;
        const auto baselineText=runtime.preparedTextCount();const auto baselineTextures=[runtime.resourceMetrics()[@"residentTextures"] unsignedIntegerValue];
        const auto initialRevision=runtime.session().presentationRevision();unsigned switches=0;
        for(unsigned round=0;round<8;++round)for(const auto& [name,id]:targets){
            (void)name;XCTAssertTrue(scene->selectEnemy(id));runtime.synchronize();++switches;
            // Target-name changes intentionally do not rebuild GameSession UI.
            XCTAssertEqual(runtime.session().presentationRevision(),initialRevision);
            XCTAssertLessThanOrEqual(runtime.preparedTextCount(),baselineText+1);
            XCTAssertLessThanOrEqual([runtime.resourceMetrics()[@"residentTextures"] unsignedIntegerValue],baselineTextures+1);
            const auto uploads=runtime.textureUploads();XCTAssertTrue(scene->selectEnemy(id));runtime.synchronize();XCTAssertEqual(runtime.textureUploads(),uploads);
        }
        // First opening legitimately prepares the original menu image in its
        // surviving field owner. It must not retain dead menu Text nodes.
        XCTAssertTrue(tap(runtime,1021));XCTAssertTrue(tap(runtime,1030));
        XCTAssertLessThanOrEqual(runtime.preparedTextCount(),baselineText+1);
        const auto menuWarmTextures=[runtime.resourceMetrics()[@"residentTextures"] unsignedIntegerValue];
        for(unsigned round=0;round<8;++round){
            XCTAssertTrue(tap(runtime,1021));XCTAssertTrue(scene->isPaused());
            XCTAssertTrue(tap(runtime,1030));XCTAssertFalse(scene->isPaused());
            // Screens.input keeps the replaced old Impl during its nested
            // action callback. Cleanup must run again after that owner dies.
            XCTAssertLessThanOrEqual(runtime.preparedTextCount(),baselineText+1);
            XCTAssertLessThanOrEqual([runtime.resourceMetrics()[@"residentTextures"] unsignedIntegerValue],menuWarmTextures);
        }
        NSLog(@"HM2_TEXT_OWNER_RELEASE realTargetSwitches=%u text=%lu resident=%@ deletes=%@ nestedMenus=8 sameNameReuse=1",switches,(unsigned long)runtime.preparedTextCount(),runtime.resourceMetrics()[@"residentTextures"],runtime.resourceMetrics()[@"textureDeletes"]);
    }catch(const std::exception& e){XCTFail(@"Text owner lifetime regression failed: %s",e.what());}
    XCTAssertTrue([NSFileManager.defaultManager removeItemAtURL:directory error:&error]);
}
- (void)testWorldPointerEndsInsidePausedSystemMenu{
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    NSURL* directory=[[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES] URLByAppendingPathComponent:NSUUID.UUID.UUIDString isDirectory:YES];NSError* error=nil;
    XCTAssertTrue([NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error]);
    const auto save=[directory URLByAppendingPathComponent:@"HeavyMach2.sav"];
    hm2::SaveData fixture;fixture.initializeSlot(0,"INPUT","kr",1794268800);fixture.slot(0)[0x1b5c]=5;hm2::DarwinWorldRandom random(1);const auto bytes=fixture.encode(random,1);
    XCTAssertTrue([[NSData dataWithBytes:bytes.data() length:bytes.size()] writeToURL:save atomically:YES]);
    try{
        hm2::apple::GameRuntime runtime(root,save,{});XCTAssertTrue(runtime.session().selectSlot(0));XCTAssertTrue(runtime.session().enterField("Stage01_01",0));runtime.synchronize();settle(runtime);
        if(runtime.session().help()){runtime.input(hm2::PointerPhase::begin,{700,512},10);runtime.input(hm2::PointerPhase::end,{700,512},10);}
        auto* scene=runtime.session().battle();XCTAssertNotEqual(scene,nullptr);if(!scene)return;
        hm2::Vec2 ground{};bool found=false;
        for(float y=180;y<960&&!found;y+=60)for(float x=60;x<720&&!found;x+=60){
            const hm2::Vec2 screen{x,y},world=scene->camera().toWorld(screen),player=scene->player().position;
            const auto distance=[](hm2::Vec2 a,hm2::Vec2 b){const auto dx=a.x-b.x,dy=a.y-b.y;return dx*dx+dy*dy;};
            if(distance(world,player)<10000)continue;bool clear=true;for(const auto& enemy:scene->enemies())if(enemy.hp>0&&distance(world,enemy.position)<10000)clear=false;
            if(clear){ground=screen;found=true;}
        }
        XCTAssertTrue(found);if(!found)return;
        XCTAssertTrue(runtime.input(hm2::PointerPhase::begin,ground,9001));XCTAssertEqual(scene->dashState().movePointer,9001u);
        XCTAssertTrue(tap(runtime,1021));XCTAssertTrue(scene->isPaused());XCTAssertTrue(runtime.session().state().systemVisible);
        const auto position=scene->player().position;const auto selected=scene->selectedEnemy();const auto special=scene->dashState().special;const auto time=scene->clock().worldTime();const auto dashStarts=scene->metrics().dashStarts;
        XCTAssertTrue(runtime.input(hm2::PointerPhase::end,ground,9001));XCTAssertEqual(scene->dashState().movePointer,0u);
        const auto history=runtime.inputHistory();XCTAssertFalse(history.empty());if(!history.empty()){XCTAssertTrue(history.back().uiConsumed);XCTAssertTrue(history.back().terminalCancelled);}
        XCTAssertEqual(scene->dashState().special,special); // Cancel itself never spends SP.
        for(unsigned i=0;i<30;++i)runtime.tick();
        const auto& stats=runtime.session().state().equipmentStats;
        XCTAssertTrue(scene->isPaused());XCTAssertEqual(scene->clock().worldTime(),time);XCTAssertEqual(scene->selectedEnemy(),selected);XCTAssertEqual(scene->dashState().special,std::min(stats.boosterMaximum,special+30*stats.boosterRegen));XCTAssertEqual(scene->metrics().dashStarts,dashStarts);XCTAssertEqual(scene->player().position.x,position.x);XCTAssertEqual(scene->player().position.y,position.y);
    }catch(const std::exception& e){XCTFail(@"World/UI ownership regression failed: %s",e.what());}
    XCTAssertTrue([NSFileManager.defaultManager removeItemAtURL:directory error:&error]);
}
- (void)testAudioInterruptionKeepsMenusAndDoorsLiveAndRequiresWorldResume{
    self.continueAfterFailure=NO;
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    NSURL* directory=[[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES] URLByAppendingPathComponent:NSUUID.UUID.UUIDString isDirectory:YES];NSError* error=nil;
    XCTAssertTrue([NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error]);
    NSURL* save=[directory URLByAppendingPathComponent:@"HeavyMach2.sav"];
    hm2::SaveData fixture;fixture.initializeSlot(0,"AUDIO","kr",1794268800);fixture.slot(0)[0x1b5c]=5;
    hm2::DarwinWorldRandom fixtureRandom(1);const auto encoded=fixture.encode(fixtureRandom,1);
    XCTAssertTrue([[NSData dataWithBytes:encoded.data() length:encoded.size()] writeToURL:save atomically:YES]);
    UIWindow* window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];UIViewController* controller=[UIViewController new];
    HM2SessionSurface* surface=[[HM2SessionSurface alloc]initWithFrame:CGRectMake(0,0,384,512)];surface.contentScaleFactor=1;
    CAEAGLLayer* layer=(CAEAGLLayer*)surface.layer;layer.opaque=YES;layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};
    controller.view=surface;window.rootViewController=controller;[window makeKeyAndVisible];surface.frame=CGRectMake(0,0,384,512);[surface layoutIfNeeded];
    try{
        hm2::apple::GameRuntime runtime(root,save,{},[]{return 1u;});XCTAssertTrue(runtime.session().selectSlot(0));runtime.session().enterBase();runtime.synchronize();settle(runtime,layer);
        const auto base=runtime.session().state();XCTAssertEqual(base.screen,hm2::ui::ScreenId::base);XCTAssertEqual(base.baseId,std::string("Base001"));XCTAssertEqual(runtime.session().help(),nullptr);
        NSDictionary* audio=runtime.audioMetrics();XCTAssertEqual([audio[@"contextsCreated"] unsignedIntegerValue],1u);XCTAssertNotEqual([audio[@"ownerContext"] unsignedLongLongValue],0ull);
        const auto checkAudioOwner=[&]{
            NSDictionary* now=runtime.audioMetrics();
            for(NSString* key in @[@"ownerContext",@"contextsCreated",@"devicesOpened",@"sourcesCreated",@"buffersCreated",@"sourcesDeleted",@"buffersDeleted",@"loadedSounds",@"effectFileReads",@"effectDecodes"])
                XCTAssertEqualObjects(audio[key],now[key],@"Audio interruption changed persistent SFX owner/cache %@",key);
        };
        const auto saves=runtime.session().metrics().saveRequests;const float menuTime=runtime.session().renderTime();
        runtime.setAudioActive(false);
        for(unsigned frame=0;frame<12;++frame){runtime.tick();runtime.draw(layer);}
        XCTAssertGreaterThan(runtime.session().renderTime(),menuTime);
        XCTAssertEqual(runtime.session().metrics().saveRequests,saves);checkAudioOwner();
        XCTAssertTrue(tap(runtime,2002,layer));XCTAssertTrue(tap(runtime,2002,layer));
        XCTAssertEqual(runtime.session().state().screen,hm2::ui::ScreenId::shop);XCTAssertEqual(runtime.session().help(),nullptr);
        XCTAssertEqual(runtime.session().screenTransition().phase,8);XCTAssertEqual(runtime.session().metrics().saveRequests,saves);
        const float shopTime=runtime.session().renderTime();XCTAssertTrue(tap(runtime,2100,layer));
        XCTAssertEqual(runtime.session().state().screen,hm2::ui::ScreenId::base);XCTAssertEqual(runtime.session().screenTransition().phase,8);
        XCTAssertGreaterThan(runtime.session().renderTime(),shopTime);XCTAssertEqual(runtime.session().state().baseId,std::string("Base001"));
        XCTAssertEqual(runtime.session().state().credits,base.credits);XCTAssertEqual(runtime.session().state().hpRate,base.hpRate);
        XCTAssertEqual(runtime.session().metrics().saveRequests,saves+1);XCTAssertEqual(runtime.session().metrics().saveCompletions,saves+1);
        NSData* written=[NSData dataWithContentsOfURL:save];XCTAssertNotNil(written);XCTAssertEqual(written.length,hm2::SaveData::byteCount);
        hm2::DarwinWorldRandom decodeRandom(1);const auto decoded=hm2::SaveData::decode({static_cast<const std::uint8_t*>(written.bytes),written.length},decodeRandom,false);
        const auto disk=decoded.slot(0),memory=runtime.session().envelope().slot(0);
        XCTAssertTrue(std::equal(disk.begin(),disk.end(),memory.begin(),memory.end()));
        XCTAssertEqualObjects(runtime.audioMetrics()[@"effectPlayRequests"],audio[@"effectPlayRequests"]); // Muted menu/door SFX never reach playback.
        runtime.setAudioActive(true);checkAudioOwner();XCTAssertEqualObjects(runtime.audioMetrics()[@"currentContext"],audio[@"ownerContext"]);

        // The same live Runtime now exercises the battlefield interruption.
        // No live HP, world clock, RNG or game outcome is assigned by the test.
        XCTAssertTrue(runtime.session().enterField("Stage01_01",0));runtime.synchronize();settle(runtime,layer);
        auto* scene=runtime.session().battle();XCTAssertNotEqual(scene,nullptr);if(!scene)throw std::runtime_error("Audio interruption field was not created");
        const auto fieldSaves=runtime.session().metrics().saveRequests;const auto position=scene->player().position;const auto worldTime=scene->clock().worldTime();const auto hp=scene->playerHP();const auto fieldTime=runtime.session().renderTime();
        runtime.setAudioActive(false);XCTAssertTrue(scene->isPaused());XCTAssertTrue(runtime.session().state().systemVisible);
        for(unsigned frame=0;frame<12;++frame){runtime.tick();runtime.draw(layer);}
        XCTAssertEqual(runtime.session().battle(),scene);XCTAssertTrue(scene->isPaused());XCTAssertTrue(runtime.session().state().systemVisible);
        XCTAssertEqual(scene->clock().worldTime(),worldTime);XCTAssertEqual(scene->player().position.x,position.x);XCTAssertEqual(scene->player().position.y,position.y);XCTAssertEqual(scene->playerHP(),hp);
        XCTAssertGreaterThan(runtime.session().renderTime(),fieldTime);XCTAssertEqual(runtime.session().metrics().saveRequests,fieldSaves);checkAudioOwner();
        runtime.setAudioActive(true);checkAudioOwner();XCTAssertTrue(scene->isPaused());XCTAssertTrue(runtime.session().state().systemVisible);
        const float restoredTime=runtime.session().renderTime();for(unsigned frame=0;frame<12;++frame){runtime.tick();runtime.draw(layer);}
        XCTAssertEqual(scene->clock().worldTime(),worldTime);XCTAssertEqual(scene->player().position.x,position.x);XCTAssertEqual(scene->player().position.y,position.y);XCTAssertEqual(scene->playerHP(),hp);
        XCTAssertGreaterThan(runtime.session().renderTime(),restoredTime);XCTAssertTrue(tap(runtime,1030,layer));XCTAssertFalse(scene->isPaused());XCTAssertFalse(runtime.session().state().systemVisible);
        runtime.tick();runtime.draw(layer);XCTAssertGreaterThan(scene->clock().worldTime(),worldTime);XCTAssertEqual(runtime.session().metrics().saveRequests,fieldSaves);checkAudioOwner();
        XCTAssertEqualObjects(runtime.audioMetrics()[@"currentContext"],audio[@"ownerContext"]);XCTAssertEqual(runtime.session().metrics().unsupportedActions,0u);
        NSLog(@"HM2_AUDIO_UI_LIFECYCLE mutedShopDoors=1 originalBaseSave=1 pausedFieldRenderTicks=24 explicitResume1030=1 persistentSFXContext1=1 cacheRereads=0");
    }catch(const std::exception& e){XCTFail(@"Audio/UI lifecycle regression failed: %s",e.what());}
    window.hidden=YES;window.rootViewController=nil;XCTAssertTrue([NSFileManager.defaultManager removeItemAtURL:directory error:&error]);
}
- (void)testPreparedTextureOwnersReleaseAndReload{
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    HM2Renderer* renderer=[[HM2Renderer alloc]initWithResourceRoot:root];
    hm2::Frame both;for(const auto* name:{"Main_BG.png","StageClearBG.png"}){hm2::Sprite sprite;sprite.image=name;both.sprites.push_back(std::move(sprite));}
    [renderer prepareFrame:both];XCTAssertEqual(renderer.residentTextures,2u);const auto owner=renderer.contextIdentity;
    const auto uploads=renderer.textureUploads,bytes=renderer.residentTextureBytes;
    hm2::Frame keep;keep.sprites.push_back(both.sprites.front());[renderer retainPreparedTextures:keep];
    XCTAssertEqual(renderer.residentTextures,1u);XCTAssertEqual(renderer.textureDeletes,1u);XCTAssertLessThan(renderer.residentTextureBytes,bytes);
    [renderer prepareFrame:keep];XCTAssertEqual(renderer.textureUploads,uploads); // Same live owner reuses the texture.
    [renderer prepareFrame:both];XCTAssertEqual(renderer.textureUploads,uploads+1);XCTAssertEqual(renderer.residentTextures,2u);
    hm2::Frame none;[renderer retainPreparedTextures:none];XCTAssertEqual(renderer.residentTextures,0u);XCTAssertEqual(renderer.residentTextureBytes,0u);XCTAssertEqual(renderer.textureDeletes,3u);
    XCTAssertEqual(renderer.contextIdentity,owner); // Resource owners do not recreate the graphics context.
}
- (void)testFiniteTerrainQuadMatchesExplicitUVs {
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;UIWindow* window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];UIViewController* controller=[UIViewController new];HM2SessionSurface* surface=[[HM2SessionSurface alloc]initWithFrame:CGRectMake(0,0,912,420)];surface.contentScaleFactor=1;CAEAGLLayer* layer=(CAEAGLLayer*)surface.layer;layer.opaque=YES;layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};controller.view=surface;window.rootViewController=controller;[window makeKeyAndVisible];surface.frame=CGRectMake(0,0,912,420);[surface layoutIfNeeded];
    try{HM2Renderer* renderer=[[HM2Renderer alloc]initWithResourceRoot:root];hm2::Sprite ground;ground.image="Stage01_01.jpg";hm2::Frame prep;prep.sprites.push_back(ground);[renderer prepareFrame:prep];
        for(unsigned sample=0;sample<33;++sample){const float scale=sample==32?958.3923f/1024:.892368f+(1-.892368f)*float(sample)/31;ground.size={1024*scale,1024*scale};ground.center={511*scale,511*scale};ground.clip=hm2::ClipRect{0,0,912,420};hm2::Frame simple;simple.width=912;simple.height=420;simple.sprites.push_back(ground);auto explicitFrame=simple;auto& sprite=explicitFrame.sprites[0];const auto x=ground.center.x,y=ground.center.y,w=ground.size.width*.5f,h=ground.size.height*.5f;sprite.strip={{{x-w,y-h},{0,1}},{{x+w,y-h},{1,1}},{{x-w,y+h},{0,0}},{{x+w,y+h},{1,0}}};
            [renderer captureNextFrame];[renderer drawFrame:simple layer:layer];NSData* actual=[renderer.readRGBA copy];[renderer captureNextFrame];[renderer drawFrame:explicitFrame layer:layer];NSData* expected=[renderer.readRGBA copy];XCTAssertEqual(actual.length,expected.length);std::size_t different=0;unsigned maxDelta=0;const auto* a=static_cast<const unsigned char*>(actual.bytes);const auto* b=static_cast<const unsigned char*>(expected.bytes);for(std::size_t i=0;i<std::min(actual.length,expected.length);++i){different+=a[i]!=b[i];maxDelta=std::max(maxDelta,unsigned(std::abs(int(a[i])-int(b[i]))));}NSLog(@"HM2_TERRAIN_RASTER sample=%u scale=%.9f differentBytes=%zu maxChannelDelta=%u",sample,scale,different,maxDelta);/* Constant color vs interpolated vertex color can differ by one 8-bit rounding step. */XCTAssertLessThanOrEqual(maxDelta,1u,@"Finite map geometry %.9f",scale);if(maxDelta>1){auto* attachment=[XCTAttachment attachmentWithImage:image(actual,renderer.drawableSize)];attachment.name=@"terrain-simple-difference";attachment.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:attachment];}
        }NSLog(@"HM2_TERRAIN_EQUIVALENCE samples=33 simple-vs-explicit-UV");
    }catch(const std::exception& e){XCTFail(@"Terrain quad: %s",e.what());}window.hidden=YES;window.rootViewController=nil;
}
- (void)testPhoneOriginalArtworkAndCanvasHUDPlacement {
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    UIWindow* window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];UIViewController* controller=[UIViewController new];HM2SessionSurface* surface=[[HM2SessionSurface alloc]initWithFrame:CGRectMake(0,0,912,420)];surface.contentScaleFactor=1;
    CAEAGLLayer* layer=(CAEAGLLayer*)surface.layer;layer.opaque=YES;layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};controller.view=surface;window.rootViewController=controller;[window makeKeyAndVisible];surface.frame=CGRectMake(0,0,912,420);[surface layoutIfNeeded];
    try{
        const auto display=hm2::DisplayConfiguration::surface({912,420},{68,20,68,0});const hm2::DisplayLayout layout(display);HM2Renderer* renderer=[[HM2Renderer alloc]initWithResourceRoot:root];HUDIdentityHost host(renderer);hm2::apple::Files files(root.fileSystemRepresentation);hm2::PreparedTiles tiles("original-phone-art");hm2::ui::Screens ui(files,tiles,host);ui.setDisplay(display);hm2::DarwinWorldRandom random(1);
        bool fieldCapture=false;const auto capture=[&](NSString* label){hm2::Frame prep;ui.appendPreparation(prep);[renderer prepareFrame:prep];hm2::Frame f;if(fieldCapture){hm2::MapPreview preview(files,"Stage01_01");hm2::MapCamera c{{-55,-301},{},{912,420}};f=preview.compose(c);[renderer prepareFrame:f];}ui.append(f,15);for(unsigned warm=0;warm<3;++warm)[renderer drawFrame:f layer:layer];[renderer captureNextFrame];[renderer drawFrame:f layer:layer];XCTAssertEqual(renderer.readRGBA.length,912u*420u*4u);if(renderer.readRGBA.length!=912u*420u*4u)return;
            auto* shot=[XCTAttachment attachmentWithImage:image(renderer.readRGBA,renderer.drawableSize)];shot.name=label;shot.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:shot];
            const auto* p=static_cast<const std::uint8_t*>(renderer.readRGBA.bytes);unsigned colored[4]={};for(unsigned y=0;y<420;++y)for(unsigned side=0;side<2;++side){const auto at=(y*912+(side?911:0))*4;if(p[at]+p[at+1]+p[at+2]>8)++colored[side];}for(unsigned x=0;x<912;++x)for(unsigned side=0;side<2;++side){const auto at=((side?419:0)*912+x)*4;if(p[at]+p[at+1]+p[at+2]>8)++colored[2+side];}for(unsigned side=0;side<4;++side)XCTAssertGreaterThan(colored[side],side<2?210u:456u,@"%@ black border side%u",label,side);NSLog(@"HM2_PHONE_ART %@ edges=%u,%u,%u,%u",label,colored[0],colored[1],colored[2],colored[3]);};
        for(const auto screen:{hm2::ui::ScreenId::title,hm2::ui::ScreenId::slots,hm2::ui::ScreenId::command,hm2::ui::ScreenId::shop,hm2::ui::ScreenId::maintenance}){hm2::ui::ScreensState state;state.screen=screen;state.locale="kr";state.inventoryVisible=screen==hm2::ui::ScreenId::shop;if(screen==hm2::ui::ScreenId::shop||screen==hm2::ui::ScreenId::maintenance){state.inventoryPages=2;for(unsigned i=0;i<25;++i){hm2::ui::ItemView item;item.dataId="item"+std::to_string(i);item.count=1;item.icon=i%2?"Icon10011.png":"Icon00001.png";state.inventory.push_back(item);if(i<24)state.shopItems.push_back(item);}for(auto& item:state.equipment){item.count=1;item.dataId="equipment";item.icon="Icon10011.png";}for(auto& item:state.bots){item.count=1;item.dataId="bot";item.icon="Icon04201.png";}}ui.refresh(state,0);for(unsigned n=0;n<800;++n)ui.tick(float(n)/60,random);capture([NSString stringWithFormat:@"phone-original-art-screen-%u",unsigned(screen)]);if(screen==hm2::ui::ScreenId::shop){const auto press=[&](unsigned id){for(auto key:ui.buttons())if(key.id==id){ui.revealButton(key);const auto p=ui.buttonScreenPosition(key);ui.input(hm2::PointerPhase::begin,p,900+id,15);ui.input(hm2::PointerPhase::end,p,900+id,15.1f);return;}};press(9201);press(9201);press(9203);capture(@"phone-original-art-HD24-HD25-overflow");press(9204);capture(@"phone-original-art-equipment-tab-HD-icons");}}
        for(unsigned base=1;base<=6;++base){hm2::ui::ScreensState state;state.screen=hm2::ui::ScreenId::base;state.locale="kr";state.baseId="Base00"+std::to_string(base);ui.refresh(state,0);capture([NSString stringWithFormat:@"phone-original-art-base-%u",base]);for(const auto& key:ui.buttons()){ui.revealButton(key);const auto point=ui.buttonScreenPosition(key);XCTAssertGreaterThanOrEqual(point.y,0);XCTAssertLessThanOrEqual(point.y,420);}}
        hm2::ui::ScreensState state;state.screen=hm2::ui::ScreenId::field;state.systemVisible=true;state.hasWorkingSlot=true;state.fieldUIRevision=1;ui.refresh(state,0);XCTAssertTrue(ui.beginHUDEdit());const auto before=hm2::hud::visualBounds(ui.hudConfiguration(),hm2::hud::controls,layout);const hm2::Vec2 from{before.x+before.width*.5f,before.y+before.height*.5f},to{from.x+2000,from.y-2000};ui.input(hm2::PointerPhase::begin,from,200,0);ui.input(hm2::PointerPhase::move,to,200,.1f);ui.input(hm2::PointerPhase::end,to,200,.2f);const auto moved=hm2::hud::visualBounds(ui.hudConfiguration(),hm2::hud::controls,layout);XCTAssertEqualWithAccuracy(moved.y,0,.001);XCTAssertEqualWithAccuracy(moved.x+moved.width,912,.001);XCTAssertTrue(ui.applyHUDEdit());XCTAssertTrue(host.savedLayout.has_value());
        hm2::PreparedTiles restartedTiles("hud-edge-restart");hm2::ui::Screens restarted(files,restartedTiles,host);restarted.setDisplay(display);restarted.refresh(state,0);XCTAssertTrue(restarted.hudConfiguration()==ui.hudConfiguration());XCTAssertTrue(host.progressWrites.empty());fieldCapture=true;capture(@"phone-HUD-physical-bottom-right-saved");NSLog(@"HM2_HUD_CANVAS_EDGE drag=1 apply=1 reload=1 bottom=%.3f right=%.3f progressWrites=%zu",moved.y,moved.x+moved.width,host.progressWrites.size());
    }catch(const std::exception& e){XCTFail(@"Phone artwork/canvas HUD: %s",e.what());}
    window.hidden=YES;window.rootViewController=nil;
}
- (void)testPhoneFullScreenMenusAndBattleEdges {
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);
    NSURL* directory=[[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES] URLByAppendingPathComponent:NSUUID.UUID.UUIDString isDirectory:YES];NSError* error=nil;
    XCTAssertTrue([NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error]);
    UIWindow* window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];UIViewController* controller=[UIViewController new];
    HM2SessionSurface* surface=[[HM2SessionSurface alloc]initWithFrame:CGRectMake(0,0,912,420)];surface.contentScaleFactor=1;
    CAEAGLLayer* layer=(CAEAGLLayer*)surface.layer;layer.opaque=YES;layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};
    controller.view=surface;window.rootViewController=controller;[window makeKeyAndVisible];surface.frame=CGRectMake(0,0,912,420);[surface layoutIfNeeded];
    try{
        const auto config=hm2::DisplayConfiguration::surface({912,420},{68,20,68,0});
        hm2::SaveData fixture;fixture.initializeSlot(0,"FULL","kr",1794268800);fixture.slot(0)[0x1b5c]=5;hm2::DarwinWorldRandom random(1);const auto bytes=fixture.encode(random,1);
        const auto save=[directory URLByAppendingPathComponent:@"HeavyMach2.sav"];XCTAssertTrue([[NSData dataWithBytes:bytes.data() length:bytes.size()] writeToURL:save atomically:YES]);
        hm2::apple::GameRuntime runtime(root,save,{},[]{return 1u;});runtime.setDisplay(config);XCTAssertTrue(runtime.session().selectSlot(0));runtime.session().enterBase();runtime.synchronize();settle(runtime,layer);
        auto capture=[&](NSString* label){
            for(unsigned n=0;n<90;++n){runtime.tick();runtime.draw(layer);}
            {const auto f=runtime.session().drawWorld();if(!f.sprites.empty()){const auto& g=f.sprites.front();NSLog(@"HM2_TERRAIN %@ image=%s center=%.4f,%.4f size=%.4f,%.4f frame=%.4f,%.4f clip=%.4f,%.4f,%.4f,%.4f strip=%zu",label,g.image.c_str(),g.center.x,g.center.y,g.size.width,g.size.height,f.width,f.height,g.clip?g.clip->x:0,g.clip?g.clip->y:0,g.clip?g.clip->width:0,g.clip?g.clip->height:0,g.strip.size());}}
            runtime.captureNextFrame();runtime.draw(layer);const auto extent=runtime.drawableSize();XCTAssertEqual(extent.width,912);XCTAssertEqual(extent.height,420);
            NSData* rgba=runtime.pixels();XCTAssertEqual(rgba.length,912u*420u*4u);if(rgba.length!=912u*420u*4u)return;
            const auto* p=static_cast<const std::uint8_t*>(rgba.bytes);unsigned nonblack[4]={};
            for(unsigned y=0;y<420;++y)for(unsigned side=0;side<2;++side){const auto at=(std::size_t(y)*912+(side?911:0))*4;if(p[at]+p[at+1]+p[at+2]>8)++nonblack[side];}
            for(unsigned x=0;x<912;++x)for(unsigned side=0;side<2;++side){const auto at=(std::size_t(side?419:0)*912+x)*4;if(p[at]+p[at+1]+p[at+2]>8)++nonblack[side+2];}
            for(unsigned side=0;side<4;++side)XCTAssertGreaterThan(nonblack[side],side<2?210u:456u,@"%@ black border side%u",label,side);
            NSLog(@"HM2_FULLSCREEN %@ drawable=912x420 coloredEdges=%u,%u,%u,%u",label,nonblack[0],nonblack[1],nonblack[2],nonblack[3]);
            UIImage* shot=image(rgba,extent);if(shot){auto* a=[XCTAttachment attachmentWithImage:shot];a.name=label;a.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:a];}
        };
        auto press=[&](unsigned id){bool found=false;for(const auto& key:runtime.screens().buttons())if(key.id==id){runtime.screens().revealButton(key);const auto point=runtime.screens().buttonScreenPosition(key);runtime.input(hm2::PointerPhase::begin,point,800);runtime.input(hm2::PointerPhase::end,point,800);found=true;break;}XCTAssertTrue(found);settle(runtime,layer);};
        press(2001);capture(@"phone-fullscreen-base-description");press(2001);XCTAssertEqual(runtime.session().state().screen,hm2::ui::ScreenId::command);capture(@"phone-fullscreen-command");
        press(2100);press(2002);press(2002);XCTAssertEqual(runtime.session().state().screen,hm2::ui::ScreenId::shop);
        if(runtime.session().help()){runtime.input(hm2::PointerPhase::begin,{450,210},801);runtime.input(hm2::PointerPhase::end,{450,210},801);}
        capture(@"phone-fullscreen-shop");press(2200);capture(@"phone-fullscreen-selected-item");press(9204);capture(@"phone-fullscreen-equipment-real-data");press(9205);press(2100);XCTAssertEqual(runtime.session().state().screen,hm2::ui::ScreenId::base);
        XCTAssertTrue(runtime.session().enterField("Stage01_01",0));runtime.synchronize();
        if(runtime.session().help()){runtime.input(hm2::PointerPhase::begin,{450,210},802);runtime.input(hm2::PointerPhase::end,{450,210},802);}
        capture(@"phone-fullscreen-field-idle");
        if(const auto* idle=runtime.session().battle()){
            XCTAssertEqual(idle->cameraPresentation().worldScale,1.f);XCTAssertEqual(idle->camera().viewport.width,912.f);XCTAssertEqual(idle->camera().viewport.height,420.f);
            for(unsigned tick=0;tick<1200;++tick)runtime.tick();
            XCTAssertEqual(idle->cameraPresentation().worldScale,1.f);XCTAssertEqual(idle->cameraPresentation().targetZoom,1.f);
            NSLog(@"HM2_CAMERA_NATIVE_IDLE ticks=1200 scale=%.9f viewport=%.3fx%.3f",idle->cameraPresentation().worldScale,idle->camera().viewport.width,idle->camera().viewport.height);
        }

        auto* scene=runtime.session().battle();XCTAssertTrue(scene!=nullptr);if(scene){for(const auto& enemy:scene->enemies())if(enemy.hp>0&&!enemy.corpse){const auto point=runtime.session().worldCanvasPoint(scene->camera().toScreen(enemy.position));if(point.x>68&&point.x<844&&point.y>145&&point.y<400){runtime.input(hm2::PointerPhase::begin,point,803);runtime.input(hm2::PointerPhase::end,point,803);if(scene->selectedEnemy())break;}}
            XCTAssertNotEqual(scene->selectedEnemy(),0u);capture(@"phone-fullscreen-field-target");
            const auto frame=runtime.session().drawWorld();XCTAssertEqual(frame.width,912);XCTAssertEqual(frame.height,420);XCTAssertGreaterThan(scene->metrics().playerEmissions,0u);
        }
    }catch(const std::exception& e){XCTFail(@"Phone full-screen failed: %s",e.what());}
    window.hidden=YES;window.rootViewController=nil;XCTAssertTrue([NSFileManager.defaultManager removeItemAtURL:directory error:&error]);
}
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
        const auto reads=runtime->resourceReads();NSDictionary* audioBefore=runtime->audioMetrics();
        const auto saves=runtime->session().metrics().saveRequests;
        const auto ownedImages=[&]{
            auto owned=runtime->session().preparationFrame();runtime->screens().appendPreparation(owned);
            std::set<std::string> images;for(const auto& sprite:owned.sprites)if(!sprite.image.starts_with("@text/"))images.insert(sprite.image);
            return images;
        };
        unsigned ownerChanges=0,steadyTicks=0,firstImageReads=0;
        for(unsigned i=0;i<180;++i){
            const auto before=runtime->resourceMetrics();const auto readsBefore=runtime->resourceReads();
            const auto countsBefore=runtime->resourceReadCounts();const auto imagesBefore=ownedImages();const auto textBeforeList=runtime->preparedTextIdentities();const std::set<std::string> textsBefore(textBeforeList.begin(),textBeforeList.end());const auto presentationBefore=runtime->session().presentationRevision();
            runtime->tick();
            const auto prepared=runtime->resourceMetrics();const auto imagesAfter=ownedImages();const auto textAfterList=runtime->preparedTextIdentities();const std::set<std::string> textsAfter(textAfterList.begin(),textAfterList.end());
            const bool ownerChanged=![before[@"worldResourceRevision"] isEqual:prepared[@"worldResourceRevision"]]||
                ![before[@"uiResourceGeneration"] isEqual:prepared[@"uiResourceGeneration"]]||
                presentationBefore!=runtime->session().presentationRevision();
            if(ownerChanged){
                ++ownerChanges;NSUInteger addedImages=0,removedImages=0,addedTexts=0,removedTexts=0;
                for(const auto& name:imagesAfter)addedImages+=!imagesBefore.contains(name);
                for(const auto& name:imagesBefore)removedImages+=!imagesAfter.contains(name);
                for(const auto& name:textsAfter)addedTexts+=!textsBefore.contains(name);
                for(const auto& name:textsBefore)removedTexts+=!textsAfter.contains(name);
                // The renderer reads PNG bytes independently of Core Files.
                // An owner change may prepare only genuinely new image IDs;
                // still-owned PNGs cannot be evicted and uploaded again.
                for(NSString* key in @[@"imageTextureUploads",@"imageResourceReads",@"imageDecodes"])
                    XCTAssertEqual([prepared[key] unsignedIntegerValue]-[before[key] unsignedIntegerValue],addedImages,@"GPU image ownership at tick %u %@",i,key);
                XCTAssertEqual([prepared[@"textureUploads"] unsignedIntegerValue]-[before[@"textureUploads"] unsignedIntegerValue],addedImages+addedTexts);
                XCTAssertEqual([prepared[@"textureDeletes"] unsignedIntegerValue]-[before[@"textureDeletes"] unsignedIntegerValue],removedImages+removedTexts);
            }
            else{
                ++steadyTicks;XCTAssertEqual(runtime->resourceReads(),readsBefore);
                for(NSString* key in @[@"textureUploads",@"textureDeletes",@"imageResourceReads",@"imageDecodes",@"textBitmaps",@"textIdentities"])
                    XCTAssertEqualObjects(before[key],prepared[key],@"Unchanged owner tick %u %@",i,key);
            }
            XCTAssertEqual([prepared[@"residentTextures"] unsignedIntegerValue],imagesAfter.size()+[prepared[@"textIdentities"] unsignedIntegerValue]);
            if(runtime->resourceReads()!=readsBefore){
                XCTAssertTrue(ownerChanged,@"Resource read without an actual owner change at tick %u",i);
                auto owned=runtime->session().preparationFrame();runtime->screens().appendPreparation(owned);
                for(const auto& [name,count]:runtime->resourceReadCounts()){
                    const auto old=countsBefore.find(name);const unsigned previous=old==countsBefore.end()?0:old->second;
                    if(count==previous)continue;
                    XCTAssertEqual(previous,0u,@"Battle re-read an already prepared resource %s",name.c_str());
                    XCTAssertEqual(count,1u,@"First preparation read more than once: %s",name.c_str());
                    XCTAssertTrue(name.ends_with(".png"),@"Unexpected battle data read %s",name.c_str());
                    XCTAssertTrue(std::any_of(owned.sprites.begin(),owned.sprites.end(),[&](const auto& sprite){return sprite.image==name;}),@"Read without a live preparation owner: %s",name.c_str());
                    ++firstImageReads;
                }
            }
            const auto drawReads=runtime->resourceReads();runtime->draw(layer);
            XCTAssertEqual(runtime->resourceReads(),drawReads);
            const auto drawn=runtime->resourceMetrics();
            for(NSString* key in @[@"textureUploads",@"textureDeletes",@"imageResourceReads",@"imageDecodes",@"textBitmaps",@"textIdentities"])
                XCTAssertEqualObjects(prepared[key],drawn[key],@"Rendering performed preparation at tick %u %@",i,key);
        }
        XCTAssertGreaterThan(runtime->session().battle()->metrics().playerEmissions,0u);
        XCTAssertGreaterThan(steadyTicks,0u);XCTAssertEqual(steadyTicks+ownerChanges,180u);
        XCTAssertEqual(runtime->resourceReads()-reads,firstImageReads);XCTAssertEqual(runtime->session().metrics().saveRequests,saves);
        NSLog(@"HM2_BATTLE_RESOURCE_BOUNDARIES ticks=180 unchangedOwners=%u changedOwners=%u firstOwnedImageReads=%u repeatedReads=0 drawPreparation=0",steadyTicks,ownerChanges,firstImageReads);
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

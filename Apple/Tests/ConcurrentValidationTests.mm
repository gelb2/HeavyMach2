#import <XCTest/XCTest.h>
#import <UIKit/UIKit.h>
#include "Platform/Apple/HM2ConcurrentRuntime.hpp"

namespace {
void attachPixels(XCTestCase* test,NSData* rgba,CGSize size,NSString* name){
    CGDataProviderRef provider=CGDataProviderCreateWithCFData((__bridge CFDataRef)rgba);CGColorSpaceRef color=CGColorSpaceCreateDeviceRGB();
    CGImageRef image=CGImageCreate(size.width,size.height,8,32,size.width*4,color,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrderDefault,provider,nullptr,false,kCGRenderingIntentDefault);
    UIImage* picture=[UIImage imageWithCGImage:image scale:1 orientation:UIImageOrientationDownMirrored];
    NSData* png=UIImagePNGRepresentation(picture);XCTAttachment* a=[XCTAttachment attachmentWithData:png uniformTypeIdentifier:@"public.png"];a.name=name;a.lifetime=XCTAttachmentLifetimeKeepAlways;[test addAttachment:a];
    CGImageRelease(image);CGColorSpaceRelease(color);CGDataProviderRelease(provider);
}
}
@interface HM2ConcurrentSurface:UIView @end
@implementation HM2ConcurrentSurface
+ (Class)layerClass{return CAEAGLLayer.class;}
@end
@interface HM2ConcurrentTimer:NSObject
@property(nonatomic,copy) void(^callback)(CADisplayLink*);
- (void)frame:(CADisplayLink*)link;
@end
@implementation HM2ConcurrentTimer
- (void)frame:(CADisplayLink*)link{if(self.callback)self.callback(link);}
@end
@interface ConcurrentValidationTests:XCTestCase @end
@implementation ConcurrentValidationTests
- (void)testAllRestoredScenariosOnOneDisplayClock {
    [self runFrames:1200 fullCoverage:YES];
}
- (void)testMenuPreparationWhileAllBattlesContinue {
    [self runFrames:210 fullCoverage:NO];
}
- (void)runFrames:(unsigned)targetFrames fullCoverage:(BOOL)fullCoverage {
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    NSURL* directory=[[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES] URLByAppendingPathComponent:NSUUID.UUID.UUIDString isDirectory:YES];
    UIWindow* window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];UIViewController* controller=[UIViewController new];HM2ConcurrentSurface* surface=[[HM2ConcurrentSurface alloc]initWithFrame:window.bounds];surface.contentScaleFactor=UIScreen.mainScreen.scale;
    CAEAGLLayer* layer=(CAEAGLLayer*)surface.layer;layer.opaque=YES;layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};controller.view=surface;window.rootViewController=controller;[window makeKeyAndVisible];[surface layoutIfNeeded];
    std::unique_ptr<hm2::apple::ConcurrentRuntime> runtime;
    try{runtime=std::make_unique<hm2::apple::ConcurrentRuntime>(root,directory);}catch(const std::exception& e){XCTFail(@"Concurrent preparation failed: %s",e.what());window.hidden=YES;return;}
    XCTestExpectation* completed=[self expectationWithDescription:@"All live scenarios share1200 display callbacks"];
    HM2ConcurrentTimer* timer=[HM2ConcurrentTimer new];__block unsigned frames=0;__block NSString* failure=nil;
    auto* live=runtime.get();
    const hm2::Size viewport{float(surface.bounds.size.width),float(surface.bounds.size.height)};
    timer.callback=^(CADisplayLink* link){@autoreleasepool{
        try{
            live->tick();++frames;if(fullCoverage&&frames==300)(void)live->verifyInactiveInputDiscard();if(frames==targetFrames)live->captureNextFrame();live->draw(layer,viewport);
            if(frames%300==0){const auto metrics=live->validation().metrics();NSLog(@"HM2_CONCURRENT_PROGRESS frame=%u cases=%lu weaponDefinitions=%lu weaponKinds=%lu menuOverlap=%llu/%llu",frames,(unsigned long)metrics.activeScenarios,(unsigned long)metrics.activeWeaponDefinitions,(unsigned long)metrics.activeWeaponKinds,metrics.menuFramesWithCombatActivity,metrics.menuActionFrames);}
            if(frames>=targetFrames){[link invalidate];[completed fulfill];}
        }catch(const std::exception& e){failure=[NSString stringWithUTF8String:e.what()];[link invalidate];[completed fulfill];}
    }};
    CADisplayLink* display=[CADisplayLink displayLinkWithTarget:timer selector:@selector(frame:)];display.preferredFramesPerSecond=60;[display addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
    [self waitForExpectations:@[completed] timeout:240];[display invalidate];timer.callback=nil;
    XCTAssertNil(failure,@"Concurrent frame failed: %@",failure);
    NSDictionary* comparison=runtime->verifyRepeatedRendering(layer,viewport);
    attachPixels(self,runtime->legacyComparisonPixels(),runtime->drawableSize(),@"Same-frame original scissor GPU");
    attachPixels(self,runtime->pixels(),runtime->drawableSize(),@"Same-frame repeated original GPU");
    XCTAssertEqual([comparison[@"differentRGBPixels"] unsignedIntegerValue],0u);
    XCTAssertEqualObjects(comparison[@"repeatDrawCalls"],comparison[@"firstDrawCalls"]);
    NSMutableDictionary* report=[runtime->report() mutableCopy];report[@"renderComparison"]=comparison;NSError* error=nil;NSData* data=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted|NSJSONWritingSortedKeys error:&error];XCTAssertNotNil(data);
    if(data){XCTAttachment* attachment=[XCTAttachment attachmentWithData:data uniformTypeIdentifier:@"public.json"];attachment.name=@"HM2 real display-clock concurrent report";attachment.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:attachment];}
    XCTAssertEqual(frames,targetFrames);XCTAssertEqual([report[@"scenarios"] unsignedIntegerValue],68u);XCTAssertEqual([report[@"combatScenarios"] unsignedIntegerValue],55u);
    XCTAssertTrue([report[@"commonFrameInterval"] boolValue]);
    if(fullCoverage){XCTAssertEqual([report[@"activeWeaponDefinitions"] unsignedIntegerValue],110u);XCTAssertEqual([report[@"activeWeaponKinds"] unsignedIntegerValue],5u);XCTAssertGreaterThanOrEqual([report[@"maxConcurrentWeaponKinds"] unsignedIntegerValue],5u);}
    XCTAssertGreaterThanOrEqual([report[@"maxProgressingEffects"] unsignedIntegerValue],2u);
    XCTAssertEqual([report[@"imageTextureUploadsAfterPreparation"] unsignedIntegerValue],0u);
    XCTAssertEqual([report[@"warmReadMisses"] unsignedIntegerValue],0u);XCTAssertEqual([report[@"warmExistsMisses"] unsignedIntegerValue],0u);
    NSDictionary* before=report[@"audioBefore"];NSDictionary* after=report[@"audioAfter"];
    XCTAssertEqual([after[@"contextsCreated"] unsignedIntegerValue],1u);
    for(NSString* key in @[@"contextsCreated",@"sourcesCreated",@"buffersCreated",@"effectFileReads",@"effectDecodes"])XCTAssertEqualObjects(before[key],after[key]);
    unsigned failedButtons=0;bool shop=false,mission=false,restarted=false,levelUp=false;
    for(NSDictionary* row in report[@"cases"]){XCTAssertEqual([row[@"ticks"] unsignedIntegerValue],targetFrames);XCTAssertEqual([row[@"firstFrame"] unsignedIntegerValue],1u);XCTAssertEqual([row[@"lastFrame"] unsignedIntegerValue],targetFrames);shop|=[row[@"shopPurchased"] boolValue]&&[row[@"insufficientRejected"] boolValue];mission|=[row[@"missionComplete"] boolValue];restarted|=[row[@"restarted"] boolValue];levelUp|=[row[@"level"] intValue]>[row[@"initialLevel"] intValue];failedButtons+=[row[@"buttonFailures"] unsignedIntegerValue];}
    XCTAssertTrue(shop);if(fullCoverage){XCTAssertTrue(mission);XCTAssertTrue(restarted);XCTAssertTrue(levelUp);
        unsigned repairs=0;for(NSDictionary* row in report[@"cases"])if([row[@"quickItemCode"] length]){
            ++repairs;XCTAssertEqual([row[@"quickUseFrame"] unsignedIntegerValue],241u);XCTAssertTrue([row[@"quickTapAccepted"] boolValue]);
            XCTAssertGreaterThan([row[@"quickHPAfter"] intValue],[row[@"quickHPBefore"] intValue]);XCTAssertEqualObjects(row[@"quickHPAfter"],row[@"quickSlotHPAfter"]);
            XCTAssertEqual([row[@"quickCountBefore"] intValue],5);XCTAssertEqual([row[@"quickCountAfter"] intValue],4);XCTAssertEqual([row[@"quickSelectionAfter"] intValue],-1);
            XCTAssertEqual([row[@"quickRepairSounds"] unsignedIntegerValue],1u);XCTAssertEqual([row[@"quickParticleAllocations"] unsignedIntegerValue],1u);
        }XCTAssertEqual(repairs,6u);XCTAssertGreaterThan([report[@"aidKitFramesWithCombatActivity"] unsignedIntegerValue],0u);
        unsigned strikes=0;for(NSDictionary* row in report[@"cases"])if([row[@"strikeItemCode"] length]){
            ++strikes;XCTAssertEqual([row[@"strikeUseFrame"] unsignedIntegerValue],261u);XCTAssertTrue([row[@"strikeTapAccepted"] boolValue]);XCTAssertTrue([row[@"strikeTargetVisibleEnemy"] boolValue]);
            XCTAssertEqual([row[@"strikeRequests"] unsignedIntegerValue],1u);XCTAssertEqual([row[@"strikeCountBefore"] intValue],5);XCTAssertEqual([row[@"strikeCountAfter"] intValue],4);XCTAssertEqual([row[@"playerEmissions"] unsignedIntegerValue],0u);
            XCTAssertEqual([row[@"strikeProjectiles"] unsignedIntegerValue],[row[@"strikeItemCode"] isEqualToString:@"02007"]?1u:10u);XCTAssertGreaterThan([row[@"strikeEnemyHPRemoved"] unsignedIntegerValue],0u);XCTAssertEqual([row[@"activeStrikes"] unsignedIntegerValue],0u);
        }XCTAssertEqual(strikes,3u);XCTAssertGreaterThan([report[@"airStrikeFramesWithCombatActivity"] unsignedIntegerValue],0u);
        XCTAssertGreaterThan([report[@"messageFonts"] count],0u);for(NSDictionary* font in report[@"messageFonts"]){XCTAssertEqualObjects(font[@"font"],@"TrebuchetMS-Bold");XCTAssertGreaterThan([font[@"inkPixels"] unsignedIntegerValue],20u);XCTAssertEqual([font[@"size"] intValue],16);XCTAssertEqual([font[@"width"] intValue],512);XCTAssertEqual([font[@"height"] intValue],24);XCTAssertFalse([font[@"retina"] boolValue]);}
        XCTAssertEqual([report[@"capturedSourceFrames"] count],3u);for(NSDictionary* capture in report[@"capturedSourceFrames"]){if([capture[@"frame"] intValue]==240)XCTAssertEqual([capture[@"messageBackgrounds"] intValue],1);if([capture[@"frame"] intValue]==241)XCTAssertEqual([capture[@"messageBackgrounds"] intValue],1);if([capture[@"frame"] intValue]==381){XCTAssertGreaterThan([capture[@"planes"] intValue],0);XCTAssertGreaterThan([capture[@"markers"] intValue],0);}}
        NSDictionary* interruption=report[@"inputInterruption"];for(NSString* key in @[@"capturedBefore",@"discardedAfter",@"beginReachedRealHandler",@"cleanupSentNoAction",@"noExtraTick",@"noSave",@"creditPreserved",@"screenPreserved"])XCTAssertTrue([interruption[key] boolValue],@"Interruption %@ failed",key);
        NSDictionary* moving=report[@"cases"][58];XCTAssertGreaterThan([moving[@"distanceMoved"] doubleValue],100.0);
        XCTAssertGreaterThan([moving[@"coinPickups"] unsignedIntegerValue],0u);XCTAssertGreaterThan([moving[@"itemPickups"] unsignedIntegerValue],0u);
        XCTAssertGreaterThan([moving[@"pickupCreditGain"] intValue],0);
        XCTAssertEqual([moving[@"credit"] intValue],[moving[@"initialCredit"] intValue]+[moving[@"pickupCreditGain"] intValue]);XCTAssertGreaterThan([moving[@"inventoryGain"] intValue],0);
        XCTAssertGreaterThan([report[@"movementEmissionFramesWithCombatActivity"] unsignedIntegerValue],0u);XCTAssertGreaterThan([report[@"pickupFramesWithCombatActivity"] unsignedIntegerValue],0u);
        for(NSDictionary* pickup in moving[@"pickups"]){XCTAssertLessThan(fabs([pickup[@"playerX"] doubleValue]-[pickup[@"itemX"] doubleValue]),30.0);XCTAssertLessThan(fabs([pickup[@"playerY"] doubleValue]-[pickup[@"itemY"] doubleValue]),30.0);}
    }XCTAssertEqual(failedButtons,0u);
    XCTAssertGreaterThan(runtime->pixels().length,0u);
    NSLog(@"HM2_CONCURRENT_GPU frames=%u cases=%@ combatCases=55 weaponDefinitions=%@ weaponKinds=%@ maxProgressingEffects=%@ sourceContexts=%@ cpuP95ms=%@ completeGameCoverage=0 device=%@",frames,report[@"scenarios"],report[@"activeWeaponDefinitions"],report[@"activeWeaponKinds"],report[@"maxProgressingEffects"],after[@"contextsCreated"],report[@"cpuAndSubmissionP95ms"],UIDevice.currentDevice.model);
    // Additional real GPU output for a compact report image, using the same
    // cached game frame. It does not replace full-size comparison evidence.
    surface.frame=CGRectMake(0,0,384,512);surface.contentScaleFactor=1;
    runtime->captureNextFrame();runtime->draw(layer,{384,512});
    attachPixels(self,runtime->pixels(),runtime->drawableSize(),@"Concurrent GPU preview384x512");
    if(fullCoverage)for(unsigned number:{240u,241u,381u}){XCTAssertTrue(runtime->captureStoredFrame(number,layer));attachPixels(self,runtime->pixels(),runtime->drawableSize(),[NSString stringWithFormat:@"Original GPU source frame%u preview384x512",number]);}
    runtime->setActive(false);runtime.reset();window.hidden=YES;window.rootViewController=nil;
    XCTAssertTrue([NSFileManager.defaultManager removeItemAtURL:directory error:&error]);
}
@end

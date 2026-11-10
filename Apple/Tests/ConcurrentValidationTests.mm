#import <XCTest/XCTest.h>
#import <UIKit/UIKit.h>
#include "Platform/Apple/HM2ConcurrentRuntime.hpp"
#include <algorithm>

namespace {
void attachPixels(XCTestCase* test,NSData* rgba,CGSize size,NSString* name){
    const auto width=std::size_t(size.width),height=std::size_t(size.height),stride=width*4;
    NSMutableData* topDown=[NSMutableData dataWithLength:rgba.length];const auto* source=static_cast<const std::uint8_t*>(rgba.bytes);auto* destination=static_cast<std::uint8_t*>(topDown.mutableBytes);
    for(std::size_t y=0;y<height;++y)std::copy_n(source+(height-1-y)*stride,stride,destination+y*stride);
    // CAEAGLLayer is opaque. Framebuffer alpha is not display opacity; export
    // its exact RGB bytes, without premultiplying/unpremultiplying them.
    CGDataProviderRef provider=CGDataProviderCreateWithCFData((__bridge CFDataRef)topDown);CGColorSpaceRef color=CGColorSpaceCreateDeviceRGB();
    CGImageRef image=CGImageCreate(width,height,8,32,stride,color,kCGImageAlphaNoneSkipLast|kCGBitmapByteOrderDefault,provider,nullptr,false,kCGRenderingIntentDefault);
    UIImage* picture=[UIImage imageWithCGImage:image];
    NSData* png=UIImagePNGRepresentation(picture);XCTAttachment* a=[XCTAttachment attachmentWithData:png uniformTypeIdentifier:@"public.png"];a.name=name;a.lifetime=XCTAttachmentLifetimeKeepAlways;[test addAttachment:a];
    if([name containsString:@"source frame"]){XCTAttachment* raw=[XCTAttachment attachmentWithData:rgba uniformTypeIdentifier:@"public.data"];raw.name=[name stringByAppendingString:@" rawRGBA"];raw.lifetime=XCTAttachmentLifetimeKeepAlways;[test addAttachment:raw];}
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
    // Original door replacement takes165 scheduled frames; the final
    // shop refusal observation is frame355. Keep its acceptance assertions.
    [self runFrames:360 fullCoverage:NO];
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
    [self waitForExpectations:@[completed] timeout:600];[display invalidate];timer.callback=nil;
    XCTAssertNil(failure,@"Concurrent frame failed: %@",failure);
    NSDictionary* comparison=runtime->verifyRepeatedRendering(layer,viewport);
    attachPixels(self,runtime->legacyComparisonPixels(),runtime->drawableSize(),@"Same-frame original scissor GPU");
    attachPixels(self,runtime->pixels(),runtime->drawableSize(),@"Same-frame repeated original GPU");
    XCTAssertEqual([comparison[@"differentRGBPixels"] unsignedIntegerValue],0u);
    XCTAssertEqualObjects(comparison[@"repeatDrawCalls"],comparison[@"firstDrawCalls"]);
    NSMutableDictionary* report=[runtime->report() mutableCopy];report[@"renderComparison"]=comparison;NSError* error=nil;NSData* data=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted|NSJSONWritingSortedKeys error:&error];XCTAssertNotNil(data);
    if(data){XCTAttachment* attachment=[XCTAttachment attachmentWithData:data uniformTypeIdentifier:@"public.json"];attachment.name=@"HM2 real display-clock concurrent report";attachment.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:attachment];}
    XCTAssertEqual(frames,targetFrames);XCTAssertEqual([report[@"scenarios"] unsignedIntegerValue],115u);XCTAssertEqual([report[@"combatScenarios"] unsignedIntegerValue],55u);
    XCTAssertTrue([report[@"commonFrameInterval"] boolValue]);
    if(fullCoverage){XCTAssertEqual([report[@"activeWeaponDefinitions"] unsignedIntegerValue],110u);XCTAssertEqual([report[@"activeWeaponKinds"] unsignedIntegerValue],5u);XCTAssertGreaterThanOrEqual([report[@"maxConcurrentWeaponKinds"] unsignedIntegerValue],5u);}
    XCTAssertGreaterThanOrEqual([report[@"maxProgressingEffects"] unsignedIntegerValue],2u);
    XCTAssertEqual([report[@"imageTextureUploadsAfterPreparation"] unsignedIntegerValue],0u);
    XCTAssertEqual([report[@"warmReadMisses"] unsignedIntegerValue],0u);XCTAssertEqual([report[@"warmExistsMisses"] unsignedIntegerValue],0u);
    NSDictionary* before=report[@"audioBefore"];NSDictionary* after=report[@"audioAfter"];
    XCTAssertEqual([after[@"contextsCreated"] unsignedIntegerValue],1u);
    for(NSString* key in @[@"contextsCreated",@"sourcesCreated",@"buffersCreated",@"effectFileReads",@"effectDecodes",@"contextBindCalls",@"contextClearCalls",@"contextBindFailures",@"effectNameConversions"])XCTAssertEqualObjects(before[key],after[key]);
    XCTAssertEqualObjects(before[@"currentContext"],before[@"ownerContext"]);XCTAssertEqualObjects(after[@"currentContext"],after[@"ownerContext"]);XCTAssertNotEqual([after[@"currentContext"] unsignedIntegerValue],0u);XCTAssertEqual([after[@"contextBindCalls"] unsignedIntegerValue],1u);XCTAssertEqual([after[@"contextClearCalls"] unsignedIntegerValue],0u);XCTAssertEqual([after[@"contextBindFailures"] unsignedIntegerValue],0u);XCTAssertEqual([after[@"effectNameConversions"] unsignedIntegerValue],0u);XCTAssertGreaterThan([after[@"effectByteNameRequests"] unsignedIntegerValue],0u);
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
        XCTAssertEqual([report[@"capturedSourceFrames"] count],15u);for(NSDictionary* capture in report[@"capturedSourceFrames"]){if([capture[@"frame"] intValue]==112)XCTAssertGreaterThan([capture[@"dashQuads"] intValue],0);if([capture[@"frame"] intValue]==240)XCTAssertEqual([capture[@"messageBackgrounds"] intValue],1);if([capture[@"frame"] intValue]==241)XCTAssertEqual([capture[@"messageBackgrounds"] intValue],1);if([capture[@"frame"] intValue]==381){XCTAssertGreaterThan([capture[@"planes"] intValue],0);XCTAssertGreaterThan([capture[@"markers"] intValue],0);}}
        unsigned baselineMenuFrames=0,baselineMenuOverlap=0;for(NSDictionary* frame in report[@"frameActivity"]){XCTAssertEqual([frame[@"tickedScenarios"] intValue],115);bool baseline=false;for(NSDictionary* b in frame[@"buttons"])if([b[@"scenario"] intValue]<68)baseline=true;
            if(baseline){++baselineMenuFrames;if([frame[@"combatActiveProjectiles"] unsignedIntegerValue]||[frame[@"combatEmittingScenarios"] unsignedIntegerValue]||[frame[@"combatDamage"] unsignedIntegerValue])++baselineMenuOverlap;}}
        XCTAssertEqual(baselineMenuFrames,11u);XCTAssertEqual(baselineMenuOverlap,11u);
        NSArray* cases=report[@"cases"];NSMutableSet* fields=[NSMutableSet setWithObject:@"Stage01_01"];unsigned long long spawns[3]={0,0,0};
        for(unsigned k=0;k<3;++k)spawns[k]=[cases[0][@"firstSpawns"][k] unsignedLongLongValue];
        for(unsigned i=68;i<105;++i){NSDictionary* row=cases[i];XCTAssertEqual([row[@"role"] intValue],7);XCTAssertFalse([fields containsObject:row[@"map"]]);[fields addObject:row[@"map"]];for(unsigned k=0;k<3;++k)spawns[k]+=[row[@"firstSpawns"][k] unsignedLongLongValue];
            NSString* map=row[@"map"];if([map isEqualToString:@"Stage01_02"]||[map isEqualToString:@"Stage02_07"]||[map isEqualToString:@"Stage03_04"]){const unsigned kind=[map isEqualToString:@"Stage01_02"]?1:2;for(NSString* key in @[@"enemyEmissions",@"enemyDamage",@"enemyKills",@"enemyRemovals"])XCTAssertGreaterThan([row[key][kind] unsignedLongLongValue],0ull);}}
        XCTAssertEqual(fields.count,38u);XCTAssertEqual(spawns[0],385ull);XCTAssertEqual(spawns[1],307ull);XCTAssertEqual(spawns[2],3ull);
        for(unsigned i=0;i<55;++i)XCTAssertEqual([cases[i][@"emittedWeaponParts"] count],2u);
        NSDictionary* dash=cases[105];XCTAssertTrue([dash[@"dashInputAccepted"] boolValue]);XCTAssertEqual([dash[@"dashInputFrame"] intValue],110);XCTAssertEqual([dash[@"dashStarts"] unsignedIntegerValue],1u);
        XCTAssertEqualWithAccuracy([dash[@"dashSPBefore"] doubleValue]-[dash[@"dashSPAfter"] doubleValue],100,0.0001);XCTAssertEqual([dash[@"dashSounds"] intValue],1);
        XCTAssertGreaterThan([dash[@"dashMoveTicks"] unsignedIntegerValue],0u);XCTAssertGreaterThan([dash[@"dashParticles"] unsignedIntegerValue],0u);XCTAssertEqualObjects(dash[@"dashParticles"],dash[@"dashRandomCalls"]);XCTAssertGreaterThan([dash[@"dashMaxDisplacement"] doubleValue],30);
        for(unsigned index:{106u,107u}){NSDictionary* row=cases[index];NSArray* bots=row[@"bots"];XCTAssertEqual(bots.count,3u);unsigned long long incoming=0,contacts=0;
            for(unsigned slot=0;slot<3;++slot){NSDictionary* b=bots[slot];XCTAssertTrue([b[@"spawned"] boolValue]);XCTAssertEqual([b[@"slot"] unsignedIntegerValue],slot);XCTAssertEqual([b[@"id"] unsignedIntegerValue],2u+slot);
                XCTAssertGreaterThan([b[@"ticks"] unsignedIntegerValue],0u);XCTAssertGreaterThan([b[@"rotorFrames"] unsignedIntegerValue],0u);XCTAssertGreaterThanOrEqual([b[@"repairFrame"] unsignedIntegerValue],180u);XCTAssertTrue([b[@"repairAccepted"] boolValue]);
                XCTAssertGreaterThan([b[@"repairAfter"] intValue],[b[@"repairBefore"] intValue]);XCTAssertEqual([b[@"repairCountAfter"] intValue],[b[@"repairCountBefore"] intValue]-1);XCTAssertEqual([b[@"repairParticles"] intValue],1);XCTAssertEqual([b[@"repairSounds"] intValue],1);
                if(index==106){XCTAssertEqualObjects(b[@"code"],([NSString stringWithFormat:@"0400%u",slot+1]));XCTAssertGreaterThan([b[@"emissions"] unsignedLongLongValue],0ull);XCTAssertGreaterThan([b[@"enemyHPRemoved"] unsignedLongLongValue],0ull);}
                else {XCTAssertEqualObjects(b[@"code"],([NSString stringWithFormat:@"0420%u",slot+1]));XCTAssertGreaterThan([b[@"itemContacts"] unsignedLongLongValue],0ull);}
                incoming+=[b[@"hpRemoved"] unsignedLongLongValue];contacts+=[b[@"itemContacts"] unsignedLongLongValue];
            }XCTAssertGreaterThan(incoming,0ull);XCTAssertGreaterThan([row[@"navigationRequests"] intValue],0);XCTAssertGreaterThan([row[@"distanceMoved"] doubleValue],30);XCTAssertGreaterThan([row[@"botThreatEmissions"] unsignedLongLongValue],0ull);if(index==106)XCTAssertEqual([row[@"playerEmissions"] unsignedIntegerValue],0u);
            else {XCTAssertGreaterThan(contacts,0ull);XCTAssertGreaterThan([row[@"pickupEvents"] intValue],0);XCTAssertGreaterThan([row[@"coinPickups"] intValue],0);XCTAssertGreaterThan([row[@"pickupCreditGain"] intValue],0);XCTAssertEqual([row[@"credit"] intValue],[row[@"initialCredit"] intValue]+[row[@"pickupCreditGain"] intValue]);}
        }
        NSArray* swaps=cases[107][@"weaponSwaps"];XCTAssertEqual(swaps.count,2u);for(unsigned side=0;side<swaps.count;++side){NSDictionary* w=swaps[side];XCTAssertEqual([w[@"inventoryPosition"] unsignedIntegerValue],10u+side);XCTAssertGreaterThan([w[@"chooserFrame"] unsignedIntegerValue],0u);XCTAssertTrue([w[@"chooserShown"] boolValue]);XCTAssertGreaterThanOrEqual([w[@"equipFrame"] unsignedIntegerValue],[w[@"chooserFrame"] unsignedIntegerValue]+31u);XCTAssertTrue([w[@"equipped"] boolValue]);XCTAssertGreaterThanOrEqual([w[@"firstEmissionFrame"] unsignedIntegerValue],[w[@"equipFrame"] unsignedIntegerValue]);for(NSString* k in @[@"ownedBefore",@"ownedAfter",@"equippedAfter"])XCTAssertTrue([w[k] boolValue]);for(NSString* k in @[@"equippedBefore",@"oldEquippedAfter",@"pausedBefore",@"pausedAfter"])XCTAssertFalse([w[k] boolValue]);for(NSString* k in @[@"countBefore",@"countAfter",@"oldCountAfter"])XCTAssertEqual([w[k] intValue],1);XCTAssertEqualObjects(w[@"savesBefore"],w[@"savesAfter"]);if(side)XCTAssertGreaterThan([w[@"chooserFrame"] unsignedIntegerValue],[swaps[0][@"equipFrame"] unsignedIntegerValue]);}
        NSDictionary* completedMission=cases[114][@"completedMissions"];
        XCTAssertEqualObjects(completedMission[@"ids"],(@[@"M002010",@"M001010"]));XCTAssertEqualObjects(completedMission[@"credits"],(@[@580,@680]));XCTAssertEqualObjects(completedMission[@"experience"],(@[@200,@400]));XCTAssertEqualObjects(completedMission[@"levels"],(@[@0,@1]));XCTAssertEqualObjects(completedMission[@"rewardCounts"],(@[@2,@3]));
        XCTAssertGreaterThan([completedMission[@"popupFrames"][0] intValue],0);XCTAssertEqual([completedMission[@"popupFrames"][1] intValue],[completedMission[@"okFrames"][0] intValue]);
        for(unsigned k=0;k<2;++k){XCTAssertEqual([completedMission[@"okFrames"][k] intValue],(k==0?[completedMission[@"doorReadyFrame"] intValue]:[completedMission[@"popupFrames"][k] intValue])+8);XCTAssertEqualObjects(completedMission[@"popupSaves"][k],completedMission[@"savesBefore"]);}
        for(NSString* key in @[@"secondOKAccepted",@"finalOKAccepted",@"savedPayloadValid"])XCTAssertTrue([completedMission[key] boolValue]);XCTAssertFalse([completedMission[@"listImmediatelyAfterFinalOK"] boolValue]);
        XCTAssertEqualObjects(completedMission[@"listFrame"],completedMission[@"okFrames"][1]);XCTAssertGreaterThan([completedMission[@"baseFrame"] intValue],[completedMission[@"exitFrame"] intValue]);XCTAssertEqual([completedMission[@"finalCredit"] intValue],680);XCTAssertEqual([completedMission[@"remainingAccepted"] intValue],2);XCTAssertEqual([completedMission[@"repairItems"] intValue],[completedMission[@"initialRepairItems"] intValue]+3);XCTAssertEqual([completedMission[@"savesAfter"] intValue],[completedMission[@"savesBefore"] intValue]+1);
        XCTAssertEqualObjects(completedMission[@"sounds"],(@[@11,@11,@7,@0,@0,@0]));XCTAssertGreaterThanOrEqual([report[@"completedMissionFramesWithCombatActivity"] intValue],3);
        NSDictionary* over=cases[112][@"gameOver"];XCTAssertEqual([over[@"initialHP"] intValue],1);XCTAssertTrue([over[@"targetInputAccepted"] boolValue]);XCTAssertGreaterThan([over[@"deathFrame"] intValue],0);XCTAssertEqual([over[@"deathHP"] intValue],0);XCTAssertEqual([over[@"deathPlayerState"] intValue],2);XCTAssertTrue([over[@"savedPayloadValid"] boolValue]);XCTAssertGreaterThan([over[@"enemyEmissions"] intValue],0);XCTAssertGreaterThan([over[@"damageCalls"] intValue],0);
        XCTAssertEqual([over[@"requests"] intValue],1);XCTAssertGreaterThan([over[@"token"] unsignedLongLongValue],0ull);XCTAssertEqual([over[@"requestHP"] intValue],0);XCTAssertEqual([over[@"requestPlayerState"] intValue],3);XCTAssertEqual([over[@"requestWorldState"] intValue],3);XCTAssertFalse([over[@"requestPaused"] boolValue]);XCTAssertEqualObjects(over[@"requestStateTime"],over[@"requestRenderTime"]);
        XCTAssertEqual([over[@"confirmFrame"] unsignedIntegerValue],[over[@"requestFrame"] unsignedIntegerValue]+8);XCTAssertTrue([over[@"waitAdvanced"] boolValue]);XCTAssertTrue([over[@"confirmAccepted"] boolValue]);XCTAssertTrue([over[@"pendingCleared"] boolValue]);XCTAssertTrue([over[@"duplicateRejected"] boolValue]);XCTAssertEqual([over[@"confirmDoorPhase"] intValue],0);XCTAssertEqualObjects(over[@"locationAtConfirm"],@"Base002");
        XCTAssertGreaterThan([over[@"baseFrame"] intValue],[over[@"confirmFrame"] intValue]);XCTAssertEqual([over[@"baseDoorPhase"] intValue],4);XCTAssertEqualObjects(over[@"baseId"],@"Base002");XCTAssertEqual([over[@"baseHP"] intValue],256);XCTAssertEqualObjects(over[@"savedBotHP"],(@[@0,@0,@0]));
        XCTAssertEqualObjects(over[@"savesBefore"],over[@"savesAtRequest"]);XCTAssertEqualObjects(over[@"savesBefore"],over[@"savesAfterConfirm"]);XCTAssertEqual([over[@"savesAtBase"] intValue],[over[@"savesBefore"] intValue]+1);XCTAssertEqualObjects(cases[112][@"saves"],over[@"savesAtBase"]);XCTAssertGreaterThanOrEqual([report[@"gameOverFramesWithCombatActivity"] intValue],3);
        NSDictionary* ranking=cases[113][@"ranking"];XCTAssertEqual([ranking[@"notices"] intValue],1);XCTAssertEqualObjects(ranking[@"title"],@"랭킹");XCTAssertEqualObjects(ranking[@"message"],@"현재 랭킹 서비스를 이용할 수 없습니다.");XCTAssertEqualObjects(ranking[@"events"],(@[@"notice",@"sfx0"]));XCTAssertEqualObjects(ranking[@"savesBefore"],ranking[@"savesAfter"]);
        for(NSString* key in @[@"slotUnchanged",@"savedBytesUnchanged",@"screenUnchanged",@"transitionUnchanged",@"unsupportedUnchanged"])XCTAssertTrue([ranking[key] boolValue]);XCTAssertEqual([report[@"rankingFramesWithCombatActivity"] intValue],1);
        XCTAssertGreaterThan([report[@"botFramesWithCombatActivity"] unsignedIntegerValue],0u);XCTAssertGreaterThan([report[@"helpFramesWithCombatActivity"] unsignedIntegerValue],0u);
        for(unsigned index=108;index<112;++index){NSDictionary* h=cases[index][@"help"];NSArray* inputs=h[@"inputs"];XCTAssertGreaterThan([h[@"runningFrames"] intValue],0);XCTAssertFalse([h[@"initialPaused"] boolValue]);
            if(index<110){XCTAssertTrue([h[@"initialPresent"] boolValue]);XCTAssertEqual([h[@"initialCounter"] intValue],index==108?0:4);XCTAssertEqual([h[@"resultingCounter"] intValue],[h[@"initialCounter"] intValue]+5);XCTAssertEqual([h[@"initialPage"] intValue],2);XCTAssertEqual([h[@"initialLayer"] intValue],9);XCTAssertFalse([h[@"initialGuide"] boolValue]);XCTAssertFalse([h[@"initialChangePages"] boolValue]);XCTAssertEqual(inputs.count,1u);XCTAssertTrue([inputs[0][@"accepted"] boolValue]);XCTAssertTrue([inputs[0][@"closed"] boolValue]);XCTAssertFalse([inputs[0][@"paused"] boolValue]);XCTAssertEqual([h[@"closedFrame"] intValue],200);XCTAssertGreaterThan([h[@"drawFrames"] intValue],0);XCTAssertEqual([h[@"guideDrawFrames"] intValue],0);XCTAssertEqualObjects(h[@"lastImage"],@"HelpScr03_kr.png");}
            if(index==110){XCTAssertEqual([h[@"initialCounter"] intValue],5);XCTAssertEqual([h[@"resultingCounter"] intValue],5);XCTAssertFalse([h[@"initialPresent"] boolValue]);XCTAssertEqual(inputs.count,1u);XCTAssertFalse([inputs[0][@"accepted"] boolValue]);XCTAssertFalse([inputs[0][@"closed"] boolValue]);XCTAssertEqual([h[@"drawFrames"] intValue],0);}
            if(index==111){XCTAssertFalse([h[@"initialPresent"] boolValue]);XCTAssertTrue([h[@"menuOpened"] boolValue]);XCTAssertTrue([h[@"menuPaused"] boolValue]);XCTAssertTrue([h[@"menuGuide"] boolValue]);XCTAssertEqual([h[@"menuPage"] intValue],2);XCTAssertEqual([h[@"menuLayer"] intValue],13);XCTAssertEqual(inputs.count,4u);
                XCTAssertTrue([inputs[0][@"beforeGuide"] boolValue]);XCTAssertFalse([inputs[0][@"afterGuide"] boolValue]);XCTAssertFalse([inputs[0][@"closed"] boolValue]);XCTAssertEqual([inputs[0][@"afterPage"] intValue],2);XCTAssertTrue([inputs[0][@"paused"] boolValue]);
                XCTAssertEqual([inputs[1][@"afterPage"] intValue],1);XCTAssertTrue([inputs[1][@"paused"] boolValue]);XCTAssertEqual([inputs[2][@"afterPage"] intValue],2);XCTAssertTrue([inputs[2][@"paused"] boolValue]);XCTAssertTrue([inputs[3][@"closed"] boolValue]);XCTAssertFalse([inputs[3][@"paused"] boolValue]);XCTAssertEqual([h[@"closedFrame"] intValue],230);XCTAssertEqualObjects(h[@"timeAtClose"],h[@"timeAtMenu"]);XCTAssertGreaterThan([h[@"lastWorldTime"] doubleValue],[h[@"timeAtClose"] doubleValue]);XCTAssertGreaterThan([h[@"guideDrawFrames"] intValue],0);XCTAssertGreaterThan([h[@"drawFrames"] intValue],[h[@"guideDrawFrames"] intValue]);}
        }
        for(NSDictionary* capture in report[@"capturedSourceFrames"]){const unsigned number=[capture[@"frame"] unsignedIntValue];
            if(number==348||number==356)XCTAssertEqual([capture[@"completedBackgrounds"] intValue],1);if(number==364)XCTAssertEqual([capture[@"completedBackgrounds"] intValue],0);if(number==348||number==356||number==364)XCTAssertEqual([capture[@"loadingPictures"] intValue],0);
            if(number==31||number==32){XCTAssertGreaterThanOrEqual([capture[@"botBodies"] intValue],3);XCTAssertGreaterThanOrEqual([capture[@"botRotors"] intValue],3);}
            if(number==40||number==183||number==201||number==221){XCTAssertEqualObjects(capture[@"helpImage"],@"HelpScr03_kr.png");XCTAssertEqual([capture[@"helpPages"] intValue],1);}
            if(number==183)XCTAssertEqual([capture[@"helpGuides"] intValue],1);
            if(number==40||number==201||number==211||number==221)XCTAssertEqual([capture[@"helpGuides"] intValue],0);
            if(number==211)XCTAssertEqualObjects(capture[@"helpImage"],@"HelpScr02_kr.png");
            if(number==231){XCTAssertEqual([capture[@"helpPages"] intValue],0);XCTAssertEqual([capture[@"helpGuides"] intValue],0);}
        }
        NSDictionary* interruption=report[@"inputInterruption"];for(NSString* key in @[@"capturedBefore",@"discardedAfter",@"beginReachedRealHandler",@"cleanupSentNoAction",@"noExtraTick",@"noSave",@"creditPreserved",@"screenPreserved"])XCTAssertTrue([interruption[key] boolValue],@"Interruption %@ failed",key);
        NSDictionary* moving=report[@"cases"][58];XCTAssertEqual([moving[@"dashStarts"] unsignedIntegerValue],0u);XCTAssertGreaterThan([moving[@"distanceMoved"] doubleValue],100.0);
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
    if(fullCoverage)for(unsigned number:{112u,240u,241u,381u,31u,32u,40u,183u,201u,211u,221u,231u,348u,356u,364u}){XCTAssertTrue(runtime->captureStoredFrame(number,layer));attachPixels(self,runtime->pixels(),runtime->drawableSize(),[NSString stringWithFormat:@"Original GPU source frame%u preview384x512",number]);}
    runtime->setActive(false);runtime.reset();window.hidden=YES;window.rootViewController=nil;
    XCTAssertTrue([NSFileManager.defaultManager removeItemAtURL:directory error:&error]);
}
@end

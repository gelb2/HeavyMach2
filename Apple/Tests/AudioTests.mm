#import <XCTest/XCTest.h>
#import "Platform/Apple/HM2Audio.h"
#include "Platform/Apple/HM2AudioQueueMusic.hpp"
#include <vector>

namespace {
template<class Predicate> bool waitFor(Predicate predicate,double seconds){
    const auto deadline=CFAbsoluteTimeGetCurrent()+seconds;
    do{if(predicate())return true;CFRunLoopRunInMode(kCFRunLoopDefaultMode,.01,false);}while(CFAbsoluteTimeGetCurrent()<deadline);
    return predicate();
}
NSData* shortWave(){
    std::vector<unsigned char> bytes;
    const auto text=[&](const char* s){for(int i=0;i<4;++i)bytes.push_back(s[i]);};
    const auto u16=[&](unsigned v){bytes.push_back(v&255);bytes.push_back((v>>8)&255);};
    const auto u32=[&](unsigned v){for(int i=0;i<4;++i)bytes.push_back((v>>(8*i))&255);};
    text("RIFF");u32(36+4096);text("WAVE");text("fmt ");u32(16);u16(1);u16(1);u32(22050);u32(22050);u16(1);u16(8);text("data");u32(4096);
    for(unsigned i=0;i<4096;++i)bytes.push_back(i%64<32?136:120);
    return [NSData dataWithBytes:bytes.data() length:bytes.size()];
}
}
@interface AudioTests:XCTestCase @end
@implementation AudioTests
- (void)testOriginalLazyNameCacheAndWarmReuse{
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    HM2Audio* audio=[[HM2Audio alloc]initWithResourceRoot:root];XCTAssertNil(audio.lastError);
    const auto owner=audio.ownerContextIdentity;XCTAssertNotEqual(owner,0u);XCTAssertEqual(audio.loadedSounds,0u);
    XCTAssertEqual([audio.metrics[@"effectFileReads"] unsignedIntegerValue],0u);
    XCTAssertTrue([audio playEffect:std::string_view("Wpn_Gun01.wav") x:0 y:0]);
    XCTAssertEqual(audio.loadedSounds,1u);XCTAssertEqual([audio.metrics[@"effectFileReads"] unsignedIntegerValue],1u);
    XCTAssertEqual([audio.metrics[@"effectColdNameConversions"] unsignedIntegerValue],1u);
    NSDictionary* warm=audio.metrics;
    for(unsigned i=0;i<100;++i)XCTAssertTrue([audio playEffect:std::string_view("Wpn_Gun01.wav") x:0 y:0]);
    for(NSString* key in @[@"contextsCreated",@"sourcesCreated",@"buffersCreated",@"effectFileReads",@"effectDecodes",@"effectColdNameConversions"])
        XCTAssertEqualObjects(warm[key],audio.metrics[key],@"Warm lazy cache changed: %@",key);
    [audio setActive:NO];XCTAssertFalse([audio playEffect:std::string_view("SFX_Dash.wav") x:0 y:0]);XCTAssertEqual(audio.loadedSounds,1u);
    [audio setActive:YES];XCTAssertTrue([audio playEffect:std::string_view("SFX_Dash.wav") x:0 y:0]);
    XCTAssertEqual(audio.loadedSounds,2u);XCTAssertEqual(audio.ownerContextIdentity,owner);
    XCTAssertEqual([audio.metrics[@"contextsCreated"] unsignedIntegerValue],1u);
    XCTAssertEqual([audio.metrics[@"effectFileReads"] unsignedIntegerValue],2u);
}
- (void)testOriginalAudioFilesAndWarmEffectResources{
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    HM2Audio* audio=[[HM2Audio alloc]initWithResourceRoot:root];XCTAssertTrue([audio prepareSounds],@"%@",audio.lastError);
    XCTAssertEqual(audio.loadedSounds,57u);const auto context=audio.currentContextIdentity;XCTAssertNotEqual(context,0u);XCTAssertEqual(context,audio.ownerContextIdentity);
    NSDictionary* before=audio.metrics;XCTAssertEqual([before[@"effectFileReads"] unsignedIntegerValue],57u);XCTAssertEqual([before[@"effectDecodes"] unsignedIntegerValue],57u);
    XCTAssertTrue([audio playEffect:std::string_view("Wpn_Gun01.wav") x:0 y:0]);
    NSInteger sampleOffset=-1;XCTAssertTrue(waitFor([&]{sampleOffset=[audio effectSampleOffset:@"Wpn_Gun01.wav"];return sampleOffset>0;},1.));
    for(int i=0;i<200;++i){XCTAssertTrue([audio prepareSound:@"Wpn_Gun01.wav"]);XCTAssertTrue([audio playEffect:std::string_view("Wpn_Gun01.wav") x:.01f y:-.02f]);}
    [audio setActive:NO];XCTAssertFalse([audio playEffect:std::string_view("Wpn_Gun01.wav") x:0 y:0]);[audio setActive:YES];XCTAssertTrue([audio playEffect:std::string_view("Wpn_Gun01.wav") x:0 y:0]);
    XCTAssertTrue(waitFor([&]{return [audio effectSampleOffset:@"Wpn_Gun01.wav"]>0;},1.));
    NSDictionary* after=audio.metrics;
    for(NSString* key in @[@"devicesOpened",@"contextsCreated",@"sourcesCreated",@"buffersCreated",@"effectFileReads",@"effectDecodes",@"contextBindCalls",@"contextClearCalls",@"contextBindFailures",@"effectNameConversions"])XCTAssertEqualObjects(before[key],after[key],@"Resource changed during warm play: %@",key);
    XCTAssertEqualObjects(after[@"currentContext"],after[@"ownerContext"]);XCTAssertEqual([after[@"contextBindCalls"] unsignedIntegerValue],1u);XCTAssertEqual([after[@"contextClearCalls"] unsignedIntegerValue],0u);XCTAssertEqual([after[@"contextBindFailures"] unsignedIntegerValue],0u);XCTAssertEqual([after[@"effectNameConversions"] unsignedIntegerValue],0u);XCTAssertGreaterThan([after[@"effectByteNameRequests"] unsignedIntegerValue],200u);
    XCTAssertEqual(audio.currentContextIdentity,context);XCTAssertEqual([after[@"contextsCreated"] unsignedIntegerValue],1u);XCTAssertEqual([after[@"sourcesCreated"] unsignedIntegerValue],57u);XCTAssertEqual([after[@"buffersCreated"] unsignedIntegerValue],57u);
    XCTAssertEqual([after[@"sourcesDeleted"] unsignedIntegerValue],0u);XCTAssertEqual([after[@"buffersDeleted"] unsignedIntegerValue],0u);
    NSLog(@"HM2_AUDIO_APPLE sampleOffset=%ld metrics=%@",(long)sampleOffset,after);
}
- (void)testCurrentContextOwnershipAcrossAudioInstances{
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    HM2Audio* first=[[HM2Audio alloc]initWithResourceRoot:root];XCTAssertTrue([first prepareSound:@"Wpn_Gun01.wav"]);const auto firstOwner=first.ownerContextIdentity;
    @autoreleasepool{HM2Audio* second=[[HM2Audio alloc]initWithResourceRoot:root];XCTAssertTrue([second prepareSound:@"Wpn_Gun01.wav"]);XCTAssertEqual(second.currentContextIdentity,second.ownerContextIdentity);XCTAssertNotEqual(second.currentContextIdentity,firstOwner);
        const auto before=[first.metrics[@"contextBindCalls"] unsignedIntegerValue];XCTAssertTrue([first playEffect:std::string_view("Wpn_Gun01.wav") x:0 y:0]);XCTAssertEqual(first.currentContextIdentity,firstOwner);XCTAssertEqual([first.metrics[@"contextBindCalls"] unsignedIntegerValue],before+1);
        const auto held=first.metrics;XCTAssertTrue([first playEffect:std::string_view("Wpn_Gun01.wav") x:0 y:0]);(void)[first effectSampleOffset:@"Wpn_Gun01.wav"];(void)first.playbackSnapshot;XCTAssertEqualObjects(held[@"contextBindCalls"],first.metrics[@"contextBindCalls"]);XCTAssertEqualObjects(held[@"contextClearCalls"],first.metrics[@"contextClearCalls"]);
    }
    XCTAssertEqual(first.currentContextIdentity,firstOwner);
}
- (void)testRepeatedInactiveCallbacksRetainAudioOwners{
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    HM2Audio* audio=[[HM2Audio alloc]initWithResourceRoot:root];XCTAssertTrue([audio prepareSounds]);XCTAssertTrue([audio playMusic:@"STAGE3.mp3"]);
    const auto owner=audio.ownerContextIdentity;NSDictionary* before=audio.metrics;
    for(unsigned i=0;i<20;++i){
        [audio setActive:NO];[audio setActive:NO];XCTAssertFalse(audio.musicPlaying);
        XCTAssertFalse([audio playEffect:std::string_view("Wpn_Gun01.wav") x:0 y:0]);
        [audio setActive:YES];[audio setActive:YES];
        XCTAssertTrue([audio playEffect:std::string_view("Wpn_Gun01.wav") x:0 y:0]);
        XCTAssertEqual(audio.ownerContextIdentity,owner);XCTAssertEqual(audio.currentContextIdentity,owner);
    }
    NSDictionary* after=audio.metrics;
    for(NSString* key in @[@"contextsCreated",@"devicesOpened",@"sourcesCreated",@"buffersCreated",@"effectFileReads",@"effectDecodes",@"musicQueuesCreated",@"musicBuffersAllocated"])
        XCTAssertEqualObjects(before[key],after[key],@"Lifecycle recreated audio owner/resource: %@",key);
    XCTAssertEqual([after[@"contextsCreated"] unsignedIntegerValue],1u);
    [audio stopMusic];
}
- (void)testOriginalMusicRestartsAndWholeFileLoop{
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    HM2Audio* audio=[[HM2Audio alloc]initWithResourceRoot:root];XCTAssertTrue([audio playMusic:@"STAGE3.mp3"],@"%@",audio.lastError);
    XCTAssertTrue(waitFor([&]{return audio.musicPlaybackTime>0;},1.));
    XCTAssertEqual([audio.metrics[@"musicBufferCount"] unsignedIntegerValue],3u);XCTAssertEqual([audio.metrics[@"musicPrimes"] unsignedIntegerValue],1u);
    XCTAssertTrue([audio playMusic:@"STAGE3.mp3"]);XCTAssertEqual([audio.metrics[@"musicQueuesCreated"] unsignedIntegerValue],2u);XCTAssertEqual([audio.metrics[@"musicPrimes"] unsignedIntegerValue],2u);
    [audio setActive:NO];XCTAssertFalse(audio.musicPlaying);[audio setActive:YES];XCTAssertTrue(waitFor([&]{return audio.musicPlaybackTime>0;},1.));
    NSLog(@"HM2_MUSIC_APPLE metrics=%@",audio.metrics);[audio stopMusic];
    NSURL* shortURL=[NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:[NSString stringWithFormat:@"hm2-audio-%@.wav",NSUUID.UUID.UUIDString]]];
    XCTAssertTrue([shortWave() writeToURL:shortURL atomically:YES]);
    hm2::audio::AudioQueueMusic music;XCTAssertTrue(music.open((__bridge CFURLRef)shortURL));XCTAssertTrue(music.policy().wholeFile);XCTAssertEqual(music.policy().buffers,1u);XCTAssertTrue(music.start());
    XCTAssertTrue(waitFor([&]{return music.time()>.4f&&music.loops()>=2;},2.));XCTAssertEqual(music.packetReads(),1u);XCTAssertEqual(music.buffersAllocated(),1u);XCTAssertGreaterThan(music.enqueues(),1u);
    NSLog(@"HM2_MUSIC_SMALL loops=%llu reads=%llu enqueues=%llu",(unsigned long long)music.loops(),(unsigned long long)music.packetReads(),(unsigned long long)music.enqueues());
    music.close();[[NSFileManager defaultManager]removeItemAtURL:shortURL error:nil];
}
@end

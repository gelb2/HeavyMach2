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
- (void)testOriginalAudioFilesAndWarmEffectResources{
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];XCTAssertNotNil(root);if(!root)return;
    HM2Audio* audio=[[HM2Audio alloc]initWithResourceRoot:root];XCTAssertTrue([audio prepareSounds],@"%@",audio.lastError);
    XCTAssertEqual(audio.loadedSounds,57u);const auto context=audio.currentContextIdentity;XCTAssertNotEqual(context,0u);
    NSDictionary* before=audio.metrics;XCTAssertEqual([before[@"effectFileReads"] unsignedIntegerValue],57u);XCTAssertEqual([before[@"effectDecodes"] unsignedIntegerValue],57u);
    XCTAssertTrue([audio play:@"Wpn_Gun01.wav" x:0 y:0]);
    NSInteger sampleOffset=-1;XCTAssertTrue(waitFor([&]{sampleOffset=[audio effectSampleOffset:@"Wpn_Gun01.wav"];return sampleOffset>0;},1.));
    for(int i=0;i<200;++i){XCTAssertTrue([audio prepareSound:@"Wpn_Gun01.wav"]);XCTAssertTrue([audio play:@"Wpn_Gun01.wav" x:.01f y:-.02f]);}
    [audio setActive:NO];XCTAssertFalse([audio play:@"Wpn_Gun01.wav" x:0 y:0]);[audio setActive:YES];XCTAssertTrue([audio play:@"Wpn_Gun01.wav" x:0 y:0]);
    XCTAssertTrue(waitFor([&]{return [audio effectSampleOffset:@"Wpn_Gun01.wav"]>0;},1.));
    NSDictionary* after=audio.metrics;
    for(NSString* key in @[@"devicesOpened",@"contextsCreated",@"sourcesCreated",@"buffersCreated",@"effectFileReads",@"effectDecodes"])XCTAssertEqualObjects(before[key],after[key],@"Resource changed during warm play: %@",key);
    XCTAssertEqual(audio.currentContextIdentity,context);XCTAssertEqual([after[@"contextsCreated"] unsignedIntegerValue],1u);XCTAssertEqual([after[@"sourcesCreated"] unsignedIntegerValue],57u);XCTAssertEqual([after[@"buffersCreated"] unsignedIntegerValue],57u);
    XCTAssertEqual([after[@"sourcesDeleted"] unsignedIntegerValue],0u);XCTAssertEqual([after[@"buffersDeleted"] unsignedIntegerValue],0u);
    NSLog(@"HM2_AUDIO_APPLE sampleOffset=%ld metrics=%@",(long)sampleOffset,after);
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

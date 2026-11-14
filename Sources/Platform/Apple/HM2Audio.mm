#import "HM2Audio.h"
#include "HM2AudioQueueMusic.hpp"
#include "Core/Audio/OpenALEffects.hpp"
#include <cmath>

namespace {
// Adapted from MachDefense 0aa03aa7 MDAudio.mm. Original HM2 0x41368
// AudioFile PCM loader: preserve 8/16-bit mono/stereo and source rate.
bool decode(NSURL* url,hm2::audio::PCM& output){
    AudioFileID file=nullptr;if(AudioFileOpenURL((__bridge CFURLRef)url,kAudioFileReadPermission,0,&file))return false;
    struct Close{AudioFileID file;~Close(){AudioFileClose(file);}} close{file};
    AudioStreamBasicDescription pcm{};UInt32 size=sizeof(pcm);
    if(AudioFileGetProperty(file,kAudioFilePropertyDataFormat,&size,&pcm))return false;
    const auto channels=pcm.mChannelsPerFrame,bits=pcm.mBitsPerChannel;
    if(pcm.mFormatID!=kAudioFormatLinearPCM||channels<1||channels>2||(bits!=8&&bits!=16)||
       !std::isfinite(pcm.mSampleRate)||pcm.mSampleRate<1||pcm.mSampleRate>192000||
       pcm.mSampleRate!=std::floor(pcm.mSampleRate)||(pcm.mFormatFlags&(kAudioFormatFlagIsBigEndian|kAudioFormatFlagIsFloat|kAudioFormatFlagIsNonInterleaved))||
       (bits==8&&(pcm.mFormatFlags&kAudioFormatFlagIsSignedInteger))||(bits==16&&!(pcm.mFormatFlags&kAudioFormatFlagIsSignedInteger))||
       pcm.mBytesPerFrame!=channels*bits/8)return false;
    UInt64 bytes=0;size=sizeof(bytes);
    if(AudioFileGetProperty(file,kAudioFilePropertyAudioDataByteCount,&size,&bytes)||!bytes||bytes>64*1024*1024||bytes%pcm.mBytesPerFrame)return false;
    output.bytes.resize(bytes);UInt32 count=UInt32(bytes);
    if(AudioFileReadBytes(file,false,0,&count,output.bytes.data())||count!=bytes)return false;
    output.sampleRate=unsigned(pcm.mSampleRate);output.channels=channels;output.bitsPerSample=bits;return true;
}
bool fileName(NSString* name){return name.length&&[name.lastPathComponent isEqualToString:name]&&![name isEqualToString:@"."]&&![name isEqualToString:@".."];}
}
@implementation HM2Audio {
    NSURL* _root;hm2::audio::OpenALEffects _effects;hm2::audio::AudioQueueMusic _music;
    BOOL _active;NSUInteger _effectFileReads,_effectDecodes,_effectNameConversions,_effectColdNameConversions,_effectByteNameRequests;NSString* _musicName;NSString* _lastError;
    NSMutableArray<NSDictionary*>* _preparationEvents;
}
- (instancetype)initWithResourceRoot:(NSURL*)root{
    if(!(self=[super init]))return nil;_root=[root copy];_active=YES;_preparationEvents=[NSMutableArray new];
    if(!_effects.valid())_lastError=[NSString stringWithUTF8String:_effects.error().c_str()];return self;
}
- (BOOL)prepareSounds{
    NSError* error=nil;NSArray<NSURL*>* files=[[NSFileManager defaultManager] contentsOfDirectoryAtURL:_root includingPropertiesForKeys:nil options:0 error:&error];
    if(!files){_lastError=error.localizedDescription;return NO;}
    BOOL ok=YES;for(NSURL* url in files)if([url.pathExtension.lowercaseString isEqualToString:@"wav"]&&! [self prepareSound:url.lastPathComponent])ok=NO;
    return ok;
}
- (BOOL)prepareSound:(NSString*)name{
    if(!fileName(name)||!_effects.valid())return NO;if(_effects.contains(name.UTF8String))return YES;
    const auto before=_effects.metrics();const auto reads=_effectFileReads,decodes=_effectDecodes,loaded=_effects.loaded();
    const auto record=[&](BOOL success){
        const auto after=_effects.metrics();
        [_preparationEvents addObject:@{@"ordinal":@(_preparationEvents.count+1),@"name":[name copy],@"success":@(success),@"fileReads":@(_effectFileReads-reads),@"decodes":@(_effectDecodes-decodes),@"sourcesCreated":@(after.sourcesCreated-before.sourcesCreated),@"buffersCreated":@(after.buffersCreated-before.buffersCreated),@"sourcesDeleted":@(after.sourcesDeleted-before.sourcesDeleted),@"buffersDeleted":@(after.buffersDeleted-before.buffersDeleted),@"loadedBefore":@(loaded),@"loadedAfter":@(_effects.loaded()),@"sourceIdentity":@(_effects.sourceIdentity(name.UTF8String)),@"ownerContext":@(_effects.contextIdentity())}];
        return success;
    };
    hm2::audio::PCM pcm;++_effectFileReads;
    if(!decode([_root URLByAppendingPathComponent:name],pcm)){_lastError=[NSString stringWithFormat:@"Cannot read original PCM %@",name];return record(NO);}++_effectDecodes;
    if(!_effects.prepare(name.UTF8String,pcm)){_lastError=[NSString stringWithUTF8String:_effects.error().c_str()];return record(NO);}return record(YES);
}
- (BOOL)play:(NSString*)name x:(float)x y:(float)y{
    if(!name.length)return NO;++_effectNameConversions;
    return [self playEffect:std::string_view(name.UTF8String) x:x y:y];
}
- (BOOL)prepareEffect:(std::string_view)name{
    if(name.empty())return NO;
    // SoundManager.getSound0x423a8 looks up the filename dictionary first.
    // Only a miss constructs/loads the effect. Keep the one context and each
    // loaded source/buffer through subsequent shots and interruptions.
    if(!_effects.contains(name)){
        NSString* filename=[[NSString alloc]initWithBytes:name.data() length:name.size() encoding:NSUTF8StringEncoding];++_effectColdNameConversions;
        if(!filename||![self prepareSound:filename])return NO;
    }
    return YES;
}
- (BOOL)playEffect:(std::string_view)name x:(float)x y:(float)y{
    if(name.empty()||!_active)return NO;++_effectByteNameRequests;
    if(![self prepareEffect:name])return NO;
    const BOOL ok=_effects.play(name,x,y);
    if(!ok&&!_effects.error().empty())_lastError=[NSString stringWithUTF8String:_effects.error().c_str()];return ok;
}
- (void)stopEffects{_effects.stop();}
- (void)setActive:(BOOL)active{
    if(_active==active)return;
    _active=active;if(active){_effects.setActive(true);if(_musicName.length&&!_music.playing()&&!_music.start())_lastError=[NSString stringWithFormat:@"AudioQueue resume failed (%d)",int(_music.error())];}
    else{_music.pause();_effects.setActive(false);}
}
- (BOOL)playMusic:(NSString*)name{
    if(!fileName(name))return NO;
    // HD0x424d8 always destroys the old queue and opens the requested track,
    // including a request for the same filename. Never deduplicate that call.
    _musicName=nil;
    if(!_music.open((__bridge CFURLRef)[_root URLByAppendingPathComponent:name])){_lastError=[NSString stringWithFormat:@"AudioQueue cannot open %@ (%d)",name,int(_music.error())];_music.close();return NO;}
    _musicName=[name copy];
    if(_active&&!_music.start()){_lastError=[NSString stringWithFormat:@"AudioQueue start failed (%d)",int(_music.error())];return NO;}return YES;
}
- (void)stopMusic{_music.close();_musicName=nil;}
- (float)effectPlaybackTime:(NSString*)name{return name.length?_effects.playbackTime(name.UTF8String):-1;}
- (NSInteger)effectSampleOffset:(NSString*)name{return name.length?_effects.sampleOffset(name.UTF8String):-1;}
- (NSUInteger)currentContextIdentity{return _effects.actualContextIdentity();}
- (NSUInteger)ownerContextIdentity{return _effects.contextIdentity();}
- (NSUInteger)loadedSounds{return _effects.loaded();}
- (NSString*)lastError{return _lastError;}
- (NSString*)musicName{return _musicName;}
- (BOOL)musicPlaying{return _music.playing();}
- (BOOL)musicLooping{return _musicName.length>0;}
- (float)musicPlaybackTime{return _music.time();}
- (NSDictionary<NSString*,NSNumber*>*)metrics{
    const auto m=_effects.metrics();const auto p=_music.policy();
    return @{@"loadedSounds":@(_effects.loaded()),@"devicesOpened":@(m.devicesOpened),@"contextsCreated":@(m.contextsCreated),@"sourcesCreated":@(m.sourcesCreated),@"buffersCreated":@(m.buffersCreated),@"sourcesDeleted":@(m.sourcesDeleted),@"buffersDeleted":@(m.buffersDeleted),@"effectPlayRequests":@(m.playRequests),@"effectPositionRequests":@(m.positionRequests),@"effectStopRequests":@(m.stopRequests),@"effectPositionErrors":@(m.positionErrors),@"contextBindCalls":@(m.contextBindCalls),@"contextClearCalls":@(m.contextClearCalls),@"contextBindFailures":@(m.contextBindFailures),@"currentContext":@(_effects.actualContextIdentity()),@"ownerContext":@(_effects.contextIdentity()),@"effectNameConversions":@(_effectNameConversions),@"effectColdNameConversions":@(_effectColdNameConversions),@"effectByteNameRequests":@(_effectByteNameRequests),@"effectFileReads":@(_effectFileReads),@"effectDecodes":@(_effectDecodes),@"mixerRate":@(_effects.mixerRate()),@"musicQueuesCreated":@(_music.queuesCreated()),@"musicBuffersAllocated":@(_music.buffersAllocated()),@"musicPacketReads":@(_music.packetReads()),@"musicEnqueues":@(_music.enqueues()),@"musicLoops":@(_music.loops()),@"musicPrimes":@(_music.primes()),@"musicStarts":@(_music.starts()),@"musicBufferBytes":@(p.bytes),@"musicBufferCount":@(p.buffers),@"musicWholeFile":@(p.wholeFile)};
}
- (NSArray<NSDictionary*>*)playbackSnapshot{
    NSMutableArray<NSDictionary*>* result=[NSMutableArray array];
    for(const auto& effect:_effects.playbackSnapshot())[result addObject:@{@"name":[NSString stringWithUTF8String:effect.name.c_str()],@"valid":@(effect.valid),@"playing":@(effect.playing),@"sampleOffset":@(effect.sampleOffset)}];return result;
}
- (NSArray<NSDictionary*>*)preparationSnapshot{return [_preparationEvents copy];}
@end

#pragma once
#include "Core/Audio/MusicBufferPolicy.hpp"
#include <AudioToolbox/AudioToolbox.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace hm2::audio {
// Structure adapted from MachDefense 0aa03aa7 MDAudioQueueMusic.hpp (MIT).
// HM2 differences follow HD0x41d04/0x41a20/0x416d4/0x40f50: one/three
// buffers, cached whole-file loop, .5s truncation, Prime(1) before first Start.
class AudioQueueMusic {
    AudioFileID file_=nullptr;AudioQueueRef queue_=nullptr;
    AudioStreamBasicDescription format_{};
    std::array<AudioQueueBufferRef,3> buffers_{};
    std::vector<AudioStreamPacketDescription> descriptions_;
    MusicBufferPolicy policy_{};SInt64 packet_=0;
    std::atomic<OSStatus> error_{noErr};
    std::atomic_bool closing_{false},playing_{false};
    std::atomic_uint64_t queues_{0},allocated_{0},reads_{0},enqueues_{0},loops_{0},primes_{0},starts_{0};
    bool wholeFilled_=false,primed_=false;
    static void refill(void* self,AudioQueueRef queue,AudioQueueBufferRef buffer){static_cast<AudioQueueMusic*>(self)->fill(queue,buffer);}
    static void runningChanged(void* self,AudioQueueRef queue,AudioQueuePropertyID){
        auto& music=*static_cast<AudioQueueMusic*>(self);if(music.closing_)return;
        UInt32 running=0,size=sizeof(running);
        const auto status=AudioQueueGetProperty(queue,kAudioQueueProperty_IsRunning,&running,&size);
        if(status)music.error_=status;else music.playing_=running!=0;
    }
    void fill(AudioQueueRef queue,AudioQueueBufferRef buffer){
        if(closing_||error_||!file_)return;
        UInt32 packets=policy_.packets;
        auto* desc=descriptions_.empty()?nullptr:descriptions_.data();
        if(policy_.wholeFile&&wholeFilled_){++loops_;}
        else{
            UInt32 bytes=policy_.bytes;
            ++reads_;error_=AudioFileReadPacketData(file_,false,&bytes,desc,packet_,&packets,buffer->mAudioData);
            if(error_)return;
            if(!packets){
                packet_=0;packets=policy_.packets;bytes=policy_.bytes;++loops_;++reads_;
                error_=AudioFileReadPacketData(file_,false,&bytes,desc,packet_,&packets,buffer->mAudioData);
            }
            if(error_||!packets){if(!error_)error_=kAudioFileInvalidPacketOffsetError;return;}
            packet_+=packets;buffer->mAudioDataByteSize=bytes;
            if(policy_.wholeFile){
                // Original marks a small track as already read and reuses its
                // queued PCM/compressed packets on the next callback.
                if(packets!=policy_.packets){error_=kAudioFileInvalidPacketOffsetError;return;}
                wholeFilled_=true;
            }
        }
        error_=AudioQueueEnqueueBuffer(queue,buffer,desc?packets:0,desc);
        if(!error_)++enqueues_;
    }
public:
    AudioQueueMusic()=default;
    AudioQueueMusic(const AudioQueueMusic&)=delete;
    AudioQueueMusic& operator=(const AudioQueueMusic&)=delete;
    ~AudioQueueMusic(){close();}
    void close(){
        closing_=true;playing_=false;
        // Synchronous disposal drains callbacks before closing their file and
        // releasing packet descriptions. Never destroy a queue per effect.
        if(queue_){AudioQueueStop(queue_,true);AudioQueueDispose(queue_,true);queue_=nullptr;}
        if(file_){AudioFileClose(file_);file_=nullptr;}
        buffers_={};descriptions_.clear();policy_={};packet_=0;wholeFilled_=false;primed_=false;
    }
    bool open(CFURLRef url){
        close();closing_=false;error_=AudioFileOpenURL(url,kAudioFileReadPermission,0,&file_);
        if(error_)return false;
        UInt32 size=sizeof(format_);error_=AudioFileGetProperty(file_,kAudioFilePropertyDataFormat,&size,&format_);if(error_)return false;
        UInt32 maxPacket=0;size=sizeof(maxPacket);error_=AudioFileGetProperty(file_,kAudioFilePropertyPacketSizeUpperBound,&size,&maxPacket);if(error_)return false;
        UInt64 fileBytes=0,filePackets=0;size=sizeof(fileBytes);error_=AudioFileGetProperty(file_,kAudioFilePropertyAudioDataByteCount,&size,&fileBytes);if(error_)return false;
        size=sizeof(filePackets);error_=AudioFileGetProperty(file_,kAudioFilePropertyAudioDataPacketCount,&size,&filePackets);if(error_)return false;
        try{policy_=musicBufferPolicy(format_.mSampleRate,format_.mFramesPerPacket,maxPacket,fileBytes,filePackets);}
        catch(...){error_=kAudioFileUnsupportedDataFormatError;return false;}
        if(!format_.mBytesPerPacket||!format_.mFramesPerPacket)descriptions_.resize(policy_.packets);
        error_=AudioQueueNewOutput(&format_,refill,this,CFRunLoopGetCurrent(),kCFRunLoopCommonModes,0,&queue_);
        if(error_)return false;++queues_;
        for(const auto prop:{std::pair{kAudioFilePropertyMagicCookieData,kAudioQueueProperty_MagicCookie},std::pair{kAudioFilePropertyChannelLayout,kAudioQueueProperty_ChannelLayout}}){
            UInt32 n=0;if(AudioFileGetPropertyInfo(file_,prop.first,&n,nullptr)==noErr&&n){
                std::vector<std::uint8_t> data(n);error_=AudioFileGetProperty(file_,prop.first,&n,data.data());if(error_)return false;
                error_=AudioQueueSetProperty(queue_,prop.second,data.data(),n);if(error_)return false;
            }
        }
        error_=AudioQueueAddPropertyListener(queue_,kAudioQueueProperty_IsRunning,runningChanged,this);if(error_)return false;
        // HD0x40644 instance gain=1; binary __DATA 0x5dea4 global gain=1.
        error_=AudioQueueSetParameter(queue_,kAudioQueueParam_Volume,1.f);if(error_)return false;
        for(unsigned i=0;i<policy_.buffers;++i){
            error_=AudioQueueAllocateBuffer(queue_,policy_.bytes,&buffers_[i]);if(error_)return false;++allocated_;
            fill(queue_,buffers_[i]);if(error_)return false;
        }
        return packet_>0;
    }
    bool start(){
        if(!queue_||error_)return false;
        if(!primed_){error_=AudioQueuePrime(queue_,1,nullptr);if(error_)return false;++primes_;primed_=true;}
        error_=AudioQueueStart(queue_,nullptr);if(!error_){++starts_;playing_=true;}return !error_;
    }
    void pause(){if(queue_){error_=AudioQueuePause(queue_);playing_=false;}}
    bool playing()const{return playing_&&!error_;}
    OSStatus error()const{return error_;}
    MusicBufferPolicy policy()const{return policy_;}
    float time()const{
        AudioTimeStamp stamp{};Boolean discontinuity=false;
        if(!queue_||AudioQueueGetCurrentTime(queue_,nullptr,&stamp,&discontinuity)!=noErr||format_.mSampleRate<=0)return -1;
        return float(stamp.mSampleTime/format_.mSampleRate);
    }
    std::uint64_t queuesCreated()const{return queues_;}
    std::uint64_t buffersAllocated()const{return allocated_;}
    std::uint64_t packetReads()const{return reads_;}
    std::uint64_t enqueues()const{return enqueues_;}
    std::uint64_t loops()const{return loops_;}
    std::uint64_t primes()const{return primes_;}
    std::uint64_t starts()const{return starts_;}
};
}

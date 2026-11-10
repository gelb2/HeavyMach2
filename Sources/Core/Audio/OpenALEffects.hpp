#pragma once
#include "Core/Audio/PCM.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hm2::audio {
enum class Output {device,loopback};
struct AudioMetrics {
    std::uint64_t devicesOpened=0,contextsCreated=0,buffersCreated=0,sourcesCreated=0;
    std::uint64_t buffersDeleted=0,sourcesDeleted=0,playRequests=0,positionRequests=0,stopRequests=0,positionErrors=0;
};
struct EffectPlayback {std::string name;bool valid=false,playing=false;int sampleOffset=-1;};
// The one-shot path actually used by HD SoundManager::getSound0x423a8.
// Source/buffer lifetime structure adapted from MachDefense 0aa03aa7, with HM2
// position->stop->play order and source-before-buffer preparation restored.
// The lower unused attack/loop/decay effect API is not silently emulated here.
class OpenALEffects {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    explicit OpenALEffects(Output output=Output::device,unsigned mixerRate=22050);
    ~OpenALEffects();
    OpenALEffects(const OpenALEffects&)=delete;
    OpenALEffects& operator=(const OpenALEffects&)=delete;
    bool valid()const;
    bool contains(std::string_view)const;
    bool prepare(std::string,const PCM&);
    bool play(std::string_view,float x=0,float y=0);
    void stop();
    void setActive(bool);
    std::size_t loaded()const;
    AudioMetrics metrics()const;
    std::uintptr_t contextIdentity()const;
    std::uint32_t sourceIdentity(std::string_view)const;
    int sampleOffset(std::string_view)const;
    float playbackTime(std::string_view)const;
    std::vector<EffectPlayback> playbackSnapshot()const;
    unsigned mixerRate()const;
    const std::string& error()const;
    // Deterministic software output test only; never substitutes for an actual
    // Apple output-device/interruption test. No image or sound is fabricated.
    bool renderLoopback(std::span<std::int16_t> stereoSamples);
};
}

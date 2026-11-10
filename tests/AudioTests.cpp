#include "Core/Audio/PCM.hpp"
#include "Core/Audio/OpenALEffects.hpp"
#include "Core/Audio/MusicBufferPolicy.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <tuple>
using namespace hm2::audio;
namespace {
void u16(std::vector<std::uint8_t>& v,unsigned n){v.push_back(n&255);v.push_back((n>>8)&255);}
void u32(std::vector<std::uint8_t>& v,unsigned n){u16(v,n);u16(v,n>>16);}
void text(std::vector<std::uint8_t>& v,const char* s){for(int i=0;i<4;++i)v.push_back(s[i]);}
std::vector<std::uint8_t> wave(unsigned channels,unsigned bits){
    const unsigned align=channels*bits/8;std::vector<std::uint8_t> v;
    text(v,"RIFF");u32(v,36+align*4);text(v,"WAVE");text(v,"fmt ");u32(v,16);u16(v,1);u16(v,channels);u32(v,22050);u32(v,22050*align);u16(v,align);u16(v,bits);text(v,"data");u32(v,align*4);for(unsigned i=0;i<align*4;++i)v.push_back((i*17+13)&255);return v;
}
std::vector<std::uint8_t> read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};}
void parser(){
    for(unsigned channels:{1u,2u})for(unsigned bits:{8u,16u}){
        const auto encoded=wave(channels,bits);const auto pcm=decodeWave(encoded);
        assert(pcm.channels==channels&&pcm.bitsPerSample==bits&&pcm.sampleRate==22050);
        assert(std::equal(pcm.bytes.begin(),pcm.bytes.end(),encoded.begin()+44,encoded.end()));
        auto truncated=encoded;truncated.pop_back();bool fail=false;try{decodeWave(truncated);}catch(...){fail=true;}assert(fail);
    }
    for(auto offset:{20u,22u,32u,34u}){
        auto b=wave(1,16);b[offset]=255;bool fail=false;try{decodeWave(b);}catch(...){fail=true;}assert(fail);
    }
    auto odd=wave(1,8);odd.insert(odd.begin()+12,{'J','U','N','K',1,0,0,0,42,0});const auto n=unsigned(odd.size()-8);for(unsigned i=0;i<4;++i)odd[4+i]=std::uint8_t(n>>(i*8));assert(decodeWave(odd).bytes==decodeWave(wave(1,8)).bytes);
    std::cout<<"PCM synthetic formats=4 invalid/truncated boundaries PASS\n";
}
void musicPolicy(){
    const auto truncated=musicBufferPolicy(44100,1152,1045,1000000,2000);
    assert(truncated.bytes==20001&&truncated.packets==19&&truncated.buffers==3&&!truncated.wholeFile);
    const auto small=musicBufferPolicy(22050,1,2,8000,4000);
    assert(small.bytes==8000&&small.packets==4000&&small.buffers==1&&small.wholeFile);
    const auto boundary=musicBufferPolicy(22050,1,2,66150,33075);
    assert(!boundary.wholeFile&&boundary.buffers==3&&boundary.bytes==22050);
    assert(musicBufferPolicy(22050,0,100,1000000,10000).bytes==65536);
    assert(musicBufferPolicy(22050,1,2,1000000,500000).bytes==22050);
    bool rejected=false;try{musicBufferPolicy(22050,1,131072,1000000,10);}catch(...){rejected=true;}assert(rejected);
    std::cout<<"BGM truncation/clamp/one-buffer/three-buffer boundaries PASS\n";
}
}
int main(int argc,char** argv){
    parser();musicPolicy();if(argc<2)return 0;
    std::map<std::string,PCM> originals;std::map<std::tuple<unsigned,unsigned,unsigned>,unsigned> formats;
    for(const auto& entry:std::filesystem::directory_iterator(argv[1]))if(entry.path().extension()==".wav"){
        auto pcm=decodeWave(read(entry.path()));++formats[{pcm.bitsPerSample,pcm.channels,pcm.sampleRate}];originals.emplace(entry.path().filename().string(),std::move(pcm));
    }
    assert(originals.size()==57);
    assert((formats[std::tuple{16u,1u,22050u}]==45));assert((formats[std::tuple{8u,1u,22050u}]==1));
    assert((formats[std::tuple{16u,1u,44100u}]==3));assert((formats[std::tuple{16u,2u,22050u}]==2));
    assert((formats[std::tuple{8u,2u,22050u}]==3));assert((formats[std::tuple{16u,1u,48000u}]==3));
    OpenALEffects effects(Output::loopback);if(!effects.valid())std::cerr<<effects.error()<<'\n';assert(effects.valid());
    for(const auto& [name,pcm]:originals)assert(effects.prepare(name,pcm));
    assert(effects.actualContextIdentity()==effects.contextIdentity());
    assert(effects.loaded()==57);assert(effects.metrics().sourcesCreated==57&&effects.metrics().buffersCreated==57);
    std::array<std::int16_t,4096> samples{};unsigned audibleResources=0;
    for(const auto& [name,pcm]:originals){
        effects.stop();assert(effects.play(name,0,0));
        const auto outputFrames=std::uint64_t(pcm.bytes.size()/(pcm.channels*pcm.bitsPerSample/8))*effects.mixerRate()/pcm.sampleRate;
        bool nonzero=false;
        for(std::uint64_t f=0;f<outputFrames+2048;f+=2048){assert(effects.renderLoopback(samples));nonzero|=std::any_of(samples.begin(),samples.end(),[](auto sample){return sample!=0;});}
        if(!nonzero)std::cerr<<"No software output for original "<<name<<'\n';assert(nonzero);++audibleResources;
    }
    assert(audibleResources==57);effects.stop();
    const auto identity=effects.contextIdentity();const auto source=effects.sourceIdentity("Wpn_Gun01.wav");assert(source);
    const auto before=effects.metrics();
    assert(effects.play("Wpn_Gun01.wav",0,0));assert(effects.renderLoopback(samples));
    assert(std::any_of(samples.begin(),samples.end(),[](auto s){return s!=0;}));assert(effects.sampleOffset("Wpn_Gun01.wav")>0);
    assert(effects.play("Wpn_Gun01.wav",0,0));assert(effects.sampleOffset("Wpn_Gun01.wav")==0);
    for(unsigned i=0;i<200;++i){
        assert(effects.prepare("Wpn_Gun01.wav",{})); // Cached name retains PCM.
        assert(effects.play("Wpn_Gun01.wav",float(int(i%7)-3)*.01f,.02f));assert(effects.renderLoopback(samples));
        assert(effects.sourceIdentity("Wpn_Gun01.wav")==source);assert(effects.contextIdentity()==identity);assert(effects.actualContextIdentity()==identity);
        assert(effects.metrics().contextBindCalls==before.contextBindCalls&&effects.metrics().contextClearCalls==before.contextClearCalls);
    }
    assert(effects.play("Wpn_Gun01.wav",std::numeric_limits<float>::quiet_NaN(),0));assert(effects.metrics().positionErrors==1);
    effects.setActive(false);const auto count=effects.metrics().playRequests;assert(!effects.play("Wpn_Gun01.wav"));assert(effects.metrics().playRequests==count);
    effects.setActive(true);assert(effects.play("Wpn_Gun01.wav"));assert(effects.renderLoopback(samples));assert(effects.sampleOffset("Wpn_Gun01.wav")>0);
    const auto after=effects.metrics();assert(after.sourcesCreated==before.sourcesCreated&&after.buffersCreated==before.buffersCreated&&after.contextsCreated==before.contextsCreated&&after.devicesOpened==before.devicesOpened);assert(after.sourcesDeleted==0&&after.buffersDeleted==0);
    assert(after.contextBindCalls==1&&after.contextClearCalls==0&&after.contextBindFailures==0);
    assert(effects.actualContextIdentity()==identity);
    // A different runtime needs one ownership switch; subsequent warm calls
    // do not restore that runtime or clear the selected context.
    {
        OpenALEffects other(Output::loopback);assert(other.valid());const auto otherIdentity=other.contextIdentity();assert(otherIdentity!=identity&&other.actualContextIdentity()==otherIdentity);
        assert(other.prepare("Wpn_Gun01.wav",originals.at("Wpn_Gun01.wav")));
        const auto beforeSwitch=effects.metrics();assert(effects.play("Wpn_Gun01.wav"));assert(effects.actualContextIdentity()==identity);
        assert(effects.metrics().contextBindCalls==beforeSwitch.contextBindCalls+1&&effects.metrics().contextClearCalls==beforeSwitch.contextClearCalls);
        const auto held=effects.metrics();assert(effects.play("Wpn_Gun01.wav"));(void)effects.sampleOffset("Wpn_Gun01.wav");(void)effects.playbackTime("Wpn_Gun01.wav");assert(!effects.playbackSnapshot().empty());assert(effects.prepare("Wpn_Gun01.wav",{}));
        assert(effects.metrics().contextBindCalls==held.contextBindCalls&&effects.metrics().contextClearCalls==held.contextClearCalls&&effects.actualContextIdentity()==identity);
        // Teardown of other restores the still-live current first owner.
    }
    assert(effects.actualContextIdentity()==identity);
    std::cout<<"HM2_AUDIO loopback originals="<<effects.loaded()<<" mixer="<<effects.mixerRate()<<" requests="<<after.playRequests<<" sampleOffset="<<effects.sampleOffset("Wpn_Gun01.wav")<<" sources="<<after.sourcesCreated<<" buffers="<<after.buffersCreated<<" contexts="<<after.contextsCreated<<" devices="<<after.devicesOpened<<" warmBindCalls="<<(after.contextBindCalls-before.contextBindCalls)<<" warmClearCalls="<<(after.contextClearCalls-before.contextClearCalls)<<"\n";
}

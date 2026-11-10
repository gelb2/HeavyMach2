#include "Core/Audio/OpenALEffects.hpp"
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <limits>
#include <map>

namespace hm2::audio {
namespace {
struct Scope {
    ALCcontext* previous=alcGetCurrentContext();ALCcontext* current=nullptr;bool valid=false;
    explicit Scope(ALCcontext* c):current(c){valid=c&&(previous==c||alcMakeContextCurrent(c));}
    ~Scope(){if(valid&&previous!=current)alcMakeContextCurrent(previous);}
};
struct Sound{ALuint source=0,buffer=0;};
}
struct OpenALEffects::Impl {
    ALCdevice* device=nullptr;ALCcontext* context=nullptr;
    LPALCDEVICEPAUSESOFT pauseDevice=nullptr;LPALCDEVICERESUMESOFT resumeDevice=nullptr;
    LPALCRENDERSAMPLESSOFT renderSamples=nullptr;
    std::map<std::string,Sound,std::less<>> sounds;
    AudioMetrics metrics;bool active=true,initialized=false,loopback=false;unsigned rate=0;std::string error;
    explicit Impl(Output output,unsigned mixerRate){
        loopback=output==Output::loopback;
        if(mixerRate<8000||mixerRate>192000){error="Invalid mixer rate";return;}
        if(output==Output::loopback){
            const auto open=reinterpret_cast<LPALCLOOPBACKOPENDEVICESOFT>(alcGetProcAddress(nullptr,"alcLoopbackOpenDeviceSOFT"));
            if(!open){error="OpenAL loopback unavailable";return;}device=open(nullptr);
        }else device=alcOpenDevice(nullptr);
        if(!device){error="Cannot open OpenAL Soft output device";return;}++metrics.devicesOpened;
        const ALCint normal[]={ALC_FREQUENCY,ALCint(mixerRate),0};
        const ALCint loopback[]={ALC_FREQUENCY,ALCint(mixerRate),ALC_FORMAT_CHANNELS_SOFT,ALC_STEREO_SOFT,ALC_FORMAT_TYPE_SOFT,ALC_SHORT_SOFT,0};
        const auto previous=alcGetCurrentContext();
        context=alcCreateContext(device,output==Output::loopback?loopback:normal);
        if(!context){error="Cannot create OpenAL Soft context";return;}++metrics.contextsCreated;
        if(!alcMakeContextCurrent(context)){error="Cannot select OpenAL Soft context";return;}
        // Original SoundManager.init0x4229c sets listener position(0,0,0).
        // Keep the original/default distance model and gain instead of copying
        // another game's positional attenuation or gain policy.
        alListener3f(AL_POSITION,0,0,0);
        pauseDevice=reinterpret_cast<LPALCDEVICEPAUSESOFT>(alcGetProcAddress(device,"alcDevicePauseSOFT"));
        resumeDevice=reinterpret_cast<LPALCDEVICERESUMESOFT>(alcGetProcAddress(device,"alcDeviceResumeSOFT"));
        if(output==Output::loopback)renderSamples=reinterpret_cast<LPALCRENDERSAMPLESSOFT>(alcGetProcAddress(device,"alcRenderSamplesSOFT"));
        ALCint actual=0;alcGetIntegerv(device,ALC_FREQUENCY,1,&actual);rate=unsigned(actual);
        if(alGetError()!=AL_NO_ERROR||alcGetError(device)!=ALC_NO_ERROR||!pauseDevice||!resumeDevice||(output==Output::loopback&&!renderSamples))error="OpenAL Soft initialization failed";
        initialized=error.empty();
        alcMakeContextCurrent(previous);
    }
    ~Impl(){
        const auto previous=alcGetCurrentContext();
        if(context&&alcMakeContextCurrent(context)){
            for(auto& [name,s]:sounds){alSourceStop(s.source);alDeleteSources(1,&s.source);++metrics.sourcesDeleted;alDeleteBuffers(1,&s.buffer);++metrics.buffersDeleted;}
            alcMakeContextCurrent(previous==context?nullptr:previous);
        }
        if(context)alcDestroyContext(context);if(device)alcCloseDevice(device);
    }
};
OpenALEffects::OpenALEffects(Output output,unsigned rate):impl_(std::make_unique<Impl>(output,rate)){}
OpenALEffects::~OpenALEffects()=default;
bool OpenALEffects::valid()const{return impl_->initialized;}
bool OpenALEffects::contains(std::string_view name)const{return impl_->sounds.contains(name);}
bool OpenALEffects::prepare(std::string name,const PCM& pcm){
    if(contains(name))return true;
    if(!valid()||name.empty()||pcm.bytes.empty()||pcm.bytes.size()>64*1024*1024||pcm.sampleRate<1||pcm.sampleRate>192000||pcm.channels<1||pcm.channels>2||(pcm.bitsPerSample!=8&&pcm.bitsPerSample!=16)||pcm.bytes.size()%(pcm.channels*pcm.bitsPerSample/8))return false;
    Scope scope(impl_->context);if(!scope.valid)return false;
    Sound sound;alGenSources(1,&sound.source);if(sound.source)++impl_->metrics.sourcesCreated;
    alGenBuffers(1,&sound.buffer);if(sound.buffer)++impl_->metrics.buffersCreated;
    const ALenum format=pcm.bitsPerSample==8?(pcm.channels==1?AL_FORMAT_MONO8:AL_FORMAT_STEREO8):(pcm.channels==1?AL_FORMAT_MONO16:AL_FORMAT_STEREO16);
    alBufferData(sound.buffer,format,pcm.bytes.data(),ALsizei(pcm.bytes.size()),ALsizei(pcm.sampleRate));
    alSourcei(sound.source,AL_BUFFER,ALint(sound.buffer));
    if(alGetError()!=AL_NO_ERROR){
        if(sound.source){alDeleteSources(1,&sound.source);++impl_->metrics.sourcesDeleted;}
        if(sound.buffer){alDeleteBuffers(1,&sound.buffer);++impl_->metrics.buffersDeleted;}
        impl_->error="Cannot prepare original PCM effect";return false;
    }
    impl_->sounds.emplace(std::move(name),sound);return true;
}
bool OpenALEffects::play(std::string_view name,float x,float y){
    if(!impl_->active||!valid())return false;
    const auto i=impl_->sounds.find(name);if(i==impl_->sounds.end()){impl_->error="Effect was not prepared before play";return false;}
    Scope scope(impl_->context);if(!scope.valid)return false;
    const auto source=i->second.source;
    // HD0x42494->0x407e4 sets position and clears its error before stop/play.
    // An invalid coordinate therefore retains AL's previous position but does
    // not silently omit the rest of the original play request.
    alSource3f(source,AL_POSITION,x,y,0);++impl_->metrics.positionRequests;
    if(alGetError()!=AL_NO_ERROR)++impl_->metrics.positionErrors;
    alSourceStop(source);++impl_->metrics.stopRequests;
    if(alGetError()!=AL_NO_ERROR){impl_->error="OpenAL effect stop failed";return false;}
    alSourcePlay(source);
    if(alGetError()!=AL_NO_ERROR){impl_->error="OpenAL effect play failed";return false;}
    ++impl_->metrics.playRequests;return true;
}
void OpenALEffects::stop(){
    Scope scope(impl_->context);if(!scope.valid)return;
    for(const auto& [name,s]:impl_->sounds){alSourceStop(s.source);++impl_->metrics.stopRequests;}
}
void OpenALEffects::setActive(bool active){
    if(active==impl_->active||!impl_->context||!impl_->pauseDevice||!impl_->resumeDevice)return;
    if(!active)stop();
    // Loopback is driven only by the test's explicit render call. OpenAL Soft
    // does not support device pause on that device, so only gate its requests.
    if(impl_->loopback){impl_->active=active;return;}
    if(active)impl_->resumeDevice(impl_->device);
    else impl_->pauseDevice(impl_->device);
    if(alcGetError(impl_->device)!=ALC_NO_ERROR){impl_->error="OpenAL output device pause/resume failed";return;}
    impl_->active=active;
}
std::size_t OpenALEffects::loaded()const{return impl_->sounds.size();}
AudioMetrics OpenALEffects::metrics()const{return impl_->metrics;}
std::uintptr_t OpenALEffects::contextIdentity()const{return reinterpret_cast<std::uintptr_t>(impl_->context);}
std::uint32_t OpenALEffects::sourceIdentity(std::string_view name)const{const auto i=impl_->sounds.find(name);return i==impl_->sounds.end()?0:i->second.source;}
int OpenALEffects::sampleOffset(std::string_view name)const{
    Scope scope(impl_->context);if(!scope.valid)return -1;const auto i=impl_->sounds.find(name);if(i==impl_->sounds.end())return -1;
    ALint offset=0;alGetSourcei(i->second.source,AL_SAMPLE_OFFSET,&offset);return alGetError()==AL_NO_ERROR?offset:-1;
}
float OpenALEffects::playbackTime(std::string_view name)const{
    Scope scope(impl_->context);if(!scope.valid)return -1;const auto i=impl_->sounds.find(name);if(i==impl_->sounds.end())return -1;
    ALfloat offset=0;alGetSourcef(i->second.source,AL_SEC_OFFSET,&offset);return alGetError()==AL_NO_ERROR?offset:-1;
}
unsigned OpenALEffects::mixerRate()const{return impl_->rate;}
std::vector<EffectPlayback> OpenALEffects::playbackSnapshot()const{
    std::vector<EffectPlayback> result;result.reserve(impl_->sounds.size());
    Scope scope(impl_->context);if(!scope.valid)return result;
    for(const auto& [name,sound]:impl_->sounds){
        ALint state=0,offset=-1;alGetSourcei(sound.source,AL_SOURCE_STATE,&state);alGetSourcei(sound.source,AL_SAMPLE_OFFSET,&offset);
        const bool valid=alGetError()==AL_NO_ERROR;result.push_back({name,valid,valid&&state==AL_PLAYING,valid?offset:-1});
    }return result;
}
const std::string& OpenALEffects::error()const{return impl_->error;}
bool OpenALEffects::renderLoopback(std::span<std::int16_t> samples){
    if(!valid()||!impl_->renderSamples||samples.empty()||samples.size()%2||samples.size()/2>std::size_t(std::numeric_limits<ALCsizei>::max()))return false;
    impl_->renderSamples(impl_->device,samples.data(),ALCsizei(samples.size()/2));return alcGetError(impl_->device)==ALC_NO_ERROR;
}
}

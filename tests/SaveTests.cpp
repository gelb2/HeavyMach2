#include "Core/Data/SaveData.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include "Core/Audio/SoundIDs.hpp"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <iostream>
using namespace hm2;
int main(int argc,char** argv){
    SaveData save;save.bytes()[0x100]=123;save.initializeSlot(0,"Player","kr",1700000000);save.initializeSlot(2,"GimmeCr","us",1700000001);
    auto slot=save.slot(0);assert(slot.size()==9056&&slot[8]==1&&std::strcmp(reinterpret_cast<const char*>(slot.data()+0x2d),"Base001")==0);
    assert(slot[0x6c]==0&&slot[0x7c]==0&&slot[0x7d]==1&&slot[0x78]==0xf4&&slot[0x79]==1&&slot[0x88]==3);
    assert(slot[0x53c]==1&&slot[0x541]==5&&slot[0x542]==5&&slot[0x543]==5);
    assert(std::strcmp(reinterpret_cast<const char*>(slot.data()+0x63a),"04201")==0&&save.slot(1)[8]==0);
    for(std::size_t i=0x1000;i<0x1100;++i)slot[i]=std::uint8_t(i); // Unknown original bytes must survive.
    const auto original=save.bytes();DarwinWorldRandom random(7);auto encoded=save.encode(random,12345678);
    assert(encoded.size()==28204&&random.calls()==28197&&random.reseeds()==2);
    DarwinWorldRandom decode;auto restored=SaveData::decode(encoded,decode,false);
    assert(decode.calls()==28196&&decode.reseeds()==1);
    assert(std::equal(restored.bytes().begin()+4,restored.bytes().end()-4,original.begin()+4));
    // Matching final stream position matters to the following game tick.
    assert(decode.random()==random.random());
    auto reopened=SaveData::decode(encoded,decode);assert(reopened.bytes()[4]==1);
    const auto retained=save.bytes();auto broken=encoded;broken[100]^=1;bool fail=false;try{SaveData::decode(broken,decode);}catch(...){fail=true;}assert(fail&&save.bytes()==retained);
    const auto calls=decode.calls();fail=false;try{SaveData::decode(std::span(encoded).first(5),decode);}catch(...){fail=true;}assert(fail&&decode.calls()==calls);
    fail=false;try{save.initializeSlot(0,std::string(32,'x'),"kr",0);}catch(...){fail=true;}assert(fail&&save.bytes()==retained);
    assert(audio::soundFilename(7)=="Game_LevelUp.wav"&&audio::soundFilename(10)=="SFX_OnMessage.wav"&&audio::soundFilename(21)=="SFX_ExplMach.wav");
    // A round trip alone could hide two matching unsigned-checksum mistakes.
    SaveData signedBytes;std::fill(signedBytes.bytes().begin()+4,signedBytes.bytes().end()-4,0x80);
    auto signedFile=signedBytes.encode(random,1);const std::int32_t expected=-128*28196;
    for(unsigned i=0;i<4;++i)assert(signedFile[28200+i]==std::uint8_t(std::uint32_t(expected)>>(i*8)));
    auto negativeCounter=SaveData::decode(signedFile,decode);assert(negativeCounter.bytes()[4]==1&&negativeCounter.bytes()[5]==0);
    if(argc==2){
        std::ifstream file(argv[1],std::ios::binary);assert(file);const Bytes originalFile{std::istreambuf_iterator<char>(file),{}};
        auto actual=SaveData::decode(originalFile,decode,false);
        // Validate against the ORIGINAL port's actual saved bytes and the
        // host's independent Darwin libc random(), not our paired encoder.
        const auto seed=std::uint32_t(originalFile[0])|(std::uint32_t(originalFile[1])<<8)|(std::uint32_t(originalFile[2])<<16)|(std::uint32_t(originalFile[3])<<24);
        ::srandom(seed);Bytes native(actual.bytes().begin(),actual.bytes().end());
        for(std::size_t i=4;i<native.size()-4;++i)native[i]^=std::uint8_t(::random());
        assert(native==originalFile);unsigned active=0;for(unsigned i=0;i<3;++i)active+=actual.slot(i)[8]!=0;
        std::ifstream check(argv[1],std::ios::binary);const Bytes after{std::istreambuf_iterator<char>(check),{}};assert(after==originalFile);
        std::cout<<"HM2_REFERENCE_SAVE nativeDarwinByteMatch=28204 activeSlots="<<active<<" sourceFileUnmodified=true\n";
    }else assert(argc==1);
}

#include "Core/Session/GameSession.hpp"
#include "Core/Progression/Progression.hpp"
#include "Core/Data/Descriptions.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace hm2;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;explicit Files(const char* p):root(p){}
    Bytes read(std::string_view n)override{std::ifstream f(root/std::string(n),std::ios::binary);if(!f)throw std::runtime_error("Missing original entry asset");return {std::istreambuf_iterator<char>(f),{}};}
    bool exists(std::string_view n)const override{return std::filesystem::is_regular_file(root/std::string(n));}
};
struct Host:SessionHost {
    Bytes stored;std::vector<SaveReason> writes;unsigned names=0,deletes=0;bool fail=false;
    void sound(std::string_view,Vec2)override{}void soundID(unsigned,Vec2)override{}void alert(std::string_view)override{}
    void requestName(unsigned)override{++names;}void requestDelete(unsigned)override{++deletes;}
    bool writeSave(std::span<const std::uint8_t> b,SaveReason r)override{writes.push_back(r);if(fail)return false;stored.assign(b.begin(),b.end());return true;}
    std::uint32_t absoluteTimeLow()override{return 301;}std::uint32_t unixTime()override{return 1794268800;}
};
void slots(GameSession& game){game.action({ui::ScreenId::title,ui::Panel::root,{ui::ButtonEventType::pop,{},0,{}},true,true});assert(game.state().screen==ui::ScreenId::slots);}
std::uint32_t word(std::span<const std::uint8_t> b,unsigned p){return std::uint32_t(b[p])|(std::uint32_t(b[p+1])<<8)|(std::uint32_t(b[p+2])<<16)|(std::uint32_t(b[p+3])<<24);}
void press(GameSession& game,unsigned id,ui::ButtonEventType phase=ui::ButtonEventType::pop,int data=0){game.action({game.state().screen,ui::Panel::root,{phase,{0,id,data},1,{}}});}
void create(Files& files){
    Host host;DarwinWorldRandom rng(3);GameSession game(files,host,rng);slots(game);press(game,1009,ui::ButtonEventType::push,0);press(game,1001);
    assert(host.names==1&&host.writes.empty());assert(game.createSlot(0,"MACH","us"));
    assert(game.state().screen==ui::ScreenId::slots&&game.state().slots[0].used&&game.state().slots[0].name=="MACH");
    assert(word(game.workingSlot(),0x1b5c)==0);assert(host.writes.size()==1&&host.writes[0]==SaveReason::selectSlot);assert(game.state().equipment[0].dataId=="11001");
    DarwinWorldRandom check(8);const auto decoded=SaveData::decode(host.stored,check,false);assert(std::equal(decoded.slot(0).begin(),decoded.slot(0).end(),game.workingSlot().begin()));
    press(game,1001);assert(game.state().screen==ui::ScreenId::base&&host.writes.size()==3&&host.writes.back()==SaveReason::enterBase&&word(game.workingSlot(),0x1b5c)==1);
    const auto stored=host.stored;host.fail=true;assert(!game.createSlot(1,"FAILED","us"));assert(host.stored==stored);
    std::cout<<"ENTRY createName->initSlot->selectSlot/save->slots; explicitStart->select/save->base/save; failedWritePreservesDisk PASS\n";
}
void erase(Files& files){
    SaveData saved;saved.initializeSlot(0,"PRESERVE","us",1794268800);saved.initializeSlot(1,"OTHER","us",1794268800);
    auto original=saved.slot(0);original[0x700]=0xa9;original[0x2000]=0x6b;Bytes expected(original.begin(),original.end());expected[8]=0;const Bytes other(saved.slot(1).begin(),saved.slot(1).end());
    Host host;DarwinWorldRandom rng(4);GameSession game(files,host,rng,saved);slots(game);assert(game.state().selectedSlot==0);press(game,1010);
    assert(host.deletes==1&&host.writes.empty());assert(game.deleteSlot(0));
    assert(!game.state().slots[0].used&&std::equal(expected.begin(),expected.end(),game.envelope().slot(0).begin()));assert(host.writes.size()==1&&host.writes[0]==SaveReason::deleteSlot);
    DarwinWorldRandom check(9);const auto disk=SaveData::decode(host.stored,check,false);assert(std::equal(expected.begin(),expected.end(),disk.slot(0).begin()));assert(std::equal(other.begin(),other.end(),disk.slot(1).begin()));
    assert(!game.deleteSlot(3)&&host.writes.size()==1);
    std::cout<<"ENTRY confirmedDelete clears usedByte only; unknown/otherSlot preserved; immediate atomicSaveRequest PASS\n";
}
void resume(Files& files){
    SaveData saved;saved.initializeSlot(0,"FIELD","us",1794268800);progression::SlotView slot(saved.slot(0));slot.string(0x2d,16,"Stage01_02");slot.u32(0x68,0);slot.hp(64);slot.u32(0x1b5c,0xffffffffu);
    Host host;DarwinWorldRandom rng(5);GameSession game(files,host,rng,saved);slots(game);press(game,1001);
    assert(game.state().screen==ui::ScreenId::field&&game.battle()&&host.writes.size()==1&&host.writes[0]==SaveReason::selectSlot);assert(game.battle()->playerHP()==64&&word(game.workingSlot(),0x1b5c)==0);DarwinWorldRandom savedCheck(31);const auto disk=SaveData::decode(host.stored,savedCheck,false);assert(word(disk.slot(0),0x1b5c)==0xffffffffu);
    std::array<std::uint8_t,SaveData::slotSize> bytes{};std::copy(game.workingSlot().begin(),game.workingSlot().end(),bytes.begin());progression::SlotView actual(bytes);assert(actual.string(0x2d,16)=="Stage01_02");
    for(unsigned i=0;i<4;++i){game.tick();assert(!game.drawWorld().sprites.empty());}assert(game.battle()->metrics().enemySpawnsByKind.at(MachineKind::turret)>0);
    std::cout<<"ENTRY savedField->screen7/recreateOriginalMapGate; positiveHP64 preserved; noBaseHeal/Save PASS\n";
}
void descriptions(Files& files){
    const auto table=DataTable::parseText("Key\tValue\nA\tbefore|after\nB\t한글|다음 줄\nA\tlast||line\nEmpty\t\n");const auto text=Descriptions::parse(table);
    assert(text.lookup("A")=="last\n\nline"&&text.lookup("B")=="한글\n다음 줄"&&text.lookup("Empty")==""&&!text.lookup("missing"));
    const auto us=Descriptions::parse(DataTable::parse(files.read("Description_us.tble")));assert(us.lookup("IncorrectName")=="Enter your name in 10 or less letters.");
    const auto kr=Descriptions::parse(DataTable::parse(files.read("Description_kr.tble")));assert(kr.lookup("DeleteSlot")=="슬롯을 삭제하면 해당 슬롯의 모든\n게임데이터가 삭제됩니다.");
    std::cout<<"ENTRY originalDescriptionLoad pipeNewline/UTF8/lastKeyWins/noFallback/usKr PASS\n";
}
}
int main(int argc,char** argv){if(argc<3)return 77;Files files(argv[1]);const std::string mode=argv[2];if(mode=="create")create(files);else if(mode=="delete")erase(files);else if(mode=="resume")resume(files);else if(mode=="descriptions")descriptions(files);else return 2;}

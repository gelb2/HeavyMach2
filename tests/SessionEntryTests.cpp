#include "PreparedTextStub.hpp"
#include "Core/Session/GameSession.hpp"
#include "Core/Progression/Progression.hpp"
#include "Core/Data/Descriptions.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace hm2;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;unsigned reads=0;explicit Files(const char* p):root(p){}
    Bytes read(std::string_view n)override{++reads;std::ifstream f(root/std::string(n),std::ios::binary);if(!f)throw std::runtime_error("Missing original entry asset");return {std::istreambuf_iterator<char>(f),{}};}
    bool exists(std::string_view n)const override{return std::filesystem::is_regular_file(root/std::string(n));}
};
struct Host:SessionHost {
    ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& r)override{return test::preparedTextStub(r);}
    Bytes stored;std::vector<SaveReason> writes;unsigned names=0,deletes=0;bool fail=false;
    void sound(std::string_view,Vec2)override{}void soundID(unsigned,Vec2)override{}void alert(std::string_view)override{}
    void requestName(unsigned)override{++names;}void requestDelete(unsigned)override{++deletes;}
    bool writeSave(std::span<const std::uint8_t> b,SaveReason r)override{writes.push_back(r);if(fail)return false;stored.assign(b.begin(),b.end());return true;}
    std::uint32_t absoluteTimeLow()override{return 301;}std::uint32_t unixTime()override{return 1794268800;}
};
void settle(GameSession& game){unsigned ticks=0;while(game.screenTransition().phase!=8&&ticks++<200){game.tick();game.drawWorld();(void)game.state();}assert(game.screenTransition().phase==8);}
void slots(GameSession& game){game.action({ui::ScreenId::title,ui::Panel::root,{ui::ButtonEventType::pop,{},0,{}},true,true});settle(game);assert(game.state().screen==ui::ScreenId::slots);}
std::uint32_t word(std::span<const std::uint8_t> b,unsigned p){return std::uint32_t(b[p])|(std::uint32_t(b[p+1])<<8)|(std::uint32_t(b[p+2])<<16)|(std::uint32_t(b[p+3])<<24);}
void press(GameSession& game,unsigned id,ui::ButtonEventType phase=ui::ButtonEventType::pop,int data=0){game.action({game.state().screen,ui::Panel::root,{phase,{0,id,data},1,{}}});settle(game);}
void create(Files& files){
    Host host;DarwinWorldRandom rng(3);GameSession game(files,host,rng);slots(game);press(game,1009,ui::ButtonEventType::push,0);press(game,1001);
    assert(host.names==1&&host.writes.empty());assert(game.createSlot(0,"MACH","us"));
    assert(game.state().screen==ui::ScreenId::slots&&game.state().slots[0].used&&game.state().slots[0].name=="MACH");
    assert(word(game.workingSlot(),0x1b5c)==0);assert(host.writes.size()==1&&host.writes[0]==SaveReason::selectSlot);assert(game.state().equipment[0].dataId=="11001");
    DarwinWorldRandom check(8);const auto decoded=SaveData::decode(host.stored,check,false);assert(std::equal(decoded.slot(0).begin(),decoded.slot(0).end(),game.workingSlot().begin()));
    press(game,1001);assert(game.state().screen==ui::ScreenId::base&&host.writes.size()==3&&host.writes.back()==SaveReason::enterBase&&word(game.workingSlot(),0x1b5c)==2);
    assert(game.help()&&game.help()->state().page==0);
    assert(game.inputOverlay(PointerPhase::end,{384,512},1)&&!game.help());
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
    Host host;DarwinWorldRandom rng(5);GameSession game(files,host,rng,saved);slots(game);
    game.action({game.state().screen,ui::Panel::root,{ui::ButtonEventType::pop,{0,1001},1,{}}});
    assert(game.state().screen==ui::ScreenId::slots&&game.screenTransition().phase==0);
    unsigned closing=0;while(game.screenTransition().phase!=4&&closing++<100)game.tick();assert(game.screenTransition().phase==4);game.tick();
    // Observe the field immediately after synchronous replacement; later open
    // frames legitimately run enemies and can damage the resumed64HP actor.
    assert(game.state().screen==ui::ScreenId::field&&game.battle()&&host.writes.size()==1&&host.writes[0]==SaveReason::selectSlot);assert(game.battle()->playerHP()==64&&word(game.workingSlot(),0x1b5c)==5&&game.help()&&game.help()->state().layer==9&&!game.battle()->isPaused());DarwinWorldRandom savedCheck(31);const auto disk=SaveData::decode(host.stored,savedCheck,false);assert(word(disk.slot(0),0x1b5c)==0xffffffffu);
    std::array<std::uint8_t,SaveData::slotSize> bytes{};std::copy(game.workingSlot().begin(),game.workingSlot().end(),bytes.begin());progression::SlotView actual(bytes);assert(actual.string(0x2d,16)=="Stage01_02");
    settle(game);for(unsigned i=0;i<4;++i){game.tick();assert(!game.drawWorld().sprites.empty());}assert(game.battle()->metrics().enemySpawnsByKind.at(MachineKind::turret)>0);
    std::cout<<"ENTRY savedField->screen7/recreateOriginalMapGate; positiveHP64 preserved; noBaseHeal/Save PASS\n";
}
void descriptions(Files& files){
    const auto table=DataTable::parseText("Key\tValue\nA\tbefore|after\nB\t한글|다음 줄\nA\tlast||line\nEmpty\t\n");const auto text=Descriptions::parse(table);
    assert(text.lookup("A")=="last\n\nline"&&text.lookup("B")=="한글\n다음 줄"&&text.lookup("Empty")==""&&!text.lookup("missing"));
    const auto us=Descriptions::parse(DataTable::parse(files.read("Description_us.tble")));assert(us.lookup("IncorrectName")=="Enter your name in 10 or less letters.");
    const auto kr=Descriptions::parse(DataTable::parse(files.read("Description_kr.tble")));assert(kr.lookup("DeleteSlot")=="슬롯을 삭제하면 해당 슬롯의 모든\n게임데이터가 삭제됩니다.");
    std::cout<<"ENTRY originalDescriptionLoad pipeNewline/UTF8/lastKeyWins/noFallback/usKr PASS\n";
}
void slotMotion(Files& files){
    struct MenuRandom:WorldRandom {
        unsigned calls=0;
        std::uint32_t random()override{++calls;return 0x50000001;}
    } rng;
    struct MenuHost:ui::ScreensHost {
        unsigned prepared=0;
        void action(const ui::ScreenAction&)override{}
        ui::PreparedScreenText prepareText(const ui::TextRasterRequest& r)override{++prepared;return test::preparedTextStub(r);}
        void bgm(std::string_view)override{}
        void sound(std::string_view)override{}
        void sfx(unsigned)override{}
    } menuHost;
    SaveData saved;saved.initializeSlot(0,"FIRST","kr",1794268800);saved.initializeSlot(1,"SECOND","kr",1794268790);
    Host host;GameSession game(files,host,rng,saved);
    game.action({ui::ScreenId::title,ui::Panel::root,{},true,true});
    settle(game);
    assert(game.state().screen==ui::ScreenId::slots&&game.state().selectedSlot==0&&game.state().slots[2].machPreview.sprites.empty());
    const auto body=[](const Frame& frame){
        const auto found=std::find_if(frame.sprites.begin(),frame.sprites.end(),[](const Sprite& s){
            return s.textureSource.variant==TextureVariant::rgba&&s.image.starts_with("P_Body_");
        });
        assert(found!=frame.sprites.end());return found->center;
    };
    const auto first=body(game.state().slots[0].machPreview),second=body(game.state().slots[1].machPreview);
    PreparedTiles tiles("slot-motion-test");ui::Screens screens(files,tiles,menuHost);
    screens.refresh(game.state(),game.renderTime());
    const auto revision=game.presentationRevision();
    const auto reads=files.reads,preparations=menuHost.prepared,initialCalls=rng.calls;
    for(unsigned frame=0;frame<12;++frame){
        game.tick();
        assert(screens.updateHUD(game.state()));
    }
    const auto moving=body(game.state().slots[0].machPreview);
    assert((moving.x!=first.x||moving.y!=first.y)&&rng.calls==initialCalls+12);
    const auto stationary=body(game.state().slots[1].machPreview);
    assert(std::hypot(stationary.x-second.x,stationary.y-second.y)<.01f);
    Frame presented;screens.append(presented,game.renderTime());
    const auto drawn=body(presented);
    assert(drawn.x==moving.x+214&&drawn.y==moving.y+794);
    assert(game.presentationRevision()==revision&&files.reads==reads&&menuHost.prepared==preparations);
    game.action({ui::ScreenId::slots,ui::Panel::root,{ui::ButtonEventType::push,{0,1009,1},1,{}}});
    const auto calls=rng.calls;
    for(unsigned frame=0;frame<12;++frame)game.tick();
    const auto other=body(game.state().slots[1].machPreview);
    assert(game.state().selectedSlot==1&&(other.x!=second.x||other.y!=second.y)&&rng.calls==calls+13);
    assert(host.writes.empty()&&!game.battle());
    for(unsigned i=0;i<3;++i)assert(std::equal(saved.slot(i).begin(),saved.slot(i).end(),game.envelope().slot(i).begin()));
}
void locale(Files& files){
    SaveData saved;saved.initializeSlot(0,"KOREAN","us",1794268800);saved.slot(0)[0x2000]=0xa9;
    const auto kr=Descriptions::parse(DataTable::parse(files.read("Description_kr.tble")));
    const auto us=Descriptions::parse(DataTable::parse(files.read("Description_us.tble")));
    Host host;DarwinWorldRandom rng(17);GameSession game(files,host,rng,saved);
    assert(game.state().locale=="kr"&&game.originalText("DeleteSlot")==kr.lookup("DeleteSlot"));
    assert(game.selectSlot(0));game.enterBase();
    assert(game.state().locale=="kr"&&game.state().slots[0].country=="us");
    assert(game.workingSlot()[0x29]=='u'&&game.workingSlot()[0x2a]=='s'&&game.workingSlot()[0x2000]==0xa9);
    const auto mission=DataTable::parse(files.read("M001010_kr.tble"));std::string name;
    for(std::size_t i=0;i<mission.rowCount();++i)if(mission.value(i,"Category")=="Name")name=mission.value(i,"Value").value();
    const auto& available=game.state().availableMissions;const auto found=std::find_if(available.begin(),available.end(),[](const auto& m){return m.dataId=="M001010";});
    assert(!name.empty()&&found!=available.end()&&found->name==name);
    Host other;DarwinWorldRandom otherRng(18);GameSession english(files,other,otherRng,saved,"us");
    assert(english.state().locale=="us"&&english.originalText("DeleteSlot")==us.lookup("DeleteSlot")&&english.originalText("DeleteSlot")!=game.originalText("DeleteSlot"));
    bool rejected=false;try{GameSession unsupported(files,other,otherRng,saved,"zz");}catch(const std::runtime_error&){rejected=true;}assert(rejected);
    std::cout<<"ENTRY KoreanDefault/OriginalKRDescriptionAndMission/ExplicitUS/IndependentSavedCountryAndUnknownBytes PASS\n";
}
}
int main(int argc,char** argv){if(argc<3)return 77;Files files(argv[1]);const std::string mode=argv[2];if(mode=="create"){create(files);slotMotion(files);}else if(mode=="delete")erase(files);else if(mode=="resume")resume(files);else if(mode=="descriptions")descriptions(files);else if(mode=="locale")locale(files);else return 2;}

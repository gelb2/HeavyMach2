#include "PreparedTextStub.hpp"
#include "Core/Session/GameSession.hpp"
#include "Core/Progression/Progression.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include <algorithm>
#include <cassert>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace hm2;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;unsigned reads=0;mutable unsigned probes=0;
    explicit Files(const char* p):root(p){}
    Bytes read(std::string_view n)override{++reads;std::ifstream f(root/std::string(n),std::ios::binary);if(!f)throw std::runtime_error("Missing Help Session asset");return {std::istreambuf_iterator<char>(f),{}};}
    bool exists(std::string_view n)const override{++probes;return std::filesystem::is_regular_file(root/std::string(n));}
};
struct Host:SessionHost {
    ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& r)override{return test::preparedTextStub(r);}
    Bytes stored;std::vector<SaveReason> writes;std::vector<unsigned> sounds;GameSession* game=nullptr;std::vector<bool> stageSavePaused;
    void sound(std::string_view,Vec2)override{}void soundID(unsigned id,Vec2)override{sounds.push_back(id);}
    void alert(std::string_view)override{}void requestName(unsigned)override{}void requestDelete(unsigned)override{}
    bool writeSave(std::span<const std::uint8_t> b,SaveReason why)override{stored.assign(b.begin(),b.end());writes.push_back(why);if(why==SaveReason::stageTransition)stageSavePaused.push_back(game->battle()->isPaused());return true;}
    std::uint32_t absoluteTimeLow()override{return 301;}std::uint32_t unixTime()override{return 1794268800;}
};
SaveData fixture(std::uint32_t count){SaveData data;data.initializeSlot(0,"HELP","us",1794268800);progression::SlotView slot(data.slot(0));slot.u32(0x1b5c,count);return data;}
std::uint32_t word(std::span<const std::uint8_t> b,unsigned p){return std::uint32_t(b[p])|(std::uint32_t(b[p+1])<<8)|(std::uint32_t(b[p+2])<<16)|(std::uint32_t(b[p+3])<<24);}
Frame overlay(GameSession& game){Frame frame;game.appendOverlays(frame);return frame;}
void menu(GameSession& game){game.action({ui::ScreenId::field,ui::Panel::system,{ui::ButtonEventType::pop,{0,1029},1,{}}});}
void initial(Files& files){
    for(const auto count:{0u,4u,5u,0xffffffffu})for(bool save:{false,true}){
        Host host;DarwinWorldRandom rng(1);GameSession game(files,host,rng,fixture(count));host.game=&game;assert(game.selectSlot(0));
        // Preserve the original World.isSave field across a real stage entry.
        assert(game.enterField("Stage01_01",0));
        game.battle()->setIsSave(save);
        // Re-select a separate working save to isolate the signed boundary,
        // while keeping this existing World's explicit save policy.
        auto bytes=std::span(const_cast<std::uint8_t*>(game.workingSlot().data()),game.workingSlot().size());progression::SlotView slot(bytes);slot.u32(0x1b5c,count);
        if(game.help())assert(game.inputOverlay(PointerPhase::end,{700,0},1));
        const auto writes=host.writes.size();assert(game.enterField("Stage01_01",0));
        const bool expected=std::bit_cast<std::int32_t>(count)<5;
        assert(bool(game.help())==expected&&word(game.workingSlot(),0x1b5c)==(expected?count+5u:count));
        assert(!game.battle()->isPaused()&&host.writes.size()==writes+unsigned(save));
        if(save){assert(host.writes.back()==SaveReason::stageTransition&&host.stageSavePaused.back()==expected);DarwinWorldRandom decode(8);const auto saved=SaveData::decode(host.stored,decode,false);assert(word(saved.slot(0),0x1b5c)==word(game.workingSlot(),0x1b5c));}
        if(expected){assert(game.help()->state().layer==9&&game.help()->state().page==2&&!game.help()->state().changePages&&!game.help()->state().guidanceVisible);const auto frame=overlay(game);assert(frame.sprites.size()==1&&frame.sprites[0].image=="HelpScr03_kr.png");assert(game.preparationFrame().sprites.size()>3);assert(game.inputOverlay(PointerPhase::end,{700,0},1)&&!game.help());}
        else assert(overlay(game).sprites.empty());
    }
    std::cout<<"HELP_SESSION signed0/4/5/minus1 fixedKRpage2/layer9/wrappedPlus5/savePolicy/postSetStageUnpause PASS\n";
}
void navigation(Files& files){
    Host host;DarwinWorldRandom rng(2);GameSession game(files,host,rng,fixture(5));assert(game.selectSlot(0)&&game.enterField("Stage01_01",0)&&!game.help());
    menu(game);assert(game.help()&&game.battle()->isPaused()&&game.help()->state().page==2&&game.help()->state().guidanceVisible&&game.help()->state().layer==13);
    assert(overlay(game).sprites.size()==2&&overlay(game).sprites[0].image=="HelpScr03_kr.png"&&overlay(game).sprites[1].image=="HelpHelp.png");
    const auto prepared=game.preparationFrame();assert(std::count_if(prepared.sprites.begin(),prepared.sprites.end(),[](const auto& s){return s.image.starts_with("HelpScr");})==3);
    const auto reads=files.reads,probes=files.probes;const auto writes=host.writes.size();const auto time=game.battle()->clock().worldTime();const auto position=game.battle()->player().position;const auto paths=game.battle()->dashState().pathCount;
    for(auto phase:{PointerPhase::begin,PointerPhase::move,PointerPhase::cancel})assert(game.inputOverlay(phase,{700,0},1));
    for(unsigned i=0;i<120;++i){game.tick();game.drawWorld();(void)overlay(game);}
    assert(game.battle()->clock().worldTime()==time&&game.battle()->player().position.x==position.x&&game.battle()->player().position.y==position.y&&game.battle()->dashState().pathCount==paths);
    assert(game.inputOverlay(PointerPhase::end,{384,512},1)&&game.help()&&!game.help()->state().guidanceVisible);
    for(unsigned i=0;i<5;++i)assert(game.inputOverlay(PointerPhase::end,{203,0},1));assert(game.help()->state().page==0);
    for(unsigned i=0;i<5;++i)assert(game.inputOverlay(PointerPhase::end,{565,0},1));assert(game.help()->state().page==2);
    assert(game.inputOverlay(PointerPhase::end,{564,0},1)&&!game.help()&&!game.battle()->isPaused()&&overlay(game).sprites.empty());
    assert(!game.inputOverlay(PointerPhase::begin,{700,0},1));game.tick();assert(game.battle()->clock().worldTime()>time);
    assert(files.reads==reads&&files.probes==probes&&host.writes.size()==writes&&std::count(host.sounds.begin(),host.sounds.end(),0)==1);
    menu(game);assert(game.help());game.setActive(false);assert(!game.inputOverlay(PointerPhase::end,{384,0},1)&&game.help()->state().guidanceVisible);game.setActive(true);
    assert(game.inputOverlay(PointerPhase::end,{384,0},1));assert(game.inputOverlay(PointerPhase::end,{204,0},1)&&!game.help());
    std::cout<<"HELP_SESSION Pop1029/KRpage2+HelpHelp/allEventsConsumed/firstEndOnlyGuidance/204+564/actualWorldPauseResume/warmIO0/save0/SFX0once/inactive PASS\n";
}
void firstMenuHelp(Files& files){
    for(const unsigned facility:{2002u,2003u}){
        Host host;DarwinWorldRandom rng(2);GameSession game(files,host,rng,fixture(1));
        host.game=&game;
        assert(game.selectSlot(0));game.enterBase();
        assert(game.help()&&game.help()->state().page==0&&game.help()->state().layer==9);
        auto frame=overlay(game);
        assert(frame.sprites.size()==1&&frame.sprites[0].image=="HelpScr01_kr.png");
        assert(frame.sprites[0].center.x==384&&frame.sprites[0].center.y==512);
        assert(word(game.workingSlot(),0x1b5c)==2);
        DarwinWorldRandom decoder(8);
        assert(word(SaveData::decode(host.stored,decoder,false).slot(0),0x1b5c)==2);
        assert(game.inputOverlay(PointerPhase::begin,{384,512},4)&&game.help());
        assert(game.inputOverlay(PointerPhase::end,{384,512},4)&&!game.help());
        const auto saves=host.writes.size();
        const auto action=[&](unsigned id){game.action({game.state().screen,ui::Panel::root,{ui::ButtonEventType::pop,{0,id},1,{}}});};
        const auto settle=[&]{unsigned ticks=0;while(game.screenTransition().phase!=8){game.tick();assert(++ticks<300);}};
        action(facility);action(facility);settle();
        assert(game.state().screen==(facility==2002?ui::ScreenId::shop:ui::ScreenId::maintenance));
        assert(game.help()&&game.help()->state().page==1&&!game.help()->state().changePages);
        frame=overlay(game);
        assert(frame.sprites.size()==1&&frame.sprites[0].image=="HelpScr02_kr.png");
        assert(frame.sprites[0].center.x==(facility==2002?384:528)&&frame.sprites[0].center.y==(facility==2002?512:864));
        assert(game.inputOverlay(PointerPhase::end,{100,100},4)&&!game.help());
        assert(word(game.workingSlot(),0x1b5c)==3&&host.writes.size()==saves);
        action(2100);settle();
        action(facility);action(facility);settle();
        assert(!game.help()&&word(game.workingSlot(),0x1b5c)==3&&host.writes.size()==saves+1);
    }
}
}
int main(int argc,char** argv){if(argc<2)return 77;Files files(argv[1]);firstMenuHelp(files);initial(files);navigation(files);}

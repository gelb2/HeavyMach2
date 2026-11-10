#include "PreparedTextStub.hpp"
#include "Core/Session/GameSession.hpp"
#include "Core/Progression/Progression.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
using namespace hm2;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;
    explicit Files(const char* p):root(p){}
    Bytes read(std::string_view name)override{std::ifstream f(root/std::string(name),std::ios::binary);if(!f)throw std::runtime_error("Missing GameOver test asset");return {std::istreambuf_iterator<char>(f),{}};}
    bool exists(std::string_view name)const override{return std::filesystem::is_regular_file(root/std::string(name));}
};
struct Host:SessionHost {
    Bytes stored;std::vector<SaveReason> writes;std::vector<std::uint64_t> requests;std::vector<std::string> alerts,events;std::vector<std::pair<std::string,std::string>> notices;
    std::function<void(std::uint64_t)> onRequest;std::function<void(SaveReason)> onSave;bool fail=false;
    void sound(std::string_view,Vec2)override{}void soundID(unsigned id,Vec2)override{events.push_back("sfx"+std::to_string(id));}
    void alert(std::string_view key)override{alerts.emplace_back(key);}
    void notice(std::string_view title,std::string_view message)override{notices.emplace_back(title,message);events.emplace_back("notice");}
    void requestName(unsigned)override{}void requestDelete(unsigned)override{}
    void requestGameOver(std::uint64_t token)override{requests.push_back(token);if(onRequest)onRequest(token);}
    bool writeSave(std::span<const std::uint8_t> data,SaveReason why)override{writes.push_back(why);if(onSave)onSave(why);if(fail)return false;stored.assign(data.begin(),data.end());return true;}
    std::uint32_t absoluteTimeLow()override{return 1;}std::uint32_t unixTime()override{return 1794268800;}
    ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& r)override{return test::preparedTextStub(r);}
};
progression::SlotView slot(GameSession& game){return progression::SlotView(std::span(const_cast<std::uint8_t*>(game.workingSlot().data()),game.workingSlot().size()));}
SaveData fixture(std::string_view base="Base002",bool bonus=false){
    SaveData data;data.initializeSlot(0,"DEATH","kr",1794268800);progression::SlotView s(data.slot(0));
    s.hp(1);s.u32(0x1b5c,5);s.string(0x55,16,base);s.u32(0x64c,50);s.u32(0x650,31);s.u32(0x654,17);
    if(bonus)s.string(0x8c,6,"11002"); // Same equipped inventory index0; original +20HP body.
    return data;
}
unsigned reachActualDeath(GameSession& game){
    auto* scene=game.battle();assert(scene&&scene->playerHP()>0&&scene->playerHP()<slot(game).maxHP());bool selected=false;unsigned frames=0;
    while(scene->playerHP()>0&&frames<3600){
        game.tick();game.drawWorld();++frames;
        if(!selected)for(const auto& enemy:scene->enemies()){
            const auto p=scene->camera().toScreen(enemy.position);
            if(enemy.hp>0&&p.x>0&&p.x<768&&p.y>100&&p.y<1024){selected=scene->selectEnemy(enemy.id);break;}
        }
    }
    assert(selected&&scene->playerHP()==0&&slot(game).hp()==0&&scene->metrics().enemyEmissions>0&&scene->metrics().damageCalls>0);
    return frames;
}
void reachAlert(GameSession& game){for(unsigned i=0;!game.pendingGameOver()&&i<3;++i){game.tick();game.drawWorld();}assert(game.pendingGameOver()&&game.battle()->player().state==3&&game.battle()->gameState()==3);}
void returnAndSave(Files& files,bool bonus){
    Host host;DarwinWorldRandom random(1);GameSession game(files,host,random,fixture("Base002",bonus));assert(game.selectSlot(0)&&game.enterField("Stage01_01",0));
    auto* scene=game.battle();const auto previousSave=host.stored;const auto saves=host.writes.size();const auto expectedHP=bonus?276:256;
    assert(scene->gameStateChangeTime()==scene->clock().renderTime());
    host.onRequest=[&](std::uint64_t token){assert(token&&game.pendingGameOver()==token&&game.battle()==scene&&scene->gameState()==3&&scene->player().state==3&&!scene->isPaused());assert(scene->gameStateChangeTime()==scene->clock().renderTime()&&slot(game).hp()==0&&host.writes.size()==saves&&host.stored==previousSave);};
    const auto deathFrame=reachActualDeath(game);
    if(!bonus){
        assert(scene->player().state==2&&scene->gameState()==0&&host.requests.empty());const auto time=scene->clock().worldTime();
        scene->setPaused(true);for(unsigned i=0;i<12;++i){game.tick();game.drawWorld();assert(scene->player().state==2&&scene->gameState()==0&&!game.pendingGameOver());}
        assert(scene->clock().worldTime()==time);scene->setPaused(false);
    }
    reachAlert(game);assert(host.requests.size()==1);const auto token=game.pendingGameOver();const auto changed=scene->gameStateChangeTime();const auto world=scene->clock().worldTime();
    for(unsigned i=0;i<12;++i){game.tick();game.drawWorld();}
    assert(host.requests.size()==1&&scene->gameStateChangeTime()==changed&&scene->clock().worldTime()>world&&!scene->isPaused());
    assert(!game.confirmGameOver(0)&&!game.confirmGameOver(token+100)&&game.pendingGameOver()==token&&slot(game).string(0x2d,16)=="Stage01_01");
    const auto credit=slot(game).credit();assert(game.confirmGameOver(token));
    assert(!game.pendingGameOver()&&!game.confirmGameOver(token)&&game.battle()==scene&&game.screenTransition().phase==0&&slot(game).hp()==0&&slot(game).string(0x2d,16)=="Base002"&&slot(game).credit()==credit&&host.writes.size()==saves);
    bool savedAtPhaseFour=false;
    host.onSave=[&](SaveReason reason){assert(reason==SaveReason::enterBase&&game.screenTransition().phase==4&&!game.battle()&&game.state().screen==ui::ScreenId::base);assert(slot(game).hp()==expectedHP&&slot(game).string(0x55,16)=="Base002");for(unsigned i=0;i<3;++i)assert(slot(game).u32(0x64c+4*i)==0);savedAtPhaseFour=true;};
    unsigned transition=0;while(game.screenTransition().phase!=8){game.tick();game.drawWorld();assert(++transition<300);}
    assert(savedAtPhaseFour&&host.writes.size()==saves+1&&game.state().screen==ui::ScreenId::base&&!game.battle()&&slot(game).hp()==expectedHP&&!game.confirmGameOver(token));
    DarwinWorldRandom decode(1);auto saved=SaveData::decode(host.stored,decode,false);progression::SlotView stored(saved.slot(0));assert(stored.hp()==expectedHP&&stored.string(0x2d,16)=="Base002");
    Host restartedHost;DarwinWorldRandom restartedRandom(1);GameSession restarted(files,restartedHost,restartedRandom,saved);assert(restarted.selectSlot(0));assert(slot(restarted).hp()==expectedHP&&slot(restarted).string(0x55,16)=="Base002"&&!restarted.pendingGameOver());
    std::cout<<"GAMEOVER realEnemyDamage/deathFrame="<<deathFrame<<" pauseGate/oneAlert/world3NotPause/token/doorsPhase4/Base002/HP="<<expectedHP<<"/BOT3reset/oneSave/restart PASS\n";
}
void staleAndFailure(Files& files){
    Host host;DarwinWorldRandom rng(1);GameSession game(files,host,rng,fixture());assert(game.selectSlot(0)&&game.enterField("Stage01_01",0));reachActualDeath(game);reachAlert(game);const auto old=game.pendingGameOver();
    assert(game.selectSlot(0)&&!game.pendingGameOver()&&!game.confirmGameOver(old));assert(game.enterField("Stage01_01",0));reachActualDeath(game);reachAlert(game);const auto current=game.pendingGameOver();assert(current!=old&&!game.confirmGameOver(old));
    Host otherHost;DarwinWorldRandom otherRng(1);GameSession other(files,otherHost,otherRng,fixture());assert(other.selectSlot(0)&&other.enterField("Stage01_01",0));reachActualDeath(other);reachAlert(other);assert(other.pendingGameOver()!=current&&!other.confirmGameOver(current));
    const auto disk=host.stored;const auto saves=host.writes.size();const auto completions=game.metrics().saveCompletions;host.fail=true;assert(game.confirmGameOver(current));
    unsigned ticks=0;while(game.screenTransition().phase!=8){game.tick();game.drawWorld();assert(++ticks<300);}
    assert(host.writes.size()==saves+1&&host.stored==disk&&game.metrics().saveCompletions==completions&&game.state().screen==ui::ScreenId::base&&slot(game).hp()==256);
    for(unsigned i=0;i<30;++i)game.tick();assert(host.writes.size()==saves+1&&!game.confirmGameOver(current));
    std::cout<<"GAMEOVER staleSlot/staleSession/uniqueToken/failedAtomicSavePreserved/noRetryNoExtraSave PASS\n";
}
void doorEntryClock(Files& files){
    auto initial=fixture();progression::SlotView(initial.slot(0)).string(0x2d,16,"Stage01_01");
    Host host;DarwinWorldRandom random(1);GameSession game(files,host,random,initial);
    game.action({ui::ScreenId::title,ui::Panel::root,{},true,true});unsigned ticks=0;
    while(game.screenTransition().phase!=8){game.tick();game.drawWorld();assert(++ticks<300);}assert(game.state().screen==ui::ScreenId::slots);
    game.action({ui::ScreenId::slots,ui::Panel::root,{ui::ButtonEventType::pop,{0,1001},1,{}}});ticks=0;
    while(game.screenTransition().phase!=8){game.tick();game.drawWorld();assert(++ticks<300);}assert(game.battle()&&game.state().screen==ui::ScreenId::field);
    // Existing scene stepping on the creation frame is intentionally left
    // unchanged. State-change timestamps must still use GameMain's clock.
    assert(game.battle()->clock().renderTime()!=game.renderTime());
    host.onRequest=[&](std::uint64_t){assert(game.battle()->gameStateChangeTime()==game.renderTime());assert(game.battle()->gameStateChangeTime()!=game.battle()->clock().renderTime());};
    reachActualDeath(game);reachAlert(game);assert(host.requests.size()==1);
    std::cout<<"GAMEOVER actualDoorEntry/stateChangeUsesGlobalClockNotAheadSceneClock PASS\n";
}
void basePrefixFallback(Files& files){
    for(const auto* lastBase:{"Stage01_09","Base004"}){
        Host host;DarwinWorldRandom random(1);GameSession game(files,host,random,fixture(lastBase));assert(game.selectSlot(0)&&game.enterField("Stage01_01",0));
        reachActualDeath(game);reachAlert(game);const auto saves=host.writes.size();assert(game.confirmGameOver(game.pendingGameOver()));
        // The original callback copies lastBase verbatim. Prefix fallback is
        // deferred to actual Base creation, not performed in the alert handler.
        assert(slot(game).string(0x2d,16)==lastBase&&slot(game).hp()==0&&host.writes.size()==saves);
        unsigned ticks=0;while(game.screenTransition().phase!=8){game.tick();game.drawWorld();assert(++ticks<300);}
        const std::string expected=std::string(lastBase)=="Stage01_09"?"Base001":"Base004";
        assert(slot(game).string(0x2d,16)==expected&&slot(game).string(0x55,16)==expected&&slot(game).hp()==256&&host.writes.size()==saves+1);
        if(expected=="Base004")assert(slot(game).string(0x3d,24).empty()); // No invented map/description.
    }
    std::cout<<"GAMEOVER nonBaseFallback/BasePrefixUnknownPreserved/deferredBaseCreation PASS\n";
}
void rankingUnavailable(Files& files){
    for(const auto* locale:{"kr","us"}){
        Host host;DarwinWorldRandom random(1);GameSession game(files,host,random,fixture(),locale);assert(game.selectSlot(0));game.enterBase();
        for(unsigned i=0;i<2;++i)game.action({ui::ScreenId::base,ui::Panel::root,{ui::ButtonEventType::pop,{0,2001},1,{}}});
        unsigned ticks=0;while(game.screenTransition().phase!=8){game.tick();game.drawWorld();assert(++ticks<300);}assert(game.state().screen==ui::ScreenId::command);
        const Bytes working(game.workingSlot().begin(),game.workingSlot().end());const auto disk=host.stored;const auto saves=host.writes.size();const auto unsupported=game.metrics().unsupportedActions;
        for(const auto phase:{ui::ButtonEventType::push,ui::ButtonEventType::cancel})game.action({ui::ScreenId::command,ui::Panel::root,{phase,{0,5009},1,{}}});assert(host.notices.empty());
        host.events.clear();game.action({ui::ScreenId::command,ui::Panel::root,{ui::ButtonEventType::pop,{0,5009},1,{}}});
        assert(host.notices.size()==1&&host.events==std::vector<std::string>({"notice","sfx0"}));
        assert(host.notices[0].first==(std::string(locale)=="kr"?"랭킹":"Ranking"));assert(host.notices[0].second==(std::string(locale)=="kr"?"현재 랭킹 서비스를 이용할 수 없습니다.":"The ranking service is currently unavailable."));
        assert(game.state().screen==ui::ScreenId::command&&std::equal(working.begin(),working.end(),game.workingSlot().begin())&&host.stored==disk&&host.writes.size()==saves&&game.metrics().unsupportedActions==unsupported);
    }
    std::cout<<"RANKING5009 PopOnly/localizedNoticeThenSFX0/noSlotChangeNoSaveNoUnsupported PASS\n";
}
}
int main(int argc,char** argv){if(argc<2)return 77;Files files(argv[1]);returnAndSave(files,false);returnAndSave(files,true);staleAndFailure(files);rankingUnavailable(files);doorEntryClock(files);basePrefixFallback(files);}

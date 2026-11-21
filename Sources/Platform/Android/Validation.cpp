#include "Validation.hpp"
#include "Storage.hpp"
#include "Core/Audio/SoundIDs.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include <cmath>
#include "Core/Presentation/MenuArtwork.hpp"
#include "Core/Validation/HUDLayoutExercise.hpp"
#include "Core/Validation/SessionFixtures.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

namespace hm2::android {
namespace {
void publishProgress(const std::filesystem::path& path, const std::string& json) {
    const auto* bytes=reinterpret_cast<const std::uint8_t*>(json.data());
    if (!writeFile(path,{bytes,json.size()})) throw std::runtime_error("Cannot write validation progress");
}
}

SaveData playableFixture() {
    SaveData data;
    data.initializeSlot(0,"SnowPilot","kr",1794268800);
    progression::SlotView slot(data.slot(0));
    slot.level(40);
    slot.experience(progression::Experience::bounds(slot.level()).previous);
    slot.credit(20000000);
    slot.hp(3236);
    slot.maxHP(3236);
    slot.string(0x2d,16,"Base004");
    slot.string(0x55,16,"Base004");
    slot.u32(0x1b5c,5);
    for(unsigned i : {6u,7u}) {
        slot.string(0x8c+6*i,6,"");
        slot.byte(0x53c+i,0);
    }
    for(unsigned i=0;i<6;++i) slot.u32(0x1b60+4*i,1);
    const auto definitions = std::array{"02004","02005","02006","02007"};
    for (unsigned i=0;i<definitions.size();++i) {
        slot.string(0x8c+6*(8+i),6,definitions[i]);
        slot.byte(0x53c+8+i,20);
        slot.string(0x61c+6*i,6,definitions[i]);
    }
    return data;
}

struct Validation::Impl final : validation::ValidationHost {
    struct Case final : ui::ScreensHost {
        Impl& host;
        unsigned index;
        PreparedTiles tiles;
        std::unique_ptr<ui::Screens> screens;
        std::uint64_t revision=0, epoch=0, generation=0, tileRevision=0;
        std::map<std::pair<unsigned,int>,unsigned> events;
        std::set<std::string,std::less<>> savedHUDProfiles;
        Case(Impl& owner,unsigned i):host(owner),index(i),tiles("androidCase"+std::to_string(i)){}
        void action(const ui::ScreenAction& a) override {
            ++events[{a.button.key.id,a.button.key.intData}];
            host.runner->session(index).action(a);
            host.refresh(index);
        }
        ui::PreparedScreenText prepareText(const ui::TextRasterRequest& r) override {return host.prepareWorldText(index,r);}
        void bgm(std::string_view file) override {
            if (host.ready && index == host.visibleCase) host.services.playMusic(file);
        }
        void sound(std::string_view file) override {host.sound(index,file,{});}
        void sfx(unsigned id) override {host.soundID(index,id,{});}
        void prepareSound(std::string_view file) override {host.services.prepareSound(file);}
        std::optional<Bytes> loadHUDLayout(std::string_view profile) override {
            if (!savedHUDProfiles.contains(profile)) return {};
            return readFile(hudPath(host.caseDirectory(index),profile),65536);
        }
        bool saveHUDLayout(std::string_view profile,std::span<const std::uint8_t> bytes) override {
            if (bytes.empty() || bytes.size()>65536 || !writeFile(hudPath(host.caseDirectory(index),profile),bytes)) return false;
            savedHUDProfiles.emplace(profile);
            return true;
        }
    };

    RuntimeServices& services;
    const std::filesystem::path directory;
    DisplayConfiguration display;
    std::vector<std::unique_ptr<Case>> cases;
    ui::TextPreparationCache text;
    std::map<const ui::TextBitmap*,std::string> names;
    std::map<std::string,const ui::TextBitmap*> bitmaps;
    std::unique_ptr<validation::ConcurrentValidation> runner;
    Frame rendered;
    std::uint64_t frames=0, nextText=0;
    unsigned visibleCase=0;
    bool ready=false,active=true;

    Impl(RuntimeServices& s,std::filesystem::path path,DisplayConfiguration d)
        : services(s),directory(std::move(path)),display(d) {
        runner=std::make_unique<validation::ConcurrentValidation>(s.resources,*this);
        runner->setDisplay(display);
        for(unsigned i=0;i<runner->scenarioCount();++i) cases.push_back(std::make_unique<Case>(*this,i));
        for(unsigned i=0;i<cases.size();++i) {
            for(const auto& name:runner->session(i).orderedStartupSounds()) services.prepareSound(name);
            refresh(i);
        }
        ready=true;
        services.playMusic("STAGE3.mp3");
    }

    std::filesystem::path caseDirectory(unsigned i)const {return directory/std::to_string(i);}
    void refresh(unsigned i) {
        auto& view=*cases.at(i);
        auto& game=runner->session(i);
        if(!view.screens) {
            view.screens=std::make_unique<ui::Screens>(runner->scenarioResources(i),view.tiles,view);
            view.screens->setDisplay(display);
            ++view.epoch;
        }
        if(view.revision!=game.presentationRevision()) {
            view.screens->refresh(game.state(),game.renderTime());
            view.revision=game.presentationRevision();
        } else view.screens->updateHUD(game.state());
    }

    ui::PreparedScreenText prepareWorldText(unsigned,const ui::TextRasterRequest& r) override {
        auto bitmap=text.prepare(services.rasterizer,r);
        auto found=names.find(bitmap.get());
        if(found==names.end()) {
            auto id="@text/validation/"+std::to_string(nextText++);
            bitmaps.emplace(id,bitmap.get());
            found=names.emplace(bitmap.get(),std::move(id)).first;
        }
        return {found->second,bitmap};
    }

    // Only the visible full-screen case owns the output device. The common
    // runner still observes every game's original sound requests independently.
    void sound(unsigned i,std::string_view file,Vec2 p) override {
        if(ready && active && i==visibleCase && !file.empty()) services.playSound(file,p);
    }
    void soundID(unsigned i,unsigned id,Vec2 p) override {sound(i,audio::soundFilename(id),p);}
    bool writeSave(unsigned i,std::span<const std::uint8_t> bytes,SaveReason) override {
        return writeFile(caseDirectory(i)/"slot.sav",bytes);
    }
    std::uint32_t absoluteTimeLow(unsigned i) override {return 97+i;}
    std::uint32_t unixTime() override {return 1794268800;}

    void tickScenarioUI(unsigned i,GameSession& game,WorldRandom& random) override {
        refresh(i);
        cases.at(i)->screens->tick(game.renderTime(),random);
    }
    void appendScenarioUI(unsigned i,GameSession& game,Frame& frame) override {
        refresh(i);
        cases.at(i)->screens->append(frame,game.renderTime());
    }
    void sessionRestarted(unsigned i,GameSession&) override {
        auto& c=*cases.at(i);
        c.screens.reset();
        c.revision=0;
    }
    std::uint64_t scenarioUIResourceRevision(unsigned i) override {
        if(i>=cases.size()) return 0;
        auto& c=*cases[i];
        if(c.screens && (c.generation!=c.screens->resourceGeneration() || c.tileRevision!=c.screens->preparedResourceRevision())) {
            c.generation=c.screens->resourceGeneration();
            c.tileRevision=c.screens->preparedResourceRevision();
            ++c.epoch;
        }
        return c.epoch;
    }
    void exerciseHUDLayout(unsigned i,GameSession& game,validation::HUDLayoutObservation& o,std::uint64_t f) override {
        if(f==340) sessionRestarted(i,game);
        refresh(i);
        validation::exerciseHUDLayoutFrame(*cases[i]->screens,game,o,f);
    }
    bool pressButton(unsigned i,unsigned id,int data) override {
        refresh(i);
        auto& c=*cases.at(i);
        auto& game=runner->session(i);
        const auto& state=game.state();
        if(menu::phone(game.display())&&(state.screen==ui::ScreenId::shop||state.screen==ui::ScreenId::maintenance)) {
            const bool stock=id>=2200&&id<2224,inventory=id>=6000&&id<6025;
            if(stock||inventory) {
                const auto& items=stock?state.shopItems:state.inventory;
                const auto index=std::size_t(std::max(0,stock?state.shopPage:state.inventoryPage))*(stock?24:25)+id-(stock?2200:6000);
                const auto panel=stock?ui::Panel::root:ui::Panel::inventory;
                c.screens->revealButton({(ui::Owner(state.screen)<<16)|ui::Owner(panel),id,data,index<items.size()?items[index].dataId:""});
            }
        }
        for(const auto& key:c.screens->buttons()) {
            if(key.id!=id||key.intData!=data) continue;
            c.screens->revealButton(key);
            auto button=c.screens->button(key);
            if(!button||!button->active||!button->visible) continue;
            const auto point=c.screens->buttonScreenPosition(key);
            const auto before=c.events[{id,data}];
            for(auto phase:{PointerPhase::begin,PointerPhase::end})
                if(!game.inputOverlay(phase,point,10000+i)) c.screens->input(phase,point,10000+i,game.renderTime());
            return c.events[{id,data}]>before;
        }
        return false;
    }
    void uploadVisible() {
        for(const auto& sprite:rendered.sprites) {
            auto it=bitmaps.find(sprite.image);
            if(it!=bitmaps.end()) services.uploadText(sprite.image,*it->second);
        }
        services.prepareFrame(rendered);
        services.retainTextures(rendered);
    }
    void step() {
        if (!active || frames>=validation::displayValidationFrames(display)) return;
        runner->tick();
        ++frames;
        // Advance all original sessions and gather all source frames exactly
        // once. Present a rotating case at full size on the phone.
        runner->drawFrame(display.canvas);
        visibleCase=unsigned((frames/120)%cases.size());
        rendered=runner->scenarioFrame(visibleCase);
        uploadVisible();
        if(frames%60==0) {
            for(const auto* expired:text.discardUnused()) {
                auto it=names.find(expired);
                if(it!=names.end()) {bitmaps.erase(it->second);names.erase(it);}
            }
        }
    }
};
Validation::Validation(RuntimeServices& s,std::filesystem::path d,DisplayConfiguration c)
    :impl_(std::make_unique<Impl>(s,std::move(d),c)) {publishProgress(impl_->directory/"report.json",status());}
Validation::~Validation()=default;
void Validation::setActive(bool active){impl_->active=active;impl_->runner->setActive(active);}
void Validation::setDisplay(DisplayConfiguration d) {
    impl_->display=d;
    impl_->runner->setDisplay(d);
    for(auto& c:impl_->cases) if(c->screens) c->screens->setDisplay(d);
}
void Validation::tick(){
    const auto before=impl_->frames;
    impl_->step();
    if(impl_->frames!=before && (impl_->frames%60==0 || impl_->frames==validation::displayValidationFrames(impl_->display)))
        publishProgress(impl_->directory/"report.json",status());
}
void Validation::draw(){if(impl_->frames)impl_->services.drawFrame(impl_->rendered);}
void Validation::restoreGraphics(){if(impl_->frames)impl_->uploadVisible();}
std::string Validation::status()const {
    const auto& p=*impl_;
    const auto m=p.runner->metrics();
    const auto target=validation::displayValidationFrames(p.display);
    std::uint64_t damage=0,kills=0,buttonFailures=0,dashes=0,saves=0,minTicks=p.frames;
    for(const auto& o:p.runner->observations()) {
        damage+=o.damage;kills+=o.kills;buttonFailures+=o.buttonFailures;dashes+=o.dashStarts;saves+=o.saves;
        minTicks=std::min(minTicks,o.ticks);
    }
    const bool done=p.frames==target;
    const bool pass=done && p.cases.size()==validation::concurrentScenarioCount && minTicks==target &&
        m.commonFrameInterval && m.activeWeaponDefinitions==110 && m.activeWeaponKinds==5 &&
        m.maxConcurrentWeaponKinds>=5 && !buttonFailures && damage && kills && dashes && saves;
    std::ostringstream out;
    out<<"{\"mode\":\"concurrent\",\"frames\":"<<p.frames<<",\"target\":"<<target
       <<",\"scenarios\":"<<p.cases.size()<<",\"minimumTicks\":"<<minTicks
       <<",\"activeWeapons\":"<<m.activeWeaponDefinitions<<",\"weaponKinds\":"<<m.activeWeaponKinds
       <<",\"damage\":"<<damage<<",\"kills\":"<<kills<<",\"dashes\":"<<dashes
       <<",\"saves\":"<<saves<<",\"buttonFailures\":"<<buttonFailures
       <<",\"failedPresses\":[";
    bool separator=false;
    for(const auto& frame:p.runner->frameActivity()) {
        for(const auto& button:frame.buttons) {
            if(button.delivered) continue;
            if(separator) out<<',';
            separator=true;
            out<<"["<<frame.frame<<","<<button.scenario<<","<<button.id<<","<<button.intData<<"]";
        }
    }
    out<<"]"<<",\"done\":"<<(done?"true":"false")<<",\"pass\":"<<(pass?"true":"false")<<"}";
    return out.str();
}
}

namespace hm2::android {
namespace {
constexpr std::array<std::string_view,4> skills{"02004","02005","02006","02007"};
constexpr std::array<std::string_view,4> bosses{"Boss_05","Boss_06","Boss_07","Boss_08"};
constexpr std::array<std::string_view,4> missions{"M041010","M041020","M041030","M041040"};

void check(bool value,std::string message) {if (!value) throw std::runtime_error(std::move(message));}

std::string workingMap(GameSession& game) {
    auto slot=game.workingSlot();
    if(slot.size()!=SaveData::slotSize) return {};
    auto first=slot.begin()+0x2d;
    return {first,std::find(first,first+16,0)};
}

bool press(Runtime& runtime,unsigned id,int data=0,std::string_view identity={}) {
    auto& screens=runtime.testScreens();
    for(const auto& key:screens.buttons()) {
        if(key.id!=id||key.intData!=data||(!identity.empty()&&key.dataId!=identity)) continue;
        screens.revealButton(key);
        auto button=screens.button(key);
        if(!button||!button->active||!button->visible) continue;
        const auto at=screens.buttonScreenPosition(key);
        runtime.input(PointerPhase::begin,at,781);
        runtime.input(PointerPhase::end,at,781);
        return true;
    }
    return false;
}

void worldTap(Runtime& runtime,Vec2 world) {
    auto& game=runtime.testSession();
    const auto point=game.worldCanvasPoint(game.battle()->camera().toScreen(world));
    runtime.input(PointerPhase::begin,point,782);
    runtime.input(PointerPhase::end,point,782);
}
bool isFreePoint(Runtime& runtime,Vec2 world) {
    auto& game=runtime.testSession();
    const auto cameraPoint=game.battle()->camera().toScreen(world);
    auto p=game.worldCanvasPoint(cameraPoint);
    auto dim=game.display().canvas;
    if(cameraPoint.y<game.battle()->camera().inputMinimumY+20||p.x<20||p.y<20||p.x>dim.width-20||p.y>dim.height-20) return false;
    for(const auto& key:runtime.testScreens().buttons()) {
        auto b=runtime.testScreens().button(key);
        if(!b||!b->active||!b->visible) continue;
        auto box=runtime.testScreens().buttonScreenBounds(key);
        if(p.x>box.x&&p.x<=box.x+box.width&&p.y>box.y&&p.y<=box.y+box.height) return false;
    }
    return true;
}
}

// Each case has a disposable save and uses the playable Android runtime. Only
// starting progress and deterministic clocks are supplied by this cheat target.
struct Campaign::Impl {
    struct Route {std::string source;MapGate gate;unsigned button;};
    RuntimeServices& services;
    std::filesystem::path directory;
    DisplayConfiguration display;
    std::vector<Route> routes;
    progression::Definitions defs;
    std::unique_ptr<Runtime> runtime;
    std::string failure,label;
    unsigned index=0,step=0,clock=0,start=0,casts=0;
    std::uint64_t target=0;
    Vec2 spawn{};
    int price=0,beforeCount=0;
    bool active=true,audioFocused=true;
    std::vector<std::string> evidence;
    enum : unsigned {routeCount=14,bossStart=14,shopIndex=18,airStart=19,total=27};

    Impl(RuntimeServices& s,std::filesystem::path path,DisplayConfiguration d)
        :services(s),directory(std::move(path)),display(d),defs(validation::missionFixtureDefinitions(s.resources)) {
        auto maps=Maps::parse(Chunk::parse(s.resources.read("MapInfo.mcb")));
        auto snow=[](std::string_view id){return id=="Base004" || id.starts_with("Snow");};
        for(auto& [id,map]:maps.all()) for(unsigned n=0;n<map.gates.size();++n) {
            const auto& gate=map.gates[n];
            if(!snow(id)&&!snow(gate.linkMap)) continue;
            auto& back=maps.map(gate.linkMap).gates;
            check(!gate.eventOnly&&std::any_of(back.begin(),back.end(),[&](const auto& g){
                return g.number==gate.linkGate&&g.linkGate==gate.number&&g.linkMap==id;
            }),"Unpaired snow portal");
            routes.push_back({id,gate,id=="Base004"?2005+n:0});
        }
        check(routes.size()==routeCount,"Expected 14 snow routes");
    }
    void advance(std::string result) {
        evidence.push_back(std::move(result));++index;runtime.reset();
    }
    void begin() {
        SaveData save;
        save.initializeSlot(0,"CampaignQA","kr",1794268800);
        progression::SlotView slot(save.slot(0));
        std::string map="Base004";
        unsigned gate=0;
        int level=99;
        if(index<routeCount) {
            const auto& route=routes[index];
            map=route.source;gate=route.gate.number;label=map+"->"+route.gate.linkMap;
        } else if(index<shopIndex) {
            unsigned b=index-bossStart;
            const auto mission=std::find_if(defs.missions().begin(),defs.missions().end(),[&](const auto& m){return m.id==missions[b];});
            check(mission!=defs.missions().end(),"Missing authored boss mission");
            const auto s=std::find_if(mission->acceptActions.begin(),mission->acceptActions.end(),[](const auto& a){return a.type==progression::Action::placeSpawner;});
            check(s!=mission->acceptActions.end(),"Missing mission spawn");
            spawn={float(s->x),float(s->y)};label=s->place;
            slot.u16(0x658,b+1);
            for(unsigned i=0;i<=b;++i) {
                const auto offset=0x65c+21*i;
                slot.string(offset,8,i==0?"M001230":missions[i-1]);slot.byte(offset+20,1);
            }
        } else if(index==shopIndex) {
            level=21;label="laser-shop";
        } else {
            const auto i=index-airStart;
            map=i<4?"SnowBasin":"SnowValley";
            level=i<4?27:48;label=map+"-"+std::string(skills[i%4]);
            for(unsigned j=0;j<5;++j)slot.string(0x61c+6*j,6,"");
            for(unsigned j:{6u,7u}){slot.string(0x8c+j*6,6,"");slot.byte(0x53c+j,0);}
            slot.string(0x8c+8*6,6,skills[i%4]);slot.byte(0x53c+8,2);
            slot.string(0x61c,6,skills[i%4]);
            for(unsigned j=0;j<3;++j)slot.string(0x63a+6*j,6,"");
        }
        slot.level(level);slot.experience(progression::Experience::bounds(level).previous);
        slot.hp(220000);slot.maxHP(220000);slot.credit(20000);
        slot.string(0x2d,16,map);slot.string(0x55,16,"Base004");slot.u32(0x68,gate);slot.u32(0x1b5c,5);
        const auto path=directory/std::to_string(index);
        const unsigned seed=index<shopIndex?7301:index==shopIndex?731:71;
        DarwinWorldRandom rng(seed);
        check(writeFile(path/"HeavyMach2.sav",save.encode(rng,seed)),"Cannot seed campaign save");
        runtime=std::make_unique<Runtime>(services,path);
        runtime->setTestClock(seed);
        runtime->setDisplay(display);
        runtime->setAudioFocused(audioFocused);
        auto& game=runtime->testSession();
        check(game.selectSlot(0),"Cannot select campaign slot");
        if(map=="Base004") game.enterBase(); else check(game.enterField(map,gate),"Cannot enter "+map);
        runtime->refreshTestScene();
        clock=step=start=casts=0;target=0;
    }
    void route() {
        auto& r=routes[index];auto& game=runtime->testSession();
        if(clock==8) {
            if(r.button)check(press(*runtime,r.button)&&press(*runtime,r.button),"Missing base exit");
            else {
                auto p=game.battle()->player().position;
                auto g=r.gate.position;
                worldTap(*runtime,{g.x+(p.x-g.x)*.24f,g.y+(p.y-g.y)*.24f});
            }
        }
        if(!start&&workingMap(game)==r.gate.linkMap)start=clock;
        if(!start||clock<start+8||game.screenTransition().phase!=8||game.mapLoading())return;
        const bool base=r.gate.linkMap=="Base004";
        check(game.state().screen==(base?ui::ScreenId::base:ui::ScreenId::field),"Wrong screen after travel");
        check(game.metrics().saveCompletions==game.metrics().saveRequests,"Incomplete route save");
        auto bytes=readFile(directory/std::to_string(index)/"HeavyMach2.sav",SaveData::byteCount);
        check(bytes.has_value(),"Missing route save");
        DarwinWorldRandom rng(1);
        auto saved=SaveData::decode(*bytes,rng,false);
        progression::SlotView slot(saved.slot(0));
        check(slot.string(0x2d,16)==(r.button?r.source:r.gate.linkMap),"Wrong persisted route");
        if(base)check(slot.u32(0x1b60+5*4)==1,"Missing White Mountain visited flag");
        advance(label);
    }
    void boss() {
        const unsigned b=index-bossStart;
        auto& game=runtime->testSession();
        if(step==0) {
            check(game.acceptMission(missions[b]),"Boss mission unavailable");
            check(game.enterField(label,0),"Missing accepted boss arena");
            runtime->refreshTestScene();step=1;return;
        }
        auto* battle=game.battle();
        const auto player=battle->player().position;
        const auto dx=spawn.x-player.x, dy=spawn.y-player.y;
        const auto distance=std::hypot(dx,dy);
        auto enemies=battle->enemies();
        auto actor=std::find_if(enemies.begin(),enemies.end(),[&](const auto& e){return e.name==bosses[b]&&e.hp>0;});
        if(clock<=50) {
            check(actor==enemies.end()&&distance>400,"Boss visible at entrance");
            return;
        }
        if(actor!=enemies.end()) { advance(label+"-"+std::string(bosses[b])); return; }
        if(!start||clock-start>=72) {
            check(distance>0,"Mission boss failed to appear");
            const auto fraction=std::min(.8f,140.f/distance);
            worldTap(*runtime,{player.x+dx*fraction,player.y+dy*fraction});
            start=clock;
        }
    }
    void shop() {
        auto& game=runtime->testSession();
        const auto& state=game.state();
        if(clock==8)check(press(*runtime,2002)&&press(*runtime,2002),"Missing shop entrance");
        if(state.screen!=ui::ScreenId::shop||game.screenTransition().phase!=8)return;
        if(state.shopCategory!=3){
            const auto keys=runtime->testScreens().buttons();
            const bool direct=std::any_of(keys.begin(),keys.end(),[](const auto& k){return k.id==2104&&k.intData==3;});
            check(press(*runtime,2104,direct?3:(state.shopCategory+1)%5),"Missing attack category");
            return;
        }
        auto countLaser=[&]{int n=0;for(auto& item:state.inventory)if(item.dataId=="02006")n+=item.count;return n;};
        if(!step) {
            auto it=std::find_if(state.shopItems.begin(),state.shopItems.end(),[](const auto& item){return item.dataId=="02006";});
            check(it!=state.shopItems.end(),"Laser absent from level-22 attack stock");
            unsigned at=unsigned(it-state.shopItems.begin());
            if(int(at/24)!=state.shopPage){check(press(*runtime,2106),"Missing stock page");return;}
            const auto cell=2200+at%24;
            const ui::ButtonKey key{(ui::Owner(state.screen)<<16)|ui::Owner(ui::Panel::root),cell,it->position,"02006"};
            runtime->testScreens().revealButton(key);
            const auto keys=runtime->testScreens().buttons();
            auto visible=std::find_if(keys.begin(),keys.end(),[&](const auto& k){return k.id==cell&&k.dataId=="02006";});
            check(visible!=keys.end()&&press(*runtime,cell,visible->intData,"02006"),"Laser stock button inaccessible");
            check(press(*runtime,2102)&&state.tradeVisible&&state.tradeItemId=="02006","Missing laser confirmation");
            price=state.credits;beforeCount=countLaser();step=1;
        } else {
            check(press(*runtime,2108)&&!state.tradeVisible&&state.credits==price-30&&countLaser()==beforeCount+1,
                  "Laser credit or inventory mismatch");
            advance("02006-level22-30Cr");
        }
    }
    void air() {
        const unsigned i=index-airStart;
        auto& game=runtime->testSession();auto* battle=game.battle();
        if(clock<30||game.screenTransition().phase!=8)return;
        auto enemies=battle->enemies();
        if(casts&&clock-start<560)return;
        if(casts) {
            const bool dead=std::none_of(enemies.begin(),enemies.end(),[&](auto& e){return e.id==target&&e.hp>0;});
            if(dead) {
                check(battle->metrics().airStrikeProjectiles==casts*(i%4==3?1:10)&&battle->metrics().airStrikeEnemyHPRemoved>0,"Missing airstrike damage or projectiles");
                advance(label+"-"+std::to_string(casts)+"casts");return;
            }
            check(casts<2,"Tank survived two centered casts");
        }
        const auto p=battle->player().position;
        std::optional<CombatEnemySnapshot> nearest,visible;
        float nearDistance=1e6f,visibleDistance=1e6f;
        for(auto& e:enemies) {
            if(e.name!=(i<4?"E_Tank_21":"E_Tank_26")||e.hp<=0||(target&&target!=e.id))continue;
            auto d=std::hypot(e.position.x-p.x,e.position.y-p.y);
            if(d<nearDistance){nearDistance=d;nearest=e;}
            if(d<visibleDistance&&isFreePoint(*runtime,e.position)){visibleDistance=d;visible=e;}
        }
        if(!visible) {
            check(nearest.has_value(),"No representative tank");
            if(!start||clock-start>=110){
                auto f=std::min(1.f,130.f/std::max(nearDistance,1.f));
                worldTap(*runtime,{p.x+(nearest->position.x-p.x)*f,p.y+(nearest->position.y-p.y)*f});start=clock;
            }
            return;
        }
        check(press(*runtime,6200),"Missing quick skill");
        target=visible->id;worldTap(*runtime,visible->position);
        ++casts;start=clock;
        check(game.state().quick[0].count==2-int(casts),"Quick slot did not consume skill");
    }
    void tick() {
        if(!active||!failure.empty()||index==total)return;
        try {
            if(!runtime)begin();
            for(unsigned n=0;n<12&&runtime;++n) {
                ++clock;runtime->tick();
                if(index<routeCount)route();else if(index<shopIndex)boss();else if(index==shopIndex)shop();else air();
                check(clock<2200,"Campaign scenario timeout");
            }
        } catch(const std::exception& e){failure=e.what();}
    }
    std::string status()const {
        std::ostringstream out;
        out<<"{\"mode\":\"campaign\",\"cases\":"<<index<<",\"target\":"<<total
           <<",\"case\":\""<<label<<"\",\"tick\":"<<clock<<",\"done\":"<<((index==total||!failure.empty())?"true":"false")
           <<",\"pass\":"<<(index==total?"true":"false")<<",\"failure\":\"";
        for(char c:failure){if(c=='"'||c=='\\')out<<'\\';out<<c;}
        out<<"\",\"completed\":[";
        for(unsigned i=0;i<evidence.size();++i){if(i)out<<',';out<<'"'<<evidence[i]<<'"';}
        out<<"]}";
        return out.str();
    }
};
Campaign::Campaign(RuntimeServices& s,std::filesystem::path path,DisplayConfiguration d)
    :impl_(std::make_unique<Impl>(s,std::move(path),d)) {publishProgress(impl_->directory/"report.json",status());}
Campaign::~Campaign()=default;
void Campaign::setActive(bool a){impl_->active=a;if(impl_->runtime)impl_->runtime->setActive(a);}
void Campaign::setAudioFocused(bool a){impl_->audioFocused=a;if(impl_->runtime)impl_->runtime->setAudioFocused(a);}
void Campaign::setDisplay(DisplayConfiguration d){impl_->display=d;if(impl_->runtime)impl_->runtime->setDisplay(d);}
void Campaign::tick(){
    const auto index=impl_->index;
    const bool failing=!impl_->failure.empty();
    const auto tick=impl_->clock;
    impl_->tick();
    if(index!=impl_->index || failing!=!impl_->failure.empty() ||
       (tick!=impl_->clock && impl_->clock%120==0))
        publishProgress(impl_->directory/"report.json",status());
}
void Campaign::draw(){if(impl_->runtime)impl_->runtime->draw();}
void Campaign::restoreGraphics(){if(impl_->runtime)impl_->runtime->restoreGraphics();}
std::string Campaign::status()const{return impl_->status();}
}

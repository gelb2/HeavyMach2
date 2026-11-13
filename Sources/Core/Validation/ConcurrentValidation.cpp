#include "Core/Validation/ConcurrentValidation.hpp"
#include "Core/Validation/SessionFixtures.hpp"
#include "Core/Progression/Progression.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>

namespace hm2::validation {
namespace {
// All original bytes and negative exists results are shared read-only between
// independent sessions. Counts below measure actual upstream calls, not guesses.
struct CachedResources final:ResourceAccess {
    ResourceAccess& source;std::map<std::string,Bytes,std::less<>> bytes;
    mutable std::map<std::string,bool,std::less<>> present;
    std::uint64_t reads=0;mutable std::uint64_t probes=0,probeMisses=0;
    std::uint64_t readMisses=0;bool warm=false;
    std::vector<std::string> warmReadNames;mutable std::vector<std::string> warmExistsNames;
    explicit CachedResources(ResourceAccess& s):source(s){}
    Bytes read(std::string_view name)override{
        auto found=bytes.find(name);if(found!=bytes.end())return found->second;
        if(warm){++readMisses;warmReadNames.emplace_back(name);}++reads;auto data=source.read(name);
        present.insert_or_assign(std::string(name),true);bytes.emplace(std::string(name),data);return data;
    }
    bool exists(std::string_view name)const override{
        const auto found=present.find(name);if(found!=present.end())return found->second;
        if(warm){++probeMisses;warmExistsNames.emplace_back(name);}++probes;const bool value=source.exists(name);present.emplace(std::string(name),value);return value;
    }
};
std::uint32_t raw32(std::span<const std::uint8_t> bytes,std::size_t offset){
    return std::uint32_t(bytes[offset])|(std::uint32_t(bytes[offset+1])<<8)|(std::uint32_t(bytes[offset+2])<<16)|(std::uint32_t(bytes[offset+3])<<24);
}
struct WeaponStart {std::string code,part;int level=0;};
void noBotFixture(progression::SlotView& slot){
    // A valid original no-BOT registration is the focused Player/weapon start
    // condition. Other field cases retain the original default Picker04201.
    // This changes only validation saves, never normal slot initialization.
    for(unsigned i=0;i<3;++i){slot.string(0x63a+i*6,6,"");slot.u32(0x64c+i*4,0);}
}
SaveData startSave(std::span<const WeaponStart> weapons,std::uint32_t time,bool shop=false){
    SaveData save;save.initializeSlot(0,"Validation","us",time);progression::SlotView slot(save.slot(0));
    noBotFixture(slot);
    // Focused weapon cases start after the original first-Help progression.
    slot.u32(0x1b5c,5);
    int level=0;for(const auto& w:weapons)level=std::max(level,w.level);
    if(level<0||level>99)throw std::runtime_error("Original item level outside supported level range");
    slot.level(level);slot.experience(level==0?299:progression::Experience::bounds(level).previous);
    slot.credit(shop?10:500);slot.hp(256+20*level);slot.maxHP(256+20*level);
    slot.string(0x2d,16,"Base001");slot.string(0x55,16,"Base001");
    for(unsigned i=0;i<weapons.size();++i){slot.string(0x8c+(2+i)*6,6,weapons[i].code);slot.byte(0x53c+2+i,1);slot.u32(0x604+(2+i)*4,2+i);}
    return save;
}
std::string bestItem(const DataTable& items,std::string_view type,std::string_view parameter){
    std::string code;float best=-1;for(std::size_t row=0;row<items.rowCount();++row)if(items.value(row,"Type")==type&&items.real(row,parameter)>best){best=items.real(row,parameter);code=items.value(row,"Code").value();}
    if(code.empty())throw std::runtime_error("Original equipped validation item absent");return code;
}
std::string bestWeapon(const DataTable& items,const Weapons& weapons,bool missile){
    std::string code;float best=-1;for(std::size_t row=0;row<items.rowCount();++row){if(items.value(row,"Type")!="PtWpn")continue;const auto& w=weapons.weapon(items.value(row,"strParam").value());
        if(missile?w.kind!=WeaponKind::missile:(w.kind!=WeaponKind::gun&&w.kind!=WeaponKind::sniper))continue;
        const float score=float(w.damage)*w.shotCount/std::max(.01f,w.reloadDelay);if(score>best){best=score;code=items.value(row,"Code").value();}}
    if(code.empty())throw std::runtime_error("Original equipped validation weapon absent");return code;
}
SaveData worldSave(const DataTable& items,const Weapons& weapons,std::uint32_t time){
    SaveData saved;saved.initializeSlot(0,"WorldValidation","us",time);progression::SlotView slot(saved.slot(0));slot.u32(0x1b5c,5);slot.level(99);slot.experience(progression::Experience::bounds(99).previous);
    const std::string equipped[]={bestItem(items,"PtBody","Param1"),bestItem(items,"PtBtm","Param3"),bestWeapon(items,weapons,false),bestWeapon(items,weapons,true),bestItem(items,"PtArmor","Param1")};
    for(unsigned i=0;i<5;++i){slot.string(0x8c+i*6,6,equipped[i]);slot.byte(0x53c+i,1);slot.u32(0x604+i*4,i);}slot.u32(0x604+5*4,std::uint32_t(-1));
    slot.string(0x8c+8*6,6,"01006");slot.byte(0x53c+8,200);slot.string(0x61c,6,"01006");
    slot.string(0x8c+9*6,6,"02007");slot.byte(0x53c+9,200);slot.string(0x622,6,"02007");return saved;
}
ClipRect intersect(ClipRect a,ClipRect b){
    const float x=std::max(a.x,b.x),y=std::max(a.y,b.y);
    return {x,y,std::max(0.f,std::min(a.x+a.width,b.x+b.width)-x),std::max(0.f,std::min(a.y+a.height,b.y+b.height)-y)};
}
}
struct ConcurrentValidation::Impl {
    struct Case;
    struct Adapter final:SessionHost {
        Impl& owner;unsigned index;Bytes saved;std::vector<std::string> alerts;
        Adapter(Impl& o,unsigned i):owner(o),index(i){}
        void sound(std::string_view name,Vec2 p)override{++owner.observations[index].sounds;owner.host.sound(index,name,p);}
        void soundID(unsigned id,Vec2 p)override{++owner.observations[index].sounds;if(index==owner.rankingIndex)owner.observations[index].ranking.events.push_back("sfx"+std::to_string(id));if(index==owner.completedMissionIndex)owner.observations[index].completedMissions.sounds.push_back(id);if(id==13)++owner.observations[index].repairSounds;if(id==14)++owner.observations[index].dashSounds;owner.host.soundID(index,id,p);}
        void alert(std::string_view key)override{alerts.emplace_back(key);}
        void requestName(unsigned)override{}void requestDelete(unsigned)override{}
        void requestGameOver(std::uint64_t token)override{
            auto& o=owner.observations[index].gameOver;++o.requests;
            if(!o.requestFrame){o.requestFrame=owner.counters.frames;o.token=token;o.savesAtRequest=owner.observations[index].saves;}
        }
        void notice(std::string_view title,std::string_view message)override{
            auto& o=owner.observations[index].ranking;++o.notices;o.frame=owner.counters.frames;o.title=title;o.message=message;o.events.emplace_back("notice");
        }
        bool writeSave(std::span<const std::uint8_t> data,SaveReason reason)override{
            if(!owner.host.writeSave(index,data,reason))return false;
            saved.assign(data.begin(),data.end());++owner.observations[index].saves;
            if(index==owner.gameOverIndex&&reason==SaveReason::enterBase&&owner.observations[index].gameOver.confirmFrame)owner.observations[index].gameOver.baseDoorPhase=owner.cases.at(index)->game->screenTransition().phase;
            return true;
        }
        ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& r)override{return owner.host.prepareWorldText(index,r);}
        std::uint32_t absoluteTimeLow()override{return owner.host.absoluteTimeLow(index);}
        std::uint32_t unixTime()override{return owner.host.unixTime();}
    };
    struct Case {
        Adapter adapter;DarwinWorldRandom random;std::unique_ptr<GameSession> game;
        std::uint64_t priorEmissions=0,priorPlayerEmissions=0,priorDamage=0,priorKills=0;
        int initialLevel=0;std::set<std::string> emitted;
        std::uint64_t worldTarget=0,lastWorldStrike=0;bool worldEnemyFired=false,worldStrikeUsed=false;
        std::uint64_t gameOverInitialTarget=0;bool gameOverRetargeted=false;
        std::map<std::string,std::uint64_t,std::less<>> lastWeaponCounts;
        Case(Impl& owner,unsigned index):adapter(owner,index),random(index+1){}
    };
    CachedResources resources;ValidationHost& host;Weapons catalog;
    std::map<std::string,int,std::less<>> botRadii;std::map<std::string,std::vector<std::string>,std::less<>> botThreats;
    std::vector<ScenarioDescription> descriptions;std::vector<ScenarioObservation> observations;
    std::vector<std::unique_ptr<Case>> cases;ConcurrentMetrics counters;
    bool active=true;std::uint64_t sourceFrameNumber=std::numeric_limits<std::uint64_t>::max();
    DisplayConfiguration display;
    std::vector<Frame> sourceFrames;std::vector<FrameActivity> activity;std::vector<ButtonActivity> pendingButtons;std::size_t shopIndex=0,missionIndex=0,restartIndex=0,movementIndex=0;
    std::vector<std::size_t> aidKitIndices,airStrikeIndices,worldIndices,botIndices,helpIndices;std::size_t dashIndex=0,gameOverIndex=112,rankingIndex=113,completedMissionIndex=114;std::uint64_t pendingAidKitUses=0,pendingAirStrikeUses=0,pendingBotRepairs=0,pendingHelpInputs=0;
    std::optional<double> compactPausedWorldTime;
    int shopCreditBefore=0,shopItemsBefore=0,shopCreditAfter=0,shopItemsAfter=0;unsigned repairButton=0;
    static int repairCount(GameSession& game){int count=0;for(const auto& item:game.state().inventory)if(item.dataId=="01001")count+=item.count;return count;}
    explicit Impl(ResourceAccess& source,ValidationHost& sink):resources(source),host(sink),catalog(Weapons::parse(DataTable::parse(resources.read("WeaponInfo.tble")))){
        const auto items=DataTable::parse(resources.read("ItemInfo.tble"));std::vector<WeaponStart> weapons;
        for(std::size_t row=0;row<items.rowCount();++row)if(items.value(row,"Type")=="PtWpn")weapons.push_back({std::string(items.value(row,"Code").value_or("")),std::string(items.value(row,"strParam").value_or("")),items.integer(row,"Lv")});
        if(weapons.size()!=110)throw std::runtime_error("Expected original HD110 player-weapon items; no enemy weapons may be substituted");
        descriptions.reserve(115);observations.resize(115);cases.reserve(115);
        for(std::size_t i=0;i<weapons.size();i+=2){const std::span pair(weapons.data()+i,std::size_t(2));
            add({"combat "+pair[0].code+" + "+pair[1].code,ScenarioRole::combat,{pair[0].code,pair[1].code}},startSave(pair,host.unixTime()),true);
        }
        counters.combatScenarios=cases.size();shopIndex=cases.size();
        add({"shop purchase / insufficient credit",ScenarioRole::shop,{}},startSave({},host.unixTime(),true),false);
        missionIndex=cases.size();add({"M001010 real battle requirement",ScenarioRole::mission,{weapons[104].code,weapons[105].code}},startSave(std::span(weapons.data()+104,2),host.unixTime()),false);
        restartIndex=cases.size();add({"encoded save / restart",ScenarioRole::saveRestart,{weapons[0].code,weapons[1].code}},startSave(std::span(weapons.data(),2),host.unixTime()),true);
        movementIndex=cases.size();
        add({"real navigation / enemy drop / Player pickup",ScenarioRole::movementLoot,{weapons[104].code,weapons[105].code}},startSave(std::span(weapons.data()+104,2),host.unixTime()),true);
        auto& movement=observations[movementIndex];auto& game=*cases[movementIndex]->game;
        movement.startPosition=movement.lastPosition=game.battle()->player().position;movement.initialCredit=cases[movementIndex]->game->state().credits;
        movement.initialExperience=raw32(game.workingSlot(),0x70);movement.initialHP=game.battle()->playerHP();
        for(const auto* code:{"01001","01002","01003","01004","01005","01006"}){
            auto save=startSave(std::span(weapons.data()+104,2),host.unixTime());progression::SlotView slot(save.slot(0));
            slot.hp(std::max(1,slot.maxHP()/4));slot.string(0x8c+5*6,6,code);slot.byte(0x53c+5,5);slot.string(0x61c,6,code);
            const auto index=cases.size();aidKitIndices.push_back(index);
            add({std::string("real AidKit ")+code,ScenarioRole::aidKit,{weapons[104].code,weapons[105].code}},std::move(save),true,false);
            observations[index].quickItemCode=code;
        }
        for(const auto* code:{"02004","02005","02007"}){
            auto save=startSave(std::span(weapons.data()+104,2),host.unixTime());progression::SlotView slot(save.slot(0));
            slot.string(0x8c+6*6,6,code);slot.byte(0x53c+6,5);slot.string(0x8c+7*6,6,"");slot.byte(0x53c+7,0);slot.string(0x622,6,code);
            const auto index=cases.size();airStrikeIndices.push_back(index);
            add({std::string("real airstrike ")+code,ScenarioRole::airStrike,{weapons[104].code,weapons[105].code}},std::move(save),true);
            observations[index].strikeItemCode=code;
        }
        const auto maps=Maps::parse(Chunk::parse(resources.read("MapInfo.mcb")));
        for(const auto& [name,map]:maps.all())if(name.starts_with("Stage")&&name!="Stage01_01"){
            if(map.gates.empty())throw std::runtime_error("Original field has no entry gate");worldIndices.push_back(cases.size());
            add({"original field "+name,ScenarioRole::worldAudit,{},name},worldSave(items,catalog,host.unixTime()),true,true,name,map.gates.front().number);
        }
        if(worldIndices.size()!=37)throw std::runtime_error("Expected37 additional original HD fields");
        dashIndex=cases.size();SaveData dash;dash.initializeSlot(0,"DashValidation","us",host.unixTime());
        progression::SlotView dashSlot(dash.slot(0));dashSlot.u32(0x1b5c,5);noBotFixture(dashSlot);
        add({"real ground double tap / SP / dash",ScenarioRole::dash,{},"Stage01_01"},std::move(dash),true,false);
        // Append only: original conditions0..105 retain every index/fixture.
        // These are explicit original-save validation starts, not changes to
        // normal initializeSlot's Picker or automatic gameplay rewards.
        const auto machines=Machines::parse(DataTable::parse(resources.read("MachList.tble")));
        const auto models=Models::parse(Chunk::parse(resources.read("PartsInfo.mcb")),Chunk::parse(resources.read("MachInfo.mcb")));
        for(const auto& [name,machine]:machines.all())botRadii.emplace(name,machine.radius);
        for(const auto& spawner:maps.map("Stage01_01").spawners){botRadii.insert_or_assign(spawner.model,machines.machine(spawner.model).radius);
            if(botThreats.contains(spawner.model))continue;
            for(const auto& part:models.modelMounts(spawner.model)){const auto weapon=catalog.all().find(part.part);if(weapon!=catalog.all().end()&&int(weapon->second.kind)>=0&&int(weapon->second.kind)<=8&&weapon->second.kind!=WeaponKind::gun&&weapon->second.kind!=WeaponKind::sniper&&weapon->second.damage>0&&weapon->second.damageRange>0)botThreats[spawner.model].push_back(part.part);}
        }
        if(botThreats.empty())throw std::runtime_error("BOT positive-damage fixture requires an original hostile GobBullet/blast consumer");
        for(bool attacker:{true,false}){
            auto saved=worldSave(items,catalog,host.unixTime());progression::SlotView slot(saved.slot(0));noBotFixture(slot);
            if(attacker){slot.u32(0x604+2*4,std::uint32_t(-1));slot.u32(0x604+3*4,std::uint32_t(-1));}
            else {
                // Original passive Tanks need a surviving real hit to
                // retaliate. This explicit validation start uses the original
                // default weapons at level0, not level99 best-weapon damage.
                slot.level(0);slot.experience(0);slot.maxHP(256);
                for(unsigned j=0;j<2;++j){slot.string(0x8c+(10+j)*6,6,slot.string(0x8c+(2+j)*6,6));slot.byte(0x53c+10+j,1);}
                for(unsigned j=0;j<2;++j){slot.string(0x8c+(2+j)*6,6,j?"10111":"10011");slot.byte(0x53c+2+j,1);slot.u32(0x604+(2+j)*4,2+j);}
            }
            for(unsigned j=0;j<3;++j){const auto code=std::string(attacker?"0400":"0420")+char('1'+j);std::string machine;
                for(std::size_t row=0;row<items.rowCount();++row)if(items.value(row,"Code")==code){machine=items.value(row,"strParam").value();break;}
                if(machine.empty())throw std::runtime_error("Original BOT item missing");
                slot.string(0x8c+(5+j)*6,6,code);slot.byte(0x53c+5+j,1);slot.string(0x63a+j*6,6,code);slot.u32(0x64c+j*4,std::uint32_t(std::max(1,machines.machine(machine).hp/4)));
            }
            // Full original level-derived/body HP keeps Player alive while
            // real hostile bullets hit BOTs. Original01006 repairs BOT HP.
            float bodyHP=0;const auto bodyCode=bestItem(items,"PtBody","Param1");for(std::size_t row=0;row<items.rowCount();++row)if(items.value(row,"Code")==bodyCode)bodyHP=items.real(row,"Param1");
            slot.hp(256+slot.level()*20+int(bodyHP));
            const auto index=cases.size();botIndices.push_back(index);
            add({attacker?"BOT attack original04001/04002/04003":"BOT pickup original04201/04202/04203",attacker?ScenarioRole::botAttack:ScenarioRole::botPicker,{},"Stage01_01"},std::move(saved),true,false);
            if(!attacker)for(unsigned j=0;j<2;++j){auto& o=observations[index].botWeaponSwaps[j];o.inventoryPosition=10+j;for(const auto& item:cases[index]->game->state().inventory)if(item.position==int(10+j))o.code=item.dataId;o.oldCode=j?"10111":"10011";for(std::size_t row=0;row<items.rowCount();++row)if(items.value(row,"Code")==o.code){o.part=items.value(row,"strParam").value();break;}}
            observations[index].startPosition=observations[index].lastPosition=cases[index]->game->battle()->player().position;observations[index].initialCredit=cases[index]->game->state().credits;observations[index].initialExperience=raw32(cases[index]->game->workingSlot(),0x70);
            for(const auto& bot:cases[index]->game->battle()->bots()){auto& o=observations[index].bots.at(bot.slot);o.code=std::string(attacker?"0400":"0420")+char('1'+bot.slot);o.machine=bot.name;o.slot=bot.slot;o.id=bot.id;o.spawned=true;o.initialHP=o.hp=bot.hp;o.maximumHP=bot.maximumHP;o.lastRotor=bot.rotorAngle;}
        }
        for(unsigned mode=0;mode<4;++mode){
            auto saved=startSave({},host.unixTime());progression::SlotView slot(saved.slot(0));const unsigned counter=mode==0?0:(mode==1?4:5);slot.u32(0x1b5c,counter);
            const auto index=cases.size();helpIndices.push_back(index);
            add({mode==3?"Help system1021/1029":"Help original first counter"+std::to_string(counter),ScenarioRole::help,{},"Stage01_01"},std::move(saved),true,false);
            auto& o=observations[index].help;o.initialCounter=counter;o.resultingCounter=raw32(cases[index]->game->workingSlot(),0x1b5c);
            auto& game=*cases[index]->game;o.initialPaused=game.battle()->isPaused();o.lastWorldTime=game.battle()->clock().worldTime();
            if(const auto* help=game.help()){o.initialPresent=true;o.initialPage=help->state().page;o.initialLayer=help->state().layer;o.initialGuide=help->state().guidanceVisible;o.initialChangePages=help->state().changePages;}
        }
        // Append-only: keep original scenarios0..111 and each seed unchanged.
        gameOverIndex=cases.size();
        add({"real enemy death / GameOver / confirm / Base002 save",ScenarioRole::gameOver,{},"Stage01_01"},gameOverFixture(host.unixTime()),true,false);
        {auto& o=observations[gameOverIndex].gameOver;o.initialHP=cases[gameOverIndex]->game->battle()->playerHP();o.savesBefore=observations[gameOverIndex].saves;}
        rankingIndex=cases.size();
        add({"Command5009 / Korean ranking unavailable notice",ScenarioRole::rankingUnavailable,{}},gameOverFixture(host.unixTime()),false);
        completedMissionIndex=cases.size();
        add({"Command ordered completion / rewards / 5007 / saved payload restore",ScenarioRole::completedMissions,{}},completedMissionFixture(resources,host.unixTime()),false);
        observations[completedMissionIndex].completedMissions.savesBefore=observations[completedMissionIndex].saves;
        observations[completedMissionIndex].completedMissions.initialRepairItems=repairCount(*cases[completedMissionIndex]->game);
        // Pre-read the actual future Base002 consumer using an independent
        // preparation session. Its state, RNG, writes and HP never enter cases.
        struct PreparationHost final:SessionHost {
            Impl& owner;explicit PreparationHost(Impl& p):owner(p){}
            void sound(std::string_view,Vec2)override{}void soundID(unsigned,Vec2)override{}void alert(std::string_view)override{}
            void requestName(unsigned)override{}void requestDelete(unsigned)override{}
            bool writeSave(std::span<const std::uint8_t>,SaveReason)override{return true;}
            ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& r)override{return owner.host.prepareWorldText(unsigned(owner.gameOverIndex),r);}
            std::uint32_t absoluteTimeLow()override{return 17;}std::uint32_t unixTime()override{return owner.host.unixTime();}
        } preparationHost(*this);
        {auto saved=gameOverFixture(host.unixTime());progression::SlotView(saved.slot(0)).string(0x2d,16,"Base002");DarwinWorldRandom rng(1);GameSession preparation(resources,preparationHost,rng,std::move(saved));if(!preparation.selectSlot(0))throw std::logic_error("Base002 preparation select failed");preparation.enterBase();(void)preparation.preparationFrame();}
        // Prewarm all original KR Help pictures, including the menu guidance
        // image whose actual creation is scheduled during concurrent battle.
        for(const auto* name:{"HelpScr01_kr.png","HelpScr02_kr.png","HelpScr03_kr.png","HelpHelp.png"})if(resources.exists(name))(void)resources.read(name);
        // Prepare the original inter-map fade asset before the warm IO gate.
        // This is resource preparation, not a simulated gate transition.
        MapLoading loadingPreparation(resources);
        sourceFrames.resize(cases.size());counters.activeScenarios=cases.size();
    }
    void add(ScenarioDescription description,SaveData save,bool field,bool visitBase=true,std::string_view map="Stage01_01",int gate=0){
        const auto index=unsigned(cases.size());descriptions.push_back(std::move(description));
        auto value=std::make_unique<Case>(*this,index);value->game=std::make_unique<GameSession>(resources,value->adapter,value->random,std::move(save));
        if(!value->game->selectSlot(0))throw std::runtime_error("Validation selectSlot save failed");if(visitBase)value->game->enterBase();
        value->initialLevel=value->game->state().level;observations[index].initialLevel=value->initialLevel;
        if(field){observations[index].missionAccepted=value->game->acceptMission("M001010");if(!value->game->enterField(map,gate))throw std::runtime_error("Validation original field unavailable");if(descriptions.back().map.empty())descriptions.back().map=map;}
        if(const auto* scene=value->game->battle())value->lastWeaponCounts=scene->metrics().playerWeaponEmissions;
        cases.push_back(std::move(value));
    }
    static int inventoryCount(GameSession& game){int result=0;for(const auto& item:game.state().inventory)result+=item.count;return result;}
    void guideMovement(){
        auto& game=*cases[movementIndex]->game;auto* scene=game.battle();if(!scene||scene->playerHP()<1)return;
        auto& observed=observations[movementIndex];
        // Original navigation input owns path changes. All destinations below
        // are observations of actual world state, never actor-position writes.
        const auto loot=game.fieldItems();const Vec2 player=scene->player().position;const FieldItemSnapshot* nearest=nullptr;float best=std::numeric_limits<float>::max();
        for(const auto& item:loot)if(!item.removed){const float dx=item.position.x-player.x,dy=item.position.y-player.y,d=dx*dx+dy*dy;if(d<best){best=d;nearest=&item;}}
        //31 ticks are .5146s. Keep this walking/pickup condition outside the
        // restored <.5s Dash gesture; the independent Dash case tests that path.
        if((counters.frames-1)%31!=0)return;
        // Walk into the field while firing before the first genuine kill/drop.
        const Vec2 destination=nearest?nearest->position:Vec2{observed.startPosition.x,observed.startPosition.y+100};
        if(!nearest&&counters.frames>60)return;
        const Vec2 projected=scene->camera().toScreen(destination);
        const Vec2 screen{std::clamp(projected.x,16.f,scene->camera().viewport.width-16),std::clamp(projected.y,fieldFloor(scene->camera()),scene->camera().viewport.height-16)};
        const Vec2 requested=scene->camera().toWorld(screen);constexpr std::uintptr_t pointer=0x4c4f4f54;
        const bool begin=game.inputWorld(PointerPhase::begin,game.worldCanvasPoint(screen),pointer);game.inputWorld(PointerPhase::end,game.worldCanvasPoint(screen),pointer);
        ++observed.navigationRequests;if(!begin)++observed.navigationRejected;
        observed.navigation.push_back({counters.frames,player,requested,screen,nearest?nearest->code:"",begin,scene->player().blocked});
    }
    bool press(std::size_t index,unsigned id,int data=0){++observations[index].buttonRequests;const bool delivered=host.pressButton(unsigned(index),id,data);if(!delivered)++observations[index].buttonFailures;pendingButtons.push_back({unsigned(index),id,data,delivered});return delivered;}
    bool worldQuick(std::size_t index,unsigned slot,Vec2 target){
        auto& game=*cases[index]->game;press(index,6200+slot);const auto screen=game.battle()->camera().toScreen(target);
        constexpr std::uintptr_t pointer=0x574f524c;const bool accepted=game.inputWorld(PointerPhase::begin,game.worldCanvasPoint(screen),pointer);if(accepted)++observations[index].worldQuickUses;
        game.inputWorld(PointerPhase::end,game.worldCanvasPoint(screen),pointer);return accepted;
    }
    static float fieldFloor(const MapCamera& camera){return camera.fillSurface?camera.inputMinimumY+20:camera.fitWorld?camera.focusMinimumY+20:120.f;}
    bool approachTarget(std::size_t index,Vec2 target){
        auto& game=*cases[index]->game;auto* scene=game.battle();const auto& camera=scene->camera();const auto projected=camera.toScreen(target);
        const float bottom=fieldFloor(camera),top=camera.viewport.height-16;
        if(bottom>=top)return false;
        const Vec2 desired{std::clamp(projected.x,16.f,camera.viewport.width-16),std::clamp(projected.y,bottom,top)};
        const auto enemies=scene->enemies();const auto player=scene->player().position;
        const auto clearSegment=[&](Vec2 a,Vec2 b){if(!camera.fillSurface)return true;const float dx=b.x-a.x,dy=b.y-a.y;const unsigned steps=std::max(1u,unsigned(std::ceil(std::hypot(dx,dy)/8)));for(unsigned n=1;n<=steps;++n){const float t=float(n)/steps;const Vec2 p{a.x+dx*t,a.y+dy*t};const auto hit=scene->terrain().resolve(p);if(std::abs(hit.position.x-p.x)>.1f||std::abs(hit.position.y-p.y)>.1f)return false;for(const auto& enemy:enemies)if(enemy.hp>0&&!enemy.corpse&&enemy.kind!=MachineKind::aircraft){const float r=float(botRadii.at(enemy.name)+botRadii.at("Player"))+4;const float before=(a.x-enemy.position.x)*(a.x-enemy.position.x)+(a.y-enemy.position.y)*(a.y-enemy.position.y),after=(p.x-enemy.position.x)*(p.x-enemy.position.x)+(p.y-enemy.position.y)*(p.y-enemy.position.y);if(after<r*r&&(before>=r*r||after<before-.1f))return false;}}return true;};
        const auto navigate=[&](Vec2 ground){const auto screen=camera.toScreen(ground);constexpr std::uintptr_t pointer=0x41505052;const bool accepted=game.inputWorld(PointerPhase::begin,game.worldCanvasPoint(screen),pointer);game.inputWorld(PointerPhase::end,game.worldCanvasPoint(screen),pointer);auto& o=observations[index];++o.navigationRequests;if(!accepted)++o.navigationRejected;o.navigation.push_back({counters.frames,player,ground,screen,"approach-visible-field-target",accepted,scene->player().blocked});return accepted;};
        for(const Vec2 delta:{Vec2{0,0},Vec2{60,0},Vec2{-60,0},Vec2{0,-60},Vec2{120,-60},Vec2{-120,-60}}){
            const Vec2 screen{std::clamp(desired.x+delta.x,16.f,camera.viewport.width-16),std::clamp(desired.y+delta.y,bottom,top)};const auto ground=camera.toWorld(screen);const float dx=ground.x-player.x,dy=ground.y-player.y;if(dx*dx+dy*dy<900)continue;
            const auto contact=scene->terrain().resolve(ground);if(std::abs(contact.position.x-ground.x)>.1f||std::abs(contact.position.y-ground.y)>.1f)continue;
            bool clear=true;for(const auto& enemy:enemies)if(enemy.hp>0&&!enemy.corpse){const float x=enemy.position.x-ground.x,y=enemy.position.y-ground.y,r=float(botRadii.at(enemy.name))+1;if(x*x+y*y<r*r)clear=false;}if(!clear)continue;
            if(clearSegment(player,ground))return navigate(ground);
        }
        if(!game.display().enabled||!game.display().compact)return false;
        // Validation input planning only. A short landscape view cannot tap a
        // distant aircraft through the coastline: route ordinary ground taps
        // around the real original polygons. No actor/HP/target outcome writes.
        constexpr int side=31,count=side*side;const auto point=[](int i){return Vec2{-480.f+32*(i%side),-480.f+32*(i/side)};};
        std::array<bool,count> open{};int start=-1,goal=-1;float startDistance=std::numeric_limits<float>::max(),goalDistance=startDistance;
        for(int i=0;i<count;++i){const auto p=point(i);const auto hit=scene->terrain().resolve(p);if(std::abs(p.x-hit.position.x)>.1f||std::abs(p.y-hit.position.y)>.1f)continue;bool occupied=false;for(const auto& enemy:enemies)if(enemy.hp>0&&!enemy.corpse&&enemy.kind!=MachineKind::aircraft&&std::hypot(p.x-enemy.position.x,p.y-enemy.position.y)<botRadii.at(enemy.name)+botRadii.at("Player")+4)occupied=true;if(occupied)continue;open[i]=true;const float a=std::hypot(p.x-player.x,p.y-player.y),b=std::hypot(p.x-target.x,p.y-target.y);if(a<startDistance&&clearSegment(player,p)){start=i;startDistance=a;}if(b<goalDistance){goal=i;goalDistance=b;}}
        if(start<0||goal<0)return false;
        std::array<float,count> cost;cost.fill(std::numeric_limits<float>::max());std::array<int,count> parent;parent.fill(-1);cost[start]=0;
        using Route=std::pair<float,int>;std::priority_queue<Route,std::vector<Route>,std::greater<Route>> queue;queue.push({0,start});
        while(!queue.empty()){const int at=queue.top().second;queue.pop();if(at==goal)break;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){if(!dx&&!dy)continue;const int x=at%side+dx,y=at/side+dy,next=y*side+x;if(x<0||x>=side||y<0||y>=side||!open[next]||!clearSegment(point(at),point(next)))continue;const float distance=cost[at]+std::hypot(float(dx),float(dy));if(distance>=cost[next])continue;cost[next]=distance;parent[next]=at;const auto p=point(next),g=point(goal);queue.push({distance+std::hypot(p.x-g.x,p.y-g.y)/32,next});}}
        if(goal!=start&&parent[goal]<0){
            // Aircraft may hover over an enclosed/sea polygon. Walk to the
            // closest reachable ground, rather than choosing an unreachable
            // grid cell merely because it lies directly under the aircraft.
            goal=start;float nearest=std::numeric_limits<float>::max();
            for(int i=0;i<count;++i)if(std::isfinite(cost[i])&&cost[i]<std::numeric_limits<float>::max()){const auto p=point(i);const float d=std::hypot(p.x-target.x,p.y-target.y);if(d<nearest){nearest=d;goal=i;}}
        }
        std::vector<int> path;for(int at=goal;at>=0;at=parent[at]){path.push_back(at);if(at==start)break;}std::reverse(path.begin(),path.end());std::optional<Vec2> nextPoint;
        for(int at:path){const auto p=point(at);const auto screen=camera.toScreen(p);if(screen.x<16||screen.x>camera.viewport.width-16||screen.y<bottom||screen.y>top||!clearSegment(player,p))break;if(std::hypot(p.x-player.x,p.y-player.y)>=32)nextPoint=p;}
        return nextPoint?navigate(*nextPoint):false;
    }
    void guideWorld(std::size_t index){
        auto& c=*cases[index];auto& game=*c.game;auto* scene=game.battle();if(!scene||scene->playerHP()<1)return;
        if(game.state().hpRate<.65f)worldQuick(index,0,scene->player().position);
        const auto& map=descriptions[index].map;const bool aircraft=map=="Stage02_07"||map=="Stage03_04",turret=map=="Stage01_02";
        if(!aircraft&&!turret)return;const auto kind=aircraft?MachineKind::aircraft:MachineKind::turret;
        if(game.display().enabled&&(scene->camera().viewport.width<768||scene->camera().viewport.height<1024)){
            const auto enemies=scene->enemies();if(!c.worldTarget&&counters.frames>=2)for(const auto& e:enemies)if(e.kind==kind&&e.hp>0&&!e.corpse){c.worldTarget=e.id;break;}
            const auto found=std::find_if(enemies.begin(),enemies.end(),[&](const auto& e){return e.id==c.worldTarget&&e.hp>0&&!e.corpse;});if(found==enemies.end())return;
            const auto point=scene->camera().toScreen(found->position);const bool visible=point.x>=16&&point.x<=scene->camera().viewport.width-16&&point.y>=fieldFloor(scene->camera())&&point.y<=scene->camera().viewport.height-16;
            if(scene->metrics().enemyEmissionsByKind.at(kind)>0)c.worldEnemyFired=true;
            if(aircraft&&(!c.worldStrikeUsed||(scene->camera().fillSurface&&c.worldEnemyFired&&counters.frames>=c.lastWorldStrike+180))){
                const Vec2 aim{std::clamp(point.x,16.f,scene->camera().viewport.width-16),std::clamp(point.y,scene->camera().inputMinimumY+1,scene->camera().viewport.height-16)};
                // Use another owned original02007 through actual QuickItems,
                // aimed at visible ground near the observed aircraft. Normal
                // missile travel/splash decides whether it hits; no HP writes.
                if(std::hypot(aim.x-point.x,aim.y-point.y)<=64&&worldQuick(index,1,scene->camera().toWorld(aim))){c.worldStrikeUsed=true;c.lastWorldStrike=counters.frames;}
            }
            if(c.worldEnemyFired&&visible&&scene->selectedEnemy()!=c.worldTarget){scene->selectEnemy(c.worldTarget);scene->selectEnemy(c.worldTarget);}
            if((!visible||!c.worldEnemyFired)&&(counters.frames-2)%31==0)approachTarget(index,{found->position.x+(turret?65.f:0.f),found->position.y});
            return;
        }
        if(counters.frames==2){
            for(const auto& e:scene->enemies())if(e.kind==kind&&e.hp>0){c.worldTarget=e.id;
                if(aircraft)worldQuick(index,1,e.position);
                else {const auto p=scene->camera().toScreen({e.position.x+65,e.position.y});const Vec2 point{std::clamp(p.x,16.f,752.f),std::clamp(p.y,120.f,1008.f)};
                    constexpr std::uintptr_t pointer=0x54555252;game.inputWorld(PointerPhase::begin,game.worldCanvasPoint(point),pointer);game.inputWorld(PointerPhase::end,game.worldCanvasPoint(point),pointer);}break;}
        }
        if(!c.worldEnemyFired&&scene->metrics().enemyEmissionsByKind.at(kind)>0){c.worldEnemyFired=true;scene->selectEnemy(c.worldTarget);scene->selectEnemy(c.worldTarget);}
    }
    void guideDash(){
        if(counters.frames!=100&&counters.frames!=110)return;auto& game=*cases[dashIndex]->game;auto* scene=game.battle();
        if(!scene||scene->playerHP()<1)throw std::runtime_error("Live original Dash start condition lost");
        constexpr Vec2 offsets[]={{100,0},{100,60},{-100,0},{0,100},{60,100},{-80,80}};Vec2 target;bool found=false;
        for(const auto d:offsets){const auto p=scene->player().position;const Vec2 world{p.x+d.x,p.y+d.y};const auto screen=scene->camera().toScreen(world);
            if(screen.x<16||screen.x>scene->camera().viewport.width-16||screen.y<fieldFloor(scene->camera())||screen.y>scene->camera().viewport.height-16)continue;bool clear=true;
            for(const auto& e:scene->enemies())if(e.hp>0){const float dx=e.position.x-world.x,dy=e.position.y-world.y;if(dx*dx+dy*dy<10000)clear=false;}
            if(clear){target=screen;found=true;break;}}
        if(!found&&game.display().enabled){for(int y=-4;y<=4&&!found;++y)for(int x=-4;x<=4&&!found;++x){if(!x&&!y)continue;const auto player=scene->player().position;const Vec2 world{player.x+float(x)*50,player.y+float(y)*50};const auto screen=scene->camera().toScreen(world);if(screen.x<16||screen.x>scene->camera().viewport.width-16||screen.y<scene->camera().inputMinimumY+20||screen.y>scene->camera().viewport.height-16)continue;bool clear=true;for(const auto& e:scene->enemies())if(e.hp>0){const float dx=e.position.x-world.x,dy=e.position.y-world.y;if(dx*dx+dy*dy<10000)clear=false;}if(clear){target=screen;found=true;}}}
        if(!found)throw std::runtime_error("Original Dash ground destination unavailable");auto& o=observations[dashIndex];
        if(counters.frames==110){o.dashSPBefore=scene->dashState().special;o.dashStartPosition=scene->player().position;}
        constexpr std::uintptr_t pointer=0x44415348;const bool accepted=game.inputWorld(PointerPhase::begin,game.worldCanvasPoint(target),pointer);game.inputWorld(PointerPhase::end,game.worldCanvasPoint(target),pointer);
        if(counters.frames==110){o.dashSPAfter=scene->dashState().special;o.dashInputAccepted=accepted;o.dashInputFrame=counters.frames;}
    }
    void guideBots(){
        for(const auto index:botIndices){auto& game=*cases[index]->game;auto* scene=game.battle();if(!scene||scene->playerHP()<1)continue;
            // Real walking keeps the registered trio in the active battle.
            // No BOT/player transform or HP setter is used by validation.
            const bool positiveIncoming=std::any_of(observations[index].bots.begin(),observations[index].bots.end(),[](const auto& bot){return bot.hpRemoved>0;});
            if(descriptions[index].role==ScenarioRole::botPicker&&((!positiveIncoming&&!scene->selectedEnemy())||(positiveIncoming&&(counters.frames-1)%31==0))){
                // Use an actual enemy tap to provoke the original passive
                // Tank. Its real damage consumer retargets Player; no hostile
                // target/fire/HP state is injected by validation. After the
                // counterattack, retap the nearest visible enemy rather than
                // waiting on a farther table-order enemy outside gun range.
                const WeaponSwapObservation* pending=nullptr;if(scene->camera().fillSurface&&positiveIncoming)for(const auto& swap:observations[index].botWeaponSwaps)if(swap.equipFrame&&!swap.firstEmissionFrame)pending=&swap;
                float closest=pending?-1:std::numeric_limits<float>::max();Vec2 chosen{};std::uint64_t target=0;const auto player=scene->player().position;
                for(const auto& e:scene->enemies())if(e.hp>0&&!e.corpse&&(pending||botThreats.contains(e.name))){const auto screen=scene->camera().toScreen(e.position);if(screen.x<16||screen.x>scene->camera().viewport.width-16||screen.y<fieldFloor(scene->camera())||screen.y>scene->camera().viewport.height-16)continue;const float dx=e.position.x-player.x,dy=e.position.y-player.y,d=dx*dx+dy*dy;if(pending){const float range=catalog.weapon(pending->part).shotRange;if(d>range*range)continue;}if(pending?d>closest:d<closest){closest=d;chosen=screen;target=e.id;}}
                if(target&&target!=scene->selectedEnemy()){constexpr std::uintptr_t pointer=0x424f5454;const bool accepted=game.inputWorld(PointerPhase::begin,game.worldCanvasPoint(chosen),pointer);game.inputWorld(PointerPhase::end,game.worldCanvasPoint(chosen),pointer);if(accepted&&scene->selectedEnemy()==target)cases[index]->worldTarget=target;}
            }
            if(!positiveIncoming&&counters.frames>120&&(counters.frames-1)%31==0){
                // Guns directly target Player; positive BOT HP damage needs an
                // actual hostile projectile/blast. The original two Tank types
                // both use kind8 Bullet; do not guess from fire-sound names.
                // Choose the original MCB
                // weapon/radius data, not an invented damaging actor.
                const auto enemies=scene->enemies();const Vec2 player=scene->player().position;float best=std::numeric_limits<float>::max();Vec2 chosen{};std::string threat;std::uint64_t chosenEnemy=0;
                const auto tracked=cases[index]->worldTarget;
                const bool tracking=std::any_of(enemies.begin(),enemies.end(),[&](const auto& e){if(e.id!=tracked||e.hp<1||e.corpse)return false;const auto p=scene->camera().toScreen(e.position);return p.x>=32&&p.x<=scene->camera().viewport.width-32&&p.y>=fieldFloor(scene->camera())&&p.y<=scene->camera().viewport.height-32;});
                for(const auto& e:enemies)if(e.hp>0&&!e.corpse&&botThreats.contains(e.name)){
                    // Approach a threat actually inside the play area. A
                    // clamped offscreen destination can chase a retreating
                    // Tank along the HUD edge without ever entering battle.
                    const auto threatScreen=scene->camera().toScreen(e.position);
                    if(threatScreen.x<32||threatScreen.x>scene->camera().viewport.width-32||threatScreen.y<fieldFloor(scene->camera())||threatScreen.y>scene->camera().viewport.height-32||(tracking&&e.id!=tracked))continue;
                    const float standOff=float(botRadii.at(e.name)+botRadii.at("Player"))+12.f;
                    for(const Vec2 direction:{Vec2{1,0},Vec2{-1,0},Vec2{0,1},Vec2{0,-1}}){
                        const auto desired=scene->camera().toScreen({e.position.x+direction.x*standOff,e.position.y+direction.y*standOff});
                        if(desired.x<16||desired.x>scene->camera().viewport.width-16||desired.y<fieldFloor(scene->camera())||desired.y>scene->camera().viewport.height-16)continue;
                        const Vec2 screen=desired;
                        const Vec2 ground=scene->camera().toWorld(screen);const float dx=ground.x-player.x,dy=ground.y-player.y;if(dx*dx+dy*dy<900)continue;
                        bool clear=true;for(const auto& other:enemies)if(other.hp>0&&!other.corpse){const float x=ground.x-other.position.x,y=ground.y-other.position.y,r=float(botRadii.at(other.name))+1;if(x*x+y*y<r*r)clear=false;}
                        const auto contact=scene->terrain().resolve(ground);if(std::abs(contact.position.x-ground.x)>.1f||std::abs(contact.position.y-ground.y)>.1f)clear=false;
                        if(!clear)continue;const float score=dx*dx+dy*dy;
                        if(score<best){best=score;chosen=screen;threat=e.name;chosenEnemy=e.id;}
                    }
                }
                if(!threat.empty()){cases[index]->worldTarget=chosenEnemy;constexpr std::uintptr_t id=0x424f544e;const bool accepted=game.inputWorld(PointerPhase::begin,game.worldCanvasPoint(chosen),id);game.inputWorld(PointerPhase::end,game.worldCanvasPoint(chosen),id);auto& o=observations[index];++o.navigationRequests;if(!accepted)++o.navigationRejected;o.botThreatName=threat;o.navigation.push_back({counters.frames,player,scene->camera().toWorld(chosen),chosen,threat+"#"+std::to_string(chosenEnemy),accepted,scene->player().blocked});}
            }
            if(descriptions[index].role==ScenarioRole::botPicker&&positiveIncoming&&!cases[index]->worldEnemyFired&&(observations[index].navigation.empty()||counters.frames-observations[index].navigation.back().frame>=31)){
                // After provoking a real counterattack, walk back into the
                // field instead of finishing the approach on top of the loot.
                // A31-frame gap avoids the original .5s double-tap dash.
                // Picker AI and actual drops decide who captures each item.
                for(const Vec2 ground:{Vec2{0,0},Vec2{-80,0},Vec2{80,0},Vec2{0,80},Vec2{0,-80}}){
                    const auto screen=scene->camera().toScreen(ground);if(screen.x<16||screen.x>scene->camera().viewport.width-16||screen.y<fieldFloor(scene->camera())||screen.y>scene->camera().viewport.height-16)continue;
                    const auto contact=scene->terrain().resolve(ground);if(std::abs(contact.position.x-ground.x)>.1f||std::abs(contact.position.y-ground.y)>.1f)continue;
                    bool clear=true;for(const auto& e:scene->enemies())if(e.hp>0&&!e.corpse){const float dx=e.position.x-ground.x,dy=e.position.y-ground.y,r=float(botRadii.at(e.name))+8;if(dx*dx+dy*dy<r*r)clear=false;}if(!clear)continue;
                    const auto player=scene->player().position;constexpr std::uintptr_t pointer=0x424f5443;const bool accepted=game.inputWorld(PointerPhase::begin,game.worldCanvasPoint(screen),pointer);game.inputWorld(PointerPhase::end,game.worldCanvasPoint(screen),pointer);auto& o=observations[index];++o.navigationRequests;if(!accepted)++o.navigationRejected;o.navigation.push_back({counters.frames,player,ground,screen,"field-centre-after-real-counterattack",accepted,scene->player().blocked});if(accepted)cases[index]->worldEnemyFired=true;break;
                }
                if(game.display().enabled&&!cases[index]->worldEnemyFired)approachTarget(index,{0,0});
            }
            if(descriptions[index].role==ScenarioRole::botPicker&&positiveIncoming){
                for(unsigned order=0;order<2;++order){const unsigned side=scene->camera().fillSurface?1-order:order;auto& swap=observations[index].botWeaponSwaps[side];if(swap.equipFrame)continue;
                    // In the compact view, exercise the new missile before
                    // equipping the one-shot gun which would kill its target
                    // first. Wait for a real emission, never inject one.
                    if(scene->camera().fillSurface){if(side==0&&!observations[index].botWeaponSwaps[1].firstEmissionFrame)break;}
                    else if(side&&!observations[index].botWeaponSwaps[0].equipFrame)break;
                    const auto& state=game.state();
                    if(!swap.chooserFrame){swap.pausedBefore=scene->isPaused();swap.savesBefore=observations[index].saves;for(const auto& item:state.inventory)if(item.position==int(swap.inventoryPosition)){swap.ownedBefore=item.dataId==swap.code;swap.countBefore=item.count;swap.equippedBefore=item.equipped;}
                        press(index,1022+side);swap.chooserFrame=counters.frames;swap.chooserShown=game.state().weaponChoiceType==int(2+side);break;}
                    if(counters.frames-swap.chooserFrame<31)break;
                    press(index,1024,int(swap.inventoryPosition));swap.equipFrame=counters.frames;const auto& after=game.state();swap.equipped=after.equipment[2+side].dataId==swap.code&&after.equipment[2+side].position==int(swap.inventoryPosition);swap.pausedAfter=scene->isPaused();swap.savesAfter=observations[index].saves;
                    for(const auto& item:after.inventory){if(item.position==int(swap.inventoryPosition)){swap.ownedAfter=item.dataId==swap.code;swap.countAfter=item.count;swap.equippedAfter=item.equipped;}if(item.position==int(2+side)){swap.oldCountAfter=item.count;swap.oldEquippedAfter=item.equipped;}}
                    break;
                }
                // A compact view changes which real enemies are active and
                // where they drop loot. After both real weapon swaps fire,
                // walk near remaining drops so the original wandering trio
                // can reach them. Keep Player outside its30-unit pickup box;
                // do not move BOTs or assign captures to a particular slot.
                const auto& observation=observations[index];
                const bool missingCapture=std::any_of(observation.bots.begin(),observation.bots.end(),[](const auto& b){return b.itemCaptures==0;});
                const bool swapsFired=std::all_of(observation.botWeaponSwaps.begin(),observation.botWeaponSwaps.end(),[](const auto& swap){return swap.firstEmissionFrame>0;});
                if(game.display().enabled&&missingCapture&&swapsFired&&(observation.navigation.empty()||counters.frames-observation.navigation.back().frame>=120)){
                    const auto player=scene->player().position;float nearest=std::numeric_limits<float>::max();Vec2 destination{};
                    for(const auto& drop:game.fieldItems())if(!drop.removed){const float dx=drop.position.x-player.x,dy=drop.position.y-player.y,d=dx*dx+dy*dy;if(d>14400&&d<nearest){nearest=d;const float distance=std::sqrt(d);destination={drop.position.x-dx*100/distance,drop.position.y-dy*100/distance};}}
                    if(nearest<std::numeric_limits<float>::max())approachTarget(index,destination);
                }
            }
            // Each original BOT receives an actual quick repair only when it
            // is separated from Player/other BOTs, so priority cannot silently
            // repair a different actor. Wait for real wander; never teleport.
            if(counters.frames<180)continue;
            const auto bots=scene->bots();for(const auto& bot:bots){auto& observed=observations[index].bots.at(bot.slot);if(observed.repairFrame||bot.removed||bot.hp<=0||bot.hp>=bot.maximumHP)continue;
                const auto point=scene->camera().toScreen(bot.position);if(point.x<16||point.x>scene->camera().viewport.width-16||point.y<fieldFloor(scene->camera())||point.y>scene->camera().viewport.height-16)continue;
                const auto apart=[](Vec2 a,Vec2 b){const float dx=a.x-b.x,dy=a.y-b.y;return dx*dx+dy*dy>=36*36;};
                bool clear=apart(bot.position,scene->player().position);for(const auto& other:bots)if(other.id!=bot.id&&!apart(bot.position,other.position))clear=false;if(!clear)continue;
                observed.repairBefore=bot.hp;observed.repairCountBefore=game.state().quick[0].count;const auto particles=scene->metrics().particleAllocations;const auto sounds=observations[index].repairSounds;press(index,6200);
                constexpr std::uintptr_t pointer=0x424f5452;observed.repairAccepted=game.inputWorld(PointerPhase::begin,game.worldCanvasPoint(point),pointer);game.inputWorld(PointerPhase::end,game.worldCanvasPoint(point),pointer);
                for(const auto& after:scene->bots())if(after.id==bot.id)observed.repairAfter=after.hp;observed.repairCountAfter=game.state().quick[0].count;observed.repairParticles=scene->metrics().particleAllocations-particles;observed.repairSounds=observations[index].repairSounds-sounds;
                if(observed.repairAccepted&&observed.repairAfter>observed.repairBefore&&observed.repairCountAfter==observed.repairCountBefore-1){observed.repairFrame=counters.frames;++pendingBotRepairs;}
                break;
            }
        }
    }
    void helpEnd(std::size_t index,float x){
        auto& game=*cases[index]->game;HelpInputObservation event;event.frame=counters.frames;event.x=x;
        const auto* before=game.help();if(before){event.beforePage=before->state().page;event.beforeGuide=before->state().guidanceVisible;}
        ++pendingHelpInputs;const auto d=game.display();const auto a=d.content();const Vec2 helpPoint=d.enabled?Vec2{a.x+a.width*x/768,a.y+a.height*.5f}:Vec2{x,512};event.accepted=game.inputOverlay(PointerPhase::end,helpPoint,0x48454c50);
        const auto* after=game.help();if(after){event.afterPage=after->state().page;event.afterGuide=after->state().guidanceVisible;}
        event.closed=before&&!after;event.paused=game.battle()->isPaused();observations[index].help.inputs.push_back(event);
        if(event.closed){observations[index].help.closedFrame=counters.frames;observations[index].help.timeAtClose=game.battle()->clock().worldTime();}
    }
    void guideHelp(){
        const auto menu=helpIndices.back();auto& game=*cases[menu]->game;auto& o=observations[menu].help;
        if(counters.frames==181)press(menu,1021);
        if(counters.frames==182){press(menu,1029);if(const auto* help=game.help()){o.menuOpened=true;o.menuPage=help->state().page;o.menuGuide=help->state().guidanceVisible;o.menuLayer=help->state().layer;o.menuPaused=game.battle()->isPaused();o.timeAtMenu=game.battle()->clock().worldTime();}}
        if(counters.frames==200){for(unsigned j=0;j<3;++j)helpEnd(helpIndices[j],384);helpEnd(menu,384);}
        if(counters.frames==210)helpEnd(menu,100);
        if(counters.frames==220)helpEnd(menu,700);
        if(counters.frames==230)helpEnd(menu,384);
    }
    void guideGameOver(){
        auto& game=*cases[gameOverIndex]->game;auto& o=observations[gameOverIndex].gameOver;
        if(o.requestFrame&&!o.confirmFrame&&counters.frames>=o.requestFrame+8){
            auto* scene=game.battle();o.confirmFrame=counters.frames;o.confirmWorldTime=scene->clock().worldTime();o.waitAdvanced=o.confirmWorldTime>o.requestWorldTime;
            o.confirmAccepted=game.confirmGameOver(o.token);o.confirmDoorPhase=game.screenTransition().phase;o.pendingCleared=!game.pendingGameOver();
            o.locationAtConfirm=progression::SlotView(std::span(const_cast<std::uint8_t*>(game.workingSlot().data()),game.workingSlot().size())).string(0x2d,16);
            o.savesAfterConfirm=observations[gameOverIndex].saves;o.duplicateRejected=!game.confirmGameOver(o.token);return;
        }
        auto* scene=game.battle();if(!scene||scene->playerHP()<1)return;
        auto& fixture=*cases[gameOverIndex];
        if(o.targetInputAccepted){
            if(!display.compact||fixture.gameOverRetargeted||scene->metrics().playerEmissions==0)return;
            // Provoke another original enemy while the first can retaliate,
            // as the actual Source UI GameOver test does. Do not set HP/death.
            for(const auto& enemy:scene->enemies())if(enemy.id!=fixture.gameOverInitialTarget&&enemy.hp>0&&!enemy.corpse){const auto point=scene->camera().toScreen(enemy.position);if(point.x<=16||point.x>=scene->camera().viewport.width-16||point.y<=fieldFloor(scene->camera())||point.y>=scene->camera().viewport.height-16)continue;
                constexpr std::uintptr_t pointer=0x474f5632;game.inputWorld(PointerPhase::begin,game.worldCanvasPoint(point),pointer);game.inputWorld(PointerPhase::end,game.worldCanvasPoint(point),pointer);
                if(scene->selectedEnemy()==enemy.id){fixture.gameOverRetargeted=true;break;}
            }return;
        }
        for(const auto& enemy:scene->enemies()){
            const auto point=scene->camera().toScreen(enemy.position);if(enemy.hp<1||enemy.corpse||point.x<=0||point.x>=scene->camera().viewport.width||point.y<=scene->camera().inputMinimumY||point.y>=scene->camera().viewport.height)continue;
            constexpr std::uintptr_t pointer=0x474f5645;const bool accepted=game.inputWorld(PointerPhase::begin,game.worldCanvasPoint(point),pointer);game.inputWorld(PointerPhase::end,game.worldCanvasPoint(point),pointer);
            o.targetInputAccepted=accepted&&scene->selectedEnemy()==enemy.id;if(o.targetInputAccepted){fixture.gameOverInitialTarget=enemy.id;break;}
        }
    }
    void guideRanking(){
        auto& game=*cases[rankingIndex]->game;auto& o=observations[rankingIndex].ranking;
        if(counters.frames==181||counters.frames==182)press(rankingIndex,2001);
        if(o.notices||counters.frames<183||game.state().screen!=ui::ScreenId::command||game.screenTransition().phase!=8)return;
        const Bytes slot(game.workingSlot().begin(),game.workingSlot().end()),disk=cases[rankingIndex]->adapter.saved;const auto screen=game.state().screen;const auto transition=game.screenTransition().phase;const auto unsupported=game.metrics().unsupportedActions;
        o.events.clear();o.savesBefore=observations[rankingIndex].saves;press(rankingIndex,5009);o.savesAfter=observations[rankingIndex].saves;
        o.slotUnchanged=std::equal(slot.begin(),slot.end(),game.workingSlot().begin());o.savedBytesUnchanged=disk==cases[rankingIndex]->adapter.saved;
        o.screenUnchanged=screen==game.state().screen;o.transitionUnchanged=transition==game.screenTransition().phase;o.unsupportedUnchanged=unsupported==game.metrics().unsupportedActions;
    }
    void guideCompletedMissions(){
        auto& game=*cases[completedMissionIndex]->game;auto& o=observations[completedMissionIndex].completedMissions;
        if(counters.frames==181||counters.frames==182)press(completedMissionIndex,2001);
        if(o.doorReadyFrame&&!o.okFrames[0]&&counters.frames>=o.doorReadyFrame+8){o.okFrames[0]=counters.frames;o.secondOKAccepted=press(completedMissionIndex,5007);}
        else if(o.popupFrames[1]&&!o.okFrames[1]&&counters.frames>=o.popupFrames[1]+8){o.okFrames[1]=counters.frames;o.finalOKAccepted=press(completedMissionIndex,5007);o.listImmediatelyAfterFinalOK=game.state().commandListVisible;}
        else if(o.listFrame&&!o.exitFrame&&counters.frames>=o.listFrame+8){o.exitFrame=counters.frames;press(completedMissionIndex,2100);}
    }
    void schedule(){
        for(const auto index:worldIndices)guideWorld(index);guideDash();guideBots();guideHelp();guideGameOver();guideRanking();guideCompletedMissions();
        // In the compact batch, pause/resume one real railgun battle through
        // the enlarged HUD while all other sessions keep advancing. This
        // also exercises overlapping firing after a menu interruption.
        if(display.enabled&&display.compact&&(counters.frames==120||counters.frames==121))for(unsigned i=0;i<counters.combatScenarios;++i){auto* battle=cases[i]->game->battle();const auto mounts=battle->playerWeapons();if(std::any_of(mounts.begin(),mounts.end(),[](const auto& w){return w.part=="P_Wpn_Rlg_101";})){
            if(counters.frames==120){compactPausedWorldTime=battle->clock().worldTime();if(!press(i,1021)||!battle->isPaused())throw std::logic_error("Compact HUD did not pause the real battle");}
            else {if(!compactPausedWorldTime||battle->clock().worldTime()!=*compactPausedWorldTime)throw std::logic_error("Compact HUD pause advanced world time");if(!press(i,1030)||battle->isPaused())throw std::logic_error("Compact HUD did not resume the real battle");}
            break;}}
        auto& shop=*cases[shopIndex]->game;const auto menuFrame=counters.frames>180?counters.frames-180:0;
        // The second Base tap requests original doors. Run all other worlds
        // while these real menu sessions close, replace at phase4, and reopen.
        const auto frame=menuFrame<=2?menuFrame:(menuFrame>167?menuFrame-165:0);
        // Menu actions overlap every one of the55 field sessions; they are not
        // a substitute for those continuously advancing original battles.
        if(frame==1||frame==2)press(shopIndex,2002);
        if(frame==3)press(shopIndex,2104,4);
        if(frame==4){const auto& stock=shop.state().shopItems;const auto item=std::find_if(stock.begin(),stock.end(),[](const auto& i){return i.dataId=="01001";});
            if(item!=stock.end()&&item-stock.begin()<24){repairButton=2200+unsigned(item-stock.begin());shopCreditBefore=shop.state().credits;shopItemsBefore=repairCount(shop);press(shopIndex,repairButton);}}
        if(frame==5)press(shopIndex,2102);
        if(frame==6)press(shopIndex,2108);
        if(frame==7){shopCreditAfter=shop.state().credits;shopItemsAfter=repairCount(shop);observations[shopIndex].shopPurchased=shopCreditAfter==shopCreditBefore-10&&shopItemsAfter==shopItemsBefore+1;if(repairButton)press(shopIndex,repairButton);}
        if(frame==8)press(shopIndex,2102);
        if(frame==9)press(shopIndex,2108);
        if(frame==10){const auto& alerts=cases[shopIndex]->adapter.alerts;observations[shopIndex].insufficientRejected=observations[shopIndex].shopPurchased&&shop.state().credits==shopCreditAfter&&repairCount(shop)==shopItemsAfter&&std::find(alerts.begin(),alerts.end(),"NotEnoughCr")!=alerts.end();}
        if(frame==1||frame==2)press(missionIndex,2001);
        if(frame==3)press(missionIndex,5100);
        if(frame==4)press(missionIndex,5004);
        if(frame==5){auto& mission=*cases[missionIndex]->game;for(const auto& item:mission.state().acceptedMissions)if(item.dataId=="M001010")observations[missionIndex].missionAccepted=true;
            if(observations[missionIndex].missionAccepted&&!mission.enterField("Stage01_01",0))throw std::runtime_error("Validation mission field unavailable");}
        if(counters.frames==240)for(const auto index:aidKitIndices)press(index,6200);
        if(counters.frames==241)for(const auto index:aidKitIndices){
            auto& game=*cases[index]->game;auto& o=observations[index];auto* scene=game.battle();
            const auto count=[&](){int result=0;for(const auto& item:game.state().inventory)if(item.dataId==o.quickItemCode)result+=item.count;return result;};
            o.quickCountBefore=count();o.quickHPBefore=scene->playerHP();o.quickSlotHPBefore=int(raw32(game.workingSlot(),0x7c));o.quickMaximumHP=int(raw32(game.workingSlot(),0x80));
            const auto particles=scene->metrics().particleAllocations;const auto sounds=o.repairSounds;
            const auto screen=scene->camera().toScreen(scene->player().position);constexpr std::uintptr_t pointer=0x4149444b;
            o.quickTapAccepted=game.inputWorld(PointerPhase::begin,game.worldCanvasPoint(screen),pointer);game.inputWorld(PointerPhase::end,game.worldCanvasPoint(screen),pointer);
            o.quickHPAfter=scene->playerHP();o.quickSlotHPAfter=int(raw32(game.workingSlot(),0x7c));o.quickCountAfter=count();o.quickSelectionAfter=game.state().selectedQuick;
            o.quickRepairSounds=o.repairSounds-sounds;o.quickParticleAllocations=scene->metrics().particleAllocations-particles;o.quickUseFrame=counters.frames;
            if(o.quickCountAfter==o.quickCountBefore-1&&o.quickHPAfter>o.quickHPBefore)++pendingAidKitUses;
        }
        if(counters.frames==260)for(const auto index:airStrikeIndices)press(index,6201);
        if(counters.frames==261)for(const auto index:airStrikeIndices){
            auto& game=*cases[index]->game;auto& o=observations[index];auto* scene=game.battle();
            const auto count=[&](){int result=0;for(const auto& item:game.state().inventory)if(item.dataId==o.strikeItemCode)result+=item.count;return result;};
            o.strikeCountBefore=count();Vec2 target{scene->player().position.x+100,scene->player().position.y+100};float best=std::numeric_limits<float>::max();
            for(const auto& enemy:scene->enemies())if(enemy.hp>0&&!enemy.corpse){const auto point=scene->camera().toScreen(enemy.position);if(point.x<32||point.x>scene->camera().viewport.width-32||point.y<fieldFloor(scene->camera())+60||point.y>scene->camera().viewport.height-44)continue;const float dx=enemy.position.x-scene->player().position.x,dy=enemy.position.y-scene->player().position.y,d=dx*dx+dy*dy;if(d<best){best=d;target=enemy.position;o.strikeTargetVisibleEnemy=true;}}
            o.strikeTarget=target;const auto screen=scene->camera().toScreen(target);constexpr std::uintptr_t pointer=0x41495253;
            o.strikeTapAccepted=game.inputWorld(PointerPhase::begin,game.worldCanvasPoint(screen),pointer);game.inputWorld(PointerPhase::end,game.worldCanvasPoint(screen),pointer);o.strikeCountAfter=count();o.strikeUseFrame=counters.frames;
            if(o.strikeTapAccepted&&o.strikeCountAfter==o.strikeCountBefore-1)++pendingAirStrikeUses;
        }
        if(counters.frames==600)restart();
    }
    void observe(std::size_t i){
        auto& value=*cases[i];auto& game=*value.game;auto& observed=observations[i];const auto& state=game.state();
        observed.level=state.level;observed.credit=state.credits;observed.experience=raw32(game.workingSlot(),0x70);
        if(descriptions[i].role==ScenarioRole::completedMissions){
            auto& o=observed.completedMissions;
            if(!o.doorReadyFrame&&state.screen==ui::ScreenId::command&&game.screenTransition().phase==8)o.doorReadyFrame=counters.frames;
            for(unsigned k=0;k<2;++k)if(!o.popupFrames[k]&&state.completedMission.dataId==(k==0?"M002010":"M001010")){
                o.popupFrames[k]=counters.frames;o.ids[k]=state.completedMission.dataId;o.credits[k]=state.credits;o.experience[k]=int(observed.experience);o.levels[k]=state.level;o.rewardCounts[k]=int(state.completedMission.rewards.size());o.popupSaves[k]=observed.saves;
            }
            if(o.okFrames[1]&&!o.listFrame&&state.screen==ui::ScreenId::command&&state.commandListVisible){o.listFrame=counters.frames;o.finalCredit=state.credits;o.remainingAccepted=int(state.acceptedMissions.size());}
            if(o.exitFrame&&!o.baseFrame&&state.screen==ui::ScreenId::base){
                o.baseFrame=counters.frames;o.savesAfter=observed.saves;DarwinWorldRandom rng(1);auto saved=SaveData::decode(value.adapter.saved,rng,false);progression::SlotView slot(saved.slot(0));const auto defs=missionFixtureDefinitions(resources);progression::Events events;progression::Progression restored(slot,defs,events);restored.restore();
                o.repairItems=restored.inventory.count(*defs.findItem("01001"));
                o.savedPayloadValid=slot.credit()==680&&slot.experience()==400&&restored.missions.state("M002010")->complete&&restored.missions.state("M001010")->complete&&!restored.missions.state("M001080")->complete&&restored.missions.acceptedCount()==2;
            }
        }
        for(const auto& mission:state.acceptedMissions)if(mission.dataId=="M001010")observed.missionComplete=mission.complete;
        if(descriptions[i].role==ScenarioRole::gameOver){
            auto& o=observed.gameOver;
            if(const auto* scene=game.battle()){
                o.enemyEmissions=scene->metrics().enemyEmissions;o.damageCalls=scene->metrics().damageCalls;
                if(scene->playerHP()==0&&!o.deathFrame){o.deathFrame=counters.frames;o.deathHP=int(raw32(game.workingSlot(),0x7c));o.deathPlayerState=scene->player().state;}
                if(o.requestFrame==counters.frames){o.requestHP=scene->playerHP();o.requestPlayerState=scene->player().state;o.requestWorldState=scene->gameState();o.requestPaused=scene->isPaused();o.requestStateTime=scene->gameStateChangeTime();o.requestRenderTime=game.renderTime();o.requestWorldTime=scene->clock().worldTime();}
            }else if(o.confirmFrame&&!o.baseFrame&&state.screen==ui::ScreenId::base){o.baseFrame=counters.frames;o.baseHP=int(raw32(game.workingSlot(),0x7c));o.baseId=state.baseId;o.savesAtBase=observed.saves;for(unsigned j=0;j<3;++j)o.savedBotHP[j]=raw32(game.workingSlot(),0x64c+j*4);
                DarwinWorldRandom decodeRandom(1);auto saved=SaveData::decode(value.adapter.saved,decodeRandom,false);progression::SlotView stored(saved.slot(0));
                o.savedPayloadValid=stored.hp()==o.baseHP&&stored.string(0x2d,16)=="Base002"&&stored.string(0x55,16)=="Base002";for(unsigned j=0;j<3;++j)o.savedPayloadValid=o.savedPayloadValid&&stored.u32(0x64c+j*4)==0;
            }
        }
        if(const auto* battle=game.battle()){observed.playerHP=battle->playerHP();if(observed.playerHP>0)++observed.aliveTicks;const auto& m=battle->metrics();observed.emissions=value.priorEmissions+m.emissions;observed.playerEmissions=value.priorPlayerEmissions+m.playerEmissions;observed.damage=value.priorDamage+m.damageCalls;observed.kills=value.priorKills+m.kills;observed.strikeRequests=m.airStrikeRequests;observed.strikeProjectiles=m.airStrikeProjectiles;observed.strikeDamageCalls=m.airStrikeDamageCalls;observed.strikeEnemyHPRemoved=m.airStrikeEnemyHPRemoved;observed.strikeTicks=m.airStrikeTicks;observed.activeStrikes=battle->activeAirStrikes();
            unsigned k=0;for(const auto kind:{MachineKind::vehicle,MachineKind::turret,MachineKind::aircraft}){observed.enemySpawns[k]=m.enemySpawnsByKind.at(kind);observed.enemyEmissions[k]=m.enemyEmissionsByKind.at(kind);observed.enemyDamage[k]=m.enemyDamageCallsByKind.at(kind);observed.enemyKills[k]=m.enemyKillsByKind.at(kind);observed.enemyRemovals[k]=m.enemyRemovalsByKind.at(kind);++k;}
            if(counters.frames==1){observed.firstSpawns=observed.enemySpawns;for(const auto& enemy:battle->enemies())if(enemy.boss)++observed.bossSpawns;}
            observed.dashStarts=m.dashStarts;observed.dashMoveTicks=m.dashMoveTicks;observed.dashParticles=m.dashParticleAllocations;observed.dashRandomCalls=m.dashRandomCalls;
            if(descriptions[i].role==ScenarioRole::dash&&observed.dashInputFrame){const auto p=battle->player().position;const float dx=p.x-observed.dashStartPosition.x,dy=p.y-observed.dashStartPosition.y;observed.dashMaxDisplacement=std::max(observed.dashMaxDisplacement,std::sqrt(dx*dx+dy*dy));}
            if(descriptions[i].role==ScenarioRole::botAttack||descriptions[i].role==ScenarioRole::botPicker){observed.botThreatEmissions=0;std::set<std::string> parts;for(const auto& [model,weapons]:botThreats){(void)model;parts.insert(weapons.begin(),weapons.end());}for(const auto& part:parts)observed.botThreatEmissions+=m.enemyWeaponEmissions.at(part);for(const auto& bot:battle->bots()){auto& o=observed.bots.at(bot.slot);o.hp=bot.hp;o.maximumHP=bot.maximumHP;o.ticks=bot.metrics.ticks;o.emissions=bot.metrics.emissions;o.damageCalls=bot.metrics.damageCalls;o.regeneratedHP=bot.metrics.regeneratedHP;o.enemyHPRemoved=bot.enemyHPRemoved;o.hpRemoved=bot.hpRemoved;o.itemCaptures=bot.itemCaptures;if(bot.rotorAngle!=o.lastRotor)++o.rotorFrames;o.lastRotor=bot.rotorAngle;o.removed=bot.removed;o.lastObservedFrame=counters.frames;}}
            if(descriptions[i].role==ScenarioRole::help){auto& o=observed.help;if(battle->isPaused()){if(battle->clock().worldTime()!=o.lastWorldTime)throw std::logic_error("Paused Help world advanced");++o.pausedFrames;}else if(battle->clock().worldTime()>o.lastWorldTime)++o.runningFrames;o.lastWorldTime=battle->clock().worldTime();}
            for(const auto& [part,count]:m.playerWeaponEmissions)if(count)value.emitted.insert(part);
            observed.emittedWeaponParts.assign(value.emitted.begin(),value.emitted.end());
            if(descriptions[i].role==ScenarioRole::botPicker)for(auto& swap:observed.botWeaponSwaps)if(swap.equipFrame&&!swap.firstEmissionFrame&&!swap.part.empty()&&m.playerWeaponEmissions.at(swap.part)>0)swap.firstEmissionFrame=counters.frames;}
    }
    void restart(){
        auto& value=*cases[restartIndex];auto& game=*value.game;
        if(!game.save(SaveReason::saveAndExit))return;
        const Bytes originalSlot(game.workingSlot().begin(),game.workingSlot().end());
        auto restored=SaveData::decode(value.adapter.saved,value.random,false);
        const bool equal=std::equal(originalSlot.begin(),originalSlot.end(),restored.slot(0).begin());
        if(const auto* scene=game.battle()){const auto& m=scene->metrics();value.priorEmissions+=m.emissions;value.priorPlayerEmissions+=m.playerEmissions;value.priorDamage+=m.damageCalls;value.priorKills+=m.kills;}
        value.game=std::make_unique<GameSession>(resources,value.adapter,value.random,std::move(restored));value.game->setDisplay(display);
        const bool selected=value.game->selectSlot(0);value.game->enterBase();const bool field=value.game->enterField("Stage01_01",0);
        observations[restartIndex].restarted=equal&&selected&&field;
        value.lastWeaponCounts=value.game->battle()->metrics().playerWeaponEmissions;
        host.sessionRestarted(unsigned(restartIndex),*value.game);
    }
    Frame compose(Size viewport){
        if(!std::isfinite(viewport.width)||!std::isfinite(viewport.height)||viewport.width<=0||viewport.height<=0)throw std::invalid_argument("Invalid concurrent viewport");
        if(sourceFrameNumber!=counters.frames){for(std::size_t i=0;i<cases.size();++i){sourceFrames[i]=cases[i]->game->drawWorld();host.appendScenarioUI(unsigned(i),*cases[i]->game,sourceFrames[i]);cases[i]->game->appendOverlays(sourceFrames[i]);if(descriptions[i].role==ScenarioRole::help){for(const auto& image:sourceFrames[i].sprites){if(image.image.starts_with("HelpScr")){++observations[i].help.drawFrames;observations[i].help.lastImage=image.image;}if(image.image=="HelpHelp.png")++observations[i].help.guideDrawFrames;}}++observations[i].draws;}
            sourceFrameNumber=counters.frames;++counters.drawFrames;}
        Frame out;out.width=viewport.width;out.height=viewport.height;
        const unsigned columns=unsigned(std::ceil(std::sqrt(float(cases.size())*viewport.width/viewport.height*(1024.f/768.f))));
        const unsigned rows=unsigned((cases.size()+columns-1)/columns);const float paneWidth=viewport.width/float(columns),paneHeight=viewport.height/float(rows);
        for(std::size_t i=0;i<sourceFrames.size();++i){const auto& source=sourceFrames[i];const float scale=std::min(std::max(1.f,paneWidth-4.f)/source.width,std::max(1.f,paneHeight-4.f)/source.height);
            const float x=float(i%columns)*paneWidth+(paneWidth-source.width*scale)*.5f,y=viewport.height-float(i/columns+1)*paneHeight+(paneHeight-source.height*scale)*.5f;
            const ClipRect pane{x,y,source.width*scale,source.height*scale};
            const auto point=[&](Vec2 p){return Vec2{x+p.x*scale,y+p.y*scale};};
            for(const auto& sourceSprite:source.sprites){auto sprite=sourceSprite;sprite.center=point(sprite.center);sprite.size={sprite.size.width*scale,sprite.size.height*scale};
                for(auto& v:sprite.strip)v.position=point(v.position);
                if(sprite.atlasBatch){auto batch=std::make_shared<AtlasBatch>(*sprite.atlasBatch);for(auto& quad:batch->quads)for(auto& v:quad.vertices)v.position=point(v.position);sprite.atlasBatch=std::move(batch);}
                if(sprite.clip){const auto old=*sprite.clip;sprite.clip=intersect(pane,{x+old.x*scale,y+old.y*scale,old.width*scale,old.height*scale});}else sprite.clip=pane;
                out.sprites.push_back(std::move(sprite));}
        }return out;
    }
};
ConcurrentValidation::ConcurrentValidation(ResourceAccess& r,ValidationHost& h):impl_(std::make_unique<Impl>(r,h)){}
ConcurrentValidation::~ConcurrentValidation()=default;
void ConcurrentValidation::tick(){auto& p=*impl_;if(!p.active)return;p.resources.warm=true;++p.counters.frames;p.schedule();
    FrameActivity activity;activity.frame=p.counters.frames;activity.buttons=std::move(p.pendingButtons);p.pendingButtons.clear();activity.saveRestart=p.counters.frames==600&&p.observations[p.restartIndex].restarted;activity.aidKitUses=p.pendingAidKitUses;p.pendingAidKitUses=0;activity.airStrikeUses=p.pendingAirStrikeUses;p.pendingAirStrikeUses=0;activity.botRepairs=p.pendingBotRepairs;p.pendingBotRepairs=0;activity.helpInputRequests=p.pendingHelpInputs;p.pendingHelpInputs=0;std::set<std::string> frameWeapons;
    for(std::size_t i=0;i<p.cases.size();++i){auto& c=*p.cases[i];auto& observed=p.observations[i];
        if(p.display.enabled&&p.descriptions[i].role==ScenarioRole::combat&&i!=38&&(p.counters.frames-1)%31==0)if(auto* scene=c.game->battle()){
            bool fired=false,missing=false;for(const auto& w:scene->playerWeapons()){const bool emitted=scene->metrics().playerWeaponEmissions.at(w.part)>0;fired|=emitted;missing|=!emitted;}
            if(fired&&missing){float closest=std::numeric_limits<float>::max();std::uint64_t target=0;const auto player=scene->player().position;for(const auto& e:scene->enemies())if(e.hp>0&&!e.corpse){const auto point=scene->camera().toScreen(e.position);if(point.x<16||point.x>scene->camera().viewport.width-16||point.y<Impl::fieldFloor(scene->camera())||point.y>scene->camera().viewport.height-16)continue;const float dx=e.position.x-player.x,dy=e.position.y-player.y,d=dx*dx+dy*dy;if(d<closest){closest=d;target=e.id;}}if(target){scene->selectEnemy(target);scene->selectEnemy(target);}}
        }
        if(i==38)if(auto* scene=c.game->battle()){
            // Real target selection gives each side a firing opportunity:
            // farther visible Tank while Missile309 has never emitted, then
            // nearest visible Tank while Gun106 has never emitted. Neither
            // weapon's controller/reload/HP is changed.
            const bool missileFired=scene->metrics().playerWeaponEmissions.at("P_Wpn_Msl_309")>0;
            const bool gunFired=scene->metrics().playerWeaponEmissions.at("P_Wpn_Gun_106")>0;
            if(!scene->selectedEnemy()||(missileFired&&!gunFired)||(p.display.enabled&&!missileFired)){
                float score=missileFired?std::numeric_limits<float>::max():-1;std::uint64_t target=0;const auto player=scene->player().position;
                for(const auto& enemy:scene->enemies())if(enemy.hp>0&&!enemy.corpse){const auto point=scene->camera().toScreen(enemy.position);if(point.x<0||point.x>scene->camera().viewport.width||point.y<100||point.y>scene->camera().viewport.height)continue;const float dx=enemy.position.x-player.x,dy=enemy.position.y-player.y,d=dx*dx+dy*dy;if(p.display.enabled&&d>(missileFired?600.f*600.f:740.f*740.f))continue;if(missileFired?d<score:d>score){score=d;target=enemy.id;}}
                if(target&&target!=scene->selectedEnemy())scene->selectEnemy(target);
            }
        }
        if(auto* scene=c.game->battle()){const auto& d=p.descriptions[i];const bool representative=d.role==ScenarioRole::worldAudit&&(d.map=="Stage01_02"||d.map=="Stage02_07"||d.map=="Stage03_04");if(d.role!=ScenarioRole::airStrike&&d.role!=ScenarioRole::dash&&d.role!=ScenarioRole::gameOver&&!representative&&!scene->selectedEnemy())for(const auto& enemy:scene->enemies())if(enemy.hp>0&&!enemy.corpse){const auto point=scene->camera().toScreen(enemy.position);if(point.x<0||point.x>scene->camera().viewport.width||point.y<100||point.y>scene->camera().viewport.height)continue;const bool botRole=d.role==ScenarioRole::botAttack||d.role==ScenarioRole::botPicker;const bool incoming=std::any_of(observed.bots.begin(),observed.bots.end(),[](const auto& b){return b.hpRemoved>0;});if(botRole&&!incoming&&p.botThreats.contains(enemy.name))continue;scene->selectEnemy(enemy.id);break;}}
        const bool movement=i==p.movementIndex;const bool botCase=p.descriptions[i].role==ScenarioRole::botAttack||p.descriptions[i].role==ScenarioRole::botPicker;const auto beforeBots=observed.bots;
        if(movement)p.guideMovement();
        const auto beforeEmissions=observed.playerEmissions,beforeDamage=observed.damage,beforeKills=observed.kills;
        const Vec2 beforePosition=(movement||botCase)?c.game->battle()->player().position:Vec2{};
        const int beforeHP=movement?c.game->battle()->playerHP():0,beforeCredit=(movement||botCase)?c.game->state().credits:0,beforeInventory=(movement||botCase)?p.inventoryCount(*c.game):0;
        c.game->tick([&p,&c,i]{p.host.tickScenarioUI(unsigned(i),*c.game,c.random);});if(observed.ticks==0)observed.firstFrame=p.counters.frames;observed.lastFrame=p.counters.frames;++observed.ticks;++activity.tickedScenarios;p.observe(i);
        if(p.descriptions[i].role==ScenarioRole::gameOver){const auto& o=observed.gameOver;activity.gameOverEvents+=(o.deathFrame==p.counters.frames)+(o.requestFrame==p.counters.frames)+(o.confirmFrame==p.counters.frames)+(o.baseFrame==p.counters.frames);}
        if(p.descriptions[i].role==ScenarioRole::rankingUnavailable&&observed.ranking.frame==p.counters.frames)activity.rankingNotices=observed.ranking.notices;
        if(p.descriptions[i].role==ScenarioRole::completedMissions){const auto& o=observed.completedMissions;activity.completedMissionEvents+=(o.popupFrames[0]==p.counters.frames)+(o.popupFrames[1]==p.counters.frames)+(o.okFrames[1]==p.counters.frames)+(o.baseFrame==p.counters.frames);}
        if(movement){const auto* scene=c.game->battle();const auto position=scene->player().position;
            const float dx=position.x-beforePosition.x,dy=position.y-beforePosition.y,step=std::sqrt(dx*dx+dy*dy);
            observed.distanceMoved+=step;observed.lastPosition=position;
            const float sx=position.x-observed.startPosition.x,sy=position.y-observed.startPosition.y;observed.maxDisplacement=std::max(observed.maxDisplacement,std::sqrt(sx*sx+sy*sy));
            activity.movementLootMoving=step>.0001f;if(activity.movementLootMoving)++observed.movementFrames;if(scene->player().blocked)++observed.blockedFrames;
            const int injury=std::max(0,beforeHP-scene->playerHP());if(injury){++observed.injuryFrames;observed.injuryPoints+=unsigned(injury);activity.movementLootInjury=unsigned(injury);}
            const auto drops=c.game->fieldItems();observed.maxObservedDrops=std::max(observed.maxObservedDrops,std::uint64_t(drops.size()));
            const int creditDelta=observed.credit-beforeCredit;
            for(const auto& item:drops)if(item.removed){++observed.pickupEvents;++activity.movementLootPickups;
                if(item.type==0){++observed.coinPickups;observed.pickupCreditGain+=item.value;}else ++observed.itemPickups;
                observed.pickups.push_back({p.counters.frames,item.code,item.type,item.value,item.pickupPlayer,item.position,creditDelta});
            }
            observed.inventoryGain+=std::max(0,p.inventoryCount(*c.game)-beforeInventory);activity.movementLootCreditGain=creditDelta;
            activity.movementLootEmissions=observed.playerEmissions-beforeEmissions;
        }
        if(botCase){
            const auto position=c.game->battle()->player().position;const float dx=position.x-beforePosition.x,dy=position.y-beforePosition.y,step=std::sqrt(dx*dx+dy*dy);observed.distanceMoved+=step;observed.lastPosition=position;if(step>.0001f)++observed.movementFrames;const float sx=position.x-observed.startPosition.x,sy=position.y-observed.startPosition.y;observed.maxDisplacement=std::max(observed.maxDisplacement,std::sqrt(sx*sx+sy*sy));
            for(unsigned j=0;j<3;++j){const auto& now=observed.bots[j];const auto& old=beforeBots[j];if(now.emissions<old.emissions||now.itemCaptures<old.itemCaptures||now.hpRemoved<old.hpRemoved)throw std::logic_error("BOT validation actor counters reset during a live condition");activity.botEmissions+=now.emissions-old.emissions;activity.botItemContacts+=now.itemCaptures-old.itemCaptures;activity.botHPRemoved+=now.hpRemoved-old.hpRemoved;}
            const auto drops=c.game->fieldItems();for(const auto& item:drops)if(item.removed){++observed.pickupEvents;++activity.botRewardArrivals;if(item.type==0){++observed.coinPickups;observed.pickupCreditGain+=item.value;}else ++observed.itemPickups;observed.pickups.push_back({p.counters.frames,item.code,item.type,item.value,item.pickupPlayer,item.position,observed.credit-beforeCredit});}
            observed.inventoryGain+=std::max(0,p.inventoryCount(*c.game)-beforeInventory);
        }
        if(observed.playerEmissions>beforeEmissions){++activity.emittingScenarios;if(i<p.counters.combatScenarios)++activity.combatEmittingScenarios;activity.emissions+=observed.playerEmissions-beforeEmissions;if(!observed.firstEmissionFrame)observed.firstEmissionFrame=p.counters.frames;observed.lastEmissionFrame=p.counters.frames;}
        activity.damage+=observed.damage-beforeDamage;activity.kills+=observed.kills-beforeKills;if(i<p.counters.combatScenarios)activity.combatDamage+=observed.damage-beforeDamage;
        if(const auto* scene=c.game->battle()){activity.activeProjectiles+=scene->activeProjectiles();if(i<p.counters.combatScenarios)activity.combatActiveProjectiles+=scene->activeProjectiles();}
        if(const auto* scene=c.game->battle())for(const auto& [part,count]:scene->metrics().playerWeaponEmissions){auto [old,inserted]=c.lastWeaponCounts.try_emplace(part,0);(void)inserted;const auto previousCount=old->second;if(count>previousCount){frameWeapons.insert(part);observed.weaponEmissions[part]+=count-previousCount;}old->second=count;}
    }
    const bool otherCombat=activity.combatEmittingScenarios||activity.combatActiveProjectiles||activity.combatDamage;
    if(otherCombat){if(activity.completedMissionEvents)++p.counters.completedMissionFramesWithCombatActivity;if(activity.gameOverEvents)++p.counters.gameOverFramesWithCombatActivity;if(activity.rankingNotices)++p.counters.rankingFramesWithCombatActivity;if(activity.botEmissions||activity.botItemContacts||activity.botRewardArrivals||activity.botHPRemoved||activity.botRepairs)++p.counters.botFramesWithCombatActivity;if(activity.helpInputRequests||std::any_of(activity.buttons.begin(),activity.buttons.end(),[&p](const auto& b){return p.descriptions.at(b.scenario).role==ScenarioRole::help;}))++p.counters.helpFramesWithCombatActivity;if(activity.airStrikeUses)++p.counters.airStrikeFramesWithCombatActivity;if(activity.aidKitUses)++p.counters.aidKitFramesWithCombatActivity;if(activity.movementLootMoving)++p.counters.movementFramesWithCombatActivity;if(activity.movementLootPickups)++p.counters.pickupFramesWithCombatActivity;if(activity.movementLootInjury)++p.counters.injuryFramesWithCombatActivity;if(activity.movementLootMoving&&activity.movementLootEmissions)++p.counters.movementEmissionFramesWithCombatActivity;}
    if(!activity.buttons.empty()){++p.counters.menuActionFrames;if(activity.combatEmittingScenarios||activity.combatActiveProjectiles||activity.combatDamage)++p.counters.menuFramesWithCombatActivity;}
    std::set<WeaponKind> kinds;for(const auto& part:frameWeapons)kinds.insert(p.catalog.weapon(part).kind);activity.emittedWeaponKinds.assign(kinds.begin(),kinds.end());
    activity.emittedWeaponParts.assign(frameWeapons.begin(),frameWeapons.end());p.counters.maxConcurrentEmittingScenarios=std::max(p.counters.maxConcurrentEmittingScenarios,activity.emittingScenarios);p.counters.maxConcurrentWeaponDefinitions=std::max(p.counters.maxConcurrentWeaponDefinitions,std::uint64_t(frameWeapons.size()));p.counters.maxConcurrentWeaponKinds=std::max(p.counters.maxConcurrentWeaponKinds,std::uint64_t(kinds.size()));if(frameWeapons.size()>=5)++p.counters.framesWithFiveWeaponDefinitions;if(kinds.size()>=5)++p.counters.framesWithFiveWeaponKinds;p.activity.push_back(std::move(activity));
}
void ConcurrentValidation::setActive(bool active){impl_->active=active;for(auto& value:impl_->cases)value->game->setActive(active);}
Frame ConcurrentValidation::drawFrame(Size viewport){return impl_->compose(viewport);}
Frame ConcurrentValidation::scenarioFrame(unsigned i)const{if(impl_->sourceFrameNumber!=impl_->counters.frames)throw std::logic_error("Capture a scenario only after its common frame draw");return impl_->sourceFrames.at(i);}
GameSession& ConcurrentValidation::session(unsigned i){return *impl_->cases.at(i)->game;}
ResourceAccess& ConcurrentValidation::cachedResources(){return impl_->resources;}
std::size_t ConcurrentValidation::scenarioCount()const{return impl_->cases.size();}
const std::vector<ScenarioDescription>& ConcurrentValidation::scenarios()const{return impl_->descriptions;}
const std::vector<ScenarioObservation>& ConcurrentValidation::observations()const{return impl_->observations;}
const std::vector<FrameActivity>& ConcurrentValidation::frameActivity()const{return impl_->activity;}
ConcurrentMetrics ConcurrentValidation::metrics()const{const auto& p=*impl_;auto result=p.counters;result.activeScenarios=p.active?p.cases.size():0;result.underlyingReads=p.resources.reads;result.underlyingExists=p.resources.probes;result.warmReadMisses=p.resources.readMisses;result.warmExistsMisses=p.resources.probeMisses;result.warmReadNames=p.resources.warmReadNames;result.warmExistsNames=p.resources.warmExistsNames;
    std::set<std::string> activeWeapons;std::uint64_t first=0,last=std::numeric_limits<std::uint64_t>::max();for(std::size_t i=0;i<p.observations.size();++i){const auto& o=p.observations[i];first=std::max(first,o.firstFrame);last=std::min(last,o.lastFrame);for(const auto& part:o.emittedWeaponParts)activeWeapons.insert(part);}result.commonFrameInterval=first>0&&first<=last;result.activeWeaponDefinitions=activeWeapons.size();std::set<WeaponKind> kinds;for(const auto& part:activeWeapons)kinds.insert(p.catalog.weapon(part).kind);result.activeWeaponKinds=kinds.size();return result;}
std::vector<std::string> ConcurrentValidation::supported()const{return {"110 original PtWpn items /55 independently saved real battles","same-frame session ticks and shared sprite-frame compositor","original target / weapon / damage / death / mission / EXP consumers","scheduled real shop purchase and insufficient-credit refusal","original save encode/decode and restart without condition reinjection","one source-frame consumption per tick, including viewport recomposition","real ground-pointer movement and actual enemy-drop pickup on the same frame clock","six original AidKit consumers and three real AirStrike/bullet consumers","38 original field entries and common vehicle / turret / aircraft consumers","original ground double tap / SP / dash movement / shared particle consumer","six original BOT registrations / real attack and pickup / damage and repair observations","four original KR Help conditions / actual menu and overlay-end input / pause and resume","real enemy damage / GameOver token / original doors / Base002 save","Command5009 / localized ranking unavailable notice / no save"};}
std::vector<std::string> ConcurrentValidation::missingFeatures()const{return {"all23 Player bottom equipment movement checks","all BOT combat outcomes and death after owner removal attribution","all mission types and all60 mission completion","all stages and final campaign completion","original port-app visual/input/audio equivalence","OS interruption / audio-route transition"};}
}

namespace hm2::validation {void ConcurrentValidation::setDisplay(DisplayConfiguration display){impl_->display=display;for(auto& value:impl_->cases)value->game->setDisplay(display);impl_->sourceFrameNumber=~std::uint64_t(0);}}

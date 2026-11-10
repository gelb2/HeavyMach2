#include "Core/Validation/ConcurrentValidation.hpp"
#include "Core/Progression/Progression.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
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
SaveData startSave(std::span<const WeaponStart> weapons,std::uint32_t time,bool shop=false){
    SaveData save;save.initializeSlot(0,"Validation","us",time);progression::SlotView slot(save.slot(0));
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
    SaveData saved;saved.initializeSlot(0,"WorldValidation","us",time);progression::SlotView slot(saved.slot(0));slot.level(99);slot.experience(progression::Experience::bounds(99).previous);
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
        void soundID(unsigned id,Vec2 p)override{++owner.observations[index].sounds;if(id==13)++owner.observations[index].repairSounds;if(id==14)++owner.observations[index].dashSounds;owner.host.soundID(index,id,p);}
        void alert(std::string_view key)override{alerts.emplace_back(key);}
        void requestName(unsigned)override{}void requestDelete(unsigned)override{}
        bool writeSave(std::span<const std::uint8_t> data,SaveReason reason)override{
            if(!owner.host.writeSave(index,data,reason))return false;
            saved.assign(data.begin(),data.end());++owner.observations[index].saves;return true;
        }
        std::uint32_t absoluteTimeLow()override{return owner.host.absoluteTimeLow(index);}
        std::uint32_t unixTime()override{return owner.host.unixTime();}
    };
    struct Case {
        Adapter adapter;DarwinWorldRandom random;std::unique_ptr<GameSession> game;
        std::uint64_t priorEmissions=0,priorPlayerEmissions=0,priorDamage=0,priorKills=0;
        int initialLevel=0;std::set<std::string> emitted;
        std::uint64_t worldTarget=0;bool worldEnemyFired=false;
        std::map<std::string,std::uint64_t,std::less<>> lastWeaponCounts;
        Case(Impl& owner,unsigned index):adapter(owner,index),random(index+1){}
    };
    CachedResources resources;ValidationHost& host;Weapons catalog;
    std::vector<ScenarioDescription> descriptions;std::vector<ScenarioObservation> observations;
    std::vector<std::unique_ptr<Case>> cases;ConcurrentMetrics counters;
    bool active=true;std::uint64_t sourceFrameNumber=std::numeric_limits<std::uint64_t>::max();
    std::vector<Frame> sourceFrames;std::vector<FrameActivity> activity;std::vector<ButtonActivity> pendingButtons;std::size_t shopIndex=0,missionIndex=0,restartIndex=0,movementIndex=0;
    std::vector<std::size_t> aidKitIndices,airStrikeIndices,worldIndices;std::size_t dashIndex=0;std::uint64_t pendingAidKitUses=0,pendingAirStrikeUses=0;
    int shopCreditBefore=0,shopItemsBefore=0,shopCreditAfter=0,shopItemsAfter=0;unsigned repairButton=0;
    static int repairCount(GameSession& game){int count=0;for(const auto& item:game.state().inventory)if(item.dataId=="01001")count+=item.count;return count;}
    explicit Impl(ResourceAccess& source,ValidationHost& sink):resources(source),host(sink),catalog(Weapons::parse(DataTable::parse(resources.read("WeaponInfo.tble")))){
        const auto items=DataTable::parse(resources.read("ItemInfo.tble"));std::vector<WeaponStart> weapons;
        for(std::size_t row=0;row<items.rowCount();++row)if(items.value(row,"Type")=="PtWpn")weapons.push_back({std::string(items.value(row,"Code").value_or("")),std::string(items.value(row,"strParam").value_or("")),items.integer(row,"Lv")});
        if(weapons.size()!=110)throw std::runtime_error("Expected original HD110 player-weapon items; no enemy weapons may be substituted");
        descriptions.reserve(106);observations.resize(106);cases.reserve(106);
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
        add({"real ground double tap / SP / dash",ScenarioRole::dash,{},"Stage01_01"},std::move(dash),true,false);
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
        const Vec2 screen{std::clamp(projected.x,16.f,scene->camera().viewport.width-16),std::clamp(projected.y,120.f,scene->camera().viewport.height-16)};
        const Vec2 requested=scene->camera().toWorld(screen);constexpr std::uintptr_t pointer=0x4c4f4f54;
        const bool begin=game.inputWorld(PointerPhase::begin,screen,pointer);game.inputWorld(PointerPhase::end,screen,pointer);
        ++observed.navigationRequests;if(!begin)++observed.navigationRejected;
        observed.navigation.push_back({counters.frames,player,requested,screen,nearest?nearest->code:"",begin,scene->player().blocked});
    }
    void press(std::size_t index,unsigned id,int data=0){++observations[index].buttonRequests;const bool delivered=host.pressButton(unsigned(index),id,data);if(!delivered)++observations[index].buttonFailures;pendingButtons.push_back({unsigned(index),id,data,delivered});}
    void worldQuick(std::size_t index,unsigned slot,Vec2 target){
        auto& game=*cases[index]->game;press(index,6200+slot);const auto screen=game.battle()->camera().toScreen(target);
        constexpr std::uintptr_t pointer=0x574f524c;if(game.inputWorld(PointerPhase::begin,screen,pointer))++observations[index].worldQuickUses;
        game.inputWorld(PointerPhase::end,screen,pointer);
    }
    void guideWorld(std::size_t index){
        auto& c=*cases[index];auto& game=*c.game;auto* scene=game.battle();if(!scene||scene->playerHP()<1)return;
        if(game.state().hpRate<.65f)worldQuick(index,0,scene->player().position);
        const auto& map=descriptions[index].map;const bool aircraft=map=="Stage02_07"||map=="Stage03_04",turret=map=="Stage01_02";
        if(!aircraft&&!turret)return;const auto kind=aircraft?MachineKind::aircraft:MachineKind::turret;
        if(counters.frames==2){
            for(const auto& e:scene->enemies())if(e.kind==kind&&e.hp>0){c.worldTarget=e.id;
                if(aircraft)worldQuick(index,1,e.position);
                else {const auto p=scene->camera().toScreen({e.position.x+65,e.position.y});const Vec2 point{std::clamp(p.x,16.f,752.f),std::clamp(p.y,120.f,1008.f)};
                    constexpr std::uintptr_t pointer=0x54555252;game.inputWorld(PointerPhase::begin,point,pointer);game.inputWorld(PointerPhase::end,point,pointer);}break;}
        }
        if(!c.worldEnemyFired&&scene->metrics().enemyEmissionsByKind.at(kind)>0){c.worldEnemyFired=true;scene->selectEnemy(c.worldTarget);scene->selectEnemy(c.worldTarget);}
    }
    void guideDash(){
        if(counters.frames!=100&&counters.frames!=110)return;auto& game=*cases[dashIndex]->game;auto* scene=game.battle();
        if(!scene||scene->playerHP()<1)throw std::runtime_error("Live original Dash start condition lost");
        constexpr Vec2 offsets[]={{100,0},{100,60},{-100,0},{0,100},{60,100},{-80,80}};Vec2 target;bool found=false;
        for(const auto d:offsets){const auto p=scene->player().position;const Vec2 world{p.x+d.x,p.y+d.y};const auto screen=scene->camera().toScreen(world);
            if(screen.x<16||screen.x>752||screen.y<120||screen.y>1008)continue;bool clear=true;
            for(const auto& e:scene->enemies())if(e.hp>0){const float dx=e.position.x-world.x,dy=e.position.y-world.y;if(dx*dx+dy*dy<10000)clear=false;}
            if(clear){target=screen;found=true;break;}}
        if(!found)throw std::runtime_error("Original Dash ground destination unavailable");auto& o=observations[dashIndex];
        if(counters.frames==110){o.dashSPBefore=scene->dashState().special;o.dashStartPosition=scene->player().position;}
        constexpr std::uintptr_t pointer=0x44415348;const bool accepted=game.inputWorld(PointerPhase::begin,target,pointer);game.inputWorld(PointerPhase::end,target,pointer);
        if(counters.frames==110){o.dashSPAfter=scene->dashState().special;o.dashInputAccepted=accepted;o.dashInputFrame=counters.frames;}
    }
    void schedule(){
        for(const auto index:worldIndices)guideWorld(index);guideDash();
        auto& shop=*cases[shopIndex]->game;const auto frame=counters.frames>180?counters.frames-180:0;
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
            o.quickTapAccepted=game.inputWorld(PointerPhase::begin,screen,pointer);game.inputWorld(PointerPhase::end,screen,pointer);
            o.quickHPAfter=scene->playerHP();o.quickSlotHPAfter=int(raw32(game.workingSlot(),0x7c));o.quickCountAfter=count();o.quickSelectionAfter=game.state().selectedQuick;
            o.quickRepairSounds=o.repairSounds-sounds;o.quickParticleAllocations=scene->metrics().particleAllocations-particles;o.quickUseFrame=counters.frames;
            if(o.quickCountAfter==o.quickCountBefore-1&&o.quickHPAfter>o.quickHPBefore)++pendingAidKitUses;
        }
        if(counters.frames==260)for(const auto index:airStrikeIndices)press(index,6201);
        if(counters.frames==261)for(const auto index:airStrikeIndices){
            auto& game=*cases[index]->game;auto& o=observations[index];auto* scene=game.battle();
            const auto count=[&](){int result=0;for(const auto& item:game.state().inventory)if(item.dataId==o.strikeItemCode)result+=item.count;return result;};
            o.strikeCountBefore=count();Vec2 target{scene->player().position.x+100,scene->player().position.y+100};float best=std::numeric_limits<float>::max();
            for(const auto& enemy:scene->enemies())if(enemy.hp>0&&!enemy.corpse){const auto point=scene->camera().toScreen(enemy.position);if(point.x<32||point.x>736||point.y<180||point.y>980)continue;const float dx=enemy.position.x-scene->player().position.x,dy=enemy.position.y-scene->player().position.y,d=dx*dx+dy*dy;if(d<best){best=d;target=enemy.position;o.strikeTargetVisibleEnemy=true;}}
            o.strikeTarget=target;const auto screen=scene->camera().toScreen(target);constexpr std::uintptr_t pointer=0x41495253;
            o.strikeTapAccepted=game.inputWorld(PointerPhase::begin,screen,pointer);game.inputWorld(PointerPhase::end,screen,pointer);o.strikeCountAfter=count();o.strikeUseFrame=counters.frames;
            if(o.strikeTapAccepted&&o.strikeCountAfter==o.strikeCountBefore-1)++pendingAirStrikeUses;
        }
        if(counters.frames==600)restart();
    }
    void observe(std::size_t i){
        auto& value=*cases[i];auto& game=*value.game;auto& observed=observations[i];const auto& state=game.state();
        observed.level=state.level;observed.credit=state.credits;observed.experience=raw32(game.workingSlot(),0x70);
        for(const auto& mission:state.acceptedMissions)if(mission.dataId=="M001010")observed.missionComplete=mission.complete;
        if(const auto* battle=game.battle()){observed.playerHP=battle->playerHP();if(observed.playerHP>0)++observed.aliveTicks;const auto& m=battle->metrics();observed.emissions=value.priorEmissions+m.emissions;observed.playerEmissions=value.priorPlayerEmissions+m.playerEmissions;observed.damage=value.priorDamage+m.damageCalls;observed.kills=value.priorKills+m.kills;observed.strikeRequests=m.airStrikeRequests;observed.strikeProjectiles=m.airStrikeProjectiles;observed.strikeDamageCalls=m.airStrikeDamageCalls;observed.strikeEnemyHPRemoved=m.airStrikeEnemyHPRemoved;observed.strikeTicks=m.airStrikeTicks;observed.activeStrikes=battle->activeAirStrikes();
            unsigned k=0;for(const auto kind:{MachineKind::vehicle,MachineKind::turret,MachineKind::aircraft}){observed.enemySpawns[k]=m.enemySpawnsByKind.at(kind);observed.enemyEmissions[k]=m.enemyEmissionsByKind.at(kind);observed.enemyDamage[k]=m.enemyDamageCallsByKind.at(kind);observed.enemyKills[k]=m.enemyKillsByKind.at(kind);observed.enemyRemovals[k]=m.enemyRemovalsByKind.at(kind);++k;}
            if(counters.frames==1){observed.firstSpawns=observed.enemySpawns;for(const auto& enemy:battle->enemies())if(enemy.boss)++observed.bossSpawns;}
            observed.dashStarts=m.dashStarts;observed.dashMoveTicks=m.dashMoveTicks;observed.dashParticles=m.dashParticleAllocations;observed.dashRandomCalls=m.dashRandomCalls;
            if(descriptions[i].role==ScenarioRole::dash&&observed.dashInputFrame){const auto p=battle->player().position;const float dx=p.x-observed.dashStartPosition.x,dy=p.y-observed.dashStartPosition.y;observed.dashMaxDisplacement=std::max(observed.dashMaxDisplacement,std::sqrt(dx*dx+dy*dy));}
            for(const auto& [part,count]:m.playerWeaponEmissions)if(count)value.emitted.insert(part);
            observed.emittedWeaponParts.assign(value.emitted.begin(),value.emitted.end());}
    }
    void restart(){
        auto& value=*cases[restartIndex];auto& game=*value.game;
        if(!game.save(SaveReason::saveAndExit))return;
        const Bytes originalSlot(game.workingSlot().begin(),game.workingSlot().end());
        auto restored=SaveData::decode(value.adapter.saved,value.random,false);
        const bool equal=std::equal(originalSlot.begin(),originalSlot.end(),restored.slot(0).begin());
        if(const auto* scene=game.battle()){const auto& m=scene->metrics();value.priorEmissions+=m.emissions;value.priorPlayerEmissions+=m.playerEmissions;value.priorDamage+=m.damageCalls;value.priorKills+=m.kills;}
        value.game=std::make_unique<GameSession>(resources,value.adapter,value.random,std::move(restored));
        const bool selected=value.game->selectSlot(0);value.game->enterBase();const bool field=value.game->enterField("Stage01_01",0);
        observations[restartIndex].restarted=equal&&selected&&field;
        value.lastWeaponCounts=value.game->battle()->metrics().playerWeaponEmissions;
        host.sessionRestarted(unsigned(restartIndex),*value.game);
    }
    Frame compose(Size viewport){
        if(!std::isfinite(viewport.width)||!std::isfinite(viewport.height)||viewport.width<=0||viewport.height<=0)throw std::invalid_argument("Invalid concurrent viewport");
        if(sourceFrameNumber!=counters.frames){for(std::size_t i=0;i<cases.size();++i){sourceFrames[i]=cases[i]->game->drawWorld();host.appendScenarioUI(unsigned(i),*cases[i]->game,sourceFrames[i]);++observations[i].draws;}
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
    FrameActivity activity;activity.frame=p.counters.frames;activity.buttons=std::move(p.pendingButtons);p.pendingButtons.clear();activity.saveRestart=p.counters.frames==600&&p.observations[p.restartIndex].restarted;activity.aidKitUses=p.pendingAidKitUses;p.pendingAidKitUses=0;activity.airStrikeUses=p.pendingAirStrikeUses;p.pendingAirStrikeUses=0;std::set<std::string> frameWeapons;
    for(std::size_t i=0;i<p.cases.size();++i){auto& c=*p.cases[i];auto& observed=p.observations[i];
        if(auto* scene=c.game->battle()){const auto& d=p.descriptions[i];const bool representative=d.role==ScenarioRole::worldAudit&&(d.map=="Stage01_02"||d.map=="Stage02_07"||d.map=="Stage03_04");if(d.role!=ScenarioRole::airStrike&&d.role!=ScenarioRole::dash&&!representative&&!scene->selectedEnemy())for(const auto& enemy:scene->enemies())if(enemy.hp>0&&!enemy.corpse){scene->selectEnemy(enemy.id);break;}}
        const bool movement=i==p.movementIndex;
        if(movement)p.guideMovement();
        const auto beforeEmissions=observed.playerEmissions,beforeDamage=observed.damage,beforeKills=observed.kills;
        const Vec2 beforePosition=movement?c.game->battle()->player().position:Vec2{};
        const int beforeHP=movement?c.game->battle()->playerHP():0,beforeCredit=movement?c.game->state().credits:0,beforeInventory=movement?p.inventoryCount(*c.game):0;
        c.game->tick();p.host.tickScenarioUI(unsigned(i),*c.game,c.random);if(observed.ticks==0)observed.firstFrame=p.counters.frames;observed.lastFrame=p.counters.frames;++observed.ticks;++activity.tickedScenarios;p.observe(i);
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
        if(observed.playerEmissions>beforeEmissions){++activity.emittingScenarios;if(i<p.counters.combatScenarios)++activity.combatEmittingScenarios;activity.emissions+=observed.playerEmissions-beforeEmissions;if(!observed.firstEmissionFrame)observed.firstEmissionFrame=p.counters.frames;observed.lastEmissionFrame=p.counters.frames;}
        activity.damage+=observed.damage-beforeDamage;activity.kills+=observed.kills-beforeKills;if(i<p.counters.combatScenarios)activity.combatDamage+=observed.damage-beforeDamage;
        if(const auto* scene=c.game->battle()){activity.activeProjectiles+=scene->activeProjectiles();if(i<p.counters.combatScenarios)activity.combatActiveProjectiles+=scene->activeProjectiles();}
        if(const auto* scene=c.game->battle())for(const auto& [part,count]:scene->metrics().playerWeaponEmissions){auto [old,inserted]=c.lastWeaponCounts.try_emplace(part,0);(void)inserted;const auto previousCount=old->second;if(count>previousCount){frameWeapons.insert(part);observed.weaponEmissions[part]+=count-previousCount;}old->second=count;}
    }
    const bool otherCombat=activity.combatEmittingScenarios||activity.combatActiveProjectiles||activity.combatDamage;
    if(otherCombat){if(activity.airStrikeUses)++p.counters.airStrikeFramesWithCombatActivity;if(activity.aidKitUses)++p.counters.aidKitFramesWithCombatActivity;if(activity.movementLootMoving)++p.counters.movementFramesWithCombatActivity;if(activity.movementLootPickups)++p.counters.pickupFramesWithCombatActivity;if(activity.movementLootInjury)++p.counters.injuryFramesWithCombatActivity;if(activity.movementLootMoving&&activity.movementLootEmissions)++p.counters.movementEmissionFramesWithCombatActivity;}
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
std::vector<std::string> ConcurrentValidation::supported()const{return {"110 original PtWpn items /55 independently saved real battles","same-frame session ticks and shared sprite-frame compositor","original target / weapon / damage / death / mission / EXP consumers","scheduled real shop purchase and insufficient-credit refusal","original save encode/decode and restart without condition reinjection","one source-frame consumption per tick, including viewport recomposition","real ground-pointer movement and actual enemy-drop pickup on the same frame clock","six original AidKit consumers and three real AirStrike/bullet consumers","38 original field entries and common vehicle / turret / aircraft consumers","original ground double tap / SP / dash movement / shared particle consumer"};}
std::vector<std::string> ConcurrentValidation::missingFeatures()const{return {"all23 Player bottom equipment movement checks","bot actor AI / attack / pickup","all mission types and all60 mission completion","all stages and final campaign completion","original port-app visual/input/audio equivalence","OS interruption / audio-route transition"};}
}

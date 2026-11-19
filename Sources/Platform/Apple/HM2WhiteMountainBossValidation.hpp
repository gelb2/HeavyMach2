#pragma once
#include "Platform/Apple/HM2NativeValidation.hpp"
#include "Platform/Apple/HM2Resources.hpp"
#include "Core/Progression/Progression.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include <array>
#include <cmath>

namespace hm2::apple {
class WhiteMountainBossValidation final:public NativeValidationScenario {
    struct Case {
        std::string_view mission,map,boss;
        int health;
    };
    static constexpr std::array<Case,4> cases_{{
        {"M041010","SnowBasin","Boss_05",100000},
        {"M041020","SnowField","Boss_06",200000},
        {"M041030","SnowRidge","Boss_07",400000},
        {"M041040","SnowValley","Boss_08",1000000},
    }};
    enum class Step {inactiveGate,commandOpening,fighting,returning,claim,closeReward,reopen,verifyReplay};
    NSURL* root_;
    NSURL* output_;
    NSURL* save_;
    DisplayConfiguration display_;
    std::unique_ptr<GameRuntime> runtime_;
    NSMutableArray* results_=[NSMutableArray new];
    NSMutableSet* played_=[NSMutableSet new];
    Step step_=Step::inactiveGate;
    unsigned index_=0,ticks_=0,stepStarted_=0,lastHeal_=0,repairs_=0,bossFirstShot_=0,lastApproach_=0;
    std::uint64_t bossID_=0;
    int beforeReward_=0,afterReward_=0,reward_=0,lowestHP_=0;
    std::uint64_t playerShots_=0,enemyShots_=0,bossShots_=0,kills_=0,playRequests_=0,damageCalls_=0;
    Vec2 spawnPoint_;
    float bossShotDistance_=0, entryDistance_=0, activationRadius_=0;
    std::uintptr_t pointer_=500;
    bool done_=false,active_=true,audio_=true,deathCapture_=false,engaged_=false;

    static NSString* string(std::string_view value){
        return [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];
    }
    static void require(bool condition,std::string_view message){
        if(!condition)throw std::runtime_error(std::string(message));
    }
    const Case& current()const{return cases_.at(index_);}
    void advance(Step next){step_=next;stepStarted_=ticks_;}
    bool settled()const{return runtime_->session().screenTransition().phase==8;}
    bool shows(ui::ScreenId id)const{return settled()&&runtime_->session().state().screen==id;}
    int workingHP()const{return runtime_->session().battle()->playerHP();}
    void report(NSString* status,NSString* failure=@""){
        NSDictionary* record=@{@"status":status,@"failure":failure,@"cases":results_,@"expected":@(cases_.size()),
            @"activeMission":index_<cases_.size()?string(current().mission):@"",@"step":@(static_cast<int>(step_)),@"tick":@(ticks_),
            @"canvasWidth":@(display_.canvas.width),@"canvasHeight":@(display_.canvas.height)};
        NSData* data=[NSJSONSerialization dataWithJSONObject:record options:NSJSONWritingPrettyPrinted|NSJSONWritingSortedKeys error:nil];
        require(data&&[data writeToURL:[output_ URLByAppendingPathComponent:@"boss-report.json"] atomically:YES],
                "Cannot write White Mountain boss validation report");
    }
    progression::Definitions definitions(){
        Files files(root_.fileSystemRepresentation);
        const auto table=[&files](std::string_view name){return DataTable::parse(files.read(name));};
        return progression::Definitions::parse(table("ItemInfo.tble"),table("ItemRegen.tble"),table("MissionList.tble"),table,"kr");
    }
    SaveData fixture(){
        SaveData data;
        data.initializeSlot(0,"SnowBossQA","kr",1794268800);
        progression::SlotView slot(data.slot(0));
        slot.string(0x2d,16,"Base004");
        slot.string(0x55,16,"Base004");
        slot.u32(0x1b5c,5);
        slot.level(99);
        slot.experience(progression::Experience::bounds(99).previous);
        slot.hp(3236);
        slot.maxHP(3236);
        slot.credit(100000);
        // Only prerequisites are seeded complete. The current mission must
        // actually be accepted, won and rewarded through the game.
        slot.u16(0x658,std::uint16_t(index_+1));
        slot.string(0x65c,8,"M001230");
        slot.byte(0x65c+20,1);
        for(unsigned i=0;i<index_;++i){
            const auto offset=0x65c+21*(i+1);
            slot.string(offset,8,cases_[i].mission);
            slot.byte(offset+20,1);
        }
        constexpr std::array<std::string_view,5> equipped{"11023","12023","10504","10503","13006"};
        for(unsigned i=0;i<equipped.size();++i){
            slot.string(0x8c+i*6,6,equipped[i]);
            slot.byte(0x53c+i,1);
            slot.u32(0x604+i*4,i);
        }
        slot.u32(0x604+5*4,std::uint32_t(-1));
        slot.string(0x8c+8*6,6,"01006");
        slot.byte(0x53c+8,200);
        slot.string(0x61c,6,"01006");
        for(unsigned i=0;i<3;++i){slot.string(0x63a+i*6,6,"");slot.u32(0x64c+i*4,0);}
        return data;
    }
    void begin(){
        const auto defs=definitions();
        const auto mission=std::find_if(defs.missions().begin(),defs.missions().end(),[this](const auto& m){return m.id==current().mission;});
        require(mission!=defs.missions().end()&&mission->owner=="Base004","Missing authored White Mountain boss mission");
        const auto spawn=std::find_if(mission->acceptActions.begin(),mission->acceptActions.end(),[](const auto& action){
            return action.type==progression::Action::placeSpawner;
        });
        require(spawn!=mission->acceptActions.end()&&spawn->activationRadius>0,"Missing authored boss approach radius");
        spawnPoint_={float(spawn->x),float(spawn->y)};
        activationRadius_=float(spawn->activationRadius);
        reward_=0;
        for(const auto& action:mission->completeActions)if(action.type==0)reward_+=action.value;
        require(reward_>0,"Missing White Mountain boss credit reward");
        DarwinWorldRandom random(7301);
        auto data=fixture();
        auto bytes=data.encode(random,7301);
        save_=[output_ URLByAppendingPathComponent:[string(current().mission) stringByAppendingString:@".sav"]];
        require([[NSData dataWithBytes:bytes.data() length:bytes.size()] writeToURL:save_ atomically:YES],
                "Cannot write isolated boss fixture");
        runtime_=std::make_unique<GameRuntime>(root_,save_,[](const NativeRequest& request){
            if(request.kind==NativeRequest::Kind::gameOver||request.kind==NativeRequest::Kind::saveError)
                throw std::runtime_error("Boss validation runtime failed: "+request.text);
        },[] {return 7301u;});
        runtime_->setDisplay(display_);
        runtime_->setAudioActive(audio_);
        require(runtime_->session().selectSlot(0)&&runtime_->session().enterField(current().map,0),"Cannot load closed boss stage");
        runtime_->synchronize();
        ticks_=stepStarted_=lastHeal_=repairs_=bossFirstShot_=lastApproach_=0;
        bossShotDistance_=0;
        bossID_=0;deathCapture_=engaged_=false;
        lowestHP_=3236;
        [played_ removeAllObjects];
        step_=Step::inactiveGate;
    }
    bool press(unsigned id,std::string_view identity={}){
        const auto buttons=runtime_->screens().buttons();
        const auto found=std::find_if(buttons.begin(),buttons.end(),[&](const auto& key){
            return key.id==id&&(identity.empty()||key.dataId==identity);
        });
        if(found==buttons.end())return false;
        const auto canvas=runtime_->screens().buttonScreenPosition(*found);
        const auto pointer=++pointer_;
        const bool pressed=runtime_->input(PointerPhase::begin,canvas,pointer);
        const bool released=runtime_->input(PointerPhase::end,canvas,pointer);
        return pressed&&released;
    }
    void command(){
        require(shows(ui::ScreenId::base)&&runtime_->session().state().baseId=="Base004","White Mountain command base not active");
        require(press(2001)&&press(2001),"Could not enter Command by normal double selection");
    }
    void capture(CAEAGLLayer* layer,NSString* role){
        const auto name=[NSString stringWithFormat:@"%@-%@.png",string(current().boss),role];
        captureValidationFrame(*runtime_,layer,[output_ URLByAppendingPathComponent:name]);
    }
    bool worldTap(Vec2 position){
        auto& game=runtime_->session();
        const auto screen=game.worldCanvasPoint(game.battle()->camera().toScreen(position));
        const auto pointer=++pointer_;
        const bool began=runtime_->input(PointerPhase::begin,screen,pointer);
        (void)runtime_->input(PointerPhase::end,screen,pointer);
        const auto trace=runtime_->inputHistory();
        return began&&trace.size()>=2&&trace[trace.size()-2].worldConsumed;
    }
    bool accepted(bool requireComplete){
        const auto& missions=runtime_->session().state().acceptedMissions;
        return std::any_of(missions.begin(),missions.end(),[&](const auto& m){return m.dataId==current().mission&&m.complete==requireComplete;});
    }
    std::optional<CombatEnemySnapshot> target(){
        const auto enemies=runtime_->session().battle()->enemies();
        const auto found=std::find_if(enemies.begin(),enemies.end(),[this](const auto& exist){return exist.name==current().boss;});
        return found==enemies.end()?std::nullopt:std::optional(*found);
    }
    void pickMission(CAEAGLLayer* layer){
        const auto buttons=runtime_->screens().buttons();
        const auto row=std::find_if(buttons.begin(),buttons.end(),[this](const auto& b){
            return b.id>=5100&&b.id<=5104&&b.dataId==current().mission;
        });
        require(row!=buttons.end()&&press(row->id,current().mission),"Expected authored mission row is absent");
        capture(layer,@"mission");
        require(press(5004)&&accepted(false),"Mission did not go to the accepted list");
        // Phase 1 verifies the actual routes. Keep this test scoped to loading
        // the same accepted world at its ordinary western arrival gate.
        auto& game=runtime_->session();
        require(game.enterField(current().map,0),"Could not enter boss field after acceptance");
        runtime_->synchronize();
        advance(Step::fighting);
    }
    void observeFiring(){
        for(NSDictionary* sample in runtime_->audioPlaybackSnapshot()){
            NSString* name=sample[@"name"];
            if([sample[@"playing"] boolValue]&&[sample[@"sampleOffset"] unsignedLongLongValue]>0)
                [played_ addObject:name];
        }
    }
    std::uint64_t shotsByBoss(const CombatMetrics& combat)const{
        const auto prefix="Boss"+std::string(current().boss.substr(5))+"_";
        std::uint64_t shots=0;
        for(const auto& [part,count]:combat.enemyWeaponEmissions)if(part.starts_with(prefix))shots+=count;
        return shots;
    }
    void heal(CombatScene& battle){
        const auto repair=runtime_->session().state().quick[0].count;
        if(workingHP()>=2300||repair<=0||ticks_-lastHeal_<=20)return;
        require(press(6200)&&worldTap(battle.player().position)&&runtime_->session().state().quick[0].count==repair-1,
                "Repair kit could not rescue the player through normal input");
        ++repairs_;lastHeal_=ticks_;
    }
    void fight(CAEAGLLayer* layer){
        auto& game=runtime_->session();
        auto* battle=game.battle();
        const auto boss=target();
        if(!bossID_){
            const auto player=battle->player().position;
            const Vec2 delta{spawnPoint_.x-player.x,spawnPoint_.y-player.y};
            const auto distance=std::hypot(delta.x,delta.y);
            const auto elapsed=ticks_-stepStarted_;
            if(elapsed<=50){
                require(!boss&&distance>400.f,"Boss is already present at the arrival gate");
                entryDistance_=distance;
                if(elapsed==12)capture(layer,@"arrival");
                return;
            }
            if(!boss){
                require(elapsed<800,"Player approached but the accepted boss did not spawn");
                if(distance>activationRadius_&&(!lastApproach_||ticks_-lastApproach_>=72)){
                    const auto fraction=std::min(1.f-160.f/distance,140.f/distance);
                    require(worldTap({player.x+delta.x*fraction,player.y+delta.y*fraction}),"Boss arena approach input failed");
                    lastApproach_=ticks_;
                }
                return;
            }
            require(distance<=activationRadius_&&boss->boss&&boss->hp==current().health,
                    "The authored boss did not spawn within the approach radius at full HP");
            bossID_=boss->id;
        }
        if(ticks_%12==0)observeFiring();
        lowestHP_=std::min(lowestHP_,workingHP());
        const auto& combat=battle->metrics();
        if(!engaged_){
            require(boss&&boss->hp>0,"Boss disappeared before target input");
            const auto player=battle->player().position;
            const Vec2 delta{boss->position.x-player.x,boss->position.y-player.y};
            const float distance=std::hypot(delta.x,delta.y);
            if(!bossFirstShot_&&shotsByBoss(combat)>0){bossFirstShot_=ticks_;bossShotDistance_=distance;}
            require(ticks_-stepStarted_<1500,"The boss never fired its own weapons before Player attack");
            if(!bossFirstShot_&&distance>170.f&&(!lastApproach_||ticks_-lastApproach_>=72)){
                // Enemy detection requires less than 200 logical units.
                // Walk nearby without tapping the actor and starting an attack.
                const float fraction=std::min(1.f-130.f/distance,120.f/distance);
                require(worldTap({player.x+delta.x*fraction,player.y+delta.y*fraction}),"Boss approach tap failed");
                lastApproach_=ticks_;
            }
            heal(*battle);
            if(!bossFirstShot_||ticks_-bossFirstShot_<36)return;
            require(boss&&boss->hp>0&&worldTap(boss->position)&&battle->selectedEnemy()==bossID_,
                    "Native target selection did not select the boss");
            capture(layer,@"combat");
            engaged_=true;
        }
        if(accepted(true)){
            playerShots_=combat.playerEmissions;enemyShots_=combat.enemyEmissions;bossShots_=shotsByBoss(combat);
            kills_=combat.kills;damageCalls_=combat.damageCalls;
            playRequests_=[runtime_->audioMetrics()[@"effectPlayRequests"] unsignedLongLongValue];
            require(playerShots_>0&&bossShots_>0&&damageCalls_>0&&kills_>0&&(!boss||boss->hp<=0),
                    "Mission completion did not follow a real armed boss defeat");
            if(!deathCapture_){capture(layer,@"defeated");deathCapture_=true;}
            const auto credits=game.state().credits;
            require(press(1021)&&press(1027),"Could not open Airlift through the system menu");
            const auto& airlift=game.state().airlifts[5];
            require(airlift.dataId=="Base004"&&airlift.price==24000&&press(4007,"Base004")&&press(4001),
                    "First visit did not unlock the actual White Mountain airlift");
            require(game.state().credits==credits-24000,"Airlift did not charge the displayed fare");
            advance(Step::returning);
            return;
        }
        require(ticks_-stepStarted_<11500,"Boss fight did not finish with original armed equipment");
        heal(*battle);
        if(boss&&!boss->corpse&&ticks_%48==0&&battle->selectedEnemy()!=bossID_){
            require(worldTap(boss->position),"Boss reacquisition tap was rejected");
        }
    }
    void verifyReward(){
        DarwinWorldRandom random(7301);
        NSData* disk=[NSData dataWithContentsOfURL:save_];
        require(disk.length==SaveData::byteCount,"Claim did not write the normal save");
        auto decoded=SaveData::decode({static_cast<const std::uint8_t*>(disk.bytes),disk.length},random);
        progression::SlotView slot(decoded.slot(0));
        bool completed=false;
        for(unsigned i=0;i<slot.u16(0x658);++i){
            const auto p=0x65c+i*21;
            if(slot.string(p,8)==current().mission)completed=slot.byte(p+20)==1;
        }
        require(completed&&int(slot.credit())==afterReward_,"The rewarded mission did not persist after decoding the save");
    }
    void stepCase(CAEAGLLayer* layer){
        auto& game=runtime_->session();
        switch(step_){
        case Step::inactiveGate:
            if(ticks_-stepStarted_<8)return;
            require(!target(),"Boss spawned before its own mission was accepted");
            require(game.selectSlot(0),"Cannot restore the isolated original base start");
            game.enterBase();runtime_->synchronize();
            command();advance(Step::commandOpening);return;
        case Step::commandOpening:
            if(!shows(ui::ScreenId::command)||!game.state().commandListVisible)return;
            pickMission(layer);return;
        case Step::fighting:
            fight(layer);return;
        case Step::returning:
            if(!shows(ui::ScreenId::base))return;
            beforeReward_=game.state().credits;
            command();advance(Step::claim);return;
        case Step::claim:
            if(!shows(ui::ScreenId::command)||game.state().completedMission.dataId.empty())return;
            require(game.state().completedMission.dataId==current().mission,"Commander showed a different completed mission");
            afterReward_=game.state().credits;
            require(afterReward_-beforeReward_==reward_,"Claim did not pay the authored mission credits");
            capture(layer,@"reward");
            require(press(5007,current().mission),"Commander reward acknowledgement rejected");
            advance(Step::closeReward);return;
        case Step::closeReward:
            if(!game.state().commandListVisible)return;
            require(press(2100),"Could not leave Command after reward");
            advance(Step::reopen);return;
        case Step::reopen:
            if(!shows(ui::ScreenId::base))return;
            verifyReward();
            command();advance(Step::verifyReplay);return;
        case Step::verifyReplay:
            if(!shows(ui::ScreenId::command)||ticks_-stepStarted_<180)return;
            require(game.state().commandListVisible&&game.state().completedMission.dataId.empty()&&game.state().credits==afterReward_,
                    "Reopening Command paid the same boss mission again");
            [results_ addObject:@{@"mission":string(current().mission),@"map":string(current().map),@"boss":string(current().boss),
                @"bossInitialHP":@(current().health),@"entryDistance":@(entryDistance_),@"playerShots":@(playerShots_),@"enemyShots":@(enemyShots_),
                @"bossShots":@(bossShots_),@"firstBossShotDistance":@(bossShotDistance_),@"kills":@(kills_),
                @"damageCalls":@(damageCalls_),@"effectPlayRequests":@(playRequests_),@"sampledPlayingEffects":[[played_ allObjects] sortedArrayUsingSelector:@selector(compare:)],
                @"repairKits":@(repairs_),@"lowestPlayerHP":@(lowestHP_),@"reward":@(reward_),@"firstCredits":@(beforeReward_),
                @"lastCredits":@(afterReward_),@"saved":@YES,@"duplicateReward":@NO,@"simulationTicks":@(ticks_)}];
            ++index_;
            done_=index_==cases_.size();
            report(done_?@"passed":@"running");
            runtime_.reset();
        }
    }
public:
    WhiteMountainBossValidation(NSURL* root,NSURL* output):root_(root),output_(output){
        require([NSFileManager.defaultManager createDirectoryAtURL:output withIntermediateDirectories:YES attributes:nil error:nil],
                "Cannot create boss validation output");
        report(@"running");
    }
    void setDisplay(DisplayConfiguration display) override{
        display_=display;
        if(runtime_)runtime_->setDisplay(display);
    }
    void setActive(bool active) override{
        active_=active;
        if(runtime_)runtime_->setActive(active);
    }
    void setAudioActive(bool active) override{
        audio_=active;
        if(runtime_)runtime_->setAudioActive(active);
    }
    bool step(CAEAGLLayer* layer) override{
        if(done_||!active_)return done_;
        try{
            if(!runtime_)begin();
            for(unsigned i=0;i<12&&runtime_;++i){
                runtime_->tick();++ticks_;
                stepCase(layer);
                if(runtime_&&step_!=Step::fighting)require(ticks_-stepStarted_<650,"Boss validation menu/transition timeout");
            }
            if(runtime_)runtime_->draw(layer);
        }catch(const std::exception& error){
            if(runtime_){
                try{capture(layer,@"failure");}catch(const std::exception&){}
            }
            report(@"failed",string(error.what()));
            done_=true;
        }
        return done_;
    }
};
}

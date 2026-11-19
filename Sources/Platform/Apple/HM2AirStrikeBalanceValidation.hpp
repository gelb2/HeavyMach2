#pragma once
#include "Platform/Apple/HM2NativeValidation.hpp"
#include "Core/Progression/Progression.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include <array>
#include <cmath>

namespace hm2::apple {
class AirStrikeBalanceValidation final:public NativeValidationScenario {
    struct MapCase {const char* map;const char* tank;unsigned level;};
    struct Skill {const char* code;NSString* sound;unsigned shots;};
    static constexpr std::array<MapCase,2> maps_{{{"SnowBasin","E_Tank_21",28},{"SnowValley","E_Tank_26",49}}};
    const std::array<Skill,4> skills_{{{"02004",@"Wpn_Missile02.wav",10},{"02005",@"Wpn_Missile01.wav",10},
                                      {"02006",@"Wpn_Laser.wav",10},{"02007",@"Wpn_Missile03.wav",1}}};
    NSURL* root_;
    NSURL* output_;
    DisplayConfiguration display_;
    std::unique_ptr<GameRuntime> runtime_;
    NSMutableArray* results_=[NSMutableArray new];
    unsigned index_=0,tick_=0,castAt_=0,casts_=0,walkAt_=0;
    std::uint64_t sample_=0,targetID_=0;
    int hpBefore_=0;
    bool active_=true,audio_=true,done_=false;

    const MapCase& mapCase()const{return maps_[index_/skills_.size()];}
    const Skill& skill()const{return skills_[index_%skills_.size()];}
    static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
    void report(NSString* status,NSString* failure=@""){
        const auto data=[NSJSONSerialization dataWithJSONObject:@{@"status":status,@"failure":failure,
            @"cases":results_,@"expected":@(maps_.size()*skills_.size()),@"index":@(index_),@"tick":@(tick_)}
            options:NSJSONWritingPrettyPrinted error:nil];
        require(data&&[data writeToURL:[output_ URLByAppendingPathComponent:@"air-balance.json"] atomically:YES],
                "Cannot write airstrike balance evidence");
    }
    void begin(){
        SaveData save;
        save.initializeSlot(0,"Air QA","kr",1794268800);
        progression::SlotView slot(save.slot(0));
        slot.string(0x2d,16,mapCase().map);slot.u32(0x1b5c,5);
        slot.level(mapCase().level-1);slot.experience(progression::Experience::bounds(mapCase().level-1).previous);
        slot.hp(220000);slot.maxHP(220000);
        for(unsigned i:{6u,7u}){
            slot.string(0x8c+i*6,6,"");
            slot.byte(0x53c+i,0);
        }
        slot.string(0x8c+8*6,6,skill().code);slot.byte(0x53c+8,2);
        slot.string(0x61c,6,skill().code);
        for(unsigned i=0;i<3;++i)slot.string(0x63a+i*6,6,"");
        DarwinWorldRandom random(71);
        const auto bytes=save.encode(random,71);
        const auto location=[output_ URLByAppendingPathComponent:[NSString stringWithFormat:@"air-%u.sav",index_]];
        require([[NSData dataWithBytes:bytes.data() length:bytes.size()] writeToURL:location atomically:YES],
                "Cannot seed isolated airstrike profile");
        runtime_=std::make_unique<GameRuntime>(root_,location,[](const NativeRequest& event){
            if(event.kind==NativeRequest::Kind::gameOver||event.kind==NativeRequest::Kind::saveError)
                throw std::runtime_error("Airstrike profile runtime failed");
        },[]{return 71u;});
        runtime_->setDisplay(display_);
        runtime_->setAudioActive(audio_);
        require(runtime_->session().selectSlot(0)&&runtime_->session().enterField(mapCase().map,0),
                "Cannot enter airstrike test stage");
        runtime_->synchronize();
        tick_=castAt_=casts_=walkAt_=0;
        sample_=targetID_=0;
        hpBefore_=0;
    }
    std::optional<CombatEnemySnapshot> target(){
        for(const auto& enemy:runtime_->session().battle()->enemies())if(enemy.id==targetID_)return enemy;
        return {};
    }
    bool coveredByButton(Vec2 point)const{
        const auto& screens=runtime_->screens();
        for(const auto& key:screens.buttons()){
            const auto button=screens.button(key);
            if(!button||!button->active||!button->visible)continue;
            const auto bounds=screens.buttonScreenBounds(key);
            if(point.x>bounds.x&&point.x<=bounds.x+bounds.width&&
               point.y>bounds.y&&point.y<=bounds.y+bounds.height)return true;
        }
        return false;
    }
    bool choose(){
        auto& game=runtime_->session();
        auto* battle=game.battle();
        const auto player=battle->player().position;
        float nearest=2000,nearestAny=2000;
        std::optional<Vec2> approach;
        for(const auto& enemy:battle->enemies()){
            if(enemy.name!=mapCase().tank||enemy.hp<=0)continue;
            const auto distance=std::hypot(enemy.position.x-player.x,enemy.position.y-player.y);
            if(distance<nearestAny){nearestAny=distance;approach=enemy.position;}
            const auto point=battle->camera().toScreen(enemy.position);
            const auto canvas=game.worldCanvasPoint(point);
            if(point.y<battle->camera().inputMinimumY+20||canvas.x<20||canvas.y<20||
               canvas.x>display_.canvas.width-20||canvas.y>display_.canvas.height-20||
               coveredByButton(canvas))continue;
            if(distance<nearest){nearest=distance;targetID_=enemy.id;hpBefore_=enemy.hp;}
        }
        if(targetID_)return true;
        require(approach&&tick_<850,"Cannot approach the representative airstrike target");
        if(!walkAt_||tick_-walkAt_>=110){
            const float fraction=std::min(1.f,130.f/nearestAny);
            const Vec2 waypoint{player.x+(approach->x-player.x)*fraction,player.y+(approach->y-player.y)*fraction};
            const auto canvas=game.worldCanvasPoint(battle->camera().toScreen(waypoint));
            require(runtime_->input(PointerPhase::begin,canvas,713),"Representative target approach was rejected");
            runtime_->input(PointerPhase::end,canvas,713);
            walkAt_=tick_;
        }
        return false;
    }
    void cast(Vec2 position){
        auto& game=runtime_->session();
        const auto keys=runtime_->screens().buttons();
        const auto quick=std::find_if(keys.begin(),keys.end(),[&](const auto& key){return key.id==6200&&key.dataId==skill().code;});
        require(quick!=keys.end(),"Equipped airstrike has no quick slot button");
        const auto button=runtime_->screens().buttonScreenPosition(*quick);
        require(runtime_->input(PointerPhase::begin,button,711),"Airstrike button input was rejected");
        const auto armed=game.state().selectedQuick;
        runtime_->input(PointerPhase::end,button,711);
        if(armed!=0)throw std::runtime_error("Airstrike quick slot did not arm; paused="+
            std::to_string(game.battle()->isPaused())+" count="+std::to_string(game.state().quick[0].count)+
            " transition="+std::to_string(game.screenTransition().phase));
        const auto target=game.worldCanvasPoint(game.battle()->camera().toScreen(position));
        require(runtime_->input(PointerPhase::begin,target,712),"Airstrike target input was rejected");
        runtime_->input(PointerPhase::end,target,712);
        ++casts_;castAt_=tick_;
        if(game.state().quick[0].count!=2-int(casts_)){
            const auto trace=runtime_->inputHistory();
            const auto& begin=trace[trace.size()-2];
            throw std::runtime_error("Airstrike target consumption missing at "+std::to_string(target.x)+","+std::to_string(target.y)+
                " overlay="+std::to_string(begin.overlayConsumed)+" ui="+std::to_string(begin.uiConsumed)+
                " world="+std::to_string(begin.worldConsumed));
        }
    }
    void observeAudio(){
        for(NSDictionary* entry in runtime_->audioPlaybackSnapshot())
            if([entry[@"name"] isEqual:skill().sound]&&[entry[@"playing"] boolValue])
                sample_=std::max(sample_,[entry[@"sampleOffset"] unsignedLongLongValue]);
    }
    void stepCase(CAEAGLLayer* layer){
        if(!casts_){
            if(tick_<30||runtime_->session().screenTransition().phase!=8)return;
            if(choose())cast(target()->position);
            return;
        }
        if(tick_-castAt_<560)return;
        auto& game=runtime_->session();
        const auto victim=target();
        if(victim&&victim->hp>0&&casts_==1){cast(victim->position);return;}
        require(!victim||victim->hp<=0,"Representative tank survived two centered airstrikes");
        const auto metrics=game.battle()->metrics();
        require(metrics.airStrikeProjectiles==casts_*skill().shots&&metrics.airStrikeEnemyHPRemoved>0&&sample_>0,
                "Missing real airstrike emissions, damage or sampled OpenAL playback");
        captureValidationFrame(*runtime_,layer,[output_ URLByAppendingPathComponent:[NSString stringWithFormat:@"air-%u.png",index_]]);
        [results_ addObject:@{@"map":@(mapCase().map),@"level":@(mapCase().level),@"tank":@(mapCase().tank),
            @"tankHP":@(hpBefore_),@"code":@(skill().code),@"uses":@(casts_),@"shots":@(metrics.airStrikeProjectiles),
            @"damage":@(metrics.airStrikeEnemyHPRemoved),@"sampleOffset":@(sample_),@"sound":skill().sound}];
        ++index_;done_=index_==maps_.size()*skills_.size();
        report(done_?@"passed":@"running");
        runtime_.reset();
    }
public:
    AirStrikeBalanceValidation(NSURL* root,NSURL* output):root_(root),output_(output){
        require([NSFileManager.defaultManager createDirectoryAtURL:output withIntermediateDirectories:YES attributes:nil error:nil],
                "Cannot create airstrike output folder");
        report(@"running");
    }
    void setDisplay(DisplayConfiguration display)override{display_=display;if(runtime_)runtime_->setDisplay(display);}
    void setActive(bool active)override{active_=active;if(runtime_)runtime_->setActive(active);}
    void setAudioActive(bool active)override{audio_=active;if(runtime_)runtime_->setAudioActive(active);}
    bool step(CAEAGLLayer* layer)override{
        if(done_||!active_)return done_;
        try{
            if(!runtime_)begin();
            observeAudio();
            for(unsigned i=0;i<12&&runtime_;++i){runtime_->tick();++tick_;stepCase(layer);}
            if(runtime_)runtime_->draw(layer);
        }catch(const std::exception& error){
            report(@"failed",@(error.what()));done_=true;
        }
        return done_;
    }
};
}

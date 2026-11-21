#pragma once
// Included only by the cheat application. Start conditions use fresh
// ordinary save envelopes. Navigation, rendering and screen actions use GameRuntime.
#include "Platform/Apple/HM2NativeValidation.hpp"
#include "Platform/Apple/HM2Resources.hpp"
#include "Core/Progression/Progression.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <set>

namespace hm2::apple {
class WhiteMountainValidation final:public NativeValidationScenario {
    struct Route {std::string source;MapGate gate;unsigned baseExit=0;};
    static constexpr std::array<std::string_view,7> maps_{
        "SnowPass","Base004","SnowBasin","SnowField","SnowRidge","SnowFort","SnowValley"
    };
    NSURL* root_;
    NSURL* directory_;
    NSURL* saveFile_;
    Maps mapData_;
    std::vector<Route> routes_;
    std::unique_ptr<GameRuntime> runtime_;
    std::set<std::string> captures_;
    NSMutableArray* results_=[NSMutableArray new];
    NSDictionary* fortCollision_;
    DisplayConfiguration display_;
    unsigned current_=0,ticks_=0,departure_=0,collisionStart_=0,contactTicks_=0;
    bool finished_=false,active_=true,audioActive_=true;
    Vec2 origin_,departurePosition_,collisionOrigin_;

    static NSString* string(std::string_view value){
        return [[NSString alloc]initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];
    }
    static void require(bool condition,std::string_view detail){
        if(!condition)throw std::runtime_error(std::string(detail));
    }
    static Maps maps(NSURL* root){
        Files files(root.fileSystemRepresentation);
        return Maps::parse(Chunk::parse(files.read("MapInfo.mcb")));
    }
    static bool isExpansion(std::string_view map){
        return std::find(maps_.begin(),maps_.end(),map)!=maps_.end();
    }
    static std::string mapID(std::span<const std::uint8_t> slot){
        const auto first=slot.begin()+0x2d;
        return {first,std::find(first,first+16,0)};
    }
    const Route& route()const{return routes_.at(current_);}
    void write(NSDictionary* report){
        NSError* error=nil;
        NSData* data=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted|NSJSONWritingSortedKeys error:&error];
        require(data&&[data writeToURL:[directory_ URLByAppendingPathComponent:@"report.json"] options:NSDataWritingAtomic error:&error],"Cannot write White Mountain validation report");
    }
    void begin(){
        runtime_.reset();
        const auto& r=route();
        SaveData save;
        save.initializeSlot(0,"SnowQA","kr",1794268800);
        progression::SlotView slot(save.slot(0));
        slot.string(0x2d,16,r.source);
        slot.u32(0x68,unsigned(r.gate.number));
        slot.u32(0x1b5c,5); // First-Help completed in the isolated start.
        slot.level(99);
        slot.experience(progression::Experience::bounds(99).previous);
        slot.hp(2236);
        slot.maxHP(2236);
        DarwinWorldRandom random(7301);
        const auto bytes=save.encode(random,7301);
        saveFile_=[directory_ URLByAppendingPathComponent:[NSString stringWithFormat:@"route-%02u.sav",current_+1]];
        require([[NSData dataWithBytes:bytes.data() length:bytes.size()] writeToURL:saveFile_ atomically:YES],"Cannot create isolated White Mountain start");
        runtime_=std::make_unique<GameRuntime>(root_,saveFile_,[](const NativeRequest& request){
            if(request.kind==NativeRequest::Kind::gameOver||request.kind==NativeRequest::Kind::saveError)
                throw std::runtime_error("White Mountain validation runtime failed: "+request.text);
        },[] {return 7301u;});
        auto& game=runtime_->session();
        runtime_->setDisplay(display_);
        runtime_->setAudioActive(audioActive_);
        require(game.selectSlot(0),"Cannot select isolated White Mountain start");
        if(r.baseExit)game.enterBase();
        else require(game.enterField(r.source,r.gate.number),"Cannot load White Mountain source map");
        runtime_->synchronize();
        ticks_=departure_=collisionStart_=contactTicks_=0;
        origin_=game.battle()?game.battle()->player().position:Vec2{};
        departurePosition_={};
    }
    void press(std::uint32_t button,std::uintptr_t pointer){
        auto& screens=runtime_->screens();
        const auto buttons=screens.buttons();
        const auto selected=std::find_if(buttons.begin(),buttons.end(),[button](const auto& key){return key.id==button;});
        require(selected!=buttons.end(),"White Mountain base exit button missing");
        const auto point=screens.buttonScreenPosition(*selected);
        const bool began=runtime_->input(PointerPhase::begin,point,pointer);
        const bool ended=runtime_->input(PointerPhase::end,point,pointer);
        require(began&&ended,"White Mountain base exit input was rejected");
    }
    void tapWorld(Vec2 destination){
        auto& game=runtime_->session();
        const auto point=game.worldCanvasPoint(game.battle()->camera().toScreen(destination));
        require(runtime_->input(PointerPhase::begin,point,419)&&runtime_->input(PointerPhase::end,point,419),
                "White Mountain gate navigation input was rejected at "+route().source+" ("+std::to_string(point.x)+","+std::to_string(point.y)+")");
        const auto inputs=runtime_->inputHistory();
        require(inputs.size()>=2&&inputs[inputs.size()-2].worldConsumed&&inputs.back().worldConsumed,
                "White Mountain gate input did not reach the world");
    }
    void walk(){
        const auto player=runtime_->session().battle()->player().position;
        const auto gate=route().gate.position;
        // Tap inside the gate's 30-point activation square, approaching from
        // the actual inlet so the target stays in the camera's input surface.
        tapWorld({gate.x+(player.x-gate.x)*.24f,gate.y+(player.y-gate.y)*.24f});
    }
    bool verifyFortCollision(CAEAGLLayer* layer){
        auto* battle=runtime_->session().battle();
        require(battle&&battle->terrain().edges().size()==13,"SnowFort collision boundaries missing");
        if(!collisionStart_){
            capture(layer,"arrival-SnowFort");
            collisionOrigin_=battle->player().position;
            tapWorld({11,80}); // Inside the authored central boulder.
            collisionStart_=ticks_;
        }
        const auto position=battle->player().position;
        require(position.x<-100.f,"Player reached the inside of the SnowFort boulder");
        const int edge=battle->dashState().currentTerrain;
        if(edge>=0)++contactTicks_;
        require(ticks_-collisionStart_<620,"SnowFort boulder did not block normal movement");
        if(contactTicks_<12)return false;
        const float traveled=std::hypot(position.x-collisionOrigin_.x,position.y-collisionOrigin_.y);
        require(traveled>250.f,"SnowFort boulder test did not approach from the west gate");
        capture(layer,"snow-fort-collision");
        fortCollision_=@{@"inputTarget":@[@11,@80],@"position":@[@(position.x),@(position.y)],
            @"terrainEdge":@(edge),@"contactTicks":@(contactTicks_),@"traveled":@(traveled),
            @"image":@"snow-fort-collision.png"};
        return true;
    }
    void capture(CAEAGLLayer* layer,const std::string& id){
        if(!captures_.insert(id).second)return;
        captureValidationFrame(*runtime_,layer,[directory_ URLByAppendingPathComponent:[string(id) stringByAppendingString:@".png"]]);
    }
    void record(CAEAGLLayer* layer){
        auto& game=runtime_->session();
        const auto& r=route();
        require(!game.help()&&game.metrics().unsupportedActions==0,"Unexpected help or unsupported action on White Mountain route");
        const auto saved=mapID(game.envelope().slot(0));
        // An app opened on a base save has no prior field-save permission.
        // A Base exit opens its field before the next normal gate grants it.
        const auto& expectedSavedMap=r.baseExit?r.source:r.gate.linkMap;
        require(mapID(game.workingSlot())==r.gate.linkMap&&saved==expectedSavedMap&&
                game.metrics().saveRequests==game.metrics().saveCompletions,
                "Unexpected White Mountain working or saved location");
        NSMutableArray* labels=[NSMutableArray new];
        NSMutableArray* enemies=[NSMutableArray new];
        const bool base=r.gate.linkMap.starts_with("Base");
        if(base){
            require(game.state().screen==ui::ScreenId::base&&game.state().baseId==r.gate.linkMap,
                    "White Mountain destination is not the base screen");
            NSData* persisted=[NSData dataWithContentsOfURL:saveFile_];
            require(persisted.length==SaveData::byteCount,"White Mountain base save was not written");
            DarwinWorldRandom reloadRandom(7301);
            auto reloaded=SaveData::decode({static_cast<const std::uint8_t*>(persisted.bytes),persisted.length},reloadRandom);
            progression::SlotView slot(reloaded.slot(0));
            require(slot.string(0x2d,16)=="Base004"&&slot.string(0x55,16)=="Base004"&&slot.u32(0x1b60+5*4)==1,
                    "White Mountain base save did not restore its location and first-visit flag");
        }else{
            require(game.state().screen==ui::ScreenId::field&&game.battle(),"White Mountain destination is not a field");
            for(const auto& gate:game.battle()->gates()){
                require(!gate.destinationName.empty(),"Missing rendered White Mountain portal label");
                [labels addObject:@{@"gate":@(gate.definition.number),@"destination":string(gate.definition.linkMap),
                    @"text":string(gate.destinationName)}];
            }
            for(const auto& actor:game.battle()->enemies()){
                if(!actor.corpse)[enemies addObject:string(actor.name)];
            }
        }
        const float movement=std::hypot(departurePosition_.x-origin_.x,departurePosition_.y-origin_.y);
        if(!r.baseExit)require(movement>19,"White Mountain portal transitioned without walking from its spawn");
        const auto name="arrival-"+r.gate.linkMap;
        capture(layer,name);
        [results_ addObject:@{@"from":string(r.source),@"gate":@(r.gate.number),@"to":string(r.gate.linkMap),
            @"gateWorld":@[@(r.gate.position.x),@(r.gate.position.y)],
            @"arrivalGate":@(r.gate.linkGate),@"baseExitButton":@(r.baseExit),@"walk":@(movement),@"simulationTicks":@(ticks_),
            @"savedMap":string(saved),@"baseSaveReloaded":@(base),@"gates":labels,@"actors":enemies,
            @"terrainEdges":@(game.battle()?game.battle()->terrain().edges().size():0),@"image":string(name+".png")}];
    }
public:
    WhiteMountainValidation(NSURL* root,NSURL* directory):root_(root),directory_(directory),mapData_(maps(root)){
        NSError* error=nil;
        require([NSFileManager.defaultManager createDirectoryAtURL:directory_ withIntermediateDirectories:YES attributes:nil error:&error],
                "Cannot create White Mountain validation directory");
        for(auto map:maps_)(void)mapData_.map(map);
        for(const auto& [id,from]:mapData_.all()){
            for(std::size_t n=0;n<from.gates.size();++n){
                const auto& gate=from.gates[n];
                if(!isExpansion(id)&&!isExpansion(gate.linkMap))continue;
                require(!gate.eventOnly,"White Mountain travel test expects an unlocked route");
                const auto& to=mapData_.map(gate.linkMap);
                const auto reciprocal=std::find_if(to.gates.begin(),to.gates.end(),[&](const auto& g){
                    return g.linkMap==id&&g.number==gate.linkGate&&g.linkGate==gate.number;
                });
                require(reciprocal!=to.gates.end(),"White Mountain route is missing a reciprocal gate");
                const unsigned button=id.starts_with("Base")?unsigned(2005+n):0;
                routes_.push_back({id,gate,button});
            }
        }
        std::stable_sort(routes_.begin(),routes_.end(),[](const Route& a,const Route& b){
            return isExpansion(a.source)<isExpansion(b.source);
        });
        require(routes_.size()==14&&routes_.front().source=="Stage04_09"&&routes_.front().gate.linkMap=="Base004",
                "Expected desert 01 to lead directly to White Mountain in the 14 portal routes");
        for(const auto& [button,destination]:{std::pair{2005u,"Stage04_09"},std::pair{2006u,"SnowPass"}}){
            require(std::any_of(routes_.begin(),routes_.end(),[&](const auto& route){
                return route.source=="Base004"&&route.baseExit==button&&route.gate.linkMap==destination;
            }),"White Mountain base exit points to the wrong map");
        }
        report(@"running");
    }
    void report(NSString* status,NSString* failure=@""){
        write(@{@"status":status,@"failure":failure,@"routes":results_,@"expectedRoutes":@(routes_.size()),
            @"snowFortCollision":fortCollision_?:@{},
            @"device":UIDevice.currentDevice.userInterfaceIdiom==UIUserInterfaceIdiomPad?@"iPad":@"iPhone",
            @"canvasWidth":@(display_.canvas.width),@"canvasHeight":@(display_.canvas.height),
            @"activeSource":current_<routes_.size()?string(route().source):@"",
            @"activeDestination":current_<routes_.size()?string(route().gate.linkMap):@""});
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
        audioActive_=active;
        if(runtime_)runtime_->setAudioActive(active);
    }
    bool step(CAEAGLLayer* layer) override{
        if(finished_||!active_)return finished_;
        try{
            if(!runtime_)begin();
            auto& game=runtime_->session();
            // Run the actual scene/transition ticks; render on each display
            // callback and once at each sampled arrival after the fade.
            constexpr unsigned ticksPerDisplay=12;
            for(unsigned batch=0;batch<ticksPerDisplay;++batch){
                runtime_->tick();
                ++ticks_;
                if(ticks_==8){
                    if(current_==0)capture(layer,"trace-first-source");
                    if(route().baseExit){press(route().baseExit,421);press(route().baseExit,422);}
                    else walk();
                }
                if(ticks_>8&&!departure_&&mapID(game.workingSlot())==route().gate.linkMap){
                    departure_=ticks_;
                    if(game.battle())departurePosition_=game.battle()->player().position;
                }
                if(departure_&&ticks_>departure_+7&&game.screenTransition().phase==8&&!game.mapLoading()){
                    if(route().source=="SnowRidge"&&route().gate.linkMap=="SnowFort"&&!verifyFortCollision(layer))continue;
                    record(layer);
                    ++current_;
                    if(current_==routes_.size()){
                        report(@"passed");finished_=true;
                        return true;
                    }
                    report(@"running");
                    runtime_.reset();
                    return false;
                }
                require(ticks_<650,"White Mountain route timeout: "+route().source+" -> "+route().gate.linkMap);
            }
            runtime_->draw(layer);
        }catch(const std::exception& e){
            report(@"failed",string(e.what()));
            finished_=true;
        }
        return finished_;
    }
};
}

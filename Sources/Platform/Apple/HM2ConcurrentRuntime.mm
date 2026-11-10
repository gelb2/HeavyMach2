#include "Platform/Apple/HM2ConcurrentRuntime.hpp"
#include "Platform/Apple/HM2Resources.hpp"
#include "Platform/Apple/HM2TextRasterizer.h"
#import "Platform/Apple/HM2Audio.h"
#import "Platform/Apple/HM2Renderer.h"
#include "Core/Audio/SoundIDs.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <map>
#include <set>

namespace hm2::apple {
namespace {
NSString* native(std::string_view s){return [[NSString alloc]initWithBytes:s.data() length:s.size() encoding:NSUTF8StringEncoding];}
}
struct ConcurrentRuntime::Impl:validation::ValidationHost {
    struct UIHost:ui::ScreensHost {
        Impl& owner;unsigned index;
        UIHost(Impl& value,unsigned i):owner(value),index(i){}
        void action(const ui::ScreenAction& action)override{if(owner.preparing)return;++owner.interfaces.at(index)->events[{action.button.key.id,action.button.key.intData}];owner.runner->session(index).action(action);owner.refresh(index);}
        ui::PreparedScreenText prepareText(const ui::TextRasterRequest& request)override{return owner.text(request);}
        void bgm(std::string_view file)override{if(!owner.preparing&&![owner.audio playMusic:native(file)])throw std::runtime_error(owner.audio.lastError.UTF8String);}
        void sound(std::string_view file)override{if(!owner.preparing)owner.sound(index,file,{});}
        void sfx(unsigned id)override{if(!owner.preparing)owner.soundID(index,id,{});}
    };
    struct UI {
        UIHost host;PreparedTiles tiles;std::unique_ptr<ui::Screens> screens;std::uint64_t revision=0;
        std::map<std::pair<unsigned,int>,std::uint64_t> events;
        std::unique_ptr<ui::Text> caption;
        UI(Impl& owner,unsigned index):host(owner,index),tiles("validationUI"+std::to_string(index)){}
    };
    Files files;NSURL* directory;HM2Renderer* renderer;HM2Audio* audio;
    std::unique_ptr<validation::ConcurrentValidation> runner;
    std::vector<std::unique_ptr<UI>> interfaces;
    std::unique_ptr<ui::TextRasterizer> rasterizer=makeTextRasterizer();ui::TextPreparationCache textCache;
    std::map<const ui::TextBitmap*,std::string> textNames;
    bool preparing=true,active=true;std::uint64_t presented=0,saveWrites=0;
    NSUInteger warmUploads=0,warmImageUploads=0;NSDictionary* warmAudio;NSData* legacyComparison;NSDictionary* interruption;
    unsigned maxPlaying=0,maxProgressing=0;NSArray<NSString*>* progressingNames=@[];
    std::vector<double> costs;double tickCost=0;
    std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now();
    Impl(NSURL* root,NSURL* saveDirectory):files(root.fileSystemRepresentation),directory(saveDirectory){
        NSError* error=nil;if(!root.isFileURL||!directory.isFileURL||![NSFileManager.defaultManager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error])throw std::runtime_error("Cannot prepare isolated concurrent-validation storage");
        renderer=[[HM2Renderer alloc]initWithResourceRoot:root];audio=[[HM2Audio alloc]initWithResourceRoot:root];if(![audio prepareSounds])throw std::runtime_error(audio.lastError.UTF8String);
        runner=std::make_unique<validation::ConcurrentValidation>(files,*this);interfaces.reserve(runner->scenarioCount());
        for(unsigned i=0;i<runner->scenarioCount();++i)interfaces.push_back(std::make_unique<UI>(*this,i));
        for(unsigned i=0;i<runner->scenarioCount();++i){refresh(i);
            const auto prepared=text({"validation.case."+std::to_string(i),std::to_string(i)+" | "+runner->scenarios()[i].label,"Arial-BoldMT",{740,24},16,ui::TextAlign::left});
            interfaces[i]->caption=std::make_unique<ui::Text>(prepared.image,prepared.bitmap);interfaces[i]->caption->position={384,1008};
            if(auto* battle=runner->session(i).battle()){const auto frame=battle->preparationFrame();[renderer prepareFrame:frame];}}
        prewarmMenus();
        preparing=false;warmUploads=renderer.textureUploads;warmImageUploads=renderer.imageTextureUploads;warmAudio=audio.metrics;costs.reserve(3600);
        if(![audio playMusic:@"STAGE3.mp3"])throw std::runtime_error(audio.lastError.UTF8String);
        started=std::chrono::steady_clock::now();
    }
    ui::PreparedScreenText text(const ui::TextRasterRequest& request){
        auto bitmap=textCache.prepare(*rasterizer,request);auto found=textNames.find(bitmap.get());
        if(found==textNames.end()){const auto name="@text/concurrent/"+std::to_string(textNames.size());[renderer prepareText:*bitmap identifier:name];found=textNames.emplace(bitmap.get(),name).first;}
        return {found->second,bitmap};
    }
    void prepare(ui::Screens& screens,PreparedTiles& tiles,float time){Frame frame;screens.append(frame,time);tiles.appendPreparation(frame);[renderer prepareFrame:frame];}
    void refresh(unsigned index){
        auto& view=*interfaces.at(index);auto& game=runner->session(index);const auto& state=game.state();
        if(!view.screens)view.screens=std::make_unique<ui::Screens>(runner->cachedResources(),view.tiles,view.host);
        if(view.revision!=game.presentationRevision()){
            view.screens->refresh(state,game.renderTime());view.revision=game.presentationRevision();prepare(*view.screens,view.tiles,game.renderTime());
        }else view.screens->updateHUD(state);
    }
    void prewarmMenus(){
        // Preparation consumes the real screen builders and original assets.
        // It does not execute game actions or count as a passing menu scenario.
        UI temporary(*this,0);ui::Screens screens(runner->cachedResources(),temporary.tiles,temporary.host);
        const auto base=runner->session(0).state();auto state=base;
        for(unsigned id=1;id<=7;++id){state=base;state.screen=ui::ScreenId(id);screens.refresh(state);prepare(screens,temporary.tiles,0);}
        for(const auto* name:{"Base001","Base002","Base003","Base004","Base005","Base006"}){state=base;state.screen=ui::ScreenId::base;state.baseId=name;screens.refresh(state);prepare(screens,temporary.tiles,0);}
        for(unsigned panel=0;panel<4;++panel){state=base;state.screen=ui::ScreenId::field;state.inventoryVisible=panel==0;state.missionVisible=panel==1;state.airliftVisible=panel==2;state.systemVisible=panel==3;screens.refresh(state);prepare(screens,temporary.tiles,0);}
        state=base;state.screen=ui::ScreenId::shop;state.inventoryVisible=true;state.selectedShop=0;state.tradeVisible=true;state.tradeItemId="01001";state.tradeCount=1;state.tradePrice=10;screens.refresh(state);prepare(screens,temporary.tiles,0);
        for(unsigned i=0;i<runner->scenarioCount();++i)if(runner->scenarios()[i].role==validation::ScenarioRole::mission){
            state=runner->session(i).state();state.screen=ui::ScreenId::command;state.selectedMission=0;
            if(!state.availableMissions.empty()){state.missionTitle=state.availableMissions[0].name;state.missionBody=state.availableMissions[0].description;}
            screens.refresh(state);prepare(screens,temporary.tiles,0);break;
        }
    }
    void appendScenarioUI(unsigned index,GameSession& game,Frame& frame)override{
        refresh(index);auto& view=*interfaces.at(index);view.screens->append(frame,game.renderTime());if(view.caption)view.caption->append(frame);
    }
    void tickScenarioUI(unsigned index,GameSession& game,WorldRandom& random)override{refresh(index);interfaces.at(index)->screens->tick(game.renderTime(),random);}
    void sessionRestarted(unsigned index,GameSession&)override{auto& view=*interfaces.at(index);view.screens.reset();view.revision=0;}
    bool pressButton(unsigned index,unsigned id,int intData)override{
        refresh(index);auto& view=*interfaces.at(index);auto& game=runner->session(index);
        for(const auto& key:view.screens->buttons())if(key.id==id&&key.intData==intData){
            auto button=view.screens->button(key);if(!button||!button->visible||!button->active)continue;
            const auto eventKey=std::pair{id,intData};const auto before=view.events[eventKey];
            const auto point=button->screenPosition();const std::uintptr_t pointer=10000+index;
            view.screens->input(PointerPhase::begin,point,pointer,game.renderTime());view.screens->input(PointerPhase::end,point,pointer,game.renderTime());return view.events[eventKey]>before;
        }
        return false;
    }
    void sound(unsigned,std::string_view name,Vec2 p)override{if(!name.empty()&&![audio play:native(name) x:p.x y:p.y])throw std::runtime_error(audio.lastError.UTF8String);}
    void soundID(unsigned index,unsigned id,Vec2 p)override{if(id<22)sound(index,audio::soundFilename(id),p);}
    bool writeSave(unsigned index,std::span<const std::uint8_t> bytes,SaveReason)override{
        NSURL* path=[directory URLByAppendingPathComponent:[NSString stringWithFormat:@"scenario-%03u.sav",index]];NSError* error=nil;
        NSData* data=[NSData dataWithBytes:bytes.data() length:bytes.size()];if(![data writeToURL:path options:NSDataWritingAtomic error:&error])return false;++saveWrites;return true;
    }
    std::uint32_t absoluteTimeLow(unsigned index)override{return 97u+index;}
    std::uint32_t unixTime()override{return 1794268800u;}
    void sampleAudio(){
        unsigned playing=0,progressing=0;NSMutableArray<NSString*>* names=[NSMutableArray array];
        for(NSDictionary* effect in audio.playbackSnapshot)if([effect[@"valid"] boolValue]&&[effect[@"playing"] boolValue]){++playing;if([effect[@"sampleOffset"] integerValue]>0){++progressing;[names addObject:effect[@"name"]];}}
        maxPlaying=std::max(maxPlaying,playing);if(progressing>maxProgressing){maxProgressing=progressing;progressingNames=[names copy];}
    }
};
ConcurrentRuntime::ConcurrentRuntime(NSURL* root,NSURL* directory):impl_(std::make_unique<Impl>(root,directory)){}
ConcurrentRuntime::~ConcurrentRuntime(){[impl_->audio setActive:NO];}
void ConcurrentRuntime::tick(){auto& p=*impl_;if(!p.active)return;const auto begin=std::chrono::steady_clock::now();p.sampleAudio();p.runner->tick();p.tickCost=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();}
void ConcurrentRuntime::draw(CAEAGLLayer* layer,Size viewport){auto& p=*impl_;const auto begin=std::chrono::steady_clock::now();const auto frame=p.runner->drawFrame(viewport);[p.renderer drawFrame:frame layer:layer];++p.presented;p.costs.push_back(p.tickCost+std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count());}
void ConcurrentRuntime::setActive(bool active){auto& p=*impl_;if(!active)for(auto& view:p.interfaces)if(view->screens)view->screens->discardInputs();p.active=active;p.runner->setActive(active);[p.audio setActive:active];}
NSDictionary* ConcurrentRuntime::verifyInactiveInputDiscard(){
    auto& p=*impl_;unsigned index=0;for(;index<p.runner->scenarioCount();++index)if(p.runner->scenarios()[index].role==validation::ScenarioRole::shop)break;
    if(index==p.runner->scenarioCount())throw std::runtime_error("Missing real shop interruption case");
    p.refresh(index);auto& view=*p.interfaces[index];auto& game=p.runner->session(index);
    std::shared_ptr<ui::Button> held;for(const auto& key:view.screens->buttons())if(key.id==2100){auto button=view.screens->button(key);if(button&&button->visible&&button->active){held=button;break;}}
    if(!held)throw std::runtime_error("Missing real shop close button for interruption");
    const auto beforeFrames=p.runner->metrics().frames,beforeWrites=p.saveWrites;const int credit=game.state().credits;const auto screen=game.state().screen;
    const auto point=held->screenPosition();const auto beforeEvents=view.events[std::pair{2100u,0}];
    view.screens->input(PointerPhase::begin,point,900000,game.renderTime());const bool captured=held->capturedPointer()==900000&&held->pressed();
    const auto afterBeginEvents=view.events[std::pair{2100u,0}];setActive(false);const bool discarded=held->capturedPointer()==0&&!held->pressed();setActive(true);
    const bool noCleanupAction=view.events[std::pair{2100u,0}]==afterBeginEvents;
    p.interruption=@{@"frame":@(beforeFrames),@"buttonID":@2100,@"capturedBefore":@(captured),@"discardedAfter":@(discarded),@"beginReachedRealHandler":@(afterBeginEvents>beforeEvents),@"cleanupSentNoAction":@(noCleanupAction),@"noExtraTick":@(p.runner->metrics().frames==beforeFrames),@"noSave":@(p.saveWrites==beforeWrites),@"creditPreserved":@(game.state().credits==credit),@"screenPreserved":@(game.state().screen==screen)};
    return p.interruption;
}
validation::ConcurrentValidation& ConcurrentRuntime::validation(){return *impl_->runner;}
NSDictionary* ConcurrentRuntime::report()const{
    auto& p=*impl_;const auto core=p.runner->metrics();NSMutableArray* rows=[NSMutableArray array];const auto& descriptions=p.runner->scenarios();const auto& observed=p.runner->observations();
    for(std::size_t i=0;i<observed.size();++i){const auto& o=observed[i];NSMutableArray* weapons=[NSMutableArray array];for(const auto& part:o.emittedWeaponParts)[weapons addObject:native(part)];
        NSMutableDictionary* emissions=[NSMutableDictionary dictionary];for(const auto& [part,count]:o.weaponEmissions)emissions[native(part)]=@(count);
        NSMutableArray* navigation=[NSMutableArray array];for(const auto& n:o.navigation)[navigation addObject:@{@"frame":@(n.frame),@"playerX":@(n.player.x),@"playerY":@(n.player.y),@"worldX":@(n.world.x),@"worldY":@(n.world.y),@"screenX":@(n.screen.x),@"screenY":@(n.screen.y),@"lootCode":native(n.lootCode),@"accepted":@(n.accepted),@"blocked":@(n.blocked)}];
        NSMutableArray* pickups=[NSMutableArray array];for(const auto& v:o.pickups)[pickups addObject:@{@"frame":@(v.frame),@"code":native(v.code),@"type":@(v.type),@"value":@(v.value),@"playerX":@(v.player.x),@"playerY":@(v.player.y),@"itemX":@(v.item.x),@"itemY":@(v.item.y),@"creditDelta":@(v.creditDelta)}];
        [rows addObject:@{@"id":@(i),@"label":native(descriptions[i].label),@"ticks":@(o.ticks),@"firstFrame":@(o.firstFrame),@"lastFrame":@(o.lastFrame),@"draws":@(o.draws),@"sounds":@(o.sounds),@"saves":@(o.saves),@"buttonRequests":@(o.buttonRequests),@"buttonFailures":@(o.buttonFailures),@"playerEmissions":@(o.playerEmissions),@"damage":@(o.damage),@"kills":@(o.kills),@"initialLevel":@(o.initialLevel),@"level":@(o.level),@"experience":@(o.experience),@"credit":@(o.credit),@"playerHP":@(o.playerHP),@"aliveTicks":@(o.aliveTicks),@"emittedWeaponParts":weapons,@"weaponEmissions":emissions,@"missionAccepted":@(o.missionAccepted),@"missionComplete":@(o.missionComplete),@"missionRequirementSatisfied":@(o.missionComplete),@"shopPurchased":@(o.shopPurchased),@"insufficientRejected":@(o.insufficientRejected),@"restarted":@(o.restarted),@"distanceMoved":@(o.distanceMoved),@"maxDisplacement":@(o.maxDisplacement),@"navigationRequests":@(o.navigationRequests),@"navigationRejected":@(o.navigationRejected),@"movementFrames":@(o.movementFrames),@"maxObservedDrops":@(o.maxObservedDrops),@"pickupEvents":@(o.pickupEvents),@"coinPickups":@(o.coinPickups),@"itemPickups":@(o.itemPickups),@"pickupCreditGain":@(o.pickupCreditGain),@"inventoryGain":@(o.inventoryGain),@"injuryFrames":@(o.injuryFrames),@"injuryPoints":@(o.injuryPoints),@"initialCredit":@(o.initialCredit),@"initialExperience":@(o.initialExperience),@"initialHP":@(o.initialHP),@"navigation":navigation,@"pickups":pickups}];}
    NSMutableArray* activity=[NSMutableArray array];for(const auto& frame:p.runner->frameActivity()){
        NSMutableArray* buttons=[NSMutableArray array];for(const auto& button:frame.buttons)[buttons addObject:@{@"scenario":@(button.scenario),@"id":@(button.id),@"intData":@(button.intData),@"delivered":@(button.delivered)}];
        [activity addObject:@{@"frame":@(frame.frame),@"tickedScenarios":@(frame.tickedScenarios),@"emittingScenarios":@(frame.emittingScenarios),@"weaponDefinitions":@(frame.emittedWeaponParts.size()),@"weaponKinds":@(frame.emittedWeaponKinds.size()),@"activeProjectiles":@(frame.activeProjectiles),@"combatActiveProjectiles":@(frame.combatActiveProjectiles),@"combatEmittingScenarios":@(frame.combatEmittingScenarios),@"combatDamage":@(frame.combatDamage),@"buttons":buttons,@"saveRestart":@(frame.saveRestart),@"movementLootMoving":@(frame.movementLootMoving),@"movementLootEmissions":@(frame.movementLootEmissions),@"movementLootPickups":@(frame.movementLootPickups),@"movementLootInjury":@(frame.movementLootInjury),@"movementLootCreditGain":@(frame.movementLootCreditGain)}];
    }
    NSMutableArray* readNames=[NSMutableArray array];for(const auto& name:core.warmReadNames)[readNames addObject:native(name)];NSMutableArray* existsNames=[NSMutableArray array];for(const auto& name:core.warmExistsNames)[existsNames addObject:native(name)];
    NSMutableArray* missing=[NSMutableArray array];for(const auto& item:p.runner->missingFeatures())[missing addObject:native(item)];
    auto costs=p.costs;std::sort(costs.begin(),costs.end());const double p95=costs.empty()?0:costs[std::min(costs.size()-1,std::size_t(double(costs.size())*.95))];
    return @{@"inputInterruption":p.interruption?:@{},@"frames":@(core.frames),@"presented":@(p.presented),@"scenarios":@(core.activeScenarios),@"combatScenarios":@(core.combatScenarios),@"activeWeaponDefinitions":@(core.activeWeaponDefinitions),@"activeWeaponKinds":@(core.activeWeaponKinds),@"maxConcurrentWeaponDefinitions":@(core.maxConcurrentWeaponDefinitions),@"maxConcurrentWeaponKinds":@(core.maxConcurrentWeaponKinds),@"menuActionFrames":@(core.menuActionFrames),@"menuFramesWithCombatActivity":@(core.menuFramesWithCombatActivity),@"commonFrameInterval":@(core.commonFrameInterval),@"movementFramesWithCombatActivity":@(core.movementFramesWithCombatActivity),@"movementEmissionFramesWithCombatActivity":@(core.movementEmissionFramesWithCombatActivity),@"pickupFramesWithCombatActivity":@(core.pickupFramesWithCombatActivity),@"injuryFramesWithCombatActivity":@(core.injuryFramesWithCombatActivity),@"warmReadNames":readNames,@"warmExistsNames":existsNames,@"warmReadMisses":@(core.warmReadMisses),@"warmExistsMisses":@(core.warmExistsMisses),@"textureUploadsAfterPreparation":@(p.renderer.textureUploads-p.warmUploads),@"imageTextureUploadsAfterPreparation":@(p.renderer.imageTextureUploads-p.warmImageUploads),@"audioBefore":p.warmAudio,@"audioAfter":p.audio.metrics,@"maxPlayingEffects":@(p.maxPlaying),@"maxProgressingEffects":@(p.maxProgressing),@"progressingEffectNames":p.progressingNames,@"atomicWrites":@(p.saveWrites),@"elapsedSeconds":@(std::chrono::duration<double>(std::chrono::steady_clock::now()-p.started).count()),@"cpuAndSubmissionP95ms":@(p95),@"cases":rows,@"frameActivity":activity,@"missingFeatures":missing,@"completeGameCoverage":@NO};
}
NSDictionary* ConcurrentRuntime::verifyRepeatedRendering(CAEAGLLayer* layer,Size viewport){
    auto& p=*impl_;const auto frame=p.runner->drawFrame(viewport);const auto before=p.renderer.drawCalls;
    [p.renderer captureNextFrame];[p.renderer drawFrame:frame layer:layer];p.legacyComparison=[p.renderer.readRGBA copy];const auto middle=p.renderer.drawCalls;
    [p.renderer captureNextFrame];[p.renderer drawFrame:frame layer:layer];NSData* repeated=p.renderer.readRGBA;
    if(p.legacyComparison.length!=repeated.length)throw std::runtime_error("Repeated rendering changed drawable size");
    const auto* a=static_cast<const std::uint8_t*>(p.legacyComparison.bytes);const auto* b=static_cast<const std::uint8_t*>(repeated.bytes);std::uint64_t different=0;unsigned maximum=0;
    for(std::size_t i=0;i<repeated.length;i+=4){bool changed=false;for(unsigned c=0;c<3;++c){const unsigned d=unsigned(std::abs(int(a[i+c])-int(b[i+c])));maximum=std::max(maximum,d);changed|=d>0;}different+=changed;}
    return @{@"renderPath":@"originalOrderedScissor",@"experimentalBatchingAdopted":@NO,@"sameCachedFrame":@YES,@"sourceSprites":@(frame.sprites.size()),@"firstDrawCalls":@(middle-before),@"repeatDrawCalls":@(p.renderer.drawCalls-middle),@"comparedPixels":@(repeated.length/4),@"differentRGBPixels":@(different),@"maxRGBDifference":@(maximum)};
}
void ConcurrentRuntime::captureNextFrame(){[impl_->renderer captureNextFrame];}
NSData* ConcurrentRuntime::pixels()const{return impl_->renderer.readRGBA;}
NSData* ConcurrentRuntime::legacyComparisonPixels()const{return impl_->legacyComparison;}
CGSize ConcurrentRuntime::drawableSize()const{return impl_->renderer.drawableSize;}
}

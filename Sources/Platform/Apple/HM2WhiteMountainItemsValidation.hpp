#pragma once
#include "Platform/Apple/HM2NativeValidation.hpp"
#include "Platform/Apple/HM2Resources.hpp"
#include "Core/Progression/Progression.hpp"
#include "Core/Presentation/MenuArtwork.hpp"
#include "Core/Presentation/MenuGridLayout.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include <array>
#include <limits>

namespace hm2::apple {
class WhiteMountainItemsValidation final : public NativeValidationScenario {
    static constexpr std::array<std::string_view,25> weapons_{
        "10311","10312","10313","10314","10315","10316","10317","10318","10319","10320","10321",
        "10331","10332","10333","10334","10335","10336","10337","10338","10339","10340",
        "10501","10502","10503","10504"
    };
    static constexpr std::array<std::string_view,3> pickerCodes_{"04204","04205","04206"};
    static constexpr std::array<std::string_view,3> pickerModels_{"Bot_P04","Bot_P05","Bot_P06"};
    static constexpr int laserPurchaseLevel_ = 22;
    enum class Step { weaponSetup, weaponTarget, weaponResult, shopOpening, laserStock, shopBuy,
                      quickSetup, quickResult, pickerSetup, pickerResult };
    NSURL* root_;
    NSURL* output_;
    DisplayConfiguration display_;
    std::unique_ptr<GameRuntime> runtime_;
    NSMutableArray* results_ = [NSMutableArray new];
    Step step_ = Step::weaponSetup;
    unsigned index_ = 0, pickerIndex_ = 0, tick_ = 0, stepAt_ = 0, lastAttack_ = 0;
    unsigned sampleOffset_ = 0;
    std::uintptr_t pointer_ = 100;
    std::uint64_t target_ = 0;
    int initialHP_ = 0, lowestHP_ = 0;
    bool active_ = true, audio_ = true, done_ = false, airstrikeCapture_ = false;
    std::string part_;

    static NSString* string(std::string_view value) {
        return [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];
    }
    static void require(bool ok, std::string_view message) {
        if(!ok) throw std::runtime_error(std::string(message));
    }
    void advance(Step next) { step_=next; stepAt_=tick_; }
    std::string_view field() const { return display_.compact?"Stage01_04":"Stage01_01"; }
    bool settled(ui::ScreenId screen) const {
        return runtime_->session().screenTransition().phase==8 && runtime_->session().state().screen==screen;
    }
    void report(NSString* status, NSString* failure=@"") {
        NSDictionary* record=@{
            @"status":status,@"failure":failure,@"expected":@(weapons_.size()+pickerCodes_.size()+1),
            @"cases":results_,@"field":string(field()),@"weaponIndex":@(index_),@"pickerIndex":@(pickerIndex_),@"tick":@(tick_),
            @"step":@(static_cast<unsigned>(step_)),@"canvasWidth":@(display_.canvas.width),@"canvasHeight":@(display_.canvas.height)
        };
        NSData* data=[NSJSONSerialization dataWithJSONObject:record options:NSJSONWritingPrettyPrinted|NSJSONWritingSortedKeys error:nil];
        require(data && [data writeToURL:[output_ URLByAppendingPathComponent:@"items-report.json"] atomically:YES],
                "Cannot write White Mountain item validation report");
    }
    void capture(CAEAGLLayer* layer, NSString* name) {
        captureValidationFrame(*runtime_,layer,[output_ URLByAppendingPathComponent:name]);
    }
    std::optional<ui::ButtonKey> button(unsigned id, std::string_view identity={}, std::optional<int> argument={}) {
        auto& screens=runtime_->screens();
        const auto buttons=screens.buttons();
        const auto it=std::find_if(buttons.begin(),buttons.end(),[&](const auto& key){
            return key.id==id && (identity.empty()||key.dataId==identity) && (!argument||key.intData==*argument);
        });
        if(it==buttons.end()) return {};
        screens.revealButton(*it);
        return *it;
    }
    bool press(unsigned id, std::string_view identity={}, std::optional<int> argument={}) {
        const auto key=button(id,identity,argument);
        if(!key) return false;
        const auto p=runtime_->screens().buttonScreenPosition(*key);
        const auto pointer=++pointer_;
        const bool began=runtime_->input(PointerPhase::begin,p,pointer);
        const bool ended=runtime_->input(PointerPhase::end,p,pointer);
        return began&&ended;
    }
    bool worldTap(Vec2 world) {
        auto& game=runtime_->session();
        const auto p=game.worldCanvasPoint(game.battle()->camera().toScreen(world));
        const auto pointer=++pointer_;
        if(!runtime_->input(PointerPhase::begin,p,pointer)) return false;
        (void)runtime_->input(PointerPhase::end,p,pointer);
        const auto trace=runtime_->inputHistory();
        return trace.size()>=2&&trace[trace.size()-2].worldConsumed;
    }
    static float distance2(Vec2 a, Vec2 b) {
        const float x=a.x-b.x,y=a.y-b.y;return x*x+y*y;
    }
    std::optional<CombatEnemySnapshot> focusEnemy(bool needSelection, float maxDistance=std::numeric_limits<float>::infinity()) {
        auto& game=runtime_->session();
        auto& battle=*game.battle();
        auto enemies=battle.enemies();
        const auto player=battle.player().position;
        std::stable_sort(enemies.begin(),enemies.end(),[&](const auto& a,const auto& b){
            return distance2(a.position,player)<distance2(b.position,player);
        });
        const auto usable=display_.content();
        for(const auto& enemy:enemies) {
            if(enemy.hp<=0||distance2(enemy.position,player)>maxDistance*maxDistance)continue;
            const auto p=game.worldCanvasPoint(battle.camera().toScreen(enemy.position));
            if(p.x<usable.x+20||p.x>usable.x+usable.width-20||p.y<usable.y+46||p.y>usable.y+usable.height-22)continue;
            if(needSelection&&(!worldTap(enemy.position)||battle.selectedEnemy()!=enemy.id))continue;
            return enemy;
        }
        return {};
    }
    std::optional<unsigned> laserSample() const {
        for(NSDictionary* entry in runtime_->audioPlaybackSnapshot())
            if([entry[@"name"] isEqualToString:@"Wpn_Laser.wav"]&&[entry[@"playing"] boolValue])
                return [entry[@"sampleOffset"] unsignedIntValue];
        return {};
    }
    void seedSlot(SaveData& save, unsigned index, std::string_view bot={}) {
        save.initializeSlot(index,"SnowItemsQA","kr",1794268800);
        progression::SlotView slot(save.slot(index));
        slot.string(0x2d,16,"Base004");
        slot.string(0x55,16,"Base004");
        slot.u32(0x1b5c,5);
        slot.level(index==2?laserPurchaseLevel_-1:index==1?40:99);
        slot.experience(progression::Experience::bounds(slot.level()).previous);
        slot.hp(3236);slot.maxHP(3236);slot.credit(20000000);
        constexpr std::array<std::string_view,5> gear{"11023","12023","10011","10111","13006"};
        for(unsigned i=0;i<gear.size();++i) {
            slot.string(0x8c+6*i,6,i==2&&!bot.empty()?"10504":gear[i]);
            slot.byte(0x53c+i,1);
            slot.u32(0x604+4*i,i);
        }
        slot.u32(0x604+4*3,std::uint32_t(-1));
        slot.u32(0x604+4*5,std::uint32_t(-1));
        for(unsigned i=0;i<5;++i)slot.string(0x61c+6*i,6,"");
        for(unsigned i=0;i<3;++i){slot.string(0x63a+6*i,6,i==0?bot:"");slot.u32(0x64c+4*i,0);}
        if(index!=0||!bot.empty())return;
        slot.u32(0x88,3);
        for(unsigned i=0;i<weapons_.size();++i){slot.string(0x8c+6*(8+i),6,weapons_[i]);slot.byte(0x53c+8+i,1);}
    }
    void openFixture(std::string_view bot={}) {
        SaveData save;
        seedSlot(save,0,bot);
        if(bot.empty()){seedSlot(save,1);seedSlot(save,2);}
        DarwinWorldRandom random(731);
        const auto bytes=save.encode(random,731);
        NSString* name=bot.empty()?@"items.sav":[string(bot) stringByAppendingString:@".sav"];
        NSURL* path=[output_ URLByAppendingPathComponent:name];
        require([[NSData dataWithBytes:bytes.data() length:bytes.size()] writeToURL:path atomically:YES],"Could not write isolated item fixture");
        runtime_=std::make_unique<GameRuntime>(root_,path,[](const NativeRequest& request){
            if(request.kind==NativeRequest::Kind::gameOver||request.kind==NativeRequest::Kind::saveError)
                throw std::runtime_error("Item validation runtime failed: "+request.text);
        },[]{return 731u;});
        runtime_->setDisplay(display_);
        runtime_->setAudioActive(audio_);
        require(runtime_->session().selectSlot(0),"Could not open isolated item fixture");
    }
    void weaponSetup() {
        auto& game=runtime_->session();
        if(index_==weapons_.size()){
            require(game.selectSlot(1),"Cannot load the White Mountain shop fixture");
            game.enterBase();runtime_->synchronize();
            require(press(2002)&&press(2002),"Could not select and open the native shop");
            advance(Step::shopOpening);return;
        }
        require(game.equip(int(index_+8),2),"Could not equip an owned inactive weapon");
        require(game.enterField(field(),0),"Could not load the original weapon validation field");
        runtime_->synchronize();
        require(game.state().equipment[2].dataId==weapons_[index_]&&game.state().equipment[3].dataId.empty(),
                "Expected only this weapon in the Player weapon slots");
        const auto slots=game.battle()->playerWeapons();
        require(slots.size()==1&&slots[0].inventoryEquipSlot==2,"Unexpected actual mounted weapon configuration");
        part_=slots[0].part;sampleOffset_=0;target_=0;
        advance(Step::weaponTarget);
    }
    void weaponTarget() {
        if(!settled(ui::ScreenId::field)||laserSample())return;
        if(const auto target=focusEnemy(true,350)){
            target_=target->id;initialHP_=lowestHP_=target->hp;
            advance(Step::weaponResult);
        }
    }
    void weaponResult(CAEAGLLayer* layer) {
        auto& battle=*runtime_->session().battle();
        const auto count=battle.metrics().playerWeaponEmissions.at(part_);
        if(count)sampleOffset_=std::max(sampleOffset_,laserSample().value_or(0));
        for(const auto& enemy:battle.enemies())if(enemy.id==target_){
            lowestHP_=std::min(lowestHP_,enemy.hp);
            // A second native tap on the same selected target enables the
            // original movement-to-target branch if it wanders out of range.
            if(!count&&enemy.hp>0&&(tick_-stepAt_)%90==0)(void)worldTap(enemy.position);
        }
        if(!count||lowestHP_>=initialHP_||!sampleOffset_)return;
        const auto code=weapons_[index_];
        if(code=="10311"||code=="10340"||code=="10501"||code=="10504")
            capture(layer,[string(code) stringByAppendingString:@"-combat.png"]);
        [results_ addObject:@{@"kind":@"weapon",@"code":string(code),@"part":string(part_),@"emissions":@(count),
                              @"targetHPBefore":@(initialHP_),@"targetHPAfter":@(lowestHP_),@"wav":@"Wpn_Laser.wav",@"sampleOffset":@(sampleOffset_)}];
        ++index_;report(@"running");advance(Step::weaponSetup);
    }
    bool stock(std::string_view id) {
        const auto& initial=runtime_->session().state().shopItems;
        const auto item=std::find_if(initial.begin(),initial.end(),[&](const auto& value){return value.dataId==id;});
        if(item==initial.end())return false;
        const auto index=unsigned(item-initial.begin());
        const auto capacity=menu::shopStockPageSize(menu::phone(runtime_->session().display()));
        const int desired=int(index/capacity);
        for(unsigned attempt=0;attempt<20;++attempt){
            const auto current=runtime_->session().state().shopPage;
            if(current==desired)return press(2200+index%capacity,id);
            if(!press(current<desired?2106:2105))return false;
        }
        return false;
    }
    void shopOpening(CAEAGLLayer* layer) {
        if(!settled(ui::ScreenId::shop))return;
        require(stock("10501"),"Native stock did not show the newly enabled Sniper");
        capture(layer,@"sniper-shop.png");
        auto& game=runtime_->session();
        require(game.selectSlot(2),"Cannot load the level-22 laser shop fixture");
        game.enterBase();runtime_->synchronize();
        require(press(2002)&&press(2002),"Could not open the level-22 native shop");
        advance(Step::laserStock);
    }
    void laserStock(CAEAGLLayer* layer) {
        if(!settled(ui::ScreenId::shop))return;
        auto& game=runtime_->session();
        require(game.state().level+1==laserPurchaseLevel_,"Unexpected level for laser stock verification");
        for(unsigned i=0;i<5&&game.state().shopCategory!=3;++i){
            const int desired=button(2104,{},3)?3:(game.state().shopCategory+1)%5;
            // Phone arrow keys change on Push, so their identity changes
            // before the original pointer End is delivered.
            (void)press(2104,{},desired);
            require(game.state().shopCategory==desired,"Could not open native Attack stock");
        }
        require(game.state().shopCategory==3&&stock("02006"),"New Laser Air-Strike absent from native Attack stock");
        capture(layer,@"laser-airstrike-shop.png");
        require(press(2102)&&game.state().tradeVisible&&game.state().tradeItemId=="02006","Native airstrike BUY did not open the expected trade");
        advance(Step::shopBuy);
    }
    void shopBuy(CAEAGLLayer* layer) {
        auto& game=runtime_->session();
        const auto before=game.state().credits;
        capture(layer,@"laser-airstrike-buy.png");
        require(press(2108)&&!game.state().tradeVisible&&game.state().credits==before-30,"Native BUY did not charge the original airstrike price");
        require(game.enterField(field(),0),"Could not load the field for purchased Laser Air-Strike");
        runtime_->synchronize();advance(Step::quickSetup);
    }
    void registerQuick() {
        auto& game=runtime_->session();
        require(press(1025)&&game.state().inventoryVisible,"Could not open field inventory to assign purchased airstrike");
        const auto keys=runtime_->screens().buttons();
        const auto item=std::find_if(keys.begin(),keys.end(),[](const auto& k){return k.id>=6000&&k.id<=6024&&k.dataId=="02006";});
        require(item!=keys.end(),"Purchased Laser Air-Strike missing from native inventory");
        const auto source=button(item->id,"02006"),target=button(6200);
        require(source&&target,"Native quick-slot drag points missing");
        const auto a=runtime_->screens().buttonScreenPosition(*source),b=runtime_->screens().buttonScreenPosition(*target);
        const auto pointer=++pointer_;
        require(runtime_->input(PointerPhase::begin,a,pointer),"Native airstrike icon could not begin drag");
        (void)runtime_->input(PointerPhase::move,{(a.x+b.x)*.5f,(a.y+b.y)*.5f},pointer);
        (void)runtime_->input(PointerPhase::move,b,pointer);
        (void)runtime_->input(PointerPhase::end,b,pointer);
        require(game.state().quick[0].dataId=="02006"&&game.state().quick[0].count==1,"Native drag did not bind purchased airstrike to first quick slot");
        // Close is subscribed to Push. Its own button is removed before End.
        (void)press(6092);
        require(!game.state().inventoryVisible,"Could not close field inventory after airstrike assignment");
    }
    void quickSetup(CAEAGLLayer* layer) {
        if(!settled(ui::ScreenId::field)||laserSample())return;
        const auto target=focusEnemy(false);
        if(!target)return;
        registerQuick();
        capture(layer,@"laser-airstrike-quick-slot.png");
        require(press(6200,"02006")&&worldTap(target->position),"Native airstrike quick input was rejected");
        require(runtime_->session().state().quick[0].count==0,"Airstrike use did not consume the purchased unit");
        sampleOffset_=0;advance(Step::quickResult);
    }
    void quickResult(CAEAGLLayer* layer) {
        const auto& m=runtime_->session().battle()->metrics();
        if(m.airStrikeProjectiles)sampleOffset_=std::max(sampleOffset_,laserSample().value_or(0));
        if(m.airStrikeProjectiles&&!airstrikeCapture_){capture(layer,@"laser-airstrike-active.png");airstrikeCapture_=true;}
        if(m.airStrikeProjectiles!=10||!m.airStrikeEnemyHPRemoved||!sampleOffset_)return;
        [results_ addObject:@{@"kind":@"airstrike",@"code":@"02006",@"purchasedPrice":@30,@"purchaseLevel":@(laserPurchaseLevel_),
            @"quickCountAfter":@0,@"projectiles":@(m.airStrikeProjectiles),@"enemyHPRemoved":@(m.airStrikeEnemyHPRemoved),
            @"wav":@"Wpn_Laser.wav",@"sampleOffset":@(sampleOffset_)}];
        report(@"running");advance(Step::pickerSetup);
    }
    void pickerSetup() {
        runtime_.reset();
        openFixture(pickerCodes_.at(pickerIndex_));
        require(runtime_->session().enterField(field(),0),"Cannot load the original Picker validation field");
        runtime_->synchronize();lastAttack_=tick_-45;advance(Step::pickerResult);
    }
    void pickerResult(CAEAGLLayer* layer) {
        if(!settled(ui::ScreenId::field))return;
        auto& game=runtime_->session();
        auto& scene=*game.battle();
        const auto bots=scene.bots();
        require(bots.size()==1&&bots[0].name==pickerModels_[pickerIndex_],"Expected acquired Picker model is absent");
        if(bots[0].itemCaptures){
            const auto items=game.fieldItems();
            const auto delivered=std::find_if(items.begin(),items.end(),[](const auto& item){return item.removed;});
            if(delivered!=items.end()){
                capture(layer,[string(pickerCodes_[pickerIndex_]) stringByAppendingString:@"-collected.png"]);
                [results_ addObject:@{@"kind":@"picker",@"code":string(pickerCodes_[pickerIndex_]),
                    @"model":string(bots[0].name),@"captures":@(bots[0].itemCaptures),@"deliveredItem":string(delivered->code),
                    @"kills":@(scene.metrics().kills)}];
                ++pickerIndex_;done_=pickerIndex_==pickerCodes_.size();
                if(done_)require(results_.count==weapons_.size()+1+pickerCodes_.size(),"Missing completed item cases");
                report(done_?@"passed":@"running");
                advance(Step::pickerSetup);return;
            }
        }
        if(!scene.selectedEnemy()&&tick_-lastAttack_>=45){
            if(focusEnemy(true))lastAttack_=tick_;
        }
    }
    void run(CAEAGLLayer* layer) {
        switch(step_){
        case Step::weaponSetup: weaponSetup(); break;
        case Step::weaponTarget: weaponTarget(); break;
        case Step::weaponResult: weaponResult(layer); break;
        case Step::shopOpening: shopOpening(layer); break;
        case Step::laserStock: laserStock(layer); break;
        case Step::shopBuy: shopBuy(layer); break;
        case Step::quickSetup: quickSetup(layer); break;
        case Step::quickResult: quickResult(layer); break;
        case Step::pickerSetup: pickerSetup(); break;
        case Step::pickerResult: pickerResult(layer); break;
        }
    }
public:
    WhiteMountainItemsValidation(NSURL* root, NSURL* output) : root_(root), output_(output) {
        require([NSFileManager.defaultManager createDirectoryAtURL:output withIntermediateDirectories:YES attributes:nil error:nil],"Cannot create item validation directory");
        report(@"running");
    }
    void setDisplay(DisplayConfiguration value) override { display_=value;if(runtime_)runtime_->setDisplay(value); }
    void setActive(bool active) override { active_=active;if(runtime_)runtime_->setActive(active); }
    void setAudioActive(bool active) override { audio_=active;if(runtime_)runtime_->setAudioActive(active); }
    bool step(CAEAGLLayer* layer) override {
        if(done_||!active_||!audio_)return done_;
        try{
            if(!runtime_)openFixture();
            const unsigned batch=step_==Step::pickerResult?8:4;
            for(unsigned i=0;i<batch&&!done_;++i){
                runtime_->tick();++tick_;run(layer);
                require(tick_-stepAt_<(step_==Step::pickerResult?4200u:1000u),"White Mountain item validation timed out");
            }
            runtime_->draw(layer);
        }catch(const std::exception& error){
            if(runtime_)try{capture(layer,@"items-failure.png");}catch(const std::exception&){}
            report(@"failed",string(error.what()));done_=true;
        }
        return done_;
    }
};
}

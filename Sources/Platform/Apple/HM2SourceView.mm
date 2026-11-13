#import "HM2SourceView.h"
#include "Platform/Apple/HM2GameRuntime.hpp"
#include <deque>
#include <memory>
#if DEBUG
#include "Core/Validation/SessionFixtures.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include "Platform/Apple/HM2Resources.hpp"
#endif

@interface HM2SourceDisplayTarget:NSObject
@property(nonatomic,weak) HM2SourceView* view;
- (void)display:(CADisplayLink*)link;
@end
@interface HM2SourceView ()
- (void)display:(CADisplayLink*)link;
- (void)enqueueRequest:(const hm2::apple::NativeRequest&)request;
- (void)startRuntime;
- (void)updateDisplay;
- (void)completeNativeRequest;
- (void)updateTestState;
- (void)failRuntime:(NSString*)message;
@end
@implementation HM2SourceDisplayTarget
- (void)display:(CADisplayLink*)link{[self.view display:link];}
@end
@implementation HM2SourceView {
    std::unique_ptr<hm2::apple::GameRuntime> _runtime;
    std::deque<hm2::apple::NativeRequest> _requests;
    CADisplayLink* _display;
    NSString* _startupError;
    NSURL* _resourceRoot;
    NSURL* _saveFile;
    BOOL _active,_requestInFlight,_uiTesting,_startupErrorShown;
    std::uint64_t _frames,_selectionTouches,_selectedByTouch,_gameOverConfirmTick;
}
+ (Class)layerClass{return CAEAGLLayer.class;}
- (instancetype)initWithFrame:(CGRect)frame {
    if(!(self=[super initWithFrame:frame]))return nil;
    self.multipleTouchEnabled=YES;self.contentScaleFactor=UIScreen.mainScreen.scale;_active=YES;
    CAEAGLLayer* layer=(CAEAGLLayer*)self.layer;layer.opaque=YES;
    layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};
    _resourceRoot=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];
    NSFileManager* manager=NSFileManager.defaultManager;NSURL* directory;
#if DEBUG
    _uiTesting=[NSProcessInfo.processInfo.environment[@"HM2_UI_TESTING"] isEqualToString:@"1"];
    if(_uiTesting){
        // Test sessions are isolated by a validated UUID. Relaunching the same
        // test UUID checks persistence without opening the player's save.
        NSUUID* run=[[NSUUID alloc]initWithUUIDString:NSProcessInfo.processInfo.environment[@"HM2_UI_TEST_RUN"]];if(!run)run=NSUUID.UUID;
        directory=[[[manager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask].firstObject URLByAppendingPathComponent:@"UITests" isDirectory:YES] URLByAppendingPathComponent:run.UUIDString isDirectory:YES];
        self.isAccessibilityElement=YES;self.accessibilityIdentifier=@"hm2.world";self.accessibilityLabel=@"HeavyMach2 Game";
    }else
#endif
    {
        directory=[[[manager URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask].firstObject URLByAppendingPathComponent:@"HeavyMach2" isDirectory:YES] copy];
    }
    NSError* error=nil;
    if(!directory||![manager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error])_startupError=error.localizedDescription?:@"Cannot create the game save directory.";
    _saveFile=[directory URLByAppendingPathComponent:@"HeavyMach2.sav"];
#if DEBUG
    // Only a fresh UUID-isolated UI-test save can receive a starting fixture.
    // Production saves and existing test saves are never replaced here.
    NSString* fixtureName=NSProcessInfo.processInfo.environment[@"HM2_UI_TEST_FIXTURE"];
    if(_uiTesting&&!_startupError&&![manager fileExistsAtPath:_saveFile.path]&&([fixtureName isEqualToString:@"gameOver"]||[fixtureName isEqualToString:@"completedMissions"])){
        try{
            hm2::SaveData fixture;
            if([fixtureName isEqualToString:@"gameOver"])fixture=hm2::validation::gameOverFixture(1794268800);
            else{if(!_resourceRoot)throw std::runtime_error("Missing completed-mission UI fixture resources");hm2::apple::Files files(_resourceRoot.fileSystemRepresentation);fixture=hm2::validation::completedMissionFixture(files,1794268800);}
            hm2::DarwinWorldRandom random(1);const auto bytes=fixture.encode(random,1);NSData* data=[NSData dataWithBytes:bytes.data() length:bytes.size()];
            if(![data writeToURL:_saveFile options:NSDataWritingAtomic error:&error])_startupError=error.localizedDescription?:@"Cannot create isolated UI fixture.";
        }catch(const std::exception& e){_startupError=[NSString stringWithUTF8String:e.what()];}
    }
#endif
    [self startRuntime];
    HM2SourceDisplayTarget* target=[HM2SourceDisplayTarget new];target.view=self;
    _display=[CADisplayLink displayLinkWithTarget:target selector:@selector(display:)];_display.preferredFramesPerSecond=60;
    [_display addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];[self updateTestState];return self;
}
- (void)startRuntime {
    if(_startupError)return;
    if(!_resourceRoot){_startupError=@"Prepared original HD resources are missing from the app.";return;}
    __weak HM2SourceView* weakSelf=self;
    std::function<std::uint32_t()> testTime;
#if DEBUG
    // Reuse the original World.setStage/save clock seam. Fix only the input
    // seed in the isolated fixture; damage, RNG calls and outcomes stay live.
    if(_uiTesting&&([NSProcessInfo.processInfo.environment[@"HM2_UI_TEST_FIXTURE"] isEqualToString:@"gameOver"]||[NSProcessInfo.processInfo.environment[@"HM2_UI_TEST_FIXTURE"] isEqualToString:@"completedMissions"]))testTime=[] {return 1u;};
#endif
    try{_runtime=std::make_unique<hm2::apple::GameRuntime>(_resourceRoot,_saveFile,[weakSelf](const hm2::apple::NativeRequest& request){[weakSelf enqueueRequest:request];},std::move(testTime));}
    catch(const std::exception& e){_startupError=[NSString stringWithUTF8String:e.what()];}
    [self updateDisplay];
}
- (void)layoutSubviews{[super layoutSubviews];[self updateDisplay];}
- (void)safeAreaInsetsDidChange{[super safeAreaInsetsDidChange];[self updateDisplay];}
- (void)updateDisplay{
    if(!_runtime||self.bounds.size.width<=0||self.bounds.size.height<=0)return;
    const auto a=self.safeAreaInsets;const auto size=self.bounds.size;
    auto next=hm2::DisplayConfiguration::surface({float(size.width),float(size.height)},{float(a.left),float(a.bottom),float(a.right),float(a.top)});
    const auto& old=_runtime->session().display();
    if(old.enabled&&old.canvas.width==next.canvas.width&&old.canvas.height==next.canvas.height&&old.insets.left==next.insets.left&&old.insets.right==next.insets.right&&old.insets.top==next.insets.top&&old.insets.bottom==next.insets.bottom)return;
    try{_runtime->setDisplay(next);}catch(const std::exception& e){[self failRuntime:[NSString stringWithUTF8String:e.what()]];}
}
- (void)enqueueRequest:(const hm2::apple::NativeRequest&)request{_requests.push_back(request);}
- (void)completeNativeRequest{_requestInFlight=NO;}
- (void)failRuntime:(NSString*)message{_startupError=message;_startupErrorShown=NO;_requestInFlight=NO;_requests.clear();if(_runtime)_runtime->setActive(false);_runtime.reset();}
- (void)presentPendingRequests {
    if(!self.window||!self.controller||self.controller.presentedViewController||_requestInFlight)return;
    __weak HM2SourceView* weakSelf=self;
    if(_startupError){
        if(_startupErrorShown)return;_startupErrorShown=YES;_requestInFlight=YES;UIAlertController* alert=[UIAlertController alertControllerWithTitle:@"HeavyMach 2" message:_startupError preferredStyle:UIAlertControllerStyleAlert];
        [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:^(UIAlertAction* action){[weakSelf completeNativeRequest];}]];
        // An unreadable/corrupt save remains on disk. There is no initialize or
        // overwrite fallback behind this error dialog.
        [self.controller presentViewController:alert animated:YES completion:nil];return;
    }
    if(!_runtime||_requests.empty())return;
    while(!_requests.empty()&&_requests.front().kind==hm2::apple::NativeRequest::Kind::gameOver&&_requests.front().token!=_runtime->session().pendingGameOver())_requests.pop_front();
    if(_requests.empty())return;
    const auto request=_requests.front();_requests.pop_front();_requestInFlight=YES;
    NSString* key=[NSString stringWithUTF8String:request.text.c_str()];
    NSString* message=(request.kind==hm2::apple::NativeRequest::Kind::saveError||request.kind==hm2::apple::NativeRequest::Kind::notice)?key:[NSString stringWithUTF8String:_runtime->nativeText(request.text).c_str()];
    NSString* title=request.title.empty()?@"HeavyMach 2":[NSString stringWithUTF8String:request.title.c_str()];
    if(request.kind==hm2::apple::NativeRequest::Kind::name){
        UIAlertController* alert=[UIAlertController alertControllerWithTitle:@"New User" message:nil preferredStyle:UIAlertControllerStyleAlert];
        [alert addTextFieldWithConfigurationHandler:^(UITextField* field){field.accessibilityIdentifier=@"hm2.name";field.placeholder=@"Name: 1~10 Character";field.autocorrectionType=UITextAutocorrectionTypeNo;field.autocapitalizationType=UITextAutocapitalizationTypeNone;field.clearButtonMode=UITextFieldViewModeWhileEditing;}];
        [alert addAction:[UIAlertAction actionWithTitle:@"cancel" style:UIAlertActionStyleCancel handler:^(UIAlertAction* action){[weakSelf completeNativeRequest];}]];
        __weak UIAlertController* weakAlert=alert;
        [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:^(UIAlertAction* action){HM2SourceView* view=weakSelf;if(view&&view->_runtime){NSString* name=weakAlert.textFields.firstObject.text?:@"";NSString* country=[[NSLocale.currentLocale objectForKey:NSLocaleCountryCode] lowercaseString]?:@"us";try{view->_runtime->provideName(request.slot,name.UTF8String?:"",country.UTF8String?:"us");}catch(const std::exception& e){[view failRuntime:[NSString stringWithUTF8String:e.what()]];}view->_requestInFlight=NO;[view updateTestState];}}]];
        [self.controller presentViewController:alert animated:YES completion:nil];return;
    }
    if(request.kind==hm2::apple::NativeRequest::Kind::deleteSlot){
        UIAlertController* alert=[UIAlertController alertControllerWithTitle:@"Delete Slot" message:[NSString stringWithUTF8String:_runtime->nativeText("DeleteSlot").c_str()] preferredStyle:UIAlertControllerStyleAlert];
        [alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:^(UIAlertAction* action){[weakSelf completeNativeRequest];}]];
        [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDestructive handler:^(UIAlertAction* action){HM2SourceView* view=weakSelf;if(view&&view->_runtime){try{view->_runtime->confirmDelete(request.slot);}catch(const std::exception& e){[view failRuntime:[NSString stringWithUTF8String:e.what()]];}view->_requestInFlight=NO;[view updateTestState];}}]];
        [self.controller presentViewController:alert animated:YES completion:nil];return;
    }
    UIAlertController* alert=[UIAlertController alertControllerWithTitle:title message:message preferredStyle:UIAlertControllerStyleAlert];
    if(request.kind==hm2::apple::NativeRequest::Kind::gameOver){
        [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:^(UIAlertAction* action){HM2SourceView* view=weakSelf;if(view&&view->_runtime){try{if(view->_runtime->confirmGameOver(request.token)&&view->_uiTesting)view->_gameOverConfirmTick=view->_frames;}catch(const std::exception& e){[view failRuntime:[NSString stringWithUTF8String:e.what()]];}view->_requestInFlight=NO;[view updateTestState];}}]];
        [self.controller presentViewController:alert animated:YES completion:nil];return;
    }
    if(request.text=="VisitIndiean"){
        [alert addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:^(UIAlertAction* action){[weakSelf completeNativeRequest];}]];
        [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:^(UIAlertAction* action){[weakSelf completeNativeRequest];[UIApplication.sharedApplication openURL:[NSURL URLWithString:@"http://www.indieapps.kr"] options:@{} completionHandler:nil];}]];
        [self.controller presentViewController:alert animated:YES completion:nil];return;
    }
    [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:^(UIAlertAction* action){[weakSelf completeNativeRequest];}]];
    [self.controller presentViewController:alert animated:YES completion:nil];
}
- (void)display:(CADisplayLink*)link {
    if(!_active||!self.window||self.bounds.size.width<=0||self.bounds.size.height<=0)return;
    if(_runtime){
        try{_runtime->tick();_runtime->draw((CAEAGLLayer*)self.layer);++_frames;}
        catch(const std::exception& e){[self failRuntime:[NSString stringWithUTF8String:e.what()]];}
    }
    [self updateTestState];[self presentPendingRequests];
}
- (void)input:(NSSet<UITouch*>*)touches phase:(hm2::PointerPhase)phase {
    if(!_runtime||!_active)return;
    try{for(UITouch* touch in touches){hm2::Vec2 logical{};const auto identifier=reinterpret_cast<std::uintptr_t>((__bridge void*)touch);
        const auto before=_uiTesting&&_runtime->session().battle()?_runtime->session().battle()->selectedEnemy():0;
        if(_runtime->logicalPoint([touch locationInView:self],logical))_runtime->input(phase,logical,identifier);
        else if(phase==hm2::PointerPhase::end||phase==hm2::PointerPhase::cancel)_runtime->input(hm2::PointerPhase::cancel,{},identifier);
        if(_uiTesting&&phase==hm2::PointerPhase::begin)if(const auto* scene=_runtime->session().battle();scene&&scene->selectedEnemy()!=before){_selectedByTouch=scene->selectedEnemy();++_selectionTouches;}
    }}catch(const std::exception& e){[self failRuntime:[NSString stringWithUTF8String:e.what()]];}[self updateTestState];
}
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{[self input:touches phase:hm2::PointerPhase::begin];}
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{[self input:touches phase:hm2::PointerPhase::move];}
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{[self input:touches phase:hm2::PointerPhase::end];}
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{[self input:touches phase:hm2::PointerPhase::cancel];}
- (void)setActive:(BOOL)active{_active=active;_display.paused=!active;if(_runtime)_runtime->setActive(active);}
- (void)updateTestState {
#if DEBUG
    if(!_uiTesting)return;
    if(!_runtime){self.accessibilityValue=_startupError?[@"error=" stringByAppendingString:_startupError]:@"loading=1";return;}
    auto& game=_runtime->session();const auto& state=game.state();const auto& metrics=game.metrics();
    NSMutableString* value=[NSMutableString stringWithFormat:@"ticks=%llu;screen=%u;selectedSlot=%d;saves=%llu;saveCompletions=%llu;unsupported=%llu;slot0Used=%d;slot1Used=%d;slot2Used=%d;slot0Name=%s",_frames,unsigned(state.screen),state.selectedSlot,metrics.saveRequests,metrics.saveCompletions,metrics.unsupportedActions,state.slots[0].used,state.slots[1].used,state.slots[2].used,state.slots[0].name.c_str()];
    [value appendFormat:@";canvasWidth=%.6f;canvasHeight=%.6f",game.display().canvas.width,game.display().canvas.height];
    const hm2::DisplayLayout layout(game.display());const auto safe=game.display().content();const auto listClip=layout.clip(hm2::DisplayRegion::list).value_or(safe);
    [value appendFormat:@";safeX=%.6f;safeY=%.6f;safeWidth=%.6f;safeHeight=%.6f;listClipY=%.6f;listClipTop=%.6f",safe.x,safe.y,safe.width,safe.height,listClip.y,listClip.y+listClip.height];
    [value appendFormat:@";doorPhase=%d;helpVisible=%d;helpPage=%d;helpGuidance=%d;mapLoadingStep=%d",game.screenTransition().phase,game.help()!=nullptr,game.help()?game.help()->state().page:-1,game.help()?game.help()->state().guidanceVisible:false,game.mapLoading()?game.mapLoading()->state().step:-1];
    [value appendFormat:@";selectionTouches=%llu;touchSelectedEnemy=%llu",_selectionTouches,_selectedByTouch];
    [value appendFormat:@";gameOverConfirmTick=%llu",_gameOverConfirmTick];
    const auto working=game.workingSlot();const auto word=[&](unsigned at){return std::uint32_t(working[at])|(std::uint32_t(working[at+1])<<8)|(std::uint32_t(working[at+2])<<16)|(std::uint32_t(working[at+3])<<24);};
    const auto stringAt=[&](unsigned at,unsigned size){unsigned length=0;while(length<size&&working[at+length])++length;return std::string_view(reinterpret_cast<const char*>(working.data()+at),length);};
    const auto place=stringAt(0x2d,16),base=stringAt(0x55,16);
    [value appendFormat:@";workingHP=%u;maximumHP=%u;location=%.*s;lastBase=%.*s;gameOverToken=%llu",word(0x7c),word(0x80),int(place.size()),place.data(),int(base.size()),base.data(),game.pendingGameOver()];
    int repairItems=0;for(const auto& item:state.inventory)if(item.dataId=="01001")repairItems+=item.count;
    [value appendFormat:@";completedMission=%s;completedRewardCount=%lu;commandListVisible=%d;commandListRevision=%llu;credits=%d;experience=%u;level=%d;repairItems=%d;acceptedCount=%lu",state.completedMission.dataId.c_str(),state.completedMission.rewards.size(),state.commandListVisible,state.commandListRevision,state.credits,word(0x70),state.level,repairItems,state.acceptedMissions.size()];
    if(const auto* scene=game.battle()){const auto& p=scene->player();const auto& m=scene->metrics();[value appendFormat:@";x=%.6f;y=%.6f;destX=%.6f;destY=%.6f;playerHP=%d;emissions=%llu;damage=%llu;kills=%llu;selectedEnemy=%llu;paused=%d;worldTime=%.9f",p.position.x,p.position.y,p.destination.x,p.destination.y,scene->playerHP(),m.playerEmissions,m.damageCalls,m.kills,scene->selectedEnemy(),scene->isPaused(),scene->clock().worldTime()];
        [value appendFormat:@";actorState=%d;gameState=%d;enemyEmissions=%llu",p.state,scene->gameState(),m.enemyEmissions];
        [value appendFormat:@";displayWorldScale=%.6f;fieldInputMinY=%.6f",layout.worldScale(scene->displayZoom()),safe.y+scene->camera().inputMinimumY*layout.worldScale(scene->displayZoom())];
        const auto playerScreen=game.worldCanvasPoint(scene->camera().toScreen(p.position));[value appendFormat:@";playerScreenX=%.6f;playerScreenY=%.6f",playerScreen.x,playerScreen.y];
        unsigned index=0;for(const auto& e:scene->enemies())if(e.hp>0&&!e.corpse){const auto point=game.worldCanvasPoint(scene->camera().toScreen(e.position));[value appendFormat:@";e%u_x=%.6f;e%u_y=%.6f;e%u_id=%llu",index,point.x,index,point.y,index,e.id];++index;}[value appendFormat:@";enemyCount=%u",index];
    }
    for(const auto& key:_runtime->screens().buttons()){const auto b=_runtime->screens().button(key);if(!b||!b->visible||!b->active)continue;const auto p=_runtime->screens().buttonScreenPosition(key);[value appendFormat:@";b%u_%d_x=%.6f;b%u_%d_y=%.6f",key.id,key.intData,p.x,key.id,key.intData,p.y];const auto original=b->screenPosition();[value appendFormat:@";b%u_%d_hdX=%.6f;b%u_%d_hdY=%.6f",key.id,key.intData,original.x,key.id,key.intData,original.y];}
    self.accessibilityValue=value;
#endif
}
- (void)dealloc{[_display invalidate];if(_runtime)_runtime->setActive(false);}
@end

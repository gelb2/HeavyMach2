#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGL.h>
#import <AVFAudio/AVFAudio.h>
#import "HM2SourceView.h"
#include "Platform/Apple/HM2ConcurrentRuntime.hpp"
#include <memory>
#include <cstdint>

#ifndef HM2_VALIDATION
#define HM2_VALIDATION 0
#endif

// Source uses the shared GPU game runtime. Validation runs independent
// restored sessions on the same display clock.
#if HM2_VALIDATION
@interface HM2PreviewView:UIView
- (void)setActive:(BOOL)active;
- (void)setAudioActive:(BOOL)active;
- (void)display:(CADisplayLink*)link;
@end
@interface HM2DisplayTarget:NSObject
@property(nonatomic,weak)HM2PreviewView* view;
- (void)display:(CADisplayLink*)link;
@end
@implementation HM2DisplayTarget
- (void)display:(CADisplayLink*)link{[self.view display:link];}
@end
@implementation HM2PreviewView {
    CADisplayLink* _display;

    std::unique_ptr<hm2::apple::ConcurrentRuntime> _concurrent;
    NSURL* _validationDirectory;
    BOOL _validationFinished;
    BOOL _uiTesting;
}
+ (Class)layerClass{return CAEAGLLayer.class;}
- (instancetype)initWithFrame:(CGRect)frame {
    if(!(self=[super initWithFrame:frame]))return nil;
    self.multipleTouchEnabled=YES;
    self.contentScaleFactor=UIScreen.mainScreen.scale;
    CAEAGLLayer* layer=(CAEAGLLayer*)self.layer;layer.opaque=YES;
    layer.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};
    NSURL* root=[NSBundle.mainBundle URLForResource:@"HD" withExtension:nil];
    if(!root)throw std::runtime_error("Prepared original HD resources are missing from app");
    NSURL* documents=[NSFileManager.defaultManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
    _validationDirectory=[[documents URLByAppendingPathComponent:@"ConcurrentValidation" isDirectory:YES] URLByAppendingPathComponent:NSUUID.UUID.UUIDString isDirectory:YES];
    _concurrent=std::make_unique<hm2::apple::ConcurrentRuntime>(root,_validationDirectory);
    _uiTesting=[NSProcessInfo.processInfo.environment[@"HM2_UI_TESTING"] isEqualToString:@"1"];
    if(_uiTesting){self.isAccessibilityElement=YES;self.accessibilityIdentifier=@"hm2.world";self.accessibilityLabel=@"HeavyMach2 World";[self updateTestState];}
    HM2DisplayTarget* target=[HM2DisplayTarget new];target.view=self;
    _display=[CADisplayLink displayLinkWithTarget:target selector:@selector(display:)];
    _display.preferredFramesPerSecond=60;[_display addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
    return self;
}
- (void)layoutSubviews{[super layoutSubviews];[self updateDisplay];}
- (void)safeAreaInsetsDidChange{[super safeAreaInsetsDidChange];[self updateDisplay];}
- (void)updateDisplay{
    if(!_concurrent||self.bounds.size.width<=0||self.bounds.size.height<=0)return;
    const auto a=self.safeAreaInsets;const auto size=self.bounds.size;
    auto next=hm2::DisplayConfiguration::surface({float(size.width),float(size.height)},{float(a.left),float(a.bottom),float(a.right),float(a.top)});
    next.device=self.traitCollection.userInterfaceIdiom==UIUserInterfaceIdiomPad?hm2::DisplayDevice::tablet:hm2::DisplayDevice::phone;
    const auto& old=_concurrent->validation().session(0).display();
    if(old.enabled&&old.device==next.device&&old.canvas.width==next.canvas.width&&old.canvas.height==next.canvas.height&&old.insets.left==next.insets.left&&old.insets.right==next.insets.right&&old.insets.top==next.insets.top&&old.insets.bottom==next.insets.bottom)return;
    _concurrent->setDisplay(next);
}
- (void)updateTestState {
    if(!_uiTesting||!_concurrent)return;const auto m=_concurrent->validation().metrics();
    self.accessibilityValue=[NSString stringWithFormat:@"frames=%llu;scenarios=%lu;weaponDefinitions=%lu;weaponKinds=%lu",m.frames,(unsigned long)m.activeScenarios,(unsigned long)m.activeWeaponDefinitions,(unsigned long)m.activeWeaponKinds];

}
- (void)display:(CADisplayLink*)link {
    if(!_validationFinished&&self.window&&self.bounds.size.width>0&&self.bounds.size.height>0){
        _concurrent->tick();const bool finished=_concurrent->validation().metrics().frames>=hm2::validation::displayValidationFrames(_concurrent->validation().session(0).display());
        if(finished)_concurrent->captureNextFrame();
        _concurrent->draw((CAEAGLLayer*)self.layer,{float(self.bounds.size.width),float(self.bounds.size.height)});[self updateTestState];
        if(finished){
            _validationFinished=YES;_display.paused=YES;NSDictionary* report=_concurrent->report();_concurrent->setActive(false);
            NSError* error=nil;NSData* data=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted|NSJSONWritingSortedKeys error:&error];
            NSURL* output=[_validationDirectory URLByAppendingPathComponent:@"report.json"];
            if(!data||![data writeToURL:output options:NSDataWritingAtomic error:&error])NSLog(@"HM2 validation report failed: %@",error);
            else NSLog(@"HM2 validation finished%lluframes: %@",_concurrent->validation().metrics().frames,output.path);
        }
    }

}
- (void)setActive:(BOOL)active{_display.paused=!active;
    if(!_validationFinished)_concurrent->setActive(active);else _display.paused=YES;

}
- (void)setAudioActive:(BOOL)active{if(!_validationFinished)_concurrent->setAudioActive(active);}
- (void)dealloc{[_display invalidate];}
@end
#endif

@interface HM2Controller:UIViewController @end
@implementation HM2Controller
- (void)loadView {
    // Hosted XCTest creates its own real surface/resources and never starts an
    // unsolicited game or opens user save/audio state in the host application.
    if(NSClassFromString(@"XCTestCase")){self.view=[[UIView alloc]initWithFrame:UIScreen.mainScreen.bounds];return;}
#if HM2_VALIDATION
    self.view=[[HM2PreviewView alloc]initWithFrame:UIScreen.mainScreen.bounds];
#else
    HM2SourceView* game=[[HM2SourceView alloc]initWithFrame:UIScreen.mainScreen.bounds];game.controller=self;self.view=game;
#endif
}
- (BOOL)prefersStatusBarHidden{return YES;}
- (UIInterfaceOrientationMask)supportedInterfaceOrientations{return UIDevice.currentDevice.userInterfaceIdiom==UIUserInterfaceIdiomPad?UIInterfaceOrientationMaskPortrait:UIInterfaceOrientationMaskLandscape;}
@end

@interface HM2SceneDelegate:UIResponder<UIWindowSceneDelegate>
@property(nonatomic,strong)UIWindow* window;
- (BOOL)reactivateForegroundAudioSession;
- (void)recoverForegroundAudioSession;
@end
@implementation HM2SceneDelegate {
    BOOL _sceneActive,_audioInterrupted,_audioActivationNeeded;
    std::uint64_t _audioStateRevision;
}
- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options {
    if(![scene isKindOfClass:UIWindowScene.class])return;
    self.window=[[UIWindow alloc]initWithWindowScene:(UIWindowScene*)scene];
    self.window.rootViewController=[HM2Controller new];[self.window makeKeyAndVisible];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(audioInterrupted:) name:AVAudioSessionInterruptionNotification object:nil];
}
- (void)setGameActive:(BOOL)active {
    _sceneActive=active;
    UIView* view=self.window.rootViewController.view;
    // Scene activity owns rendering/input. Audio interruption pauses combat
    // through the common runtime while foreground menus continue responding.
    if([view respondsToSelector:@selector(setActive:)])[(HM2SourceView*)view setActive:_sceneActive];
    if([view respondsToSelector:@selector(setAudioActive:)])[(HM2SourceView*)view setAudioActive:_sceneActive&&!_audioInterrupted];
}
- (BOOL)reactivateForegroundAudioSession {
    // This is OS session ownership, not an effect player/context replacement.
    // An interruption Begin has no guaranteed matching Ended notification.
    // Retry at actual foreground/Ended boundaries, never on display callbacks.
    NSError* error=nil;const BOOL ready=[AVAudioSession.sharedInstance setActive:YES error:&error];
    NSLog(@"HM2_AUDIO_SESSION_REACTIVATE ready=%d error=%ld sceneActive=%d",ready,(long)error.code,_sceneActive);
    return ready;
}
- (void)recoverForegroundAudioSession {
    if(!_sceneActive)return;
    const auto revision=_audioStateRevision;const BOOL ready=[self reactivateForegroundAudioSession];
    // A synchronous OS call may deliver a newer interruption/background event.
    // Its state wins over the result of the older activation attempt.
    if(!_sceneActive||revision!=_audioStateRevision)return;
    _audioInterrupted=!ready;_audioActivationNeeded=!ready;
}
- (void)audioInterrupted:(NSNotification*)notification {
    // Session interruption can begin while the Scene is still foreground.
    // Ending the interruption must never reactivate a background Scene.
    if(!NSThread.isMainThread){__weak HM2SceneDelegate* weakSelf=self;dispatch_async(dispatch_get_main_queue(),^{[weakSelf audioInterrupted:notification];});return;}
    NSNumber* type=notification.userInfo[AVAudioSessionInterruptionTypeKey];if(!type)return;
    if(type.unsignedIntegerValue!=AVAudioSessionInterruptionTypeBegan&&type.unsignedIntegerValue!=AVAudioSessionInterruptionTypeEnded)return;
    ++_audioStateRevision;_audioActivationNeeded=YES;
    if(type.unsignedIntegerValue==AVAudioSessionInterruptionTypeBegan){
        const NSNumber* reason=notification.userInfo[AVAudioSessionInterruptionReasonKey];
        const BOOL suspended=[notification.userInfo[AVAudioSessionInterruptionWasSuspendedKey] boolValue]||(reason&&reason.unsignedIntegerValue==AVAudioSessionInterruptionReasonAppWasSuspended);
        // The legacy suspension reason describes the prior background period,
        // not a new call. A delayed report must not freeze an already restored
        // foreground or override a newer ordinary interruption still pending.
        if(suspended){
            if(_sceneActive&&!_audioInterrupted)[self recoverForegroundAudioSession];
        }else _audioInterrupted=YES;
    }else if(type.unsignedIntegerValue==AVAudioSessionInterruptionTypeEnded){
        if(_sceneActive)[self recoverForegroundAudioSession];else _audioInterrupted=NO;
    }
    [self setGameActive:_sceneActive];
}
- (void)sceneWillResignActive:(UIScene*)scene{++_audioStateRevision;_audioActivationNeeded=YES;[self setGameActive:NO];}
- (void)sceneDidEnterBackground:(UIScene*)scene{++_audioStateRevision;_audioActivationNeeded=YES;[self setGameActive:NO];}
- (void)sceneDidBecomeActive:(UIScene*)scene{
    _sceneActive=YES;
    if(_audioInterrupted||_audioActivationNeeded)[self recoverForegroundAudioSession];
    // Keep a reentrant background change inactive. Becoming active restores
    // the view clock/input; shared GameSession still requires manual Resume.
    [self setGameActive:_sceneActive];
}
- (void)sceneDidDisconnect:(UIScene*)scene{[self setGameActive:NO];self.window=nil;}
- (void)dealloc{[NSNotificationCenter.defaultCenter removeObserver:self];}
@end
@interface HM2AppDelegate:UIResponder<UIApplicationDelegate> @end
@implementation HM2AppDelegate
- (UISceneConfiguration*)application:(UIApplication*)application configurationForConnectingSceneSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options {
    UISceneConfiguration* config=[[UISceneConfiguration alloc]initWithName:@"Game" sessionRole:session.role];
    config.sceneClass=UIWindowScene.class;config.delegateClass=HM2SceneDelegate.class;return config;
}
@end
int main(int argc,char** argv){@autoreleasepool{return UIApplicationMain(argc,argv,nil,NSStringFromClass(HM2AppDelegate.class));}}

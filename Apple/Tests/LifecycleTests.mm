#import <XCTest/XCTest.h>
#import <UIKit/UIKit.h>
#import <AVFAudio/AVFAudio.h>

// Exercise the application's actual Scene interruption handlers without
// opening a game, user save, renderer or audio device in the unit-test host.
@interface HM2SceneDelegate:UIResponder
@property(nonatomic,strong)UIWindow* window;
- (void)sceneWillResignActive:(UIScene*)scene;
- (void)sceneDidEnterBackground:(UIScene*)scene;
- (void)sceneDidBecomeActive:(UIScene*)scene;
- (void)audioInterrupted:(NSNotification*)notification;
- (BOOL)reactivateForegroundAudioSession;
@end
@interface HM2RecoverySceneDelegate:HM2SceneDelegate
@property(nonatomic)BOOL activationSucceeds;
@property(nonatomic)NSUInteger activationAttempts;
@property(nonatomic)BOOL backgroundsDuringActivation;
@property(nonatomic)BOOL interruptsDuringActivation;
@end
@implementation HM2RecoverySceneDelegate
- (instancetype)init{if((self=[super init]))_activationSucceeds=YES;return self;}
- (BOOL)reactivateForegroundAudioSession{++_activationAttempts;if(_backgroundsDuringActivation){[self sceneWillResignActive:nil];[self sceneDidEnterBackground:nil];}if(_interruptsDuringActivation){_interruptsDuringActivation=NO;[self audioInterrupted:[NSNotification notificationWithName:AVAudioSessionInterruptionNotification object:nil userInfo:@{AVAudioSessionInterruptionTypeKey:@(AVAudioSessionInterruptionTypeBegan)}]];}return _activationSucceeds;}
@end
@interface HM2ActivationProbe:UIView
@property(nonatomic)BOOL gameActive;
@property(nonatomic)BOOL audioActive;
- (void)setActive:(BOOL)active;
- (void)setAudioActive:(BOOL)active;
@end
@implementation HM2ActivationProbe
- (void)setActive:(BOOL)active{self.gameActive=active;}
- (void)setAudioActive:(BOOL)active{self.audioActive=active;}
@end

@interface LifecycleTests:XCTestCase @end
@implementation LifecycleTests
- (NSNotification*)interruption:(AVAudioSessionInterruptionType)type {
    return [NSNotification notificationWithName:AVAudioSessionInterruptionNotification object:nil userInfo:@{AVAudioSessionInterruptionTypeKey:@(type)}];
}
- (void)testSceneAndAudioInterruptionNeverResumeBackground {
    HM2RecoverySceneDelegate* delegate=[HM2RecoverySceneDelegate new];
    UIWindow* window=[[UIWindow alloc]initWithFrame:CGRectMake(0,0,912,420)];
    UIViewController* controller=[UIViewController new];HM2ActivationProbe* view=[[HM2ActivationProbe alloc]initWithFrame:window.bounds];controller.view=view;window.rootViewController=controller;delegate.window=window;
    [delegate sceneDidBecomeActive:nil];XCTAssertTrue(view.gameActive);
    [delegate audioInterrupted:[self interruption:AVAudioSessionInterruptionTypeBegan]];XCTAssertTrue(view.gameActive);XCTAssertFalse(view.audioActive);
    // Foreground UI remains usable while a call still owns audio. Combat's
    // manual-pause state is checked through the real GameRuntime separately.
    delegate.activationSucceeds=NO;
    [delegate sceneDidBecomeActive:nil];XCTAssertTrue(view.gameActive);XCTAssertFalse(view.audioActive);
    [delegate sceneWillResignActive:nil];[delegate sceneDidEnterBackground:nil];
    [delegate audioInterrupted:[self interruption:AVAudioSessionInterruptionTypeEnded]];XCTAssertFalse(view.gameActive);
    delegate.activationSucceeds=YES;[delegate sceneDidBecomeActive:nil];XCTAssertTrue(view.gameActive);XCTAssertTrue(view.audioActive);
    // Opposite ordering: interruption ends in foreground; runtime becomes
    // active, but shared GameSession retains the explicit pause/menu state.
    [delegate audioInterrupted:[self interruption:AVAudioSessionInterruptionTypeBegan]];XCTAssertTrue(view.gameActive);XCTAssertFalse(view.audioActive);
    [delegate audioInterrupted:[self interruption:AVAudioSessionInterruptionTypeEnded]];XCTAssertTrue(view.gameActive);XCTAssertTrue(view.audioActive);
    [delegate sceneWillResignActive:nil];[delegate sceneWillResignActive:nil];[delegate sceneDidEnterBackground:nil];XCTAssertFalse(view.gameActive);
    [delegate audioInterrupted:[NSNotification notificationWithName:AVAudioSessionInterruptionNotification object:nil]];XCTAssertFalse(view.gameActive);
}
- (void)testForegroundRecoversWhenInterruptionEndedNeverArrives{
    HM2RecoverySceneDelegate* delegate=[HM2RecoverySceneDelegate new];UIWindow* window=[[UIWindow alloc]initWithFrame:CGRectMake(0,0,912,420)];UIViewController* controller=[UIViewController new];HM2ActivationProbe* view=[HM2ActivationProbe new];controller.view=view;window.rootViewController=controller;delegate.window=window;
    [delegate sceneDidBecomeActive:nil];XCTAssertTrue(view.gameActive);
    [delegate sceneWillResignActive:nil];[delegate sceneDidEnterBackground:nil];[delegate audioInterrupted:[self interruption:AVAudioSessionInterruptionTypeBegan]];XCTAssertFalse(view.gameActive);
    // Apple does not promise a corresponding Ended notification. The next
    // actual foreground activation must consult OS activation, not a timer.
    delegate.activationSucceeds=NO;[delegate sceneDidBecomeActive:nil];XCTAssertTrue(view.gameActive);XCTAssertFalse(view.audioActive);
    delegate.activationSucceeds=YES;[delegate sceneDidBecomeActive:nil];XCTAssertTrue(view.gameActive);XCTAssertTrue(view.audioActive);
    XCTAssertEqual(delegate.activationAttempts,2u);
}
- (void)testForegroundEndedActivationFailureRemainsRecoverable{
    HM2RecoverySceneDelegate* delegate=[HM2RecoverySceneDelegate new];UIWindow* window=[[UIWindow alloc]initWithFrame:CGRectMake(0,0,912,420)];UIViewController* controller=[UIViewController new];HM2ActivationProbe* view=[HM2ActivationProbe new];controller.view=view;window.rootViewController=controller;delegate.window=window;
    [delegate sceneDidBecomeActive:nil];[delegate audioInterrupted:[self interruption:AVAudioSessionInterruptionTypeBegan]];XCTAssertTrue(view.gameActive);XCTAssertFalse(view.audioActive);
    delegate.activationSucceeds=NO;[delegate audioInterrupted:[self interruption:AVAudioSessionInterruptionTypeEnded]];XCTAssertTrue(view.gameActive);XCTAssertFalse(view.audioActive);
    [delegate sceneWillResignActive:nil];[delegate sceneDidEnterBackground:nil];delegate.activationSucceeds=YES;[delegate sceneDidBecomeActive:nil];XCTAssertTrue(view.gameActive);
    XCTAssertEqual(delegate.activationAttempts,2u);
}
- (void)testDelayedSuspensionBeginDoesNotOverrideNewOrdinaryInterruption{
    HM2RecoverySceneDelegate* delegate=[HM2RecoverySceneDelegate new];UIWindow* window=[[UIWindow alloc]initWithFrame:CGRectMake(0,0,912,420)];UIViewController* controller=[UIViewController new];HM2ActivationProbe* view=[HM2ActivationProbe new];controller.view=view;window.rootViewController=controller;delegate.window=window;
    NSNotification* suspended=[NSNotification notificationWithName:AVAudioSessionInterruptionNotification object:nil userInfo:@{AVAudioSessionInterruptionTypeKey:@(AVAudioSessionInterruptionTypeBegan),AVAudioSessionInterruptionReasonKey:@(AVAudioSessionInterruptionReasonAppWasSuspended)}];
    [delegate sceneDidBecomeActive:nil];[delegate audioInterrupted:suspended];XCTAssertTrue(view.gameActive);XCTAssertTrue(view.audioActive);
    [delegate audioInterrupted:[self interruption:AVAudioSessionInterruptionTypeBegan]];XCTAssertTrue(view.gameActive);XCTAssertFalse(view.audioActive);
    const auto attempts=delegate.activationAttempts;[delegate audioInterrupted:suspended];XCTAssertTrue(view.gameActive);XCTAssertFalse(view.audioActive);XCTAssertEqual(delegate.activationAttempts,attempts);
    [delegate audioInterrupted:[self interruption:AVAudioSessionInterruptionTypeEnded]];XCTAssertTrue(view.gameActive);XCTAssertTrue(view.audioActive);
}
- (void)testBackgroundDuringRecoveryCannotRestartView{
    HM2RecoverySceneDelegate* delegate=[HM2RecoverySceneDelegate new];UIWindow* window=[[UIWindow alloc]initWithFrame:CGRectMake(0,0,912,420)];UIViewController* controller=[UIViewController new];HM2ActivationProbe* view=[HM2ActivationProbe new];controller.view=view;window.rootViewController=controller;delegate.window=window;
    [delegate sceneDidBecomeActive:nil];[delegate audioInterrupted:[self interruption:AVAudioSessionInterruptionTypeBegan]];XCTAssertTrue(view.gameActive);XCTAssertFalse(view.audioActive);
    delegate.backgroundsDuringActivation=YES;[delegate sceneDidBecomeActive:nil];XCTAssertFalse(view.gameActive);
    delegate.backgroundsDuringActivation=NO;[delegate sceneDidBecomeActive:nil];XCTAssertTrue(view.gameActive);XCTAssertTrue(view.audioActive);
}
- (void)testBackgroundEndedStillRequiresSuccessfulForegroundActivation{
    HM2RecoverySceneDelegate* delegate=[HM2RecoverySceneDelegate new];UIWindow* window=[[UIWindow alloc]initWithFrame:CGRectMake(0,0,912,420)];UIViewController* controller=[UIViewController new];HM2ActivationProbe* view=[HM2ActivationProbe new];controller.view=view;window.rootViewController=controller;delegate.window=window;
    [delegate sceneDidBecomeActive:nil];[delegate audioInterrupted:[self interruption:AVAudioSessionInterruptionTypeBegan]];[delegate sceneWillResignActive:nil];[delegate sceneDidEnterBackground:nil];
    [delegate audioInterrupted:[self interruption:AVAudioSessionInterruptionTypeEnded]];XCTAssertFalse(view.gameActive);XCTAssertEqual(delegate.activationAttempts,0u);
    delegate.activationSucceeds=NO;[delegate sceneDidBecomeActive:nil];XCTAssertTrue(view.gameActive);XCTAssertFalse(view.audioActive);XCTAssertEqual(delegate.activationAttempts,1u);
    delegate.activationSucceeds=YES;[delegate sceneDidBecomeActive:nil];XCTAssertTrue(view.gameActive);XCTAssertTrue(view.audioActive);XCTAssertEqual(delegate.activationAttempts,2u);
}
- (void)testLegacyDelayedSuspensionKeyAndNewInterruptionDuringRecovery{
    HM2RecoverySceneDelegate* delegate=[HM2RecoverySceneDelegate new];UIWindow* window=[[UIWindow alloc]initWithFrame:CGRectMake(0,0,912,420)];UIViewController* controller=[UIViewController new];HM2ActivationProbe* view=[HM2ActivationProbe new];controller.view=view;window.rootViewController=controller;delegate.window=window;
    NSNotification* suspended=[NSNotification notificationWithName:AVAudioSessionInterruptionNotification object:nil userInfo:@{AVAudioSessionInterruptionTypeKey:@(AVAudioSessionInterruptionTypeBegan),AVAudioSessionInterruptionWasSuspendedKey:@YES}];
    [delegate sceneDidBecomeActive:nil];[delegate audioInterrupted:suspended];XCTAssertTrue(view.gameActive);XCTAssertTrue(view.audioActive);
    [delegate audioInterrupted:[self interruption:AVAudioSessionInterruptionTypeBegan]];XCTAssertTrue(view.gameActive);XCTAssertFalse(view.audioActive);
    delegate.interruptsDuringActivation=YES;[delegate sceneDidBecomeActive:nil];XCTAssertTrue(view.gameActive);XCTAssertFalse(view.audioActive); // New Begin wins over the older successful activation.
    [delegate audioInterrupted:suspended];XCTAssertTrue(view.gameActive);XCTAssertFalse(view.audioActive); // An old suspension cannot clear that new ordinary Begin.
    [delegate audioInterrupted:[self interruption:AVAudioSessionInterruptionTypeEnded]];XCTAssertTrue(view.gameActive);XCTAssertTrue(view.audioActive);
}
@end

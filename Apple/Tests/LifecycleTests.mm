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
@end
@interface HM2ActivationProbe:UIView
@property(nonatomic)BOOL gameActive;
- (void)setActive:(BOOL)active;
@end
@implementation HM2ActivationProbe
- (void)setActive:(BOOL)active{self.gameActive=active;}
@end

@interface LifecycleTests:XCTestCase @end
@implementation LifecycleTests
- (NSNotification*)interruption:(AVAudioSessionInterruptionType)type {
    return [NSNotification notificationWithName:AVAudioSessionInterruptionNotification object:nil userInfo:@{AVAudioSessionInterruptionTypeKey:@(type)}];
}
- (void)testSceneAndAudioInterruptionNeverResumeBackground {
    HM2SceneDelegate* delegate=[HM2SceneDelegate new];
    UIWindow* window=[[UIWindow alloc]initWithFrame:CGRectMake(0,0,912,420)];
    UIViewController* controller=[UIViewController new];HM2ActivationProbe* view=[[HM2ActivationProbe alloc]initWithFrame:window.bounds];controller.view=view;window.rootViewController=controller;delegate.window=window;
    [delegate sceneDidBecomeActive:nil];XCTAssertTrue(view.gameActive);
    [delegate audioInterrupted:[self interruption:AVAudioSessionInterruptionTypeBegan]];XCTAssertFalse(view.gameActive);
    // Foreground callback while a call still owns audio must remain inactive.
    [delegate sceneDidBecomeActive:nil];XCTAssertFalse(view.gameActive);
    [delegate sceneWillResignActive:nil];[delegate sceneDidEnterBackground:nil];
    [delegate audioInterrupted:[self interruption:AVAudioSessionInterruptionTypeEnded]];XCTAssertFalse(view.gameActive);
    [delegate sceneDidBecomeActive:nil];XCTAssertTrue(view.gameActive);
    // Opposite ordering: interruption ends in foreground; runtime becomes
    // active, but shared GameSession retains the explicit pause/menu state.
    [delegate audioInterrupted:[self interruption:AVAudioSessionInterruptionTypeBegan]];XCTAssertFalse(view.gameActive);
    [delegate audioInterrupted:[self interruption:AVAudioSessionInterruptionTypeEnded]];XCTAssertTrue(view.gameActive);
    [delegate sceneWillResignActive:nil];[delegate sceneWillResignActive:nil];[delegate sceneDidEnterBackground:nil];XCTAssertFalse(view.gameActive);
    [delegate audioInterrupted:[NSNotification notificationWithName:AVAudioSessionInterruptionNotification object:nil]];XCTAssertFalse(view.gameActive);
}
@end

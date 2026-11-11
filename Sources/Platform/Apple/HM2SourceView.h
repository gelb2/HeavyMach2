#import <UIKit/UIKit.h>

// UIKit owns the window, touch delivery and native name/confirmation dialogs.
// The game, HUD, controls and text remain in GameRuntime's common GPU path.
@interface HM2SourceView:UIView
@property(nonatomic,weak) UIViewController* controller;
- (void)setActive:(BOOL)active;
- (void)presentPendingRequests;
@end

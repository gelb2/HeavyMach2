#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGL.h>
#import "HM2Renderer.h"
#include "HM2Resources.hpp"
#include "Core/Simulation/LocomotionScene.hpp"
#include <memory>
#include <numbers>

#ifndef HM2_VALIDATION
#define HM2_VALIDATION 0
#endif

// Restored walking slice. Combat, progress and audio remain unimplemented.
@interface HM2PreviewView:UIView
- (void)setActive:(BOOL)active;
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
    HM2Renderer* _renderer;
    CADisplayLink* _display;
    std::unique_ptr<hm2::apple::Files> _files;
    std::unique_ptr<hm2::LocomotionScene> _scene;
    hm2::Frame _frame;
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
    _files=std::make_unique<hm2::apple::Files>(root.fileSystemRepresentation);
    const hm2::Size viewport=UIDevice.currentDevice.userInterfaceIdiom==UIUserInterfaceIdiomPad?hm2::Size{768,1024}:hm2::Size{480,320};
    std::vector<hm2::LocomotionActor> cases;
#if HM2_VALIDATION
    const float q=std::numbers::pi_v<float>/2;
    cases={{hm2::PlayerEquipment::startingCodes,{-80,0},0,std::nullopt},{{"11002","12002","10012","10112"},{80,0},q,hm2::Vec2{80,200}}};
#else
    cases={{hm2::PlayerEquipment::startingCodes,{},0,std::nullopt}};
#endif
    _scene=std::make_unique<hm2::LocomotionScene>(*_files,"Stage01_01",viewport,cases);
    _frame=_scene->frame();
    _renderer=[[HM2Renderer alloc]initWithResourceRoot:root];[_renderer prepareFrame:_frame];
    _uiTesting=[NSProcessInfo.processInfo.environment[@"HM2_UI_TESTING"] isEqualToString:@"1"];
    if(_uiTesting){self.isAccessibilityElement=YES;self.accessibilityIdentifier=@"hm2.world";self.accessibilityLabel=@"HeavyMach2 World";[self updateTestState];}
    HM2DisplayTarget* target=[HM2DisplayTarget new];target.view=self;
    _display=[CADisplayLink displayLinkWithTarget:target selector:@selector(display:)];
    _display.preferredFramesPerSecond=60;[_display addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
    return self;
}
- (void)updateTestState {
    if(!_uiTesting||!_scene)return;const auto& s=_scene->actors().front().state();
    self.accessibilityValue=[NSString stringWithFormat:@"ticks=%lu;x=%.6f;y=%.6f;destX=%.6f;destY=%.6f;cameraX=%.6f;cameraY=%.6f",_scene->ticks(),s.position.x,s.position.y,s.destination.x,s.destination.y,_scene->camera().center.x,_scene->camera().center.y];
}
- (void)display:(CADisplayLink*)link {
    if(self.window&&self.bounds.size.width>0&&self.bounds.size.height>0){_scene->tick();_frame=_scene->frame();[_renderer drawFrame:_frame layer:(CAEAGLLayer*)self.layer];[self updateTestState];}
}
- (void)input:(NSSet<UITouch*>*)touches phase:(hm2::PointerPhase)phase {
    for(UITouch* touch in touches){hm2::Vec2 logical{};const auto identifier=reinterpret_cast<std::uintptr_t>((__bridge void*)touch);
        if([_renderer logicalPoint:[touch locationInView:self] result:&logical])_scene->input(phase,logical,identifier);
        else if(phase==hm2::PointerPhase::end||phase==hm2::PointerPhase::cancel)_scene->input(hm2::PointerPhase::cancel,{},identifier);
    }[self updateTestState];
}
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{[self input:touches phase:hm2::PointerPhase::begin];}
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{[self input:touches phase:hm2::PointerPhase::move];}
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{[self input:touches phase:hm2::PointerPhase::end];}
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{[self input:touches phase:hm2::PointerPhase::cancel];}
- (void)setActive:(BOOL)active{_display.paused=!active;_scene->setPaused(!active);}
- (void)dealloc{[_display invalidate];}
@end

@interface HM2Controller:UIViewController @end
@implementation HM2Controller
- (void)loadView {
    // Hosted XCTest creates its own real surface/resources and never starts an
    // unsolicited game or opens user save/audio state in the host application.
    self.view=NSClassFromString(@"XCTestCase")?[[UIView alloc]initWithFrame:UIScreen.mainScreen.bounds]:[[HM2PreviewView alloc]initWithFrame:UIScreen.mainScreen.bounds];
}
- (BOOL)prefersStatusBarHidden{return YES;}
- (UIInterfaceOrientationMask)supportedInterfaceOrientations{return UIDevice.currentDevice.userInterfaceIdiom==UIUserInterfaceIdiomPad?UIInterfaceOrientationMaskPortrait:UIInterfaceOrientationMaskLandscape;}
@end

@interface HM2AppDelegate:UIResponder<UIApplicationDelegate>
@property(nonatomic,strong)UIWindow* window;
@end
@implementation HM2AppDelegate
- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)options {
    self.window=[[UIWindow alloc]initWithFrame:UIScreen.mainScreen.bounds];self.window.rootViewController=[HM2Controller new];[self.window makeKeyAndVisible];return YES;
}
- (void)applicationWillResignActive:(UIApplication*)app{UIView* view=self.window.rootViewController.view;if([view isKindOfClass:HM2PreviewView.class])[(HM2PreviewView*)view setActive:NO];}
- (void)applicationDidBecomeActive:(UIApplication*)app{UIView* view=self.window.rootViewController.view;if([view isKindOfClass:HM2PreviewView.class])[(HM2PreviewView*)view setActive:YES];}
@end
int main(int argc,char** argv){@autoreleasepool{return UIApplicationMain(argc,argv,nil,NSStringFromClass(HM2AppDelegate.class));}}

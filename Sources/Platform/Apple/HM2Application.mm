#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/EAGL.h>
#import "HM2Renderer.h"
#include "HM2Resources.hpp"
#include "Core/Simulation/LocomotionScene.hpp"
#include "Platform/Apple/HM2ConcurrentRuntime.hpp"
#include <memory>
#include <numbers>

#ifndef HM2_VALIDATION
#define HM2_VALIDATION 0
#endif

// Source keeps the walking entry until its full game entry is validated.
// Validation runs independent restored sessions on one display clock.
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
#if HM2_VALIDATION
    std::unique_ptr<hm2::apple::ConcurrentRuntime> _concurrent;
    NSURL* _validationDirectory;
    BOOL _validationFinished;
#endif
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
#if HM2_VALIDATION
    NSURL* documents=[NSFileManager.defaultManager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
    _validationDirectory=[[documents URLByAppendingPathComponent:@"ConcurrentValidation" isDirectory:YES] URLByAppendingPathComponent:NSUUID.UUID.UUIDString isDirectory:YES];
    _concurrent=std::make_unique<hm2::apple::ConcurrentRuntime>(root,_validationDirectory);
#else
    std::vector<hm2::LocomotionActor> cases;
    cases={{hm2::PlayerEquipment::startingCodes,{},0,std::nullopt}};
    _scene=std::make_unique<hm2::LocomotionScene>(*_files,"Stage01_01",viewport,cases);
    _frame=_scene->frame();
    _renderer=[[HM2Renderer alloc]initWithResourceRoot:root];[_renderer prepareFrame:_frame];
#endif
    _uiTesting=[NSProcessInfo.processInfo.environment[@"HM2_UI_TESTING"] isEqualToString:@"1"];
    if(_uiTesting){self.isAccessibilityElement=YES;self.accessibilityIdentifier=@"hm2.world";self.accessibilityLabel=@"HeavyMach2 World";[self updateTestState];}
    HM2DisplayTarget* target=[HM2DisplayTarget new];target.view=self;
    _display=[CADisplayLink displayLinkWithTarget:target selector:@selector(display:)];
    _display.preferredFramesPerSecond=60;[_display addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];
    return self;
}
- (void)updateTestState {
#if HM2_VALIDATION
    if(!_uiTesting||!_concurrent)return;const auto m=_concurrent->validation().metrics();
    self.accessibilityValue=[NSString stringWithFormat:@"frames=%llu;scenarios=%lu;weaponDefinitions=%lu;weaponKinds=%lu",m.frames,(unsigned long)m.activeScenarios,(unsigned long)m.activeWeaponDefinitions,(unsigned long)m.activeWeaponKinds];
#else
    if(!_uiTesting||!_scene)return;const auto& s=_scene->actors().front().state();
    self.accessibilityValue=[NSString stringWithFormat:@"ticks=%lu;x=%.6f;y=%.6f;destX=%.6f;destY=%.6f;cameraX=%.6f;cameraY=%.6f",_scene->ticks(),s.position.x,s.position.y,s.destination.x,s.destination.y,_scene->camera().center.x,_scene->camera().center.y];
#endif
}
- (void)display:(CADisplayLink*)link {
#if HM2_VALIDATION
    if(!_validationFinished&&self.window&&self.bounds.size.width>0&&self.bounds.size.height>0){
        _concurrent->tick();const bool finished=_concurrent->validation().metrics().frames>=1200;
        if(finished)_concurrent->captureNextFrame();
        _concurrent->draw((CAEAGLLayer*)self.layer,{float(self.bounds.size.width),float(self.bounds.size.height)});[self updateTestState];
        if(finished){
            _validationFinished=YES;_display.paused=YES;NSDictionary* report=_concurrent->report();_concurrent->setActive(false);
            NSError* error=nil;NSData* data=[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted|NSJSONWritingSortedKeys error:&error];
            NSURL* output=[_validationDirectory URLByAppendingPathComponent:@"report.json"];
            if(!data||![data writeToURL:output options:NSDataWritingAtomic error:&error])NSLog(@"HM2 validation report failed: %@",error);
            else NSLog(@"HM2 validation finished1200frames: %@",output.path);
        }
    }
#else
    if(self.window&&self.bounds.size.width>0&&self.bounds.size.height>0){_scene->tick();_frame=_scene->frame();[_renderer drawFrame:_frame layer:(CAEAGLLayer*)self.layer];[self updateTestState];}
#endif
}
- (void)input:(NSSet<UITouch*>*)touches phase:(hm2::PointerPhase)phase {
#if !HM2_VALIDATION
    for(UITouch* touch in touches){hm2::Vec2 logical{};const auto identifier=reinterpret_cast<std::uintptr_t>((__bridge void*)touch);
        if([_renderer logicalPoint:[touch locationInView:self] result:&logical])_scene->input(phase,logical,identifier);
        else if(phase==hm2::PointerPhase::end||phase==hm2::PointerPhase::cancel)_scene->input(hm2::PointerPhase::cancel,{},identifier);
    }[self updateTestState];
#endif
}
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{[self input:touches phase:hm2::PointerPhase::begin];}
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{[self input:touches phase:hm2::PointerPhase::move];}
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{[self input:touches phase:hm2::PointerPhase::end];}
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{[self input:touches phase:hm2::PointerPhase::cancel];}
- (void)setActive:(BOOL)active{_display.paused=!active;
#if HM2_VALIDATION
    if(!_validationFinished)_concurrent->setActive(active);else _display.paused=YES;
#else
    _scene->setPaused(!active);
#endif
}
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

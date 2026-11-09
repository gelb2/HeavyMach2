#import <XCTest/XCTest.h>
#import <UIKit/UIKit.h>
#include <cmath>

@interface WorldInputTests:XCTestCase @end
@implementation WorldInputTests
- (NSDictionary<NSString*,NSNumber*>*)state:(XCUIElement*)world {
    NSMutableDictionary* result=[NSMutableDictionary new];
    for(NSString* field in [(NSString*)world.value componentsSeparatedByString:@";"]){NSArray* pair=[field componentsSeparatedByString:@"="];if(pair.count==2)result[pair[0]]=@([pair[1] doubleValue]);}
    return result;
}
- (void)testRealTouchMovesPlayerOnOriginalTerrain {
    XCUIApplication* app=[[XCUIApplication alloc]initWithBundleIdentifier:@"com.gelb2.heavymach2.restored"];
    app.launchEnvironment=@{@"HM2_UI_TESTING":@"1"};[app launch];
    XCUIElement* world=app.otherElements[@"hm2.world"];XCTAssertTrue([world waitForExistenceWithTimeout:10]);
    NSPredicate* warmed=[NSPredicate predicateWithBlock:^BOOL(id ignored,NSDictionary* values){return [[self state:world][@"ticks"] doubleValue]>5;}];
    [self expectationForPredicate:warmed evaluatedWithObject:world handler:nil];[self waitForExpectationsWithTimeout:10 handler:nil];
    const CGRect bounds=world.frame;const BOOL pad=UIDevice.currentDevice.userInterfaceIdiom==UIUserInterfaceIdiomPad;
    const CGFloat width=pad?768:480,height=pad?1024:320;
    const CGFloat scale=MIN(bounds.size.width/width,bounds.size.height/height);
    const CGFloat left=(bounds.size.width-width*scale)/2,top=(bounds.size.height-height*scale)/2;
    NSDictionary* before=[self state:world];
    // Tap at logical (viewport center + 70, center), a real UIKit event. Both
    // dimensions use the renderer's aspect fit, not a guessed screen fraction.
    XCUICoordinate* point=[[world coordinateWithNormalizedOffset:CGVectorMake(0,0)] coordinateWithOffset:CGVectorMake(left+(width/2+70)*scale,top+height/2*scale)];
    [point tap];
    NSPredicate* changed=[NSPredicate predicateWithBlock:^BOOL(id ignored,NSDictionary* values){NSDictionary* now=[self state:world];return fabs([now[@"destX"] doubleValue])>30&&hypot([now[@"x"] doubleValue]-[before[@"x"] doubleValue],[now[@"y"] doubleValue]-[before[@"y"] doubleValue])>5;}];
    [self expectationForPredicate:changed evaluatedWithObject:world handler:nil];[self waitForExpectationsWithTimeout:10 handler:nil];
    NSDictionary* after=[self state:world];XCTAssertGreaterThan([after[@"ticks"] doubleValue],[before[@"ticks"] doubleValue]);
    NSLog(@"HM2_WORLD_TOUCH device=%@ before=%@ after=%@",UIDevice.currentDevice.model,before,after);
    // Application-cropped captures on this runtime pair landscape pixels with
    // portrait EXIF rotation. Capture the device surface without that crop.
    XCUIScreenshot* capture=XCUIScreen.mainScreen.screenshot;
    UIImage* capturedImage=[UIImage imageWithData:capture.PNGRepresentation];
    NSLog(@"HM2_TOUCH_SCREENSHOT raw=%zux%zu orientation=%ld",CGImageGetWidth(capturedImage.CGImage),CGImageGetHeight(capturedImage.CGImage),(long)capturedImage.imageOrientation);
    XCTAttachment* shot=[XCTAttachment attachmentWithScreenshot:capture];shot.name=@"walking-after-real-touch";shot.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:shot];[app terminate];
}
@end

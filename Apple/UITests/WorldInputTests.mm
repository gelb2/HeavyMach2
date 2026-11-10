#import <XCTest/XCTest.h>
#import <UIKit/UIKit.h>
#include <cmath>

@interface WorldInputTests:XCTestCase @end
@implementation WorldInputTests
- (NSDictionary<NSString*,NSString*>*)state:(XCUIElement*)world {
    NSMutableDictionary* result=[NSMutableDictionary new];
    for(NSString* field in [(NSString*)world.value componentsSeparatedByString:@";"]){NSRange delimiter=[field rangeOfString:@"="];if(delimiter.location!=NSNotFound)result[[field substringToIndex:delimiter.location]]=[field substringFromIndex:delimiter.location+1];}
    return result;
}
- (void)wait:(BOOL(^)(void))condition description:(NSString*)description {
    NSPredicate* predicate=[NSPredicate predicateWithBlock:^BOOL(id object,NSDictionary* bindings){return condition();}];
    XCTestExpectation* expectation=[[XCTNSPredicateExpectation alloc]initWithPredicate:predicate object:nil];expectation.expectationDescription=description;
    [self waitForExpectations:@[expectation] timeout:15];
}
- (void)tapLogical:(CGPoint)logical world:(XCUIElement*)world {
    const auto state=[self state:world];const CGRect bounds=world.frame;const CGFloat width=[state[@"canvasWidth"] doubleValue],height=[state[@"canvasHeight"] doubleValue];
    XCTAssertGreaterThan(width,0);XCTAssertGreaterThan(height,0);const CGFloat scale=MIN(bounds.size.width/width,bounds.size.height/height);
    const CGFloat x=(bounds.size.width-width*scale)/2+logical.x*scale,y=(bounds.size.height-height*scale)/2+(height-logical.y)*scale;
    XCUICoordinate* point=[[world coordinateWithNormalizedOffset:CGVectorMake(0,0)] coordinateWithOffset:CGVectorMake(x,y)];[point tap];
}
- (void)tapButton:(unsigned)identifier data:(int)data world:(XCUIElement*)world {
    const auto state=[self state:world];NSString* x=[NSString stringWithFormat:@"b%u_%d_x",identifier,data],*y=[NSString stringWithFormat:@"b%u_%d_y",identifier,data];
    XCTAssertNotNil(state[x]);XCTAssertNotNil(state[y]);[self tapLogical:CGPointMake([state[x] doubleValue],[state[y] doubleValue]) world:world];
}
- (void)titleToSlots:(XCUIElement*)world {
    [self wait:^BOOL{return [[self state:world][@"ticks"] doubleValue]>5;} description:@"real display callbacks started"];
    for(unsigned i=0;i<2&&[[self state:world][@"screen"] intValue]==1;++i)[self tapLogical:CGPointMake(384,512) world:world];
    [self wait:^BOOL{return [[self state:world][@"screen"] intValue]==2;} description:@"original title input opens slots"];
}
- (void)capture:(NSString*)name {
    XCTAttachment* shot=[XCTAttachment attachmentWithScreenshot:XCUIScreen.mainScreen.screenshot];shot.name=name;shot.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:shot];
}
- (void)testRealTouchMovesPlayerOnOriginalTerrain {
    self.continueAfterFailure=NO;XCUIApplication* app=[[XCUIApplication alloc]initWithBundleIdentifier:@"com.gelb2.heavymach2.restored"];
    NSString* run=NSUUID.UUID.UUIDString;app.launchEnvironment=@{@"HM2_UI_TESTING":@"1",@"HM2_UI_TEST_RUN":run};[app launch];
    XCUIElement* world=app.otherElements[@"hm2.world"];XCTAssertTrue([world waitForExistenceWithTimeout:15]);[self titleToSlots:world];[self capture:@"source-slots-before-create"];
    [self tapButton:1009 data:0 world:world];[self tapButton:1001 data:0 world:world];XCUIElement* name=app.alerts[@"New User"];XCTAssertTrue([name waitForExistenceWithTimeout:10]);
    XCTAssertEqualObjects(name.textFields.firstMatch.placeholderValue,@"Name: 1~10 Character");[name.buttons[@"cancel"] tap];
    [self wait:^BOOL{return world.exists;} description:@"cancel returns to original GPU slots"];XCTAssertEqual([[self state:world][@"saves"] intValue],0);
    [self tapButton:1001 data:0 world:world];XCTAssertTrue([name waitForExistenceWithTimeout:10]);[name.textFields.firstMatch typeText:@"01234567890"];[name.buttons[@"OK"] tap];
    XCUIElement* invalid=app.alerts[@"Incorrect Name"];XCTAssertTrue([invalid waitForExistenceWithTimeout:10]);XCTAssertTrue(invalid.staticTexts[@"Enter your name in 10 or less letters."].exists);[invalid.buttons[@"OK"] tap];
    [self wait:^BOOL{return world.exists;} description:@"invalid name preserved empty slot"];XCTAssertEqual([[self state:world][@"slot0Used"] intValue],0);XCTAssertEqual([[self state:world][@"saves"] intValue],0);
    [self tapButton:1001 data:0 world:world];XCTAssertTrue([name waitForExistenceWithTimeout:10]);[name.textFields.firstMatch typeText:@"MACH"];[name.buttons[@"OK"] tap];
    [self wait:^BOOL{return world.exists&&[[self state:world][@"slot0Used"] intValue]==1;} description:@"original initSlot selected and saved"];
    XCTAssertEqual([[self state:world][@"screen"] intValue],2);XCTAssertEqual([[self state:world][@"saves"] intValue],1);XCTAssertEqualObjects([self state:world][@"slot0Name"],@"MACH");
    [self tapButton:1001 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"screen"] intValue]==3;} description:@"explicit start enters base"];XCTAssertEqual([[self state:world][@"saves"] intValue],3);[self capture:@"source-base-created-save"];
    [self tapButton:2005 data:0 world:world];[self tapButton:2005 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"screen"] intValue]==7&&[[self state:world][@"enemyCount"] intValue]>0;} description:@"real base gate enters original field"];
    NSDictionary* before=[self state:world];CGPoint destination=CGPointZero;BOOL found=NO;const CGFloat offsets[][2]={{100,0},{0,100},{100,100},{-100,0},{-100,80}};
    for(const auto& delta:offsets){const CGFloat x=[before[@"playerScreenX"] doubleValue]+delta[0],y=[before[@"playerScreenY"] doubleValue]+delta[1];if(x<20||x>748||y<120||y>1000)continue;BOOL clear=YES;
        for(int i=0;i<[before[@"enemyCount"] intValue];++i){const CGFloat ex=[before[[NSString stringWithFormat:@"e%d_x",i]] doubleValue],ey=[before[[NSString stringWithFormat:@"e%d_y",i]] doubleValue];if(hypot(x-ex,y-ey)<100)clear=NO;}
        if(clear){destination=CGPointMake(x,y);found=YES;break;}}
    XCTAssertTrue(found);[self tapLogical:destination world:world];
    [self wait:^BOOL{const auto now=[self state:world];return hypot([now[@"x"] doubleValue]-[before[@"x"] doubleValue],[now[@"y"] doubleValue]-[before[@"y"] doubleValue])>5;} description:@"real UIKit touch moves Player on original terrain"];
    NSDictionary* moving=[self state:world];XCTAssertGreaterThan([moving[@"ticks"] doubleValue],[before[@"ticks"] doubleValue]);[self capture:@"source-field-after-real-touch"];
    found=NO;unsigned long long targetID=0;const auto targets=[self state:world];for(int i=0;i<[targets[@"enemyCount"] intValue];++i){const CGFloat x=[targets[[NSString stringWithFormat:@"e%d_x",i]] doubleValue],y=[targets[[NSString stringWithFormat:@"e%d_y",i]] doubleValue];if(x>32&&x<736&&y>180&&y<980){destination=CGPointMake(x,y);targetID=[targets[[NSString stringWithFormat:@"e%d_id",i]] longLongValue];found=YES;break;}}
    XCTAssertTrue(found);[self tapLogical:destination world:world];[self wait:^BOOL{const auto now=[self state:world];return [now[@"selectionTouches"] longLongValue]>[targets[@"selectionTouches"] longLongValue]&&[now[@"touchSelectedEnemy"] longLongValue]==targetID&&[now[@"emissions"] longLongValue]>[targets[@"emissions"] longLongValue];} description:@"real enemy selection ID causes additional original weapon emissions"];
    [self tapButton:1021 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"paused"] intValue]==1;} description:@"system GPU menu pauses world"];
    NSDictionary* paused=[self state:world];[self wait:^BOOL{return [[self state:world][@"ticks"] doubleValue]>[paused[@"ticks"] doubleValue]+10;} description:@"display continues while world is paused"];
    XCTAssertEqualWithAccuracy([[self state:world][@"worldTime"] doubleValue],[paused[@"worldTime"] doubleValue],0.000001);
    [self tapButton:1030 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"paused"] intValue]==0;} description:@"resume GPU control unpauses world"];
    XCTAssertEqual([[self state:world][@"unsupported"] intValue],0);NSLog(@"HM2_SOURCE_ENTRY run=%@ movementBefore=%@ after=%@ titleSlotsNameSaveBaseFieldRealInput=1",run,before,[self state:world]);
    [app terminate];[app launch];world=app.otherElements[@"hm2.world"];XCTAssertTrue([world waitForExistenceWithTimeout:15]);[self titleToSlots:world];XCTAssertEqualObjects([self state:world][@"slot0Name"],@"MACH");
    [self tapButton:1010 data:0 world:world];XCUIElement* erase=app.alerts[@"Delete Slot"];XCTAssertTrue([erase waitForExistenceWithTimeout:10]);XCTAssertTrue(erase.staticTexts[@"Deleting the slot will permanently destroy all game data in it."].exists);[erase.buttons[@"Cancel"] tap];
    [self wait:^BOOL{return world.exists;} description:@"delete cancel preserves save"];XCTAssertEqual([[self state:world][@"slot0Used"] intValue],1);XCTAssertEqual([[self state:world][@"saves"] intValue],0);
    [self tapButton:1010 data:0 world:world];XCTAssertTrue([erase waitForExistenceWithTimeout:10]);[erase.buttons[@"OK"] tap];[self wait:^BOOL{return world.exists&&[[self state:world][@"slot0Used"] intValue]==0;} description:@"confirmed delete persists used byte"];
    XCTAssertEqual([[self state:world][@"saves"] intValue],1);[self capture:@"source-slot-after-confirmed-delete"];
    [app terminate];[app launch];world=app.otherElements[@"hm2.world"];XCTAssertTrue([world waitForExistenceWithTimeout:15]);[self titleToSlots:world];XCTAssertEqual([[self state:world][@"slot0Used"] intValue],0);[app terminate];
}
@end

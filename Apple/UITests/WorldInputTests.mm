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
    [self wait:condition description:description timeout:15];
}
- (void)wait:(BOOL(^)(void))condition description:(NSString*)description timeout:(NSTimeInterval)timeout {
    NSPredicate* predicate=[NSPredicate predicateWithBlock:^BOOL(id object,NSDictionary* bindings){return condition();}];
    XCTestExpectation* expectation=[[XCTNSPredicateExpectation alloc]initWithPredicate:predicate object:nil];expectation.expectationDescription=description;
    [self waitForExpectations:@[expectation] timeout:timeout];
}
- (void)tapLogical:(CGPoint)logical world:(XCUIElement*)world {
    const auto state=[self state:world];const CGRect bounds=world.frame;const CGFloat width=[state[@"canvasWidth"] doubleValue],height=[state[@"canvasHeight"] doubleValue];
    XCTAssertGreaterThan(width,0);XCTAssertGreaterThan(height,0);const CGFloat scale=MIN(bounds.size.width/width,bounds.size.height/height);
    const CGFloat x=(bounds.size.width-width*scale)/2+logical.x*scale,y=(bounds.size.height-height*scale)/2+(height-logical.y)*scale;
    XCUICoordinate* point=[[world coordinateWithNormalizedOffset:CGVectorMake(0,0)] coordinateWithOffset:CGVectorMake(x,y)];[point tap];
}
- (void)tapButton:(unsigned)identifier data:(int)data world:(XCUIElement*)world {
    [self wait:^BOOL{return [[self state:world][@"doorPhase"] intValue]==8;} description:@"original loading doors finished opening"];
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
    XCUIElement* invalid=app.alerts[@"Incorrect Name"];XCTAssertTrue([invalid waitForExistenceWithTimeout:10]);XCTAssertTrue(invalid.staticTexts[@"이름은 1~10글자로 입력해야합니다."].exists);[invalid.buttons[@"OK"] tap];
    [self wait:^BOOL{return world.exists;} description:@"invalid name preserved empty slot"];XCTAssertEqual([[self state:world][@"slot0Used"] intValue],0);XCTAssertEqual([[self state:world][@"saves"] intValue],0);
    [self tapButton:1001 data:0 world:world];XCTAssertTrue([name waitForExistenceWithTimeout:10]);[name.textFields.firstMatch typeText:@"MACH"];[name.buttons[@"OK"] tap];
    [self wait:^BOOL{return world.exists&&[[self state:world][@"slot0Used"] intValue]==1;} description:@"original initSlot selected and saved"];
    XCTAssertEqual([[self state:world][@"screen"] intValue],2);XCTAssertEqual([[self state:world][@"saves"] intValue],1);XCTAssertEqualObjects([self state:world][@"slot0Name"],@"MACH");
    [self tapButton:1001 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"screen"] intValue]==3;} description:@"explicit start enters base"];XCTAssertEqual([[self state:world][@"saves"] intValue],3);[self capture:@"source-base-created-save"];
    XCTAssertEqual([[self state:world][@"helpVisible"] intValue],1);XCTAssertEqual([[self state:world][@"helpPage"] intValue],0);
    [self tapLogical:CGPointMake(384,512) world:world];[self wait:^BOOL{return [[self state:world][@"helpVisible"] intValue]==0;} description:@"first Korean base Help consumes the closing tap"];
    [self tapButton:2001 data:0 world:world];[self tapButton:2001 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"screen"] intValue]==4;} description:@"command center opens"];
    const int rankingSaves=[[self state:world][@"saves"] intValue];[self tapButton:5009 data:0 world:world];
    XCUIElement* ranking=app.alerts[@"랭킹"];XCTAssertTrue([ranking waitForExistenceWithTimeout:10]);XCTAssertTrue(ranking.staticTexts[@"현재 랭킹 서비스를 이용할 수 없습니다."].exists);XCTAssertEqual(ranking.buttons.count,1u);[self capture:@"ranking-service-unavailable"];[ranking.buttons[@"OK"] tap];
    XCTAssertEqual([[self state:world][@"screen"] intValue],4);XCTAssertEqual([[self state:world][@"saves"] intValue],rankingSaves);[self tapButton:2100 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"screen"] intValue]==3;} description:@"ranking notice closes without leaving command; exit returns to base"];
    [self tapButton:2005 data:0 world:world];[self tapButton:2005 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"screen"] intValue]==7&&[[self state:world][@"enemyCount"] intValue]>0;} description:@"real base gate enters original field"];
    [self wait:^BOOL{return [[self state:world][@"doorPhase"] intValue]==8;} description:@"field opening completes while original world advances"];
    XCTAssertEqual([[self state:world][@"helpVisible"] intValue],1);XCTAssertEqual([[self state:world][@"helpPage"] intValue],2);XCTAssertEqual([[self state:world][@"helpGuidance"] intValue],0);
    [self capture:@"source-initial-korean-help"];[self tapLogical:CGPointMake(700,512) world:world];[self wait:^BOOL{return [[self state:world][@"helpVisible"] intValue]==0;} description:@"fixed initial Help consumes closing tap"];
    NSDictionary* before=[self state:world];CGPoint destination=CGPointZero;BOOL found=NO;const CGFloat offsets[][2]={{100,0},{0,100},{100,100},{-100,0},{-100,80}};
    for(const auto& delta:offsets){const CGFloat x=[before[@"playerScreenX"] doubleValue]+delta[0],y=[before[@"playerScreenY"] doubleValue]+delta[1];if(x<20||x>748||y<120||y>1000)continue;BOOL clear=YES;
        for(int i=0;i<[before[@"enemyCount"] intValue];++i){const CGFloat ex=[before[[NSString stringWithFormat:@"e%d_x",i]] doubleValue],ey=[before[[NSString stringWithFormat:@"e%d_y",i]] doubleValue];if(hypot(x-ex,y-ey)<100)clear=NO;}
        if(clear){destination=CGPointMake(x,y);found=YES;break;}}
    XCTAssertTrue(found);[self tapLogical:destination world:world];
    [self wait:^BOOL{const auto now=[self state:world];return hypot([now[@"x"] doubleValue]-[before[@"x"] doubleValue],[now[@"y"] doubleValue]-[before[@"y"] doubleValue])>5;} description:@"real UIKit touch moves Player on original terrain"];
    NSDictionary* moving=[self state:world];XCTAssertGreaterThan([moving[@"ticks"] doubleValue],[before[@"ticks"] doubleValue]);[self capture:@"source-field-after-real-touch"];
    found=NO;unsigned long long targetID=0;CGFloat nearest=CGFLOAT_MAX;const auto targets=[self state:world];for(int i=0;i<[targets[@"enemyCount"] intValue];++i){const CGFloat x=[targets[[NSString stringWithFormat:@"e%d_x",i]] doubleValue],y=[targets[[NSString stringWithFormat:@"e%d_y",i]] doubleValue];const CGFloat distance=hypot(x-[targets[@"playerScreenX"] doubleValue],y-[targets[@"playerScreenY"] doubleValue]);if(x>32&&x<736&&y>120&&y<980&&distance<nearest){nearest=distance;destination=CGPointMake(x,y);targetID=[targets[[NSString stringWithFormat:@"e%d_id",i]] longLongValue];found=YES;}}
    XCTAssertTrue(found);[self tapLogical:destination world:world];[self wait:^BOOL{const auto now=[self state:world];return [now[@"selectionTouches"] longLongValue]>[targets[@"selectionTouches"] longLongValue]&&[now[@"touchSelectedEnemy"] longLongValue]==targetID&&[now[@"emissions"] longLongValue]>[targets[@"emissions"] longLongValue];} description:@"real enemy selection ID causes additional original weapon emissions"];
    [self tapButton:1021 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"paused"] intValue]==1;} description:@"system GPU menu pauses world"];
    NSDictionary* paused=[self state:world];[self wait:^BOOL{return [[self state:world][@"ticks"] doubleValue]>[paused[@"ticks"] doubleValue]+10;} description:@"display continues while world is paused"];
    XCTAssertEqualWithAccuracy([[self state:world][@"worldTime"] doubleValue],[paused[@"worldTime"] doubleValue],0.000001);
    [self tapButton:1030 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"paused"] intValue]==0;} description:@"resume GPU control unpauses world"];
    [self tapButton:1021 data:0 world:world];[self tapButton:1029 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"helpVisible"] intValue]==1&&[[self state:world][@"paused"] intValue]==1;} description:@"actual menu Help opens third Korean page and pauses world"];
    XCTAssertEqual([[self state:world][@"helpPage"] intValue],2);XCTAssertEqual([[self state:world][@"helpGuidance"] intValue],1);[self capture:@"source-menu-korean-help-guidance"];
    [self tapLogical:CGPointMake(384,512) world:world];XCTAssertEqual([[self state:world][@"helpVisible"] intValue],1);XCTAssertEqual([[self state:world][@"helpGuidance"] intValue],0);
    [self tapLogical:CGPointMake(100,512) world:world];XCTAssertEqual([[self state:world][@"helpPage"] intValue],1);[self tapLogical:CGPointMake(700,512) world:world];XCTAssertEqual([[self state:world][@"helpPage"] intValue],2);
    [self tapLogical:CGPointMake(384,512) world:world];[self wait:^BOOL{return [[self state:world][@"helpVisible"] intValue]==0&&[[self state:world][@"paused"] intValue]==0;} description:@"Help closes without leaking its input to world"];

    XCTAssertEqual([[self state:world][@"unsupported"] intValue],0);NSLog(@"HM2_SOURCE_ENTRY run=%@ movementBefore=%@ after=%@ titleSlotsNameSaveBaseFieldRealInput=1",run,before,[self state:world]);
    [app terminate];[app launch];world=app.otherElements[@"hm2.world"];XCTAssertTrue([world waitForExistenceWithTimeout:15]);[self titleToSlots:world];XCTAssertEqualObjects([self state:world][@"slot0Name"],@"MACH");
    [self tapButton:1010 data:0 world:world];XCUIElement* erase=app.alerts[@"Delete Slot"];XCTAssertTrue([erase waitForExistenceWithTimeout:10]);XCTAssertTrue(erase.staticTexts[@"슬롯을 삭제하면 해당 슬롯의 모든\n게임데이터가 삭제됩니다."].exists);[erase.buttons[@"Cancel"] tap];
    [self wait:^BOOL{return world.exists;} description:@"delete cancel preserves save"];XCTAssertEqual([[self state:world][@"slot0Used"] intValue],1);XCTAssertEqual([[self state:world][@"saves"] intValue],0);
    [self tapButton:1010 data:0 world:world];XCTAssertTrue([erase waitForExistenceWithTimeout:10]);[erase.buttons[@"OK"] tap];[self wait:^BOOL{return world.exists&&[[self state:world][@"slot0Used"] intValue]==0;} description:@"confirmed delete persists used byte"];
    XCTAssertEqual([[self state:world][@"saves"] intValue],1);[self capture:@"source-slot-after-confirmed-delete"];
    [app terminate];[app launch];world=app.otherElements[@"hm2.world"];XCTAssertTrue([world waitForExistenceWithTimeout:15]);[self titleToSlots:world];XCTAssertEqual([[self state:world][@"slot0Used"] intValue],0);[app terminate];
}
- (void)testActualGameOverAlertAndReturnToBase {
    self.continueAfterFailure=NO;XCUIApplication* app=[[XCUIApplication alloc]initWithBundleIdentifier:@"com.gelb2.heavymach2.restored"];
    NSString* run=NSUUID.UUID.UUIDString;app.launchEnvironment=@{@"HM2_UI_TESTING":@"1",@"HM2_UI_TEST_RUN":run,@"HM2_UI_TEST_FIXTURE":@"gameOver"};[app launch];
    XCUIElement* world=app.otherElements[@"hm2.world"];XCTAssertTrue([world waitForExistenceWithTimeout:15]);[self titleToSlots:world];XCTAssertEqualObjects([self state:world][@"slot0Name"],@"GAMEOVER");
    [self tapButton:1001 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"screen"] intValue]==7&&[[self state:world][@"enemyCount"] intValue]>0;} description:@"saved original field starts"];
    [self wait:^BOOL{return [[self state:world][@"doorPhase"] intValue]==8;} description:@"field doors finish before real target input"];
    NSDictionary* field=[self state:world];XCTAssertEqualObjects(field[@"lastBase"],@"Base002");XCTAssertGreaterThan([field[@"playerHP"] intValue],0);XCTAssertLessThan([field[@"playerHP"] intValue],[field[@"maximumHP"] intValue]);
    const int saves=[field[@"saves"] intValue];BOOL selected=NO;unsigned long long target=0;
    for(int i=0;i<[field[@"enemyCount"] intValue];++i){const double x=[field[[NSString stringWithFormat:@"e%d_x",i]] doubleValue],y=[field[[NSString stringWithFormat:@"e%d_y",i]] doubleValue];if(x>0&&x<768&&y>100&&y<1024){target=[field[[NSString stringWithFormat:@"e%d_id",i]] longLongValue];[self tapLogical:CGPointMake(x,y) world:world];selected=YES;break;}}
    // Provoke the first enemy, then change target while it is still alive.
    // Finishing that single enemy can leave the Player regenerating safely.
    const auto pursuing=[self state:world];BOOL firstAlive=NO,retargeted=NO;CGFloat nearest=CGFLOAT_MAX;CGPoint next=CGPointZero;unsigned long long second=0;
    for(int i=0;i<[pursuing[@"enemyCount"] intValue];++i){const auto id=[pursuing[[NSString stringWithFormat:@"e%d_id",i]] longLongValue];if(id==target){firstAlive=YES;continue;}const CGFloat x=[pursuing[[NSString stringWithFormat:@"e%d_x",i]] doubleValue],y=[pursuing[[NSString stringWithFormat:@"e%d_y",i]] doubleValue],distance=hypot(x-[pursuing[@"playerScreenX"] doubleValue],y-[pursuing[@"playerScreenY"] doubleValue]);if(x>20&&x<748&&y>120&&y<1000&&distance<nearest){nearest=distance;next=CGPointMake(x,y);second=id;retargeted=YES;}}
    XCTAssertTrue(firstAlive&&retargeted);[self tapLogical:next world:world];XCTAssertEqual([[self state:world][@"touchSelectedEnemy"] longLongValue],static_cast<long long>(second));
    XCTAssertTrue(selected);NSLog(@"HM2_GAMEOVER_INPUT run=%@ before=%@ after=%@",run,field,[self state:world]);
    XCUIElement* destroyed=app.alerts[@"Destroy Your Mach."];XCTAssertTrue([destroyed waitForExistenceWithTimeout:60],@"Actual battle state: %@",[self state:world]);
    XCTAssertTrue(destroyed.staticTexts[@"매크가 파괴되었습니다.\n기지로 돌아가서 매크를 수리합니다."].exists);XCTAssertEqual(destroyed.buttons.count,1u);
    NSDictionary* dead=[self state:world];XCTAssertEqual([dead[@"workingHP"] intValue],0);XCTAssertEqual([dead[@"actorState"] intValue],3);XCTAssertEqual([dead[@"gameState"] intValue],3);XCTAssertEqual([dead[@"paused"] intValue],0);XCTAssertGreaterThan([dead[@"enemyEmissions"] longLongValue],0ll);XCTAssertEqual([dead[@"saves"] intValue],saves);[self capture:@"actual-gameover-original-alert"];
    NSDate* returnStart=[NSDate date];[destroyed.buttons[@"OK"] tap];NSDate* predicateStart=[NSDate date];__block NSDictionary* returned=nil;__block NSTimeInterval wall=0,predicateWall=0;
    // Software OpenGL in the simulator can render fewer than10 frames/s. Keep
    // the actual frame bound; record the former15-second wall failure below.
    [self wait:^BOOL{const auto now=[self state:world];NSLog(@"HM2_GAMEOVER_RETURN ticks=%@ screen=%@ phase=%@ token=%@ HP=%@",now[@"ticks"],now[@"screen"],now[@"doorPhase"],now[@"gameOverToken"],now[@"workingHP"]);const BOOL done=[now[@"screen"] intValue]==3&&[now[@"doorPhase"] intValue]==8;if(done&&!returned){returned=now;wall=-[returnStart timeIntervalSinceNow];predicateWall=-[predicateStart timeIntervalSinceNow];}return done;} description:@"GameOver OK uses original doors and enters base" timeout:60];
    NSDictionary* base=returned;XCTAssertNotNil(base);XCTAssertEqualObjects(base[@"location"],@"Base002");XCTAssertEqual([base[@"workingHP"] intValue],[base[@"maximumHP"] intValue]);XCTAssertEqual([base[@"saves"] intValue],saves+1);XCTAssertEqual([base[@"gameOverToken"] longLongValue],0ll);[self capture:@"gameover-base-return-saved"];
    const long long returnTick=[base[@"gameOverConfirmTick"] longLongValue],returnFrames=[base[@"ticks"] longLongValue]-returnTick;XCTAssertGreaterThan(returnTick,0ll);XCTAssertGreaterThanOrEqual(returnFrames,166ll);XCTAssertLessThanOrEqual(returnFrames,300ll);
    NSDictionary* observation=@{@"tapAndReturnWallSeconds":@(wall),@"predicateWallSeconds":@(predicateWall),@"confirmTick":@(returnTick),@"completionTick":[base[@"ticks"] copy],@"ticksFromActualConfirm":@(returnFrames),@"originalTransitionTicks":@166,@"uiObservationFrameLimit":@300,@"formerWallLimitSeconds":@15,@"formerWallLimitPassed":@(predicateWall<=15)};NSData* observationData=[NSJSONSerialization dataWithJSONObject:observation options:NSJSONWritingPrettyPrinted error:nil];XCTAttachment* timing=[XCTAttachment attachmentWithData:observationData uniformTypeIdentifier:@"public.json"];timing.name=@"GameOver transition frame and wall observation";timing.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:timing];NSLog(@"HM2_GAMEOVER_TIMING %@",observation);
    [app terminate];[app launch];world=app.otherElements[@"hm2.world"];XCTAssertTrue([world waitForExistenceWithTimeout:15]);[self titleToSlots:world];[self tapButton:1001 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"screen"] intValue]==3;} description:@"restarted save remains at restored base"];
    XCTAssertEqualObjects([self state:world][@"location"],@"Base002");XCTAssertEqual([[self state:world][@"workingHP"] intValue],[[self state:world][@"maximumHP"] intValue]);XCTAssertFalse(destroyed.exists);[app terminate];
}

- (void)testCompletedMissionWindowsAndSavedRewards {
    self.continueAfterFailure=NO;XCUIApplication* app=[[XCUIApplication alloc]initWithBundleIdentifier:@"com.gelb2.heavymach2.restored"];
    NSString* run=NSUUID.UUID.UUIDString;app.launchEnvironment=@{@"HM2_UI_TESTING":@"1",@"HM2_UI_TEST_RUN":run,@"HM2_UI_TEST_FIXTURE":@"completedMissions"};[app launch];
    XCUIElement* world=app.otherElements[@"hm2.world"];XCTAssertTrue([world waitForExistenceWithTimeout:15]);[self titleToSlots:world];[self tapButton:1001 data:0 world:world];
    [self wait:^BOOL{const auto s=[self state:world];return [s[@"screen"] intValue]==3&&[s[@"doorPhase"] intValue]==8;} description:@"original-format completed-mission start reaches Base001" timeout:60];
    const auto base=[self state:world];const int saves=[base[@"saves"] intValue],items=[base[@"repairItems"] intValue];XCTAssertEqual([base[@"credits"] intValue],500);
    [self tapButton:2001 data:0 world:world];[self tapButton:2001 data:0 world:world];
    [self wait:^BOOL{const auto s=[self state:world];return [s[@"completedMission"] isEqualToString:@"M002010"]&&[s[@"doorPhase"] intValue]==8;} description:@"first completed window awards first accepted eligible mission" timeout:60];
    NSDictionary* first=[self state:world];XCTAssertEqual([first[@"credits"] intValue],580);XCTAssertEqual([first[@"experience"] intValue],200);XCTAssertEqual([first[@"completedRewardCount"] intValue],2);XCTAssertEqual([first[@"saves"] intValue],saves);XCTAssertEqual([first[@"commandListVisible"] intValue],0);[self capture:@"completed-first-M002010-rewards"];
    [self tapButton:5009 data:0 world:world];XCUIElement* notice=app.alerts[@"랭킹"];XCTAssertTrue([notice waitForExistenceWithTimeout:10]);XCTAssertTrue(notice.staticTexts[@"현재 랭킹 서비스를 이용할 수 없습니다."].exists);[notice.buttons[@"OK"] tap];
    XCTAssertEqualObjects([self state:world][@"completedMission"],@"M002010");
    [self tapButton:5007 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"completedMission"] isEqualToString:@"M001010"];} description:@"actual result OK advances once to second eligible mission"];
    NSDictionary* second=[self state:world];XCTAssertEqual([second[@"credits"] intValue],680);XCTAssertEqual([second[@"experience"] intValue],400);XCTAssertEqual([second[@"level"] intValue],1);XCTAssertEqual([second[@"repairItems"] intValue],items+3);XCTAssertEqual([second[@"completedRewardCount"] intValue],3);XCTAssertEqual([second[@"saves"] intValue],saves);[self capture:@"completed-second-M001010-rewards"];
    [self tapButton:5007 data:0 world:world];[self wait:^BOOL{const auto s=[self state:world];return [s[@"completedMission"] length]==0&&[s[@"commandListVisible"] intValue]==1;} description:@"last OK removes result and next Command tick displays remaining missions"];
    NSDictionary* list=[self state:world];XCTAssertEqual([list[@"credits"] intValue],680);XCTAssertEqual([list[@"acceptedCount"] intValue],2);XCTAssertEqual([list[@"saves"] intValue],saves);[self capture:@"completed-remaining-mission-list"];
    const float acceptedY=[list[@"b5200_0_y"] floatValue];
    const float newY=[list[@"b5100_0_y"] floatValue];
    [self tapButton:5200 data:0 world:world];
    [self wait:^BOOL{
        const auto s=[self state:world];
        return [s[@"b5200_0_y"] floatValue]==acceptedY+128&&s[@"b5004_0_y"]==nil;
    } description:@"accepted mission opens its detail without an ACCEPT button"];
    [self capture:@"command-accepted-mission-detail"];
    [self tapButton:5100 data:0 world:world];
    [self wait:^BOOL{
        const auto s=[self state:world];
        return [s[@"b5100_0_y"] floatValue]==newY+128
            &&[s[@"b5200_0_y"] floatValue]==acceptedY-128
            &&[s[@"b5004_0_y"] floatValue]==newY-24-84;
    } description:@"new mission opens its detail, moves the accepted rows, and positions ACCEPT"];
    NSDictionary* expanded=[self state:world];
    XCTAssertEqual([expanded[@"saves"] intValue],saves);
    [self capture:@"command-new-mission-detail"];
    [self tapButton:2100 data:0 world:world];[self wait:^BOOL{const auto s=[self state:world];return [s[@"screen"] intValue]==3&&[s[@"doorPhase"] intValue]==8;} description:@"original doors return to Base and save rewards once" timeout:60];XCTAssertEqual([[self state:world][@"saves"] intValue],saves+1);
    [app terminate];[app launch];world=app.otherElements[@"hm2.world"];XCTAssertTrue([world waitForExistenceWithTimeout:15]);[self titleToSlots:world];[self tapButton:1001 data:0 world:world];
    [self wait:^BOOL{const auto s=[self state:world];return [s[@"screen"] intValue]==3&&[s[@"doorPhase"] intValue]==8;} description:@"restart decodes completed progress" timeout:60];
    [self tapButton:2001 data:0 world:world];[self tapButton:2001 data:0 world:world];[self wait:^BOOL{const auto s=[self state:world];return [s[@"screen"] intValue]==4&&[s[@"doorPhase"] intValue]==8&&[s[@"commandListVisible"] intValue]==1;} description:@"restart does not award completed missions again" timeout:60];
    NSDictionary* restarted=[self state:world];XCTAssertEqual([restarted[@"credits"] intValue],680);XCTAssertEqual([restarted[@"experience"] intValue],400);XCTAssertEqual([restarted[@"repairItems"] intValue],items+3);XCTAssertEqual([restarted[@"completedMission"] length],0u);XCTAssertEqual([restarted[@"unsupported"] intValue],0);
    NSData* data=[NSJSONSerialization dataWithJSONObject:@{@"first":first,@"second":second,@"list":list,@"expanded":expanded,@"restarted":restarted} options:NSJSONWritingPrettyPrinted error:nil];XCTAttachment* a=[XCTAttachment attachmentWithData:data uniformTypeIdentifier:@"public.json"];a.name=@"Completed mission actual Source UI and restart";a.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:a];[app terminate];
}

@end

#import <XCTest/XCTest.h>
#import <UIKit/UIKit.h>
#include <cmath>
#include <algorithm>

@interface WorldInputTests:XCTestCase @end
@implementation WorldInputTests
- (void)setUp{
    [super setUp];XCUIDevice.sharedDevice.orientation=UIDevice.currentDevice.userInterfaceIdiom==UIUserInterfaceIdiomPad?UIDeviceOrientationPortrait:UIDeviceOrientationLandscapeRight;
}
- (void)tearDown{
    // A failing path test must not leave the following test in another
    // device's reference orientation.
    XCUIDevice.sharedDevice.orientation=UIDevice.currentDevice.userInterfaceIdiom==UIUserInterfaceIdiomPad?UIDeviceOrientationPortrait:UIDeviceOrientationLandscapeRight;[super tearDown];
}
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
- (XCUICoordinate*)coordinateLogical:(CGPoint)logical world:(XCUIElement*)world {
    const auto state=[self state:world];const CGRect bounds=world.frame;const CGFloat width=[state[@"canvasWidth"] doubleValue],height=[state[@"canvasHeight"] doubleValue];
    XCTAssertGreaterThan(width,0);XCTAssertGreaterThan(height,0);const CGFloat scale=MIN(bounds.size.width/width,bounds.size.height/height);
    const CGFloat x=(bounds.size.width-width*scale)/2+logical.x*scale,y=(bounds.size.height-height*scale)/2+(height-logical.y)*scale;
    return [[world coordinateWithNormalizedOffset:CGVectorMake(0,0)] coordinateWithOffset:CGVectorMake(x,y)];
}
- (void)tapLogical:(CGPoint)logical world:(XCUIElement*)world {[[self coordinateLogical:logical world:world] tap];}
- (BOOL)fieldPointX:(double)x y:(double)y state:(NSDictionary*)state{
    return x>[state[@"safeX"] doubleValue]+16&&x<[state[@"safeX"] doubleValue]+[state[@"safeWidth"] doubleValue]-16&&y>[state[@"fieldInputMinY"] doubleValue]+20&&y<[state[@"safeY"] doubleValue]+[state[@"safeHeight"] doubleValue]-16;
}
- (void)tapHelpX:(double)x world:(XCUIElement*)world{
    const auto state=[self state:world];[self tapLogical:CGPointMake([state[@"canvasWidth"] doubleValue]*x/768,[state[@"canvasHeight"] doubleValue]*.5) world:world];
}
- (void)tapButton:(unsigned)identifier data:(int)data world:(XCUIElement*)world {
    [self wait:^BOOL{return [[self state:world][@"doorPhase"] intValue]==8;} description:@"original loading doors finished opening"];
    NSString* x=[NSString stringWithFormat:@"b%u_%d_x",identifier,data],*y=[NSString stringWithFormat:@"b%u_%d_y",identifier,data];
    for(unsigned attempt=0;attempt<6;++attempt){const auto current=[self state:world];if(!current[y])break;const double py=[current[y] doubleValue],low=[current[@"listClipY"] doubleValue]+24,high=[current[@"listClipTop"] doubleValue]-24;if(identifier<5100||identifier>5204||(py>=low&&py<=high))break;
        const double width=[current[@"canvasWidth"] doubleValue],height=[current[@"canvasHeight"] doubleValue];const CGRect bounds=world.frame;const auto point=[&](double px,double yUp){return [[world coordinateWithNormalizedOffset:CGVectorMake(0,0)] coordinateWithOffset:CGVectorMake(px/width*bounds.size.width,(height-yUp)/height*bounds.size.height)];};
        const double from=(low+high)*.5,to=from+(py<low?1:-1)*std::min((high-low)*.6,std::abs(py-std::clamp(py,low,high)));
        [point(width*.5,from) pressForDuration:.05 thenDragToCoordinate:point(width*.5,to)];
    }
    const auto state=[self state:world];
    XCTAssertNotNil(state[x]);XCTAssertNotNil(state[y]);[self tapLogical:CGPointMake([state[x] doubleValue],[state[y] doubleValue]) world:world];
}
- (void)titleToSlots:(XCUIElement*)world {
    [self wait:^BOOL{return [[self state:world][@"ticks"] doubleValue]>5;} description:@"real display callbacks started"];
    for(unsigned i=0;i<2&&[[self state:world][@"screen"] intValue]==1;++i)[self tapHelpX:384 world:world];
    [self wait:^BOOL{return [[self state:world][@"screen"] intValue]==2;} description:@"original title input opens slots"];
}
- (void)capture:(NSString*)name {
    XCTAttachment* shot=[XCTAttachment attachmentWithScreenshot:XCUIScreen.mainScreen.screenshot];shot.name=name;shot.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:shot];
}
- (NSDictionary*)inputHistory:(NSDictionary*)state{
    NSData* bytes=[[NSData alloc]initWithBase64EncodedString:state[@"inputHistoryBase64"]?:@"" options:0];
    return bytes?[NSJSONSerialization JSONObjectWithData:bytes options:0 error:nil]:@{@"columns":@[],@"events":@[]};
}
- (void)attachPathState:(NSDictionary*)state name:(NSString*)name{
    NSMutableDictionary* scalar=[state mutableCopy];[scalar removeObjectForKey:@"inputHistoryBase64"];
    NSData* data=[NSJSONSerialization dataWithJSONObject:@{@"state":scalar,@"nativeInputHistory":[self inputHistory:state]} options:NSJSONWritingPrettyPrinted error:nil];
    XCTAttachment* attachment=[XCTAttachment attachmentWithData:data uniformTypeIdentifier:@"public.json"];attachment.name=name;attachment.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:attachment];
}
- (NSArray<NSValue*>*)clearGroundPair:(NSDictionary*)state avoiding:(NSDictionary*)history{
    const double scale=[state[@"displayWorldScale"] doubleValue];NSMutableArray<NSValue*>* candidates=[NSMutableArray new];
    const double left=[state[@"safeX"] doubleValue]+36,right=[state[@"safeX"] doubleValue]+[state[@"safeWidth"] doubleValue]-36;
    const double bottom=std::max([state[@"fieldInputMinY"] doubleValue]+30,[state[@"safeY"] doubleValue]+30),top=[state[@"safeY"] doubleValue]+[state[@"safeHeight"] doubleValue]-30;
    const NSArray* columns=history[@"columns"],*events=history[@"events"];const auto wx=[columns indexOfObject:@"worldX"],wy=[columns indexOfObject:@"worldY"],consumed=[columns indexOfObject:@"world"];
    for(double y=bottom;y<top;y+=std::max(24.,48*scale))for(double x=left;x<right;x+=std::max(24.,48*scale)){
        if(hypot(x-[state[@"playerScreenX"] doubleValue],y-[state[@"playerScreenY"] doubleValue])<100*scale)continue;BOOL clear=YES;
        for(int i=0;i<[state[@"enemyCount"] intValue];++i)if(hypot(x-[state[[NSString stringWithFormat:@"e%d_x",i]] doubleValue],y-[state[[NSString stringWithFormat:@"e%d_y",i]] doubleValue])<70*scale){clear=NO;break;}
        // A replacement starts away from every recorded old path point. It
        // therefore exercises a new path instead of truncating a nearby one.
        if(clear&&wx!=NSNotFound&&wy!=NSNotFound&&consumed!=NSNotFound)for(NSArray* e in events)if([e[consumed] boolValue]){
            const double oldX=[state[@"playerScreenX"] doubleValue]+([e[wx] doubleValue]-[state[@"x"] doubleValue])*scale;
            const double oldY=[state[@"playerScreenY"] doubleValue]+([e[wy] doubleValue]-[state[@"y"] doubleValue])*scale;
            if(hypot(x-oldX,y-oldY)<45*scale){clear=NO;break;}
        }
        if(clear)[candidates addObject:[NSValue valueWithCGPoint:CGPointMake(x,y)]];
    }
    double longest=0;NSArray* pair=@[];for(NSValue* a in candidates)for(NSValue* b in candidates){const auto p=a.CGPointValue,q=b.CGPointValue;const double length=hypot(p.x-q.x,p.y-q.y);if(length>longest){longest=length;pair=@[a,b];}}
    return longest>=180*scale?pair:@[];
}
- (void)verifyOSPathReplacementMany:(BOOL)many{
    self.continueAfterFailure=NO;
    XCUIApplication* app=[[XCUIApplication alloc]initWithBundleIdentifier:@"com.gelb2.heavymach2.restored"];NSString* run=NSUUID.UUID.UUIDString;
    app.launchEnvironment=@{@"HM2_UI_TESTING":@"1",@"HM2_UI_TEST_RUN":run,@"HM2_UI_TEST_FIXTURE":@"recentInput"};[app launch];
    XCUIElement* world=app.otherElements[@"hm2.world"];XCTAssertTrue([world waitForExistenceWithTimeout:15]);[self titleToSlots:world];[self tapButton:1001 data:0 world:world];
    [self wait:^BOOL{const auto s=[self state:world];return [s[@"screen"] intValue]==7&&[s[@"doorPhase"] intValue]==8&&![s[@"helpVisible"] boolValue];} description:@"isolated original-format field start, doors open" timeout:60];
    const auto start=[self state:world];if(UIDevice.currentDevice.userInterfaceIdiom==UIUserInterfaceIdiomPad)XCTAssertGreaterThan([start[@"canvasHeight"] doubleValue],[start[@"canvasWidth"] doubleValue]);else XCTAssertGreaterThan([start[@"canvasWidth"] doubleValue],[start[@"canvasHeight"] doubleValue]);
    XCTAssertGreaterThanOrEqual([start[@"dashMaximumSP"] doubleValue],100);XCTAssertGreaterThan([start[@"dashRegeneration"] doubleValue],0);
    const auto firstPair=[self clearGroundPair:start avoiding:nil];[self attachPathState:start name:@"OS path initial fixture"];XCTAssertEqual(firstPair.count,2u);
    [[self coordinateLogical:[firstPair[0] CGPointValue] world:world] pressForDuration:.05 thenDragToCoordinate:[self coordinateLogical:[firstPair[1] CGPointValue] world:world] withVelocity:300 thenHoldForDuration:0];
    const auto first=[self state:world];[self attachPathState:first name:@"OS first multi-waypoint drag"];
    const auto firstHistory=[self inputHistory:first];const NSArray* columns=firstHistory[@"columns"];const auto pathAfter=[columns indexOfObject:@"pathAfter"];unsigned maximumPath=0;
    if(pathAfter!=NSNotFound)for(NSArray* row in firstHistory[@"events"])maximumPath=std::max(maximumPath,[row[pathAfter] unsignedIntValue]);
    XCTAssertGreaterThan(maximumPath,1u);XCTAssertEqual([first[@"movePointer"] longLongValue],0ll);
    const auto replacement=[self clearGroundPair:first avoiding:firstHistory];XCTAssertEqual(replacement.count,2u);
    if(many)[[self coordinateLogical:[replacement[0] CGPointValue] world:world] pressForDuration:.05 thenDragToCoordinate:[self coordinateLogical:[replacement[1] CGPointValue] world:world] withVelocity:300 thenHoldForDuration:0];
    else [self tapLogical:[replacement[0] CGPointValue] world:world];
    const auto replaced=[self state:world];[self attachPathState:replaced name:many?@"OS replacement multi-waypoint drag":@"OS replacement single waypoint"];
    const auto replacedHistory=[self inputHistory:replaced];const NSArray* replacedColumns=replacedHistory[@"columns"];const auto phase=[replacedColumns indexOfObject:@"phase"],afterPath=[replacedColumns indexOfObject:@"pathAfter"];NSArray* lastEnd=nil;
    if(phase!=NSNotFound)for(NSArray* e in replacedHistory[@"events"])if([e[phase] unsignedIntValue]==2)lastEnd=e;
    XCTAssertNotNil(lastEnd);if(lastEnd&&afterPath!=NSNotFound){if(many)XCTAssertGreaterThan([lastEnd[afterPath] unsignedIntValue],1u);else XCTAssertEqual([lastEnd[afterPath] unsignedIntValue],1u);}
    XCTAssertEqual([replaced[@"movePointer"] longLongValue],0ll);
    const double afterReplacement=[replaced[@"worldTime"] doubleValue];
    // Wait for the game's actual Equipment.tickSpecial and original .5s
    // double-tap window. No runtime SP or tap timestamp is assigned by tests.
    [self wait:^BOOL{const auto s=[self state:world];return [s[@"dashSP"] doubleValue]>=100&&[s[@"worldTime"] doubleValue]>afterReplacement+.6;} description:@"original live SP regeneration and previous tap interval" timeout:60];
    const auto before=[self state:world];const auto dashPair=[self clearGroundPair:before avoiding:nil];XCTAssertEqual(dashPair.count,2u);
    [[self coordinateLogical:[dashPair[0] CGPointValue] world:world] doubleTap];
    const auto after=[self state:world];[self attachPathState:before name:@"OS double-tap before"];[self attachPathState:after name:@"OS double-tap after with native 256-event ring"];[self capture:many?@"path-many-then-OS-doubletap":@"path-single-then-OS-doubletap"];
    XCTAssertGreaterThan([after[@"dashStarts"] longLongValue],[before[@"dashStarts"] longLongValue]);XCTAssertEqual([after[@"movePointer"] longLongValue],0ll);
    const auto history=[self inputHistory:after];const NSArray* finalColumns=history[@"columns"];const auto dashBefore=[finalColumns indexOfObject:@"dashBefore"],dashAfter=[finalColumns indexOfObject:@"dashAfter"],spBefore=[finalColumns indexOfObject:@"spBefore"],spAfter=[finalColumns indexOfObject:@"spAfter"];BOOL actualCost=NO;
    if(dashBefore!=NSNotFound&&dashAfter!=NSNotFound&&spBefore!=NSNotFound&&spAfter!=NSNotFound)for(NSArray* e in history[@"events"])if([e[dashBefore] unsignedLongLongValue]>=static_cast<unsigned long long>([before[@"dashStarts"] longLongValue])&&[e[dashAfter] unsignedLongLongValue]>[e[dashBefore] unsignedLongLongValue]&&std::abs([e[spBefore] doubleValue]-[e[spAfter] doubleValue]-100)<.001)actualCost=YES;
    XCTAssertTrue(actualCost,@"Actual OS begin must start Dash and consume100SP. Inspect attached native input phases/tap times.");
    NSLog(@"HM2_OS_PATH_DASH run=%@ replacement=%@ firstMaximumPath=%u beforeDash=%@ afterDash=%@ beforeSP=%@ afterSP=%@",run,many?@"many":@"single",maximumPath,before[@"dashStarts"],after[@"dashStarts"],before[@"dashSP"],after[@"dashSP"]);[app terminate];
}
- (void)testOSMultiplePathThenSinglePathThenDoubleTap{[self verifyOSPathReplacementMany:NO];}
- (void)testOSMultiplePathThenMultiplePathThenDoubleTap{[self verifyOSPathReplacementMany:YES];}
- (void)testOSBackgroundReturnKeepsMenuPaused{
    self.continueAfterFailure=NO;XCUIApplication* app=[[XCUIApplication alloc]initWithBundleIdentifier:@"com.gelb2.heavymach2.restored"];NSString* run=NSUUID.UUID.UUIDString;
    app.launchEnvironment=@{@"HM2_UI_TESTING":@"1",@"HM2_UI_TEST_RUN":run,@"HM2_UI_TEST_FIXTURE":@"recentInput"};[app launch];
    XCUIElement* world=app.otherElements[@"hm2.world"];XCTAssertTrue([world waitForExistenceWithTimeout:15]);[self titleToSlots:world];[self tapButton:1001 data:0 world:world];
    [self wait:^BOOL{const auto s=[self state:world];return [s[@"screen"] intValue]==7&&[s[@"doorPhase"] intValue]==8&&![s[@"paused"] boolValue]&&![s[@"helpVisible"] boolValue];} description:@"live isolated field before real OS background" timeout:60];
    for(unsigned cycle=0;cycle<3;++cycle){
        const auto before=[self state:world];const auto saves=[before[@"saves"] longLongValue];
        [XCUIDevice.sharedDevice pressButton:XCUIDeviceButtonHome];
        [self wait:^BOOL{return app.state==XCUIApplicationStateRunningBackground||app.state==XCUIApplicationStateRunningBackgroundSuspended;} description:@"Home actually backgrounds the application"];
        [app activate];world=app.otherElements[@"hm2.world"];
        [self wait:^BOOL{const auto s=[self state:world];return world.exists&&[s[@"paused"] boolValue]&&[s[@"systemVisible"] boolValue]&&s[@"b1030_0_x"]&&s[@"b1030_0_y"];} description:@"OS foreground returns to paused GPU system menu with Resume1030"];
        const auto paused=[self state:world];[self attachPathState:paused name:[NSString stringWithFormat:@"OS background cycle%u returned menu",cycle+1]];
        XCTAssertEqual([paused[@"saves"] longLongValue],saves);const auto pausedTicks=[paused[@"ticks"] longLongValue];
        [self wait:^BOOL{return [[self state:world][@"ticks"] longLongValue]>=pausedTicks+12;} description:@"display callbacks continue while foreground World remains paused"];
        const auto held=[self state:world];[self attachPathState:held name:[NSString stringWithFormat:@"OS background cycle%u twelve paused callbacks",cycle+1]];
        XCTAssertTrue([held[@"paused"] boolValue]);XCTAssertTrue([held[@"systemVisible"] boolValue]);XCTAssertNotNil(held[@"b1030_0_x"]);
        XCTAssertEqualWithAccuracy([held[@"worldTime"] doubleValue],[paused[@"worldTime"] doubleValue],.000000001);
        XCTAssertEqualWithAccuracy([held[@"x"] doubleValue],[paused[@"x"] doubleValue],.000001);XCTAssertEqualWithAccuracy([held[@"y"] doubleValue],[paused[@"y"] doubleValue],.000001);
        XCTAssertEqual([held[@"saves"] longLongValue],saves);
        [self tapButton:1030 data:0 world:world];
        [self wait:^BOOL{const auto s=[self state:world];return ![s[@"paused"] boolValue]&&![s[@"systemVisible"] boolValue]&&[s[@"worldTime"] doubleValue]>[held[@"worldTime"] doubleValue];} description:@"actual GPU Resume touch restarts World time"];
        const auto resumed=[self state:world];XCTAssertEqual([resumed[@"saves"] longLongValue],saves);
        NSMutableDictionary* original=[before mutableCopy],*after=[resumed mutableCopy];[original removeObjectForKey:@"inputHistoryBase64"];[after removeObjectForKey:@"inputHistoryBase64"];
        NSData* data=[NSJSONSerialization dataWithJSONObject:@{@"kind":@"OS Home and foreground activation",@"telephoneInterruptionTested":@NO,@"run":run,@"cycle":@(cycle+1),@"before":original,@"resumed":after} options:NSJSONWritingPrettyPrinted error:nil];
        XCTAttachment* attachment=[XCTAttachment attachmentWithData:data uniformTypeIdentifier:@"public.json"];attachment.name=[NSString stringWithFormat:@"OS background actual resume cycle%u",cycle+1];attachment.lifetime=XCTAttachmentLifetimeKeepAlways;[self addAttachment:attachment];
        NSLog(@"HM2_OS_BACKGROUND cycle=%u run=%@ pausedWorldTime=%@ heldWorldTime=%@ resumedWorldTime=%@ unchangedSaves=%lld",cycle+1,run,paused[@"worldTime"],held[@"worldTime"],resumed[@"worldTime"],saves);
    }
    [app terminate];
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
    [self tapHelpX:384 world:world];[self wait:^BOOL{return [[self state:world][@"helpVisible"] intValue]==0;} description:@"first Korean base Help consumes the closing tap"];
    [self tapButton:2001 data:0 world:world];[self tapButton:2001 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"screen"] intValue]==4;} description:@"command center opens"];
    const int rankingSaves=[[self state:world][@"saves"] intValue];[self tapButton:5009 data:0 world:world];
    XCUIElement* ranking=app.alerts[@"랭킹"];XCTAssertTrue([ranking waitForExistenceWithTimeout:10]);XCTAssertTrue(ranking.staticTexts[@"현재 랭킹 서비스를 이용할 수 없습니다."].exists);XCTAssertEqual(ranking.buttons.count,1u);[self capture:@"ranking-service-unavailable"];[ranking.buttons[@"OK"] tap];
    XCTAssertEqual([[self state:world][@"screen"] intValue],4);XCTAssertEqual([[self state:world][@"saves"] intValue],rankingSaves);[self tapButton:2100 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"screen"] intValue]==3;} description:@"ranking notice closes without leaving command; exit returns to base"];
    [self tapButton:2005 data:0 world:world];[self tapButton:2005 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"screen"] intValue]==7&&[[self state:world][@"enemyCount"] intValue]>0;} description:@"real base gate enters original field"];
    [self wait:^BOOL{return [[self state:world][@"doorPhase"] intValue]==8;} description:@"field opening completes while original world advances"];
    XCTAssertEqual([[self state:world][@"helpVisible"] intValue],1);XCTAssertEqual([[self state:world][@"helpPage"] intValue],2);XCTAssertEqual([[self state:world][@"helpGuidance"] intValue],0);
    [self capture:@"source-initial-korean-help"];[self tapHelpX:700 world:world];[self wait:^BOOL{return [[self state:world][@"helpVisible"] intValue]==0;} description:@"fixed initial Help consumes closing tap"];
    NSDictionary* before=[self state:world];CGPoint destination=CGPointZero;BOOL found=NO;const CGFloat offsets[][2]={{100,0},{0,100},{100,100},{-100,0},{-100,80},{0,-100},{100,-100},{-100,-100}};const CGFloat worldScale=[before[@"displayWorldScale"] doubleValue];XCTAssertGreaterThan(worldScale,0);
    for(const auto& delta:offsets){const CGFloat x=[before[@"playerScreenX"] doubleValue]+delta[0]*worldScale,y=[before[@"playerScreenY"] doubleValue]+delta[1]*worldScale;if(![self fieldPointX:x y:y state:before])continue;BOOL clear=YES;
        for(int i=0;i<[before[@"enemyCount"] intValue];++i){const CGFloat ex=[before[[NSString stringWithFormat:@"e%d_x",i]] doubleValue],ey=[before[[NSString stringWithFormat:@"e%d_y",i]] doubleValue];if(hypot(x-ex,y-ey)<100*worldScale)clear=NO;}
        if(clear){destination=CGPointMake(x,y);found=YES;break;}}
    XCTAssertTrue(found);[self tapLogical:destination world:world];
    [self wait:^BOOL{const auto now=[self state:world];return hypot([now[@"x"] doubleValue]-[before[@"x"] doubleValue],[now[@"y"] doubleValue]-[before[@"y"] doubleValue])>5;} description:@"real UIKit touch moves Player on original terrain"];
    NSDictionary* moving=[self state:world];XCTAssertGreaterThan([moving[@"ticks"] doubleValue],[before[@"ticks"] doubleValue]);[self capture:@"source-field-after-real-touch"];
    found=NO;unsigned long long targetID=0;CGFloat nearest=CGFLOAT_MAX;const auto targets=[self state:world];for(int i=0;i<[targets[@"enemyCount"] intValue];++i){const CGFloat x=[targets[[NSString stringWithFormat:@"e%d_x",i]] doubleValue],y=[targets[[NSString stringWithFormat:@"e%d_y",i]] doubleValue];const CGFloat distance=hypot(x-[targets[@"playerScreenX"] doubleValue],y-[targets[@"playerScreenY"] doubleValue]);if([self fieldPointX:x y:y state:targets]&&distance<nearest){nearest=distance;destination=CGPointMake(x,y);targetID=[targets[[NSString stringWithFormat:@"e%d_id",i]] longLongValue];found=YES;}}
    XCTAssertTrue(found);[self tapLogical:destination world:world];[self wait:^BOOL{const auto now=[self state:world];return [now[@"selectionTouches"] longLongValue]>[targets[@"selectionTouches"] longLongValue]&&[now[@"touchSelectedEnemy"] longLongValue]==targetID&&[now[@"emissions"] longLongValue]>[targets[@"emissions"] longLongValue];} description:@"real enemy selection ID causes additional original weapon emissions"];
    [self tapButton:1021 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"paused"] intValue]==1;} description:@"system GPU menu pauses world"];
    NSDictionary* paused=[self state:world];[self wait:^BOOL{return [[self state:world][@"ticks"] doubleValue]>[paused[@"ticks"] doubleValue]+10;} description:@"display continues while world is paused"];
    XCTAssertEqualWithAccuracy([[self state:world][@"worldTime"] doubleValue],[paused[@"worldTime"] doubleValue],0.000001);
    [self tapButton:1030 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"paused"] intValue]==0;} description:@"resume GPU control unpauses world"];
    [self tapButton:1021 data:0 world:world];[self tapButton:1029 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"helpVisible"] intValue]==1&&[[self state:world][@"paused"] intValue]==1;} description:@"actual menu Help opens third Korean page and pauses world"];
    XCTAssertEqual([[self state:world][@"helpPage"] intValue],2);XCTAssertEqual([[self state:world][@"helpGuidance"] intValue],1);[self capture:@"source-menu-korean-help-guidance"];
    [self tapHelpX:384 world:world];XCTAssertEqual([[self state:world][@"helpVisible"] intValue],1);XCTAssertEqual([[self state:world][@"helpGuidance"] intValue],0);
    [self tapHelpX:100 world:world];XCTAssertEqual([[self state:world][@"helpPage"] intValue],1);[self tapHelpX:700 world:world];XCTAssertEqual([[self state:world][@"helpPage"] intValue],2);
    [self tapHelpX:384 world:world];[self wait:^BOOL{return [[self state:world][@"helpVisible"] intValue]==0&&[[self state:world][@"paused"] intValue]==0;} description:@"Help closes without leaking its input to world"];

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
    for(int i=0;i<[field[@"enemyCount"] intValue];++i){const double x=[field[[NSString stringWithFormat:@"e%d_x",i]] doubleValue],y=[field[[NSString stringWithFormat:@"e%d_y",i]] doubleValue];if([self fieldPointX:x y:y state:field]){target=[field[[NSString stringWithFormat:@"e%d_id",i]] longLongValue];[self tapLogical:CGPointMake(x,y) world:world];selected=YES;break;}}
    // Provoke the first enemy, then change target while it is still alive.
    // Finishing that single enemy can leave the Player regenerating safely.
    BOOL retargeted=NO;
    // XCTest reads and synthesized touches run while the world/camera move.
    // Track the live target within this same battle; never reset its outcome.
    for(unsigned attempt=0;attempt<3&&!retargeted;++attempt){
        const auto pursuing=[self state:world];BOOL firstAlive=NO,found=NO;CGFloat nearest=CGFLOAT_MAX;CGPoint next=CGPointZero;unsigned long long second=0;
        for(int i=0;i<[pursuing[@"enemyCount"] intValue];++i){const auto id=[pursuing[[NSString stringWithFormat:@"e%d_id",i]] longLongValue];if(id==target){firstAlive=YES;continue;}const CGFloat x=[pursuing[[NSString stringWithFormat:@"e%d_x",i]] doubleValue],y=[pursuing[[NSString stringWithFormat:@"e%d_y",i]] doubleValue],distance=hypot(x-[pursuing[@"playerScreenX"] doubleValue],y-[pursuing[@"playerScreenY"] doubleValue]);if([self fieldPointX:x y:y state:pursuing]&&distance<nearest){nearest=distance;next=CGPointMake(x,y);second=id;found=YES;}}
        XCTAssertTrue(firstAlive&&found);if(!firstAlive||!found)break;
        [self tapLogical:next world:world];const auto after=[self state:world];retargeted=static_cast<unsigned long long>([after[@"touchSelectedEnemy"] longLongValue])==second;
        NSLog(@"HM2_GAMEOVER_RETARGET attempt=%u requested=%llu point=%.3f,%.3f before=%@ after=%@",attempt,second,next.x,next.y,pursuing,after);
    }
    XCTAssertTrue(retargeted);
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
    if([base[@"helpVisible"] boolValue]){[self tapHelpX:384 world:world];[self wait:^BOOL{return ![[self state:world][@"helpVisible"] boolValue];} description:@"initial base help closes before mission navigation"];}
    [self tapButton:2001 data:0 world:world];[self tapButton:2001 data:0 world:world];
    [self wait:^BOOL{const auto s=[self state:world];return [s[@"completedMission"] isEqualToString:@"M002010"]&&[s[@"doorPhase"] intValue]==8;} description:@"first completed window awards first accepted eligible mission" timeout:60];
    NSDictionary* first=[self state:world];XCTAssertEqual([first[@"credits"] intValue],580);XCTAssertEqual([first[@"experience"] intValue],200);XCTAssertEqual([first[@"completedRewardCount"] intValue],2);XCTAssertEqual([first[@"saves"] intValue],saves);XCTAssertEqual([first[@"commandListVisible"] intValue],0);[self capture:@"completed-first-M002010-rewards"];
    [self tapButton:5009 data:0 world:world];XCUIElement* notice=app.alerts[@"랭킹"];XCTAssertTrue([notice waitForExistenceWithTimeout:10]);XCTAssertTrue(notice.staticTexts[@"현재 랭킹 서비스를 이용할 수 없습니다."].exists);[notice.buttons[@"OK"] tap];
    XCTAssertEqualObjects([self state:world][@"completedMission"],@"M002010");
    [self tapButton:5007 data:0 world:world];[self wait:^BOOL{return [[self state:world][@"completedMission"] isEqualToString:@"M001010"];} description:@"actual result OK advances once to second eligible mission"];
    NSDictionary* second=[self state:world];XCTAssertEqual([second[@"credits"] intValue],680);XCTAssertEqual([second[@"experience"] intValue],400);XCTAssertEqual([second[@"level"] intValue],1);XCTAssertEqual([second[@"repairItems"] intValue],items+3);XCTAssertEqual([second[@"completedRewardCount"] intValue],3);XCTAssertEqual([second[@"saves"] intValue],saves);[self capture:@"completed-second-M001010-rewards"];
    [self tapButton:5007 data:0 world:world];[self wait:^BOOL{const auto s=[self state:world];return [s[@"completedMission"] length]==0&&[s[@"commandListVisible"] intValue]==1;} description:@"last OK removes result and next Command tick displays remaining missions"];
    NSDictionary* list=[self state:world];XCTAssertEqual([list[@"credits"] intValue],680);XCTAssertEqual([list[@"acceptedCount"] intValue],2);XCTAssertEqual([list[@"saves"] intValue],saves);[self capture:@"completed-remaining-mission-list"];
    const float acceptedY=[list[@"b5200_0_hdY"] floatValue];
    const float newY=[list[@"b5100_0_hdY"] floatValue];
    [self tapButton:5200 data:0 world:world];
    [self wait:^BOOL{
        const auto s=[self state:world];
        return [s[@"b5200_0_hdY"] floatValue]==acceptedY+128&&s[@"b5004_0_hdY"]==nil;
    } description:@"accepted mission opens its detail without an ACCEPT button"];
    [self capture:@"command-accepted-mission-detail"];
    [self tapButton:5100 data:0 world:world];
    [self wait:^BOOL{
        const auto s=[self state:world];
        return [s[@"b5100_0_hdY"] floatValue]==newY+128
            &&[s[@"b5200_0_hdY"] floatValue]==acceptedY-128
            &&[s[@"b5004_0_hdY"] floatValue]==newY-24-84;
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

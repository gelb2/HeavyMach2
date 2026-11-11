#import <Foundation/Foundation.h>
#ifdef __cplusplus
#include <string_view>
#endif

// Call from the game/platform owner thread. Preparation performs file reads;
// play only uses a prepared source/buffer. Positional values are the original
// SoundManager x/y values, without a second platform-side pan conversion.
@interface HM2Audio:NSObject
- (instancetype)initWithResourceRoot:(NSURL*)root;
- (BOOL)prepareSounds;
- (BOOL)prepareSound:(NSString*)name;
- (BOOL)play:(NSString*)name x:(float)x y:(float)y;
#ifdef __cplusplus
// Prepared catalogue/static-ID names cross without allocating an NSString.
- (BOOL)playEffect:(std::string_view)name x:(float)x y:(float)y;
#endif
- (void)stopEffects;
- (void)setActive:(BOOL)active;
- (BOOL)playMusic:(NSString*)name;
- (void)stopMusic;
- (float)effectPlaybackTime:(NSString*)name;
- (NSInteger)effectSampleOffset:(NSString*)name;
@property(nonatomic,readonly) NSUInteger currentContextIdentity;
@property(nonatomic,readonly) NSUInteger ownerContextIdentity;
@property(nonatomic,readonly) NSUInteger loadedSounds;
@property(nonatomic,readonly) NSDictionary<NSString*,NSNumber*>* metrics;
@property(nonatomic,readonly) NSArray<NSDictionary*>* playbackSnapshot;
@property(nonatomic,readonly) NSString* lastError;
@property(nonatomic,readonly) NSString* musicName;
@property(nonatomic,readonly) BOOL musicPlaying;
@property(nonatomic,readonly) BOOL musicLooping;
@property(nonatomic,readonly) float musicPlaybackTime;
@end

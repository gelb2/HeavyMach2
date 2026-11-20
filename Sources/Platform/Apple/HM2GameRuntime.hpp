#pragma once
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#include "Core/Session/GameSession.hpp"
#include <functional>
#include <map>

namespace hm2::apple {
struct NativeRequest {
    enum class Kind {name,deleteSlot,alert,saveError,gameOver,notice,
                     recoveryMenu,recoveryInput,recoveryConfirm};
    Kind kind;unsigned slot=0;std::string text;std::string title{};
    std::uint64_t token=0;
};
struct InputObservation {
    PointerPhase phase{};Vec2 canvas{};std::uintptr_t pointer=0,movePointerBefore=0,movePointerAfter=0;
    unsigned pathBefore=0,pathAfter=0;float tapBefore=0,tapAfter=0,specialBefore=0,specialAfter=0;
    double worldTimeBefore=0,worldTimeAfter=0;std::uint64_t dashStartsBefore=0,dashStartsAfter=0;
    Vec2 playerBefore{},worldPoint{};
    bool overlayConsumed=false,uiConsumed=false,worldConsumed=false,terminalCancelled=false,sameCanvasMove=false;
};
// Apple adapter for the shared HD game state. The view supplies logical
// canvas/safe-area geometry; game UI and world output remain in the core.
class GameRuntime {
    struct Impl;std::unique_ptr<Impl> impl_;
    bool dispatchInput(PointerPhase,Vec2,std::uintptr_t,bool sameCanvasMove);
public:
    GameRuntime(NSURL* resourceRoot,NSURL* isolatedSaveFile,std::function<void(const NativeRequest&)>,std::function<std::uint32_t()> absoluteTimeLow={});
    ~GameRuntime();
    GameRuntime(const GameRuntime&)=delete;
    void tick();
    void draw(CAEAGLLayer*);
    bool input(PointerPhase,Vec2,std::uintptr_t);
    bool inputFromView(PointerPhase,CGPoint current,CGPoint previous,std::uintptr_t);
    bool provideName(unsigned,std::string_view,std::string_view country);
    bool confirmDelete(unsigned);
    bool confirmGameOver(std::uint64_t token);
    [[nodiscard]] std::string recoveryCode() const;
    void validateRecoveryCode(std::string_view code) const;
    bool restoreRecoveryCode(std::string_view code);
    bool logicalPoint(CGPoint,Vec2&)const;
    [[nodiscard]] std::string nativeText(std::string_view)const;
    void setActive(bool);
    void setAudioActive(bool);
    void setDisplay(DisplayConfiguration);
    void synchronize();
    GameSession& session();
    ui::Screens& screens();
    [[nodiscard]] std::uint64_t resourceReads()const;
    [[nodiscard]] std::map<std::string,unsigned> resourceReadCounts()const;
    [[nodiscard]] std::size_t preparedTextCount()const;
    [[nodiscard]] std::vector<std::string> preparedTextIdentities()const;
    [[nodiscard]] NSUInteger textureUploads()const;
    [[nodiscard]] NSDictionary<NSString*,NSNumber*>* audioMetrics()const;
    [[nodiscard]] NSArray<NSDictionary*>* audioPlaybackSnapshot()const;
    [[nodiscard]] NSDictionary<NSString*,NSNumber*>* resourceMetrics()const;
    [[nodiscard]] std::vector<InputObservation> inputHistory()const;
    [[nodiscard]] NSData* pixels();
    void captureNextFrame();
    [[nodiscard]] CGSize drawableSize()const;
};
}

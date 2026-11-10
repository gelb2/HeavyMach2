#pragma once
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#include "Core/Session/GameSession.hpp"
#include <functional>

namespace hm2::apple {
struct NativeRequest {
    enum class Kind {name,deleteSlot,alert,saveError,gameOver};
    Kind kind;unsigned slot=0;std::string text;std::string title{};
    std::uint64_t token=0;
};
// Apple adapter for the restored HD game canvas. Device-specific canvas/HUD
// adaptation remains a separate task; this is also the GPU integration harness.
class GameRuntime {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    GameRuntime(NSURL* resourceRoot,NSURL* isolatedSaveFile,std::function<void(const NativeRequest&)>);
    ~GameRuntime();
    GameRuntime(const GameRuntime&)=delete;
    void tick();
    void draw(CAEAGLLayer*);
    bool input(PointerPhase,Vec2,std::uintptr_t);
    bool provideName(unsigned,std::string_view,std::string_view country);
    bool confirmDelete(unsigned);
    bool confirmGameOver(std::uint64_t token);
    bool logicalPoint(CGPoint,Vec2&)const;
    [[nodiscard]] std::string nativeText(std::string_view)const;
    void setActive(bool);
    void synchronize();
    GameSession& session();
    ui::Screens& screens();
    [[nodiscard]] std::uint64_t resourceReads()const;
    [[nodiscard]] std::size_t preparedTextCount()const;
    [[nodiscard]] NSUInteger textureUploads()const;
    [[nodiscard]] NSDictionary<NSString*,NSNumber*>* audioMetrics()const;
    [[nodiscard]] NSData* pixels();
    void captureNextFrame();
    [[nodiscard]] CGSize drawableSize()const;
};
}

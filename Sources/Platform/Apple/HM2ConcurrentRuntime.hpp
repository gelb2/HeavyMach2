#pragma once
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#include "Core/Validation/ConcurrentValidation.hpp"

namespace hm2::apple {
// One renderer/output device. Independent original game states advance on
// every display callback; menu scenarios never stop the other battle cases.
class ConcurrentRuntime {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    ConcurrentRuntime(NSURL* resourceRoot,NSURL* validationSaveDirectory);
    ~ConcurrentRuntime();
    ConcurrentRuntime(const ConcurrentRuntime&)=delete;
    void tick();
    void draw(CAEAGLLayer*,Size viewport);
    void setActive(bool);
    void setAudioActive(bool);
    void setDisplay(DisplayConfiguration);
    [[nodiscard]] NSDictionary* verifyInactiveInputDiscard();
    [[nodiscard]] NSDictionary* report()const;
    [[nodiscard]] validation::ConcurrentValidation& validation();
    // Same cached frame, one renderer/context, no simulation/UI/VFX advance.
    [[nodiscard]] NSDictionary* verifyRepeatedRendering(CAEAGLLayer*,Size viewport);
    [[nodiscard]] bool captureStoredFrame(unsigned frame,CAEAGLLayer*);
    // QA observation only: render an already composed live scenario at full
    // surface size, without revealButton, input, or an extra simulation tick.
    [[nodiscard]] NSDictionary* captureScenarioUI(unsigned scenario,CAEAGLLayer*);
    void captureNextFrame();
    [[nodiscard]] NSData* pixels()const;
    [[nodiscard]] NSData* legacyComparisonPixels()const;
    [[nodiscard]] CGSize drawableSize()const;
};
}

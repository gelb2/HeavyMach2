#pragma once
#include "Core/Session/GameSession.hpp"
#include "Core/UI/Text.hpp"
#include <filesystem>
#include <functional>

namespace hm2::android {

enum class DialogKind {notice, name, deleteSlot, gameOver, visitWebsite,
                       recoveryMenu, recoveryInput, recoveryConfirm, recoveryCopy};
struct Dialog {
    DialogKind kind;
    std::uint64_t argument;
    std::string title, message;
};

// All services run on the game thread; the Android window owns their lifetime.
struct RuntimeServices {
    ResourceAccess& resources;
    ui::TextRasterizer& rasterizer;
    std::function<void(const Frame&)> prepareFrame;
    std::function<void(const Frame&)> retainTextures;
    std::function<void(std::string_view,const ui::TextBitmap&)> uploadText;
    std::function<void(const Frame&)> drawFrame;
    std::function<void(std::string_view)> prepareSound;
    std::function<void(std::string_view,Vec2)> playSound;
    std::function<void(std::string_view)> playMusic;
    std::function<void(bool)> setAudioActive;
    std::function<void(const Dialog&)> showDialog;
};

class Runtime {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    Runtime(RuntimeServices, std::filesystem::path filesDirectory);
    ~Runtime();
    void tick();
    void draw();
    void setDisplay(DisplayConfiguration);
    void restoreGraphics();
    void setActive(bool);
    void setAudioFocused(bool);
    void input(PointerPhase, Vec2 canvas, std::uintptr_t pointer);
    void cancelInputs();
    void confirm(DialogKind,std::uint64_t argument,std::string_view name,std::string_view country);
    std::string status();
#if HM2_CHEAT
    std::string validate(std::string_view command);
    void setTestClock(std::uint32_t absoluteTimeLow);
    GameSession& testSession();
    ui::Screens& testScreens();
    void refreshTestScene();
#endif
};
}

#pragma once
#include "Runtime.hpp"
namespace hm2::android {
SaveData playableFixture();
class Campaign {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    Campaign(RuntimeServices&,std::filesystem::path,DisplayConfiguration);
    ~Campaign();
    void setActive(bool);
    void setAudioFocused(bool);
    void setDisplay(DisplayConfiguration);
    void tick();
    void draw();
    void restoreGraphics();
    std::string status() const;
};

// The Android cheat edition links the same multi-session runner as the Apple
// validation target. It owns independent disk slots and presents a real case.
class Validation {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    Validation(RuntimeServices&, std::filesystem::path directory, DisplayConfiguration);
    ~Validation();
    void setActive(bool);
    void setDisplay(DisplayConfiguration);
    void tick();
    void draw();
    void restoreGraphics();
    std::string status() const;
};
}

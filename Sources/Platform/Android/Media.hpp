#pragma once

#include "Core/Audio/OpenALEffects.hpp"
#include "Core/Data/Resources.hpp"
#include "Core/Types.hpp"
#include <android/asset_manager.h>

namespace hm2::android {

// The Java AssetManager must outlive this adapter.
class Assets final : public ResourceAccess {
public:
    explicit Assets(AAssetManager* manager);
    Bytes read(std::string_view name) override;
    bool exists(std::string_view name) const override;

private:
    AAssetManager* manager_;
};

// One OpenAL device and one prepared source per sound for the game session.
class Effects final {
public:
    explicit Effects(ResourceAccess& assets);
    void prepare(std::string_view name);
    void play(std::string_view name, Vec2 position);
    void setActive(bool active);
    audio::AudioMetrics metrics() const { return effects_.metrics(); }

private:
    ResourceAccess& assets_;
    audio::OpenALEffects effects_;
    bool active_ = true;
};
}

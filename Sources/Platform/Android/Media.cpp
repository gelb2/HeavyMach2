#include "Media.hpp"
#include "Core/Data/ResourceName.hpp"
#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>

namespace hm2::android {
namespace {
using Asset = std::unique_ptr<AAsset, decltype(&AAsset_close)>;

Asset openAsset(AAssetManager* manager, std::string_view name) {
    if (!validResourceName(name)) throw std::runtime_error("Invalid game asset path");
    return Asset(AAssetManager_open(manager, std::string(name).c_str(), AASSET_MODE_STREAMING), AAsset_close);
}
}

Assets::Assets(AAssetManager* manager) : manager_(manager) {
    if (!manager_) throw std::invalid_argument("Missing Android AssetManager");
}

bool Assets::exists(std::string_view name) const {
    return static_cast<bool>(openAsset(manager_, name));
}

Bytes Assets::read(std::string_view name) {
    auto asset = openAsset(manager_, name);
    if (!asset) throw std::runtime_error("Missing APK asset: " + std::string(name));
    const auto length = AAsset_getLength64(asset.get());
    if (length < 0 || static_cast<std::uint64_t>(length) > Bytes().max_size())
        throw std::runtime_error("Invalid asset length: " + std::string(name));
    Bytes bytes(static_cast<std::size_t>(length));
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        auto remaining = std::min<std::size_t>(bytes.size() - offset, std::numeric_limits<int>::max());
        int count = AAsset_read(asset.get(), bytes.data() + offset, remaining);
        if (count <= 0) throw std::runtime_error("Incomplete APK asset: " + std::string(name));
        offset += static_cast<std::size_t>(count);
    }
    return bytes;
}

Effects::Effects(ResourceAccess& assets) : assets_(assets) {
    if (!effects_.valid()) throw std::runtime_error("Cannot open Android effects output: " + effects_.error());
}

void Effects::prepare(std::string_view name) {
    if (effects_.contains(name)) return;
    auto pcm = audio::decodeWave(assets_.read(name));
    if (!effects_.prepare(std::string(name), pcm))
        throw std::runtime_error("Cannot prepare " + std::string(name) + ": " + effects_.error());
}

void Effects::play(std::string_view name, Vec2 position) {
    if (!active_ || name.empty()) return;
    prepare(name);
    if (!effects_.play(name, position.x, position.y))
        throw std::runtime_error("Cannot play " + std::string(name) + ": " + effects_.error());
}

void Effects::setActive(bool active) {
    if (active_ == active) return;
    effects_.setActive(active);
    if (!effects_.error().empty()) throw std::runtime_error(effects_.error());
    active_ = active;
}
}

#pragma once
#include "Core/Data/Resources.hpp"
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <stdexcept>
#include <system_error>
#include <unistd.h>

namespace hm2::android {
inline std::optional<Bytes> readFile(const std::filesystem::path& path, std::size_t limit) {
    if (!std::filesystem::exists(path)) return {};
    const auto size = std::filesystem::file_size(path);
    if (size > limit) throw std::runtime_error("Invalid size for " + path.filename().string());
    std::ifstream file(path, std::ios::binary);
    Bytes bytes(size);
    if (!file || !file.read(reinterpret_cast<char*>(bytes.data()), size))
        throw std::runtime_error("Could not read " + path.filename().string());
    return bytes;
}

// Both paths are in the app-private directory on the same filesystem.
inline bool writeFile(const std::filesystem::path& path, std::span<const std::uint8_t> bytes) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    auto temporary = path.string() + ".XXXXXX";
    const int fd = mkstemp(temporary.data());
    if (fd == -1) return false;
    while (!bytes.empty()) {
        const auto written = ::write(fd, bytes.data(), bytes.size());
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) break;
        bytes = bytes.subspan(static_cast<std::size_t>(written));
    }
    const bool flushed = bytes.empty() && fsync(fd) == 0;
    const bool closed = ::close(fd) == 0;
    if (flushed && closed && std::rename(temporary.c_str(), path.c_str()) == 0) return true;
    std::remove(temporary.c_str());
    return false;
}

inline std::filesystem::path hudPath(const std::filesystem::path& directory, std::string_view profile) {
    if (profile != "compact" && profile != "regular") throw std::invalid_argument("Invalid HUD profile");
    return directory / ("HUDLayoutV1-" + std::string(profile) + ".json");
}
}

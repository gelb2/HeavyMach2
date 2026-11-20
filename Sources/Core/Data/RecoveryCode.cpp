#include "Core/Data/RecoveryCode.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace hm2::recovery {
namespace {
constexpr std::string_view version = "HM2-1-";
constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
constexpr std::size_t bodySize = SaveData::byteCount - 8;
constexpr std::size_t maxPacked = 4 + bodySize + (bodySize + 127) / 128;
constexpr std::size_t maxCode = version.size() + (maxPacked * 4 + 2) / 3;

std::uint32_t crc32(std::span<const std::uint8_t> bytes) {
    std::uint32_t crc = 0xffffffffu;
    for (auto value : bytes) {
        crc ^= value;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
    }
    return crc ^ 0xffffffffu;
}

std::size_t repeatLength(std::span<const std::uint8_t> data, std::size_t at) {
    const auto limit = std::min(data.size(), at + 130);
    std::size_t end = at + 1;
    while (end < limit && data[end] == data[at]) ++end;
    return end - at;
}

Bytes pack(std::span<const std::uint8_t> data) {
    Bytes output;
    const auto checksum = crc32(data);
    for (unsigned shift = 0; shift < 32; shift += 8)
        output.push_back(std::uint8_t(checksum >> shift));
    std::size_t at = 0;
    while (at < data.size()) {
        const auto run = repeatLength(data, at);
        if (run >= 3) {
            output.push_back(std::uint8_t(0x80 | (run - 3)));
            output.push_back(data[at]);
            at += run;
            continue;
        }
        const auto start = at;
        do {
            ++at;
        } while (at < data.size() && at - start < 128 && repeatLength(data, at) < 3);
        output.push_back(std::uint8_t(at - start - 1));
        output.insert(output.end(), data.begin() + start, data.begin() + at);
    }
    return output;
}

std::string toBase64(std::span<const std::uint8_t> data) {
    std::string output(version);
    std::uint32_t recent = 0;
    unsigned bits = 0;
    for (auto value : data) {
        recent = (recent << 8) | value;
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            output += alphabet[(recent >> bits) & 63u];
        }
    }
    if (bits) output += alphabet[(recent << (6 - bits)) & 63u];
    return output;
}

Bytes fromBase64(std::string_view code) {
    Bytes output;
    std::uint32_t recent = 0;
    unsigned bits = 0;
    for (char character : code) {
        const auto value = alphabet.find(character);
        if (value == alphabet.npos) throw std::invalid_argument("Invalid recovery code character");
        recent = (recent << 6) | std::uint32_t(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            output.push_back(std::uint8_t(recent >> bits));
        }
    }
    if (bits >= 6 || (recent & ((1u << bits) - 1)))
        throw std::invalid_argument("Invalid recovery code ending");
    return output;
}
}

std::string encode(const SaveData& save) {
    return toBase64(pack(std::span(save.bytes()).subspan(4, bodySize)));
}

SaveData decode(std::string_view code) {
    if (code.size() > maxCode * 2) throw std::invalid_argument("Recovery code is too long");
    std::string trimmed;
    for (char character : code)
        if (!std::isspace(static_cast<unsigned char>(character))) trimmed += character;
    if (trimmed.size() > maxCode || !trimmed.starts_with(version))
        throw std::invalid_argument("Unsupported HeavyMach 2 recovery code");

    const auto input = fromBase64(std::string_view(trimmed).substr(version.size()));
    if (input.size() < 4) throw std::invalid_argument("Incomplete recovery code");
    std::uint32_t checksum = 0;
    for (unsigned i = 0; i < 4; ++i) checksum |= std::uint32_t(input[i]) << (8 * i);

    SaveData save;
    auto body = std::span(save.bytes()).subspan(4, bodySize);
    std::size_t at = 4, used = 0;
    while (at < input.size()) {
        const auto tag = input[at++];
        const bool repeat = (tag & 0x80) != 0;
        const std::size_t count = repeat ? (tag & 0x7f) + 3 : tag + 1;
        if (count > body.size() - used || (repeat ? 1 : count) > input.size() - at)
            throw std::invalid_argument("Invalid recovery code length");
        if (repeat) {
            std::fill_n(body.begin() + used, count, input[at++]);
        } else {
            std::copy_n(input.begin() + at, count, body.begin() + used);
            at += count;
        }
        used += count;
    }
    if (used != body.size() || crc32(body) != checksum)
        throw std::invalid_argument("Damaged recovery code");
    return save;
}
}

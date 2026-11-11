#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace hm2 {
// HD QChunk::loadBuffer, 0x3db10. Adapted from the MIT MachDefense reader;
// source provenance and changes are recorded in docs/source-reuse.md.
struct Chunk {
    std::uint32_t declaredSize=0, type=0;
    std::size_t sourceOffset=0, consumedSize=0;
    std::vector<std::uint8_t> data;
    std::vector<Chunk> children;
    static Chunk parse(std::span<const std::uint8_t>);
    // Preserve original headers and unknown payload bytes, including stale sizes.
    [[nodiscard]] std::vector<std::uint8_t> encode() const;
    [[nodiscard]] std::string text(std::size_t offset,std::size_t capacity) const;
    [[nodiscard]] std::uint8_t byte(std::size_t offset) const;
    [[nodiscard]] std::uint32_t uint32(std::size_t offset) const;
    [[nodiscard]] float real32(std::size_t offset) const;
};
}

#pragma once
#include "Core/Data/Resources.hpp"
#include "Core/Simulation/WorldRandom.hpp"
#include <array>
#include <span>
namespace hm2 {
// Original HD save envelope. Unknown header/slot bytes survive decode/encode.
// No filesystem/DB calls exist here; only explicit platform save events write.
class SaveData {
    std::array<std::uint8_t,28204> bytes_{};
public:
    static constexpr std::size_t byteCount=28204,slotOffset=0x408,slotSize=0x2360,slotCount=3;
    static SaveData decode(std::span<const std::uint8_t>,WorldRandom&,bool countOpen=true);
    Bytes encode(WorldRandom&,std::uint32_t absoluteTimeLow);
    std::span<std::uint8_t> slot(unsigned);
    std::span<const std::uint8_t> slot(unsigned)const;
    void initializeSlot(unsigned,std::string_view name,std::string_view country,std::uint32_t unixTime);
    [[nodiscard]] const auto& bytes()const{return bytes_;}
    [[nodiscard]] auto& bytes(){return bytes_;}
};
}

#pragma once
#include "Core/Data/DataTable.hpp"
#include "Core/Data/Models.hpp"
#include <array>

namespace hm2 {
struct EquipmentItem {std::string code,type,part;float parameter3=0;};
using EquipmentCodes=std::array<std::string_view,4>;
struct PlayerPartAngles {float base=0,footLeft=0,footRight=0,body=0,weaponLeft=0,weaponRight=0;};
struct PlayerPartOffsets {Vec2 footLeft,footRight,body,weaponLeft,weaponRight;};
// Current scope: mounting part equipment, not inventory/HP/save/progression.
// initSlot 0x17e10 / addMyMach 0x11770 / Player::setParts 0xb7b4.
class PlayerEquipment {
    std::map<std::string,EquipmentItem,std::less<>> items_;
public:
    static constexpr EquipmentCodes startingCodes{"11001","12001","10011","10111"};
    static PlayerEquipment parse(const DataTable&);
    // Slot order body/bottom/left/right; empty means unequipped. Numeric part types
    // reproduce the original mounting branches; IDs/images come from ItemInfo.
    [[nodiscard]] std::vector<PartMount> mounts(const Models&,EquipmentCodes codes=startingCodes,PlayerPartAngles angles={},PlayerPartOffsets offsets={})const;
    [[nodiscard]] const EquipmentItem& item(std::string_view code)const;
};
}

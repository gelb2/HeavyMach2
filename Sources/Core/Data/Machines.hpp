#pragma once
#include "Core/Data/DataTable.hpp"
#include <map>

namespace hm2 {
enum class MachineKind:std::uint8_t {bipod=0,vehicle=1,aircraft=2,turret=3,train=4};
struct MachineInfo {
    std::string name,itemRegeneration;
    MachineKind kind=MachineKind::bipod;
    std::uint8_t referenceType=0,attack=0;
    std::int32_t hp=0,experience=0,radius=0,coinRate=0,coin=0;
};
// GlobalInfo::openMachInfo0x1689c table consumer. Model chunks remain in Models;
// do not equate Player's table HP with the actual saved Player HP.
class Machines {
    DataTable table_;
    std::map<std::string,MachineInfo,std::less<>> records_;
public:
    static Machines parse(DataTable);
    [[nodiscard]] const MachineInfo& machine(std::string_view)const;
    [[nodiscard]] const auto& all()const{return records_;}
    [[nodiscard]] const DataTable& raw()const{return table_;}
};
}

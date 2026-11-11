#include "Core/Data/Machines.hpp"
#include <stdexcept>

namespace hm2 {
Machines Machines::parse(DataTable table){
    Machines out;
    for(std::size_t row=0;row<table.rowCount();++row){
        MachineInfo m;
        m.name=table.value(row,"MachName").value_or("");
        if(m.name.empty())throw std::runtime_error("Empty original machine ID");
        const auto type=table.value(row,"Type");
        if(type=="TANK")m.kind=MachineKind::vehicle;
        else if(type=="TURRET")m.kind=MachineKind::turret;
        else if(type=="PLANE")m.kind=MachineKind::aircraft;
        // Original PLAYER/unknown strings leave the allocated byte at0.
        const auto reference=table.value(row,"Refer");
        if(reference=="BOT_D")m.referenceType=1;
        else if(reference=="BOT_P")m.referenceType=2;
        else if(reference=="BOT_R")m.referenceType=3;
        m.attack=std::uint8_t(table.integer(row,"Atk"));m.hp=table.integer(row,"HP");m.experience=table.integer(row,"EXP");
        m.radius=table.integer(row,"Rad");m.coinRate=table.integer(row,"Coin%");m.coin=table.integer(row,"Coin");
        m.itemRegeneration=table.value(row,"ItemRegen").value_or("");
        out.records_.insert_or_assign(m.name,std::move(m));
    }
    out.table_=std::move(table);return out;
}
const MachineInfo& Machines::machine(std::string_view id)const{
    const auto i=records_.find(id);if(i==records_.end())throw std::runtime_error("Unresolved original machine: "+std::string(id));return i->second;
}
}

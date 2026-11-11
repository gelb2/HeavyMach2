#include "Core/Data/PlayerEquipment.hpp"
#include <stdexcept>

namespace hm2 {
PlayerEquipment PlayerEquipment::parse(const DataTable& table){
    PlayerEquipment out;
    for(std::size_t row=0;row<table.rowCount();++row){
        const auto code=table.value(row,"Code"),type=table.value(row,"Type"),part=table.value(row,"strParam");
        if(!code||code->empty()||!type||!part)throw std::runtime_error("Incomplete original equipment row");
        // openItemInfo uses the exact Code string, including leading zeros.
        out.items_.insert_or_assign(std::string(*code),EquipmentItem{std::string(*code),std::string(*type),std::string(*part),table.real(row,"Param3")});
    }
    return out;
}
const EquipmentItem& PlayerEquipment::item(std::string_view code)const{const auto i=items_.find(code);if(i==items_.end())throw std::runtime_error("Unresolved equipment item");return i->second;}
std::vector<PartMount> PlayerEquipment::mounts(const Models& models,EquipmentCodes codes,PlayerPartAngles angles,PlayerPartOffsets offsets)const{
    // Original addMyMach constructs a fresh Player, not the MCB Player graph.
    // Current verified bipod/body/weapon mounting. Other bottom variants must
    // be traced before use; no generic fake feet/body/image substitution.
    std::vector<PartMount> result{{"DummyParts",-1,0,3,0,angles.base,false,{}}};
    const auto add=[&](std::string_view name,int parent,unsigned socket,bool reverse,float angle,Vec2 offset){(void)models.part(name);result.push_back({std::string(name),parent,socket,3,0,angle,reverse,offset});};
    (void)models.part("DummyParts");
    if(!codes[1].empty()){const auto& foot=item(codes[1]);if(foot.type!="PtBtm")throw std::runtime_error("Equipment slot requires bottom part");if(models.part(foot.part).type!=2)throw std::runtime_error("Bottom mounting type is not yet restored");add(foot.part,0,1,false,angles.footLeft,offsets.footLeft);add(foot.part,0,2,false,angles.footRight,offsets.footRight);}
    int body=-1;
    if(!codes[0].empty()){const auto& value=item(codes[0]);if(value.type!="PtBody")throw std::runtime_error("Equipment slot requires body part");body=int(result.size());add(value.part,0,3,false,angles.body,offsets.body);}
    for(unsigned slot=2;slot<4;++slot)if(!codes[slot].empty()){
        if(body<0)throw std::runtime_error("Weapon requires equipped body");
        const auto& value=item(codes[slot]);if(value.type!="PtWpn")throw std::runtime_error("Equipment slot requires weapon part");add(value.part,body,slot-1,slot==3,slot==2?angles.weaponLeft:angles.weaponRight,slot==2?offsets.weaponLeft:offsets.weaponRight);
    }
    return result;
}
}

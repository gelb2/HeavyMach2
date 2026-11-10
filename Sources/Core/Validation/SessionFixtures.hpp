#pragma once
#include "Core/Data/SaveData.hpp"
#include "Core/Progression/Progression.hpp"
#include "Core/UI/Screens.hpp"
#include <algorithm>

namespace hm2::validation {
// Original-format starting data for a real damage/OS-alert/return-to-base
// validation. It never mutates a live actor or injects a death result.
inline SaveData gameOverFixture(std::uint32_t timestamp){
    SaveData data;data.initializeSlot(0,"GAMEOVER","kr",timestamp);
    progression::SlotView slot(data.slot(0));slot.hp(1);slot.u32(0x1b5c,5);
    slot.string(0x2d,16,"Stage01_01");slot.string(0x55,16,"Base002");
    slot.u32(0x64c,50);slot.u32(0x650,31);slot.u32(0x654,17);
    return data;
}
inline progression::Definitions missionFixtureDefinitions(ResourceAccess& resources){
    const auto table=[&](std::string_view name){return DataTable::parse(resources.read(name));};
    return progression::Definitions::parse(table("ItemInfo.tble"),table("ItemRegen.tble"),table("MissionList.tble"),table,"kr");
}
// Only original-format starting progress is prepared. The live Command must
// award, finish, advance its windows and save through normal consumers.
inline SaveData completedMissionFixture(ResourceAccess& resources,std::uint32_t timestamp){
    SaveData data;data.initializeSlot(0,"MISSIONS","kr",timestamp);progression::SlotView slot(data.slot(0));
    const auto defs=missionFixtureDefinitions(resources);progression::Events events;progression::Progression prep(slot,defs,events);prep.restore();
    const auto require=[](bool ok){if(!ok)throw std::logic_error("Invalid original completed-mission starting fixture");};
    require(prep.missions.accept("M004010"));require(prep.missions.accept("M001080"));require(prep.inventory.add(*defs.findItem("05103"),1));
    require(prep.acceptMission("M002010","Base001"));require(prep.acceptMission("M001010","Base001"));
    require(prep.inventory.add(*defs.findItem("05001"),5));require(prep.inventory.add(*defs.findItem("05025"),5));
    for(unsigned i=0;i<5;++i)prep.killed("E_Tank_01",0,"Stage01_01");
    slot.u16(0x658,4);unsigned i=0;for(auto id:{"M004010","M001080","M002010","M001010"})slot.string(0x65c+21*i++,8,id);
    prep.prepareSave();return data;
}
// Future resource preparation only; never reported as an executed result.
inline std::vector<ui::CompletedMissionView> completedMissionPreparation(ResourceAccess& resources){
    const auto defs=missionFixtureDefinitions(resources);std::vector<ui::CompletedMissionView> result;
    for(auto id:{"M002010","M001010"}){const auto mission=std::find_if(defs.missions().begin(),defs.missions().end(),[&](const auto& m){return m.id==id;});
        if(mission==defs.missions().end())throw std::logic_error("Missing original completed-mission preparation");
        ui::CompletedMissionView view{mission->id,mission->name,{}};
        for(const auto& a:mission->completeActions)if(a.type>=0&&a.type<=2)view.rewards.push_back({a.type,a.value,a.type==0?"Icon00001.png":a.type==1?"Icon00010.png":"Icon"+a.item+".png"});
        result.push_back(std::move(view));
    }return result;
}
}

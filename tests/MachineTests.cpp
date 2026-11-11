#include "Core/Data/Machines.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
using namespace hm2;
int main(int argc,char** argv){
    const auto input=DataTable::parseText("MachName\tType\tHP\tEXP\tAtk\tRefer\tRad\tCoin%\tCoin\tItemRegen\tBaseParts\nFixture\tTANK\t200\t20\t257\tBOT_D\t30\t100\t3\tTank_L01\tUntouched\nOther\tUnknown\t-1\t0\t0\tBOT_A\t0\t0\t0\tNULL\t-\n");
    auto records=Machines::parse(input);assert(records.all().size()==2&&records.raw().decoded()==input.decoded());
    const auto& r=records.machine("Fixture");assert(r.kind==MachineKind::vehicle&&r.attack==1&&r.referenceType==1&&r.hp==200&&r.coinRate==100&&r.coin==3&&r.radius==30&&r.experience==20&&r.itemRegeneration=="Tank_L01");
    assert(records.machine("Other").kind==MachineKind::bipod&&records.machine("Other").hp==-1);
    bool rejected=false;try{(void)records.machine("Missing");}catch(...){rejected=true;}assert(rejected);
    if(argc>1){
        std::ifstream file(argv[1],std::ios::binary);assert(file);std::vector<std::uint8_t> b((std::istreambuf_iterator<char>(file)),{});
        const auto all=Machines::parse(DataTable::parse(b));assert(all.all().size()==78);
        const auto& player=all.machine("Player");assert(player.hp==512&&player.kind==MachineKind::bipod); // table, not save initialization
        const auto& tank=all.machine("E_Tank_01");assert(tank.hp==200&&tank.radius==30&&tank.experience==20&&tank.coin==3&&tank.attack==0);
    }
}

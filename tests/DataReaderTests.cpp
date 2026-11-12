#include "Core/Data/Chunk.hpp"
#include "Core/Data/DataTable.hpp"
#include "Core/Data/DarwinRandom.hpp"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>

using namespace hm2;
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}assert(rejected);}
void chunkTests(){
    Chunk child;child.type=1;child.declaredSize=3;child.data={0x41,0,0xed,0x3f};
    Chunk root;root.type=99;root.declaredSize=17;root.data={1,2,3};root.children={child};
    const auto bytes=root.encode();auto decoded=Chunk::parse(bytes);
    assert(decoded.encode()==bytes&&decoded.declaredSize==17&&decoded.type==99);
    assert(decoded.consumedSize==bytes.size()&&decoded.children[0].declaredSize==3);
    assert(decoded.children[0].data==child.data&&decoded.children[0].sourceOffset==19);
    for(std::size_t n=0;n<bytes.size();++n)rejects([&]{Chunk::parse(std::span(bytes).first(n));});
    auto extra=bytes;extra.push_back(0);rejects([&]{Chunk::parse(extra);});
    auto bad=bytes;bad[8]=bad[9]=bad[10]=bad[11]=255;rejects([&]{Chunk::parse(bad);});
    bad=bytes;for(unsigned i=15;i<19;++i)bad[i]=255;rejects([&]{Chunk::parse(bad);});
    rejects([&]{(void)decoded.text(2,2);});rejects([&]{(void)decoded.uint32(1);});
    Chunk deep;for(int i=0;i<130;++i){Chunk outer;outer.children.push_back(std::move(deep));deep=std::move(outer);}rejects([&]{(void)deep.encode();});
}
void tableTests(){
    const auto table=DataTable::parseText("\r\nKey\tValue\tOther\tLast\t\r\n// comment\r\nA\t v  \t\tZ\textra\r\nB\tB2\nC\nA\t3\t4\t5\n // kept\tX\tY\tZ\n");
    assert(table.columns().size()==4&&table.rowCount()==5);
    assert(table.value(0,"Value")==" v"&&table.value(0,"Other")==""&&table.value(0,"Last")=="Z");
    assert(table.value(1,"Other")==""&&!table.value(1,"Last"));
    assert(table.value(2,"Value")==""&&!table.value(2,"Other"));
    assert(table.findRow("A")==0&&!table.findRow("unknown"));
    assert(table.value(4,"Key")==" // kept"&&!table.value(100,"Value")&&!table.value(0,"missing"));
    const auto commentHeader=DataTable::parseText("// actual header\nx\n");
    assert(commentHeader.columns().at(0)=="// actual header"&&commentHeader.rowCount()==1);
    std::string nul="Key\tValue\nA\t1";nul.push_back(0);nul+="\nB\t2\n";
    const auto nulTable=DataTable::parseText(nul);assert(nulTable.decoded()==nul&&nulTable.rowCount()==1&&!nulTable.findRow("B"));
    const auto duplicate=DataTable::parseText("A\tB\tA\tC\nx\nx\ty\nx\ty\tz\tq\n");
    assert(duplicate.value(0,"A")=="x"&&duplicate.value(1,"A")==""&&duplicate.value(2,"A")=="z");
    const std::string plain="Key\tN\nA\t512\n";DarwinRandom random(0xb27f7);
    std::vector<std::uint8_t> encoded{'E','n','c','T','b','l'};
    for(unsigned char c:plain)encoded.push_back(c^(random.next()%227));
    const auto decoded=DataTable::parse(encoded);assert(decoded.decoded()==plain&&decoded.encoded()==encoded&&decoded.value(0,"N")=="512");
    rejects([]{DataTable::parseText("");});
}
void randomTests(){
#if defined(__APPLE__)
    for(std::uint32_t seed:{0u,1u,0xb27f7u,0x7fffffffu,0xffffffffu}){
        ::srandom(seed);DarwinRandom local(seed);
        for(int i=0;i<5000;++i)assert(local.next()==std::uint32_t(::random()));
    }
#else
    DarwinRandom r(1);assert(r.next()==1804289383u);assert(r.next()==846930886u);
#endif
}
int main(int argc,char** argv){assert(argc==2);const std::string_view which=argv[1];
    if(which=="chunk")chunkTests();else if(which=="table")tableTests();else if(which=="random")randomTests();else throw std::runtime_error("Unknown test");
    std::cout<<which<<" passed\n";
}

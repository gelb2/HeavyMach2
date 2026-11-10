#include "Core/Presentation/PreparedTiles.hpp"
#include <cassert>
using namespace hm2;
struct Files:ResourceAccess {
    unsigned reads=0,checks=0;bool fail=false;
    bool exists(std::string_view n)const override{++const_cast<Files*>(this)->checks;return n!="Missing.png";}
    Bytes read(std::string_view)override{
        ++reads;if(fail)throw std::runtime_error("read fixture failure");
        return {137,80,78,71,13,10,26,10,0,0,0,13,'I','H','D','R',0,0,0,30,0,0,0,17,8,6,0,0,0,0,0,0,0};
    }
};
int main(){
    Files files;PreparedTiles cache;auto* first=cache.prepare(files,"Fixture.png");assert(first&&files.reads==1&&first->pixels.width==30);
    first->columns=3;assert(cache.prepare(files,"Fixture.png")==first&&first->columns==3&&files.reads==1);
    assert(!cache.prepare(files,"Missing.png")&&!cache.lookup("Missing.png"));
    const auto reads=files.reads,checks=files.checks;
    for(int i=0;i<100;++i){assert(cache.lookup("Fixture.png")==first);assert(cache.visual(*first).storage.width==32);assert(!cache.lookup("Missing.png"));}
    assert(files.reads==reads&&files.checks==checks);
    bool rejected=false;try{cache.lookup("Unprepared.png");}catch(...){rejected=true;}assert(rejected);
    rejected=false;try{cache.prepare(files,"../outside.png");}catch(...){rejected=true;}assert(rejected);
    files.fail=true;rejected=false;try{cache.prepare(files,"Retry.png");}catch(...){rejected=true;}assert(rejected);
    files.fail=false;assert(cache.prepare(files,"Retry.png"));
    cache.prepareWorldEffects(files);const auto* explosion=cache.lookup("Efx_Explode02.png");assert(explosion&&explosion->columns==6&&explosion->rows==4&&explosion->origin.y==-44);
    const auto tile=cache.visual(*explosion);assert(tile.origin.y==-44&&tile.identity=="shared:Efx_Explode02.png");
    Frame prep;cache.appendPreparation(prep);assert(prep.sprites.size()==10); // 2 fixtures plus eight original effects, missing stays nil
}

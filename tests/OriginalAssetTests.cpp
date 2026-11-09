#include "Core/Data/DataTable.hpp"
#include "Assets/Image.hpp"
#include "Core/Presentation/AssemblyPreview.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <numbers>
#include <stdexcept>

using namespace hm2;
struct Files:ResourceAccess {
    std::filesystem::path root;std::map<std::string,unsigned> reads;
    Bytes read(std::string_view name)override {
        if(std::filesystem::path(name).filename()!=name)throw std::runtime_error("Nonlocal resource name");
        ++reads[std::string(name)];std::ifstream f(root/name,std::ios::binary);
        if(!f)throw std::runtime_error("Missing resource: "+std::string(name));
        return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
    }
};
void same(float actual,float expected){assert(std::abs(actual-expected)<.0001f);}
int main(int argc,char** argv){assert(argc==3);Files files;files.root=argv[1];const std::string_view test=argv[2];
    if(test=="roundtrip"){
        for(const auto* name:{"PartsInfo.mcb","MachInfo.mcb","MapInfo.mcb"}){
            const auto b=files.read(name);const auto chunk=Chunk::parse(b);assert(chunk.encode()==b&&chunk.consumedSize==b.size());
            std::cout<<name<<" byte-preserving round trip; declared="<<chunk.declaredSize<<" actual="<<b.size()<<'\n';
        }
    }else if(test=="tables"){
        unsigned tables=0,rows=0;
        for(const auto& entry:std::filesystem::directory_iterator(files.root)){
            if(entry.path().extension()!=".tble")continue;
            const auto b=files.read(entry.path().filename().string());const auto t=DataTable::parse(b);
            assert(t.encoded()==b&&t.rowCount()>0);++tables;rows+=t.rowCount();
        }
        assert(tables==135);
        const auto parts=DataTable::parse(files.read("PartsList.tble"));assert(parts.rowCount()==338&&parts.columns().size()==3);
        const auto weapons=DataTable::parse(files.read("WeaponInfo.tble"));assert(weapons.rowCount()==213);
        const auto mach=DataTable::parse(files.read("MachList.tble"));assert(mach.rowCount()==78&&mach.value(*mach.findRow("E_Tank_01"),"HP")=="200");
        std::cout<<tables<<" original tables; "<<rows<<" rows preserved\n";
    }else if(test=="images"){
        unsigned images=0,pngs=0;
        for(const auto& entry:std::filesystem::directory_iterator(files.root)){
            const auto ext=entry.path().extension();if(ext!=".png"&&ext!=".jpg")continue;
            const auto encoded=files.read(entry.path().filename().string());const auto image=decodeImage(encoded);
            assert(image.width>0&&image.height>0&&image.rgba.size()==std::size_t(image.width)*image.height*4);
            if(ext==".png"){const auto size=AssemblyPreview::pngSize(encoded);same(float(image.width),size.width);same(float(image.height),size.height);++pngs;}
            ++images;
        }
        assert(images==1276&&pngs==1238);
        for(const Bytes invalid:{Bytes{},Bytes{137,80,78,71,13,10,26,10}}){bool rejected=false;try{(void)decodeImage(invalid);}catch(const std::exception&){rejected=true;}assert(rejected);}
        std::cout<<images<<" original PNG/JPEG decoded with common stb; "<<pngs<<" original PNG dimensions matched\n";
    }else if(test=="assembly"){
        const auto models=Models::parse(Chunk::parse(files.read("PartsInfo.mcb")),Chunk::parse(files.read("MachInfo.mcb")));
        assert(models.parts().size()==338&&models.models().size()==78);
        const auto tank=models.staticPose("E_Tank_01");assert(tank.size()==2);
        assert(tank[0].part->name=="E_Tank_Body_01"&&tank[1].part->name=="E_Tank_Wpn_01");
        same(tank[0].center.x,0);same(tank[0].center.y,0);same(tank[1].center.x,13.5);same(tank[1].center.y,-1.5);
        assert(tank[0].layer==0&&tank[1].layer==1);
        const auto player=models.staticPose("Player");assert(player.size()==4);
        same(player[1].center.x,7);same(player[1].center.y,9.5);same(player[2].center.x,7.5);same(player[2].center.y,-10.5);same(player[3].center.x,1);same(player[3].center.y,1.5);
        unsigned complete=0,incomplete=0;
        for(const auto& [name,m]:models.models()){
            if(m.completeOriginalSlice){const auto pose=models.staticPose(name,.4f,-.2f);assert(!pose.empty());++complete;}
            else{bool rejected=false;try{(void)models.staticPose(name);}catch(const std::exception&){rejected=true;}assert(rejected);++incomplete;}
        }
        assert(complete==77&&incomplete==1);
        // Multiple model/direction conditions coexist in one actual frame.
        AssemblyCase combined[]={{"E_Tank_01",{100,100},0,0},{"E_Tank_01",{200,100},std::numbers::pi_v<float>/2,0},{"E_Tank_01",{100,200},0,std::numbers::pi_v<float>/2},{"E_Tank_01",{200,200},.7f,-.4f}};
        const AssemblyPreview scene(files,combined,{320,320});assert(scene.frame().sprites.size()==8);
        same(scene.frame().sprites[3].center.x,205.5);same(scene.frame().sprites[3].center.y,108.5);
        same(scene.frame().sprites[5].center.x,109.5);same(scene.frame().sprites[5].center.y,203.5);
        const auto before=files.reads;
        for(int i=0;i<1200;++i){combined[1].machineAngle=float(i)*.01f;combined[2].weaponAngle=float(i)*-.02f;const auto f=scene.compose(combined,{320,320});assert(f.sprites.size()==8);for(const auto& s:f.sprites)assert(s.size.width>0&&s.size.height>0&&std::isfinite(s.center.x)&&std::isfinite(s.center.y));}
        assert(files.reads==before);
        std::cout<<"338 parts, 77 complete models, 1 incomplete original slice rejected; simultaneous assembly cases, 1200 frames without reads\n";
    }else throw std::runtime_error("Unknown original-resource test");
}

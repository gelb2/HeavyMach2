#include "Core/Data/DataTable.hpp"
#include "Assets/Image.hpp"
#include "Core/Presentation/AssemblyPreview.hpp"
#include "Core/Presentation/MapPreview.hpp"
#include "Core/Presentation/PlayerMapPreview.hpp"
#include "Core/Simulation/LocomotionScene.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include "Core/Data/Weapons.hpp"
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
    bool exists(std::string_view name)const override {if(std::filesystem::path(name).filename()!=name)throw std::runtime_error("Nonlocal resource name");return std::filesystem::is_regular_file(root/name);}
    Bytes read(std::string_view name)override {
        if(std::filesystem::path(name).filename()!=name)throw std::runtime_error("Nonlocal resource name");
        ++reads[std::string(name)];std::ifstream f(root/name,std::ios::binary);
        if(!f)throw std::runtime_error("Missing resource: "+std::string(name));
        return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
    }
};
void same(float actual,float expected){assert(std::abs(actual-expected)<.0001f);}
Frame originalBodies(const Frame& source,std::size_t expectedShadows){
    Frame result;result.width=source.width;result.height=source.height;std::size_t shadows=0;
    for(const auto& s:source.sprites){
        if(s.textureSource.variant==TextureVariant::rgba){result.sprites.push_back(s);continue;}
        assert(s.textureSource.variant==TextureVariant::originalHalfAlpha&&s.visualLayer==0);
        assert(s.image==s.textureSource.image+"~shd"&&s.atlasBatch&&s.atlasBatch->pixelFormat==3);
        assert(s.atlasBatch->textureSource==s.textureSource);
        shadows+=s.atlasBatch->quads.size();
    }
    assert(shadows==expectedShadows);return result;
}
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
    }else if(test=="weapons"){
        const auto encoded=files.read("WeaponInfo.tble");const auto weapons=Weapons::parse(DataTable::parse(encoded));assert(weapons.all().size()==213&&weapons.raw().encoded()==encoded);
        const auto& mg=weapons.weapon("P_Wpn_Gun_101");assert(mg.kind==WeaponKind::gun&&mg.damage==7&&mg.shotRange==400&&mg.shotCount==10&&mg.attackPoint==17&&mg.bulletImage.empty());same(mg.shotDelay,.16f);same(mg.reloadDelay,2.5f);
        const auto& ml=weapons.weapon("P_Wpn_Msl_101");assert(ml.kind==WeaponKind::missile&&ml.damage==26&&ml.damageRange==60&&ml.shotRange==600&&ml.shotCount==2&&ml.attackPoint==17);same(ml.speed,.2f);same(ml.shotDelay,.5f);same(ml.reloadDelay,2.f);
        unsigned missing=0;
        for(const auto& [name,w]:weapons.all()){
            assert(w.recognizedKind);for(const auto& image:{w.bulletImage,w.effectImage,w.explodeImage})if(!image.empty()&&!files.exists(image)){assert((name=="E_Turret17_Wpn_01"&&image=="Blt_Missile06.png")||(name=="Boss08_Wpn_01"&&image=="Blt_Missile07.png"));++missing;}
            assert(files.exists(w.fireSound));
        }assert(missing==2); // Confirmed absent in original IPA, no substitution.
        const auto models=Models::parse(Chunk::parse(files.read("PartsInfo.mcb")),Chunk::parse(files.read("MachInfo.mcb")));
        const auto equip=PlayerEquipment::parse(DataTable::parse(files.read("ItemInfo.tble")));const auto pose=models.mountedPose(equip.mounts(models));
        const auto gun=Models::muzzlePosition(pose[4],1),missile=Models::muzzlePosition(pose[5],1);same(gun.x,38);same(gun.y,24.5);same(missile.x,27);same(missile.y,-26.5);
        std::cout<<"213 original weapons, doubled runtime ranges, narrow types/effects/sounds; two original absent images retained as unresolved; original muzzle formula verified\n";
    }else if(test=="locomotion"){
        const auto maps=Maps::parse(Chunk::parse(files.read("MapInfo.mcb")));unsigned edges=0;
        for(const auto& [id,map]:maps.all()){const TerrainCollision terrain(map);edges+=terrain.edges().size();}
        assert(edges==1354);
        const TerrainCollision firstTerrain(maps.map("Stage01_01"));const auto wall=firstTerrain.resolve({-45,130});
        same(wall.position.x,-52.8894424438f);same(wall.position.y,130);assert(wall.current==62&&wall.corrections==1);
        const LocomotionActor definitions[]={{PlayerEquipment::startingCodes,{},0,std::nullopt},{{"11002","12002","10012","10112"},{80,0},0,Vec2{80,200}}};
        LocomotionScene phone(files,"Stage01_01",{480,320},definitions),ipad(files,"Stage01_01",{768,1024},definitions);
        same(phone.camera().center.x,128);same(ipad.camera().center.x,128);
        assert(phone.input(PointerPhase::begin,phone.camera().toScreen({100,0}),1));assert(ipad.input(PointerPhase::begin,ipad.camera().toScreen({100,0}),1));
        phone.input(PointerPhase::end,phone.camera().toScreen({100,0}),1);ipad.input(PointerPhase::end,ipad.camera().toScreen({100,0}),1);
        const auto before=files.reads;unsigned collisions=0;
        for(int i=0;i<600;++i){phone.tick();ipad.tick();const auto& p=phone.actors().front().state();const auto& q=ipad.actors().front().state();same(p.position.x,q.position.x);same(p.position.y,q.position.y);same(p.angle,q.angle);assert(originalBodies(phone.frame(),10).sprites.size()==11&&originalBodies(ipad.frame(),10).sprites.size()==11);collisions+=phone.contacts().front().corrections;}
        assert(phone.actors().front().state().position.x>5&&phone.actors()[1].state().weaponReady);
        const auto state=phone.actors().front().state();const auto time=phone.time();const auto tick=phone.ticks();phone.setPaused(true);
        for(int i=0;i<120;++i){phone.tick();assert(!phone.input(PointerPhase::begin,{300,160},1));}
        same(phone.actors().front().state().position.x,state.position.x);assert(phone.time()==time&&phone.ticks()==tick);phone.setPaused(false);phone.tick();assert(phone.ticks()==tick+1);
        assert(files.reads==before);
        // Dynamic optional background uses the original .94 camera parallax.
        LocomotionScene water(files,"Stage02_04",{768,1024},std::span(definitions).first(1));water.tick();const auto waterFrame=water.frame();const auto bg=water.camera().toScreen(water.camera().backgroundWorld);same(waterFrame.sprites[0].center.x,bg.x);same(waterFrame.sprites[0].center.y,bg.y);
        std::cout<<"1354 original edges; identical world movement on two display configurations, 600 mixed walk/aim ticks, pause, warm reads0; corrections="<<collisions<<'\n';
    }else if(test=="player"){
        const auto models=Models::parse(Chunk::parse(files.read("PartsInfo.mcb")),Chunk::parse(files.read("MachInfo.mcb")));
        const auto equipment=PlayerEquipment::parse(DataTable::parse(files.read("ItemInfo.tble")));
        auto mounts=equipment.mounts(models);assert(mounts.size()==6);
        const auto initial=models.mountedPose(mounts);assert(initial.size()==6);
        const Vec2 expected[]={{0,0},{7,9.5},{7.5,-10.5},{1,1.5},{12,21},{7,-22.5}};
        for(unsigned i=0;i<6;++i){same(initial[i].center.x,expected[i].x);same(initial[i].center.y,expected[i].y);assert(initial[i].layer==3&&initial[i].zOrder==0&&initial[i].reverseY==(i==5));}
        assert(initial[4].part->name=="P_Wpn_Gun_101"&&initial[5].part->name=="P_Wpn_Msl_101");
        const float quarter=std::numbers::pi_v<float>/2;
        const auto turned=models.mountedPose(equipment.mounts(models,PlayerEquipment::startingCodes,{quarter,quarter,quarter,quarter,0,0}),quarter);
        same(turned[3].center.x,-1.5);same(turned[3].center.y,1);same(turned[4].center.x,-9);same(turned[4].center.y,6);same(turned[5].center.x,20.5);same(turned[5].center.y,-3);
        const auto rotated=models.mountedPose(equipment.mounts(models,PlayerEquipment::startingCodes,{quarter,quarter,quarter,quarter,quarter,quarter}),quarter);
        same(rotated[4].center.x,-21);same(rotated[4].center.y,12);same(rotated[5].center.x,22.5);same(rotated[5].center.y,7);
        auto rejects=[](auto f){bool fail=false;try{f();}catch(const std::exception&){fail=true;}assert(fail);};
        rejects([&]{(void)equipment.mounts(models,{"11001","12001","00001","10111"});});
        rejects([&]{(void)equipment.mounts(models,{"","12001","10011","10111"});});
        rejects([&]{(void)equipment.item("011001");}); // String IDs are not normalized to integers.
        mounts[1].parent=1;rejects([&]{(void)models.mountedPose(mounts);});
        struct Missing final:ResourceAccess {bool exists(std::string_view)const override{return false;}Bytes read(std::string_view)override{throw std::runtime_error("Missing resource");}} missing;
        PartSprites connector;connector.prepare(missing,std::span(initial).first(1));Frame invisible;connector.append(invisible,std::span(initial).first(1),{});assert(invisible.sprites.empty());
        rejects([&]{PartSprites required;required.prepare(missing,std::span(initial).subspan(1,1));});
        PlayerAssemblyCase cases[]={{PlayerEquipment::startingCodes,{-80,0},0,{}},{{"11002","12002","10012","10112"},{80,0},quarter,{quarter,quarter,quarter,quarter,0,0}}};
        const PlayerMapPreview preview(files,"Stage01_01",cases);const MapCamera camera{{},{},{480,320}};
        const auto frame=originalBodies(preview.compose(camera,cases),10);assert(frame.sprites.size()==11&&frame.sprites[5].image=="P_Wpn_Msl_101.png"&&frame.sprites[5].reverseY);
        const auto before=files.reads;
        for(int i=0;i<1200;++i){cases[1].angles.weaponLeft=float(i)*.01f;cases[1].angles.weaponRight=-float(i)*.02f;const auto f=originalBodies(preview.compose({{float(i)/10,float(i)/20},{},{480,320}},cases),10);assert(f.sprites.size()==11);}
        assert(files.reads==before);
        unsigned count=0;const auto table=DataTable::parse(files.read("ItemInfo.tble"));
        for(std::size_t row=0;row<table.rowCount();++row){const auto type=table.value(row,"Type");if(type!="PtBody"&&type!="PtBtm"&&type!="PtWpn")continue;const auto code=*table.value(row,"Code");auto codes=PlayerEquipment::startingCodes;codes[type=="PtBody"?0:type=="PtBtm"?1:2]=code;assert(equipment.mounts(models,codes).size()==6);++count;}
        std::cout<<count<<" original part items resolve; default equipped Player 6 mounts/5 visible sprites, mirror/absolute angles, 2 equipment conditions together, 1200 warm frames without reads\n";
    }else if(test=="maps"){
        const auto raw=files.read("MapInfo.mcb");const auto maps=Maps::parse(Chunk::parse(raw));
        assert(maps.all().size()==43&&maps.raw().encode()==raw);
        unsigned gates=0,polygons=0,spawns=0;
        for(const auto& [id,m]:maps.all()){gates+=m.gates.size();polygons+=m.collisionPolygons.size();spawns+=m.spawners.size();for(const auto& spawn:m.spawners)assert(!spawn.model.empty());}
        assert(gates==90&&polygons==152&&spawns==695);
        const auto& first=maps.map("Stage01_01");assert(first.gates.size()==3&&first.spawners.size()==16&&first.collisionPolygons.size()==7);
        same(first.gates[0].position.x,-275);same(first.gates[0].position.y,-462);assert(first.gates[0].linkMap=="Base001");
        same(first.spawners[0].position.x,-284);same(first.spawners[0].position.y,-79);
        assert(maps.map("Stage03_07").spawners.size()==16); // Original stale size does not discard its final spawner.
        same(maps.map("Stage03_07").spawners.back().position.x,187);same(maps.map("Stage03_07").spawners.back().position.y,-95);
        MapPreview scene(files,"Stage01_01");const MapCamera camera{{-275,-462},{},{480,320}};
        const auto frame=scene.compose(camera);assert(frame.sprites.size()==1&&frame.sprites[0].image=="Stage01_01.jpg");same(frame.sprites[0].size.width,1024);same(frame.sprites[0].center.x,515);same(frame.sprites[0].center.y,622);
        MapPreview water(files,"Stage02_04");const auto waterFrame=water.compose({{.25f,-.75f},{},{768,1024}});assert(waterFrame.sprites.size()==2&&waterFrame.sprites[0].image=="Stage02_04BG.jpg"&&waterFrame.sprites[1].image=="Stage02_04.png");same(waterFrame.sprites[0].size.width,768);same(waterFrame.sprites[0].size.height,1024);same(waterFrame.sprites[0].center.x,383.75);same(waterFrame.sprites[1].center.x,384);same(waterFrame.sprites[1].center.y,513);
        MapCamera skyway;
        skyway.fillSurface=true;
        skyway.viewport={912.f/.9957588911056519f,420.f/.9957588911056519f};
        skyway.backgroundWorld={-3.2f,10.f};
        const auto opaque=scene.compose(skyway);
        assert(opaque.sprites.size()==1);
        const auto& terrain=opaque.sprites[0];
        assert(terrain.strip.size()==4);
        const float left=terrain.center.x-terrain.size.width*.5f;
        const float top=terrain.center.y+terrain.size.height*.5f;
        for(const auto& vertex:terrain.strip){
            assert(vertex.position.x>=0&&vertex.position.x<=skyway.viewport.width);
            assert(vertex.position.y>=0&&vertex.position.y<=skyway.viewport.height);
            same(vertex.uv.x,(vertex.position.x-left)/terrain.size.width);
            same(vertex.uv.y,(top-vertex.position.y)/terrain.size.height);
        }
        const auto tablet=water.compose(skyway);
        assert(tablet.sprites[0].size.width<skyway.viewport.width);
        assert(tablet.sprites[1].strip.size()==4); // Transparent Skyway keeps its existing path.
        same(tablet.sprites[0].center.x,skyway.toScreen(skyway.backgroundWorld).x);
        skyway.clampBackgroundXToViewport=true;
        const auto phone=water.compose(skyway);
        same(phone.sprites[0].center.x,skyway.viewport.width*.5f);
        assert(phone.sprites[0].size.width==tablet.sprites[0].size.width&&phone.sprites[0].size.height==tablet.sprites[0].size.height);
        assert(phone.sprites[0].center.y==tablet.sprites[0].center.y);
        assert(phone.sprites[1].center.x==tablet.sprites[1].center.x&&phone.sprites[1].center.y==tablet.sprites[1].center.y);
        const auto before=files.reads;for(int i=0;i<1200;++i){const auto a=scene.compose({{float(i)/4,float(-i)/8},{},{480,320}});const auto b=water.compose({{float(i)/4,float(-i)/8},{},{768,1024}});assert(a.sprites.size()==1&&b.sprites.size()==2);}assert(files.reads==before);
        std::cout<<"43 maps, 90 gates, 152 polygons, 695 spawners; JPEG/PNG fallback and optional 2x background, 2400 warm frames without reads\n";
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
        const AssemblyPreview scene(files,combined,{320,320});const auto bodies=originalBodies(scene.frame(),8);assert(bodies.sprites.size()==8);
        same(bodies.sprites[3].center.x,205.5);same(bodies.sprites[3].center.y,108.5);
        same(bodies.sprites[5].center.x,109.5);same(bodies.sprites[5].center.y,203.5);
        const auto before=files.reads;
        for(int i=0;i<1200;++i){combined[1].machineAngle=float(i)*.01f;combined[2].weaponAngle=float(i)*-.02f;const auto f=originalBodies(scene.compose(combined,{320,320}),8);assert(f.sprites.size()==8);for(const auto& s:f.sprites)assert(s.size.width>0&&s.size.height>0&&std::isfinite(s.center.x)&&std::isfinite(s.center.y));}
        assert(files.reads==before);
        std::cout<<"338 parts, 77 complete models, 1 incomplete original slice rejected; simultaneous assembly cases, 1200 frames without reads\n";
    }else throw std::runtime_error("Unknown original-resource test");
}

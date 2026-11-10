#include "PreparedTextStub.hpp"
#include "Assets/Image.hpp"
#include "Core/Presentation/PartSprites.hpp"
#include "Core/Simulation/CombatScene.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numbers>
using namespace hm2;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;unsigned reads=0;mutable unsigned probes=0;
    explicit Files(const char* p):root(p){}
    Bytes read(std::string_view n)override{++reads;std::ifstream f(root/std::string(n),std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
    bool exists(std::string_view n)const override{++probes;return std::filesystem::exists(root/std::string(n));}
};
void near(float a,float b){assert(std::abs(a-b)<.0002f);}
Vec2 center(const AtlasQuad& q){Vec2 c{};for(const auto& v:q.vertices){c.x+=v.position.x*.25f;c.y+=v.position.y*.25f;}return c;}
void exactBytes(){
    DecodedImage source{256,3,4,{}};source.rgba.resize(256*3*4);
    for(unsigned y=0;y<3;++y)for(unsigned x=0;x<256;++x){auto* p=source.rgba.data()+(y*256+x)*4;p[0]=13;p[1]=127;p[2]=251;p[3]=x;}
    const auto unchanged=source.rgba;const auto alpha=prepareOriginalPartAlpha(source);
    assert(alpha.width==256&&alpha.height==3&&alpha.storageWidth==256&&alpha.storageHeight==4);
    for(unsigned y=0;y<4;++y)for(unsigned x=0;x<256;++x)assert(alpha.alpha[y*256+x]==(y<3?x>>1:0));
    assert(source.rgba==unchanged);
    for(auto bad:{DecodedImage{},DecodedImage{1025,1,4,Bytes(1025*4)},DecodedImage{2,2,4,Bytes(15)}}){bool rejected=false;try{(void)prepareOriginalPartAlpha(bad);}catch(...){rejected=true;}assert(rejected);}
}
#ifndef HM2_AIRCRAFT_SCENE_ONLY
void catalogueAndGeometry(Files& files){
    const auto models=Models::parse(Chunk::parse(files.read("PartsInfo.mcb")),Chunk::parse(files.read("MachInfo.mcb")));
    PreparedTiles tiles("shadow-test");unsigned present=0,nil=0;std::uint64_t texels=0;std::vector<PartPose> poses;
    for(const auto& [name,part]:models.parts()){
        const auto source=name+".png";auto* body=tiles.prepare(files,source);assert(part.tileCount==1);
        const auto* shadow=tiles.prepareShadow(files,source);assert(bool(body)==bool(shadow));poses.push_back({&part,{},0,3,0,false,poses.size()});
        if(!body){assert(name=="DummyParts");++nil;continue;}
        ++present;assert(tiles.lookupShadow(source)==shadow&&tiles.prepareShadow(files,source)==shadow);
        assert(shadow->name==source+"~shd"&&shadow->pixelFormat==3&&shadow->textureSource.image==source&&shadow->textureSource.variant==TextureVariant::originalHalfAlpha);
        assert(shadow->color.r==0&&shadow->color.g==0&&shadow->color.b==0&&shadow->color.a==1);
        const auto bytes=files.read(source),encodedCopy=bytes;const auto rgba=decodeImage(bytes);const auto alpha=prepareOriginalPartAlpha(rgba);
        assert(bytes==encodedCopy);texels+=alpha.alpha.size();
        for(unsigned y=0;y<alpha.storageHeight;++y)for(unsigned x=0;x<alpha.storageWidth;++x)
            assert(alpha.alpha[std::size_t(y)*alpha.storageWidth+x]==(x<rgba.width&&y<rgba.height?rgba.rgba[(std::size_t(y)*rgba.width+x)*4+3]>>1:0));
    }
    assert(present==337&&nil==1&&texels==1801472&&tiles.all().size()==676);
    Frame prepare;tiles.appendPreparation(prepare);assert(prepare.sprites.size()==674);
    unsigned shadowCommands=0;for(const auto& s:prepare.sprites)if(s.textureSource.variant==TextureVariant::originalHalfAlpha){assert(s.image==s.textureSource.image+"~shd");++shadowCommands;}assert(shadowCommands==337);
    auto pose=*std::find_if(poses.begin(),poses.end(),[](const auto& p){return p.part->type==2;});
    const auto* tile=tiles.lookupShadow(pose.part->name+".png");const auto reads=files.reads,probes=files.probes;
    for(float angle:{0.f,.371f,std::numbers::pi_v<float>/2,std::numbers::pi_v<float>})for(bool reverse:{false,true}){
        pose.angle=angle;pose.reverseY=reverse;VisualLayer layer;VisibilityState state;
        collectOriginalPartShadow(layer,tiles,pose,{100,80},nullptr,state);const auto output=layer.drain();assert(output.atlases.size()==1);
        const auto& batch=output.atlases.front();assert(batch.pixelFormat==3&&batch.image==tile->name&&batch.textureSource==tile->textureSource);
        assert(batch.blend.source==BlendFactor::sourceAlpha&&batch.blend.destination==BlendFactor::oneMinusSourceAlpha);
        const auto& q=batch.quads.front();const auto c=center(q);near(c.x,104);near(c.y,76);
        const float x=-tile->pixels.width/2,y=(reverse?-1.f:1.f)*tile->pixels.height/2;
        near(q.vertices[0].position.x,104+x*std::cos(angle)-y*std::sin(angle));near(q.vertices[0].position.y,76+x*std::sin(angle)+y*std::cos(angle));
        for(const auto& v:q.vertices)assert(v.color==ColorBytes(0,0,0,255));
        Frame cached;output.appendBatches(cached);assert(cached.sprites.front().textureSource==tile->textureSource);
        Frame compatibility;output.append(compatibility);assert(compatibility.sprites.front().textureSource==tile->textureSource);
        VisualLayer aircraftLayer;VisibilityState aircraftShadow;
        collectOriginalPartShadow(aircraftLayer,tiles,pose,{100,80},nullptr,aircraftShadow,MachineKind::aircraft);
        const auto aircraft=aircraftLayer.drain();assert(aircraft.atlases.size()==1);
        const auto aircraftCenter=center(aircraft.atlases.front().quads.front());near(aircraftCenter.x,110);near(aircraftCenter.y,70);
        // pose.part is actual partsType2 here: the post-creation length2
        // assignment must not replace the ownerType2 child's initial10.
        assert(pose.part->type==2);
    }
    MapCamera camera{{},{},{100,100},{}};VisibilityState state;VisualLayer layer;
    collectOriginalPartShadow(layer,tiles,pose,{100000,100000},&camera,state);assert(!state.inScreen);assert(layer.drain().atlases.front().quads.size()==1);
    collectOriginalPartShadow(layer,tiles,pose,{100000,100000},&camera,state);assert(layer.drain().atlases.empty());
    collectOriginalPartShadow(layer,tiles,pose,{},&camera,state);assert(state.inScreen);assert(layer.drain().atlases.empty());
    collectOriginalPartShadow(layer,tiles,pose,{},&camera,state);state.removed=true;assert(layer.drain().atlases.front().quads.size()==1); // terminal registration is retained
    collectOriginalPartShadow(layer,tiles,pose,{},&camera,state);assert(layer.drain().atlases.empty());
    // Original P_Wpn_Gun_101 has image35x8 and raw socket0(-9,-3).
    // refreshRotationPos already subtracts its rotated/mirrored socket0;
    // setPartsInfo separately stores bounds(-8.5,-1,26.5,7) unchanged by
    // that rotation/reverse. Both original operations are retained.
    const auto& gun=models.part("P_Wpn_Gun_101");assert(gun.socket(0).x==-9&&gun.socket(0).y==-3);
    const auto* gunTile=tiles.lookup("P_Wpn_Gun_101.png");assert(gunTile->pixels.width==35&&gunTile->pixels.height==8);
    const PartMount mount{"P_Wpn_Gun_101",-1,0,3,0,.371f,true,{}};
    const auto mounted=models.mountedPose(std::span(&mount,1),.371f);const auto& gunPose=mounted.front();
    near(gunPose.center.x,9.f*std::cos(.371f)+3.f*std::sin(.371f));near(gunPose.center.y,9.f*std::sin(.371f)-3.f*std::cos(.371f));
    VisualLayer directLayer,shadowLayer;VisibilityState direct=originalPartVisibility(gunTile,gun),partShadow;
    const auto collect=[&](MachineKind kind,Vec2 position){collectOriginalPart(directLayer,shadowLayer,tiles,gunPose,position,&camera,direct,partShadow,kind);return directLayer.drain();};
    direct.inScreen=false;auto visible=collect(MachineKind::bipod,{118.5f,0});
    assert(visible.nonAtlas.size()==1&&visible.atlases.empty()&&direct.inScreen);
    assert(direct.bounds.left==-8.5f&&direct.bounds.bottom==-1&&direct.bounds.right==26.5f&&direct.bounds.top==7);
    const auto& body=visible.nonAtlas.front();assert(!body.atlasBatch&&body.strip.size()==4);
    assert(body.blend.source==BlendFactor::one&&body.blend.destination==BlendFactor::oneMinusSourceAlpha);
    assert(body.strip[0].uv.x==0&&body.strip[0].uv.y==0&&body.strip[3].uv.x==1&&body.strip[3].uv.y==1);
    for(const auto& vertex:body.strip)assert(vertex.color.r==1&&vertex.color.g==1&&vertex.color.b==1&&vertex.color.a==1);
    const float localX=-17.5f,localY=-4.f;
    near(body.strip[0].position.x,168.5f+localX*std::cos(.371f)-localY*std::sin(.371f));
    near(body.strip[0].position.y,50.f+localX*std::sin(.371f)+localY*std::cos(.371f));
    const float outside=std::nextafter(168.5f,std::numeric_limits<float>::infinity())-50.f;
    auto hidden=collect(MachineKind::bipod,{outside,0});assert(hidden.nonAtlas.empty()&&hidden.atlases.empty()&&!direct.inScreen);
    visible=collect(MachineKind::bipod,{118.5f,0});assert(visible.nonAtlas.size()==1&&visible.atlases.empty()&&direct.inScreen); // no direct one-frame cull lag
    auto previous=collect(MachineKind::aircraft,{outside,0});assert(previous.nonAtlas.empty()&&previous.atlases.size()==1&&!direct.inScreen);
    hidden=collect(MachineKind::aircraft,{outside,0});assert(hidden.nonAtlas.empty()&&hidden.atlases.empty());
    hidden=collect(MachineKind::aircraft,{118.5f,0});assert(hidden.atlases.empty()&&direct.inScreen);
    visible=collect(MachineKind::aircraft,{118.5f,0});assert(visible.nonAtlas.empty()&&visible.atlases.size()==1);
    collectOriginalPart(directLayer,shadowLayer,tiles,gunPose,{},&camera,direct,partShadow,MachineKind::aircraft);
    collectOriginalPart(directLayer,shadowLayer,tiles,gunPose,{},&camera,direct,partShadow,MachineKind::bipod);
    Frame mixed;directLayer.drain().appendBatches(mixed);assert(mixed.sprites.size()==2&&!mixed.sprites[0].atlasBatch&&mixed.sprites[1].atlasBatch); // nonatlas before prior-registered atlas
    direct.show=false;hidden=collect(MachineKind::bipod,{});assert(hidden.nonAtlas.empty()&&hidden.atlases.empty());
    direct.show=true;direct.removed=true;hidden=collect(MachineKind::bipod,{});assert(hidden.nonAtlas.empty()&&hidden.atlases.empty());
    assert(files.reads==reads&&files.probes==probes);
    PartSprites preview;preview.prepare(files,poses);Frame composed;Sprite terrain;terrain.image="terrain";composed.sprites.push_back(std::move(terrain));
    const auto warmReads=files.reads,warmProbes=files.probes;
    preview.append(composed,std::span(&pose,1),{20,10});preview.append(composed,std::span(&pose,1),{40,30});
    assert(composed.sprites.size()==5&&composed.sprites[0].image=="terrain");
    for(unsigned i=1;i<=2;++i)assert(composed.sprites[i].atlasBatch&&composed.sprites[i].visualLayer==0);
    assert(!composed.sprites[3].atlasBatch&&!composed.sprites[4].atlasBatch);
    for(int i=0;i<120;++i){Frame frame;preview.append(frame,poses,{});assert(frame.sprites.size()==674);}
    assert(files.reads==warmReads&&files.probes==warmProbes);
    std::cout<<"parts=338 PNG=337 nil=1 A8texels="<<texels<<" exactBytes=all warmReads=0 warmExists=0 geometry/firstTick/cull/terminal/layer/directCurrentVsAtlasPrevious/nonzeroSocketRotatedBounds=passed\n";
}
#endif
struct Hooks:CombatSceneHooks{
    ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& r)override{return test::preparedTextStub(r);}
    void sound(std::string_view,Vec2)override{}void soundID(unsigned,Vec2)override{}
    void dropEnemyLoot(const MachineInfo&,Vec2,double,float,WorldRandom&)override{}void rewardEnemyKill(const MachineInfo&,Vec2,double,float,WorldRandom&)override{}
    int playerDamage(int n,WorldRandom&)override{return n;}int damageBonus()const override{return 0;}
};
std::size_t aircraftOffsets(const Frame& frame,const std::vector<std::string>& images){
    std::size_t matched=0;
    for(const auto& image:images){std::vector<Vec2> body,shadow;
        for(const auto& sprite:frame.sprites)if(sprite.atlasBatch){
            if(sprite.image==image)for(const auto& q:sprite.atlasBatch->quads)body.push_back(center(q));
            if(sprite.image==image+"~shd")for(const auto& q:sprite.atlasBatch->quads)shadow.push_back(center(q));
        }
        assert(body.size()==shadow.size());std::vector<bool> used(shadow.size());
        for(const auto& b:body){std::size_t found=shadow.size();
            for(std::size_t i=0;i<shadow.size();++i)if(!used[i]&&std::abs(shadow[i].x-(b.x+10.f))<.0005f&&std::abs(shadow[i].y-(b.y-10.f))<.0005f){found=i;break;}
            assert(found<shadow.size());used[found]=true;++matched;
        }
    }return matched;
}
void actualAircraftEnemy(Files& files){
    const auto models=Models::parse(Chunk::parse(files.read("PartsInfo.mcb")),Chunk::parse(files.read("MachInfo.mcb")));
    const auto machines=Machines::parse(DataTable::parse(files.read("MachList.tble")));const auto maps=Maps::parse(Chunk::parse(files.read("MapInfo.mcb")));
    const Map* selected=nullptr;int gate=0;
    for(const auto& [name,map]:maps.all()){
        const auto normal=std::find_if(map.gates.begin(),map.gates.end(),[](const auto& g){return !g.eventOnly;});if(normal==map.gates.end())continue;
        if(std::any_of(map.spawners.begin(),map.spawners.end(),[&](const auto& s){return machines.machine(s.model).kind==MachineKind::aircraft;})){selected=&map;gate=normal->number;break;}
    }
    assert(selected);std::vector<std::string> images,otherImages;
    for(const auto& spawner:selected->spawners)for(const auto& mount:models.modelMounts(spawner.model)){
        auto& list=machines.machine(spawner.model).kind==MachineKind::aircraft?images:otherImages;list.push_back(mount.part+".png");
    }
    std::sort(images.begin(),images.end());images.erase(std::unique(images.begin(),images.end()),images.end());
    std::erase_if(images,[&](const auto& image){return std::find(otherImages.begin(),otherImages.end(),image)!=otherImages.end();});assert(!images.empty());
    Hooks host;CombatSetup setup;setup.map=selected->name;setup.gate=gate;CombatScene scene(files,host,setup);
    const auto reads=files.reads,probes=files.probes;scene.tick();scene.tick();const auto frame=scene.drawFrame();
    const auto enemies=scene.enemies();assert(std::any_of(enemies.begin(),enemies.end(),[](const auto& e){return e.kind==MachineKind::aircraft;}));
    const auto matched=aircraftOffsets(frame,images);assert(matched>0&&files.reads==reads&&files.probes==probes);
    std::cout<<"actualEnemyAircraft map="<<selected->name<<" bodyShadowPairs="<<matched<<" offset10,-10 warmIO0\n";
}
void actualScene(Files& files){
    Hooks host;CombatSetup setup;setup.map="Stage01_02";setup.offsetInsideGate=true;setup.botSlots={CombatBotSetup{"Bot_P01",11},CombatBotSetup{"Bot_A01",22},CombatBotSetup{"Bot_P02",0}};
    const auto models=Models::parse(Chunk::parse(files.read("PartsInfo.mcb")),Chunk::parse(files.read("MachInfo.mcb")));std::vector<std::string> botImages;
    for(const auto& bot:setup.botSlots)for(const auto& mount:models.modelMounts(bot.machine))botImages.push_back(mount.part+".png");
    std::sort(botImages.begin(),botImages.end());botImages.erase(std::unique(botImages.begin(),botImages.end()),botImages.end());
    CombatScene scene(files,host,setup);Frame preparation;preparation=scene.preparationFrame();unsigned prepared=0;for(const auto& s:preparation.sprites)prepared+=s.textureSource.variant==TextureVariant::originalHalfAlpha;assert(prepared==337);
    const auto reads=files.reads,probes=files.probes;assert(scene.bots().size()==3);std::size_t maxShadows=0;bool playerDirect=false,botAtlas=false,enemyAtlas=false;
    std::size_t botOffsetPairs=0;
    for(unsigned tick=0;tick<180;++tick){scene.tick();const auto frame=scene.drawFrame();unsigned shadows=0;bool bodyStarted=false;
        if(tick==0){botOffsetPairs=aircraftOffsets(frame,botImages);assert(botOffsetPairs>0);}
        for(const auto& sprite:frame.sprites){
            if(sprite.textureSource.variant==TextureVariant::originalHalfAlpha){assert(!bodyStarted);assert(sprite.atlasBatch&&sprite.atlasBatch->pixelFormat==3);shadows+=sprite.atlasBatch->quads.size();}
            else if(sprite.atlasBatch&&sprite.image!="HPGauge.png")bodyStarted=true;
            if(sprite.textureSource.variant==TextureVariant::rgba){
                if(sprite.image.starts_with("P_")){assert(!sprite.atlasBatch&&sprite.strip.size()==4);playerDirect=true;}
                if(std::find(botImages.begin(),botImages.end(),sprite.image)!=botImages.end()){assert(sprite.atlasBatch);botAtlas=true;}
                if(sprite.image.starts_with("E_")){assert(sprite.atlasBatch);enemyAtlas=true;}
            }
        }maxShadows=std::max(maxShadows,std::size_t(shadows));
    }
    assert(maxShadows>10&&files.reads==reads&&files.probes==probes&&playerDirect&&botAtlas&&enemyAtlas);
    std::cout<<"actualScene=Stage01_02 bots=3 frames=180 maxShadowQuads="<<maxShadows<<" playerDirect=1 enemyAtlas=1 botAtlas=1 botShadowOffset10Pairs="<<botOffsetPairs<<" warmReads=0 warmExists=0\n";
}
}
int main(int argc,char** argv){assert(argc==2);Files files(argv[1]);
#ifndef HM2_AIRCRAFT_SCENE_ONLY
    exactBytes();catalogueAndGeometry(files);actualScene(files);
#endif
    actualAircraftEnemy(files);
}

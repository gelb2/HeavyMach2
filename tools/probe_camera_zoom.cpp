// Offline measurement of the real shared battle camera. No device/save writes.
// Build against HeavyMach2Core; pass the local prepared HD resource directory.
#include "Core/Simulation/CombatScene.hpp"
#include "Core/Presentation/DisplayLayout.hpp"
#include "PreparedTextStub.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

using namespace hm2;
namespace {
struct Files final:ResourceAccess {
    std::filesystem::path root;
    explicit Files(const char* p):root(p){}
    Bytes read(std::string_view name)override {
        std::ifstream f(root/std::string(name),std::ios::binary);
        if(!f)throw std::runtime_error("Missing camera fixture "+std::string(name));
        return {std::istreambuf_iterator<char>(f),{}};
    }
    bool exists(std::string_view name)const override{return std::filesystem::exists(root/std::string(name));}
};
struct Hooks final:CombatSceneHooks {
    void sound(std::string_view,Vec2)override{}
    void soundID(unsigned,Vec2)override{}
    void dropEnemyLoot(const MachineInfo&,Vec2,double,float,WorldRandom&)override{}
    void rewardEnemyKill(const MachineInfo&,Vec2,double,float,WorldRandom&)override{}
    int playerDamage(int damage,WorldRandom&)override{return damage;}
    int damageBonus()const override{return 0;}
    ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& r)override{return test::preparedTextStub(r);}
};
void run(Files& files,const char* name,DisplayConfiguration config,bool paused) {
    Hooks hooks;CombatSetup setup;setup.offsetInsideGate=true;
    CombatScene scene(files,hooks,setup);scene.setDisplay(config);scene.setPaused(paused);
    const DisplayLayout layout(config);
    for(unsigned frame=0;frame<=1200;++frame){
        if(frame)scene.tick();
        if(frame!=0&&frame!=1&&frame!=60&&frame!=120&&frame!=300&&frame!=600&&frame!=1200)continue;
        const float zoom=scene.displayZoom(),scale=layout.worldScale(zoom);
        const auto view=scene.camera().viewport;
        const auto player=scene.player().position;
        const auto path=scene.dashState();
        // A known distance is mapped through the same camera and display path
        // as world sprites and input, independent of animation/transparent art.
        const auto a=layout.world(zoom).map(scene.camera().toScreen(player));
        const auto b=layout.world(zoom).map(scene.camera().toScreen({player.x+100,player.y}));
        const auto inverse=scene.camera().toWorld(layout.world(zoom).inverse(b));
        if(std::abs((b.x-a.x)-100*scale)>.002f||std::abs(inverse.x-player.x-100)>.002f)
            throw std::runtime_error("World/display inverse mismatch");
        std::cout<<"{\"case\":\""<<name<<"\",\"paused\":"<<(paused?"true":"false")
            <<",\"frame\":"<<frame<<",\"renderTime\":"<<scene.clock().renderTime()
            <<",\"worldTime\":"<<scene.clock().worldTime()<<",\"zoom\":"<<zoom
            <<",\"worldScale\":"<<scale<<",\"viewportWidth\":"<<view.width
            <<",\"viewportHeight\":"<<view.height<<",\"world100InPoints\":"<<b.x-a.x
            <<",\"playerX\":"<<player.x<<",\"playerY\":"<<player.y
            <<",\"pathCount\":"<<path.pathCount<<",\"selectedEnemy\":"<<scene.selectedEnemy()
            <<",\"hp\":"<<scene.playerHP()<<"}\n";
    }
}
}
int main(int argc,char** argv){
    if(argc!=2)return 64;
    std::cout<<std::setprecision(10);
    Files files(argv[1]);
    run(files,"restored-legacy-hd-768x1024",{},true);
    run(files,"restored-ipad-1024x1366",DisplayConfiguration::surface({1024,1366},{0,20,0,24}),true);
    run(files,"restored-ipad-air13-1032x1376",DisplayConfiguration::surface({1032,1376},{0,20,0,24}),true);
    run(files,"restored-iphone-air-912x420",DisplayConfiguration::surface({912,420},{68,20,68,0}),true);
    run(files,"restored-iphone-852x393",DisplayConfiguration::surface({852,393},{59,21,59,0}),true);
    auto noFocus=DisplayConfiguration::surface({912,420},{68,20,68,0});noFocus.focusZoom=false;
    run(files,"diagnostic-iphone-no-focus",noFocus,true);
    run(files,"restored-iphone-air-912x420-live-idle",DisplayConfiguration::surface({912,420},{68,20,68,0}),false);
}

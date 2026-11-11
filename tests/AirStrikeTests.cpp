#include "Core/Simulation/AirStrike.hpp"
#include <cassert>
#include <cmath>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
using namespace hm2;
namespace {
struct Files:ResourceAccess {
    std::filesystem::path root;unsigned reads=0;mutable unsigned probes=0;
    explicit Files(const char* p):root(p){}
    Bytes read(std::string_view name)override{++reads;std::ifstream f(root/std::string(name),std::ios::binary);if(!f)throw std::runtime_error("Missing original airstrike resource "+std::string(name));return {std::istreambuf_iterator<char>(f),{}};}
    bool exists(std::string_view name)const override{++probes;return std::filesystem::exists(root/std::string(name));}
};
struct Random:WorldRandom {
    unsigned calls=0;std::deque<std::uint32_t> values;
    std::uint32_t random()override{++calls;if(values.empty())return 1;const auto v=values.front();values.pop_front();return v;}
    void sequence(std::initializer_list<std::uint32_t> v){assert(values.empty());values.assign(v);}
};
struct Host:AirStrikeHost {
    Random& random;PreparedTiles& tiles;ParticlePool pool;std::vector<AirStrikeBullet> bullets;
    std::vector<std::string> events;std::vector<int> countsBeforeBullet;const AirStrike* actor=nullptr;bool consumeBulletRandom=false;
    Host(Random& r,PreparedTiles& t):random(r),tiles(t),pool(0,r){}
    TileImage* preparedTile(std::string_view name)override{events.push_back("tile:"+std::string(name));return tiles.lookup(name);}
    Particle* allocateParticle(float time)override{events.push_back("marker");return pool.allocate(time,random);}
    void sound(unsigned id)override{assert(id==9);events.push_back("sfx9");}
    void createBullet(const AirStrikeBullet& value)override{assert(value.weapon&&!value.toPlayer&&value.drop);events.push_back("bullet");bullets.push_back(value);if(actor)countsBeforeBullet.push_back(actor->state().attackCount);if(consumeBulletRandom)(void)random.random();}
};
bool near(float a,float b){return std::abs(a-b)<.00001f;}
void initialize(PreparedTiles& tiles,Files& files){tiles.prepare(files,"Airplane.png");tiles.prepare(files,"Target.png");}
void markerAndDirection(PreparedTiles& tiles,const WeaponInfo& weapon){
    Random random;Host host(random,tiles);assert(random.calls==64);AirStrike actor(&weapon,1.5f,{300,400},{0,0},7.25,2.5f,host,random);
    assert(random.calls==65&&actor.state().attackCount==weapon.shotCount&&actor.state().startTime==7.25f&&actor.state().fireDelay==1.5f);
    assert(near(actor.state().position.x,-420)&&near(actor.state().position.y,-560)&&near(actor.state().velocity.x,2.4f)&&near(actor.state().velocity.y,3.2f));
    assert(actor.state().angle==float(std::atan2(double(.8f),double(.6f)))&&actor.state().scale==1.3f);
    const std::vector<std::string> expected={"tile:Airplane.png","marker","tile:Target.png","sfx9"};assert(host.events==expected);
    const auto& p=host.pool.particles()[0].state();assert(p.show&&p.useAtlas&&p.fadeOut&&p.blendType==2&&p.tile&&p.tile->name=="Target.png"&&p.tile->columns==2&&p.tile->rows==1&&p.tileNumber==0);
    assert(p.position.x==300&&p.position.y==400&&p.life==3&&p.startTime==2.5f&&p.scale.x==15&&p.scale.y==15&&p.easyOutScale==1.5f&&p.easyOutValue==6&&p.rotationVelocity==.1f);
    const auto before=actor.state();actor.tick(7.5);assert(actor.state().position.x==before.position.x+before.velocity.x&&actor.state().position.y==before.position.y+before.velocity.y);
    assert(actor.state().velocity.x==before.velocity.x+before.velocity.x*.01f&&random.calls==65&&host.bullets.empty());
    Random zero;Host zeroHost(zero,tiles);AirStrike stationary(&weapon,1,{10,10},{10,10},0,0,zeroHost,zero);assert(stationary.state().position.x==10&&stationary.state().position.y==10&&stationary.state().velocity.x==0&&stationary.state().velocity.y==0&&stationary.state().angle==0);
    Random tiny;Host tinyHost(tiny,tiles);AirStrike small(&weapon,1,{.5e-5f,0},{0,0},0,0,tinyHost,tiny);assert(small.state().velocity.x==0);
    Random edge;Host edgeHost(edge,tiles);AirStrike threshold(&weapon,1,{1e-5f,0},{0,0},0,0,edgeHost,edge);assert(threshold.state().velocity.x==4);
}
void fullPoolAndNil(PreparedTiles& tiles,const WeaponInfo& weapon){
    Random random;Host host(random,tiles);for(auto& p:host.pool.particles())p.state().show=true;
    AirStrike actor(&weapon,1,{},{},0,0,host,random);assert(random.calls==64&&host.pool.allocations()==0);assert((host.events==std::vector<std::string>{"tile:Airplane.png","marker","sfx9"}));
    Random other;Host nil(other,tiles);AirStrike missing(nullptr,10,{100,0},{0,0},0,0,nil,other);const auto initial=other.calls;missing.tick(4);assert(!missing.state().removed&&other.calls==initial);missing.tick(4.000001);assert(missing.state().removed&&nil.bullets.empty()&&other.calls==initial);const auto position=missing.state().position;missing.tick(100);assert(missing.state().position.x==position.x);
}
void firingOrder(PreparedTiles& tiles,const WeaponInfo& original){
    auto weapon=original;weapon.shotCount=2;weapon.aimRate=1;Random random;Host host(random,tiles);AirStrike actor(&weapon,1.5f,{100,0},{0,0},0,0,host,random);host.actor=&actor;const auto setup=random.calls;
    actor.tick(1.5);assert(random.calls==setup&&host.bullets.empty());
    random.sequence({3});actor.tick(1.500001);assert(random.calls==setup+1&&host.bullets.empty()&&actor.state().attackCount==2);
    // Success consumes gate+angle+two triangular-radius draws, then synchronously
    // delegates common GobBullet. No muzzle flash or controller sound is called.
    random.sequence({0,0,0,0});const float beforeX=actor.state().position.x,velocity=actor.state().velocity.x;actor.tick(5);
    assert(random.calls==setup+5&&!actor.state().removed&&actor.state().attackCount==1&&actor.bulletRequests()==1&&host.bullets.size()==1);
    const auto& bullet=host.bullets[0];assert(bullet.angle==0&&bullet.source.x==(beforeX+velocity)-36&&bullet.source.y==0&&bullet.destination.x==-20&&bullet.destination.y==0&&host.countsBeforeBullet[0]==2);
    random.sequence({1});actor.tick(6);assert(!actor.state().removed&&actor.state().attackCount==1); // random failure does not run4s removal.
    host.consumeBulletRandom=true;random.sequence({0,0x20000000u,0x20000000u,0x60000000u,123});actor.tick(7);assert(actor.state().attackCount==0&&!actor.state().removed&&host.bullets.size()==2&&random.values.empty()&&host.countsBeforeBullet[1]==1);
    assert(host.bullets[1].destination.x==100&&host.bullets[1].destination.y==0);const auto after=random.calls;actor.tick(7.0001);assert(actor.state().removed&&random.calls==after);
    assert(actor.state().startTime==0&&actor.state().fireDelay==1.5f); // Neither is updated per shot.
    // World.pause freezes worldTime but continues objectBase child ticks. The
    // original AirStrike has no local pause check, so two identical timestamps
    // still move/accelerate and may each request a bullet after delay opens.
    Random frozen;Host frozenHost(frozen,tiles);AirStrike duringPause(&weapon,1,{100,0},{0,0},0,0,frozenHost,frozen);const auto pauseSetup=frozen.calls;const auto startX=duringPause.state().position.x;
    frozen.sequence({0,0,0,0});duringPause.tick(2);const auto firstX=duringPause.state().position.x;frozen.sequence({0,0,0,0});duringPause.tick(2);
    assert(firstX>startX&&duringPause.state().position.x>firstX&&duringPause.bulletRequests()==2&&frozen.calls==pauseSetup+8&&!duringPause.state().removed);
    const auto afterPauseShots=frozen.calls;duringPause.tick(2);assert(frozen.calls==afterPauseShots&&!duringPause.state().removed);
    Random precise;Host hp(precise,tiles);AirStrike large(&weapon,1.5f,{},{},16777216.25,1,hp,precise);const auto count=precise.calls;large.tick(16777218);assert(precise.calls==count);precise.sequence({1});large.tick(std::nextafter(16777218.,std::numeric_limits<double>::infinity()));assert(precise.calls==count+1);
}
void actualWeaponsAndVisual(PreparedTiles& tiles,const Weapons& weapons){
    const char* names[]={"AS_BOMB_01","AS_MISSILE_01","AS_NUCLEAR_01"};const unsigned counts[]={10,10,1};const float delays[]={1.5f,1.f,2.f};
    for(unsigned i=0;i<3;++i){const auto& weapon=weapons.weapon(names[i]);assert(weapon.shotCount==counts[i]);Random random;Host host(random,tiles);AirStrike actor(&weapon,delays[i],{100,0},{0,0},0,0,host,random);host.actor=&actor;const auto begin=random.calls;
        for(unsigned shot=0;shot<counts[i];++shot){random.sequence({0,0,0,0});actor.tick(5.+double(shot));assert(actor.state().attackCount==int(counts[i]-shot-1)&&!actor.state().removed);}
        assert(actor.bulletRequests()==counts[i]&&host.bullets.size()==counts[i]&&random.calls==begin+counts[i]*4);actor.tick(100);assert(actor.state().removed);for(const auto& b:host.bullets)assert(b.weapon==&weapon&&b.drop&&!b.toPlayer);
    }
    Random random;Host host(random,tiles);AirStrike plane(&weapons.weapon(names[0]),1.5f,{100,0},{0,0},0,0,host,random);MapCamera camera;camera.center=plane.state().position;plane.tick(.1,&camera);VisualLayer layer;const auto before=random.calls;plane.collect(layer,camera,tiles);auto frame=layer.drain();assert(frame.nonAtlas.size()==1&&frame.atlases.empty());const auto& sprite=frame.nonAtlas[0];assert(sprite.image=="Airplane.png"&&sprite.strip.size()==4&&sprite.color.a==1&&random.calls==before);const float width=sprite.strip[2].position.x-sprite.strip[0].position.x;assert(std::abs(width-tiles.lookup("Airplane.png")->pixels.width*1.3f)<.0001f);
    camera.center={10000,10000};plane.tick(.2,&camera);plane.collect(layer,camera,tiles);assert(layer.drain().nonAtlas.empty());
}
}
int main(int argc,char** argv){if(argc<2)return 77;Files files(argv[1]);PreparedTiles tiles("airstrike-tests");initialize(tiles,files);const auto weapons=Weapons::parse(DataTable::parse(files.read("WeaponInfo.tble")));const auto reads=files.reads,probes=files.probes;
    markerAndDirection(tiles,weapons.weapon("AS_BOMB_01"));fullPoolAndNil(tiles,weapons.weapon("AS_BOMB_01"));firingOrder(tiles,weapons.weapon("AS_BOMB_01"));actualWeaponsAndVisual(tiles,weapons);assert(files.reads==reads&&files.probes==probes);
    std::cout<<"HM2_AIRSTRIKE PASS original3weapons/10+10+1 bulletRequests normalize0/epsilon floatClock/strictDelay/RNG4 callbackOrder/after4s markerPool/actualTile/VisualLayer warmReads0 warmExists0; common bullet damage remains integration-owned\n";
}

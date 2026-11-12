#include "Core/Simulation/EnemyVehicle.hpp"
#include "Core/Data/Resources.hpp"
#include <bit>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

using namespace hm2;
namespace {
void near(float a,float b){assert(std::abs(a-b)<.0001f);}
void putFloat(Bytes& bytes,std::size_t offset,float value){
    auto word=std::bit_cast<std::uint32_t>(value);for(unsigned i=0;i<4;++i)bytes[offset+i]=std::uint8_t(word>>(8*i));
}
void name(Bytes& bytes,std::string_view text){std::copy(text.begin(),text.end(),bytes.begin());}
Chunk part(std::string_view id,unsigned type,std::initializer_list<Vec2> sockets){
    Chunk c;c.type=1;c.data.resize(27+sockets.size()*8);name(c.data,id);c.data[24]=std::uint8_t(type);c.data[25]=1;c.data[26]=std::uint8_t(sockets.size());
    std::size_t offset=27;for(auto p:sockets){putFloat(c.data,offset,p.x);putFloat(c.data,offset+4,p.y);offset+=8;}return c;
}
Models fixtureModels(){
    Chunk parts;parts.type=0;parts.children={part("FixtureBody",3,{{1,2},{12,0}}),part("FixtureWeapon",4,{{-2,-3},{5,1},{7,1}})};
    Chunk weapon;weapon.type=4;weapon.data.resize(26);name(weapon.data,"FixtureWeapon");weapon.data[24]=1;
    Chunk body;body.type=4;body.data.resize(26);name(body.data,"FixtureBody");body.children={weapon};
    Chunk model;model.type=3;model.data.resize(33);name(model.data,"FixtureVehicle");model.children={body};model.declaredSize=model.consumedSize=1;
    Chunk machines;machines.type=2;machines.children={model};return Models::parse(parts,machines);
}
Weapons fixtureWeapons(){
    return Weapons::parse(DataTable::parseText("Parts\tType\tDmg\tRange\tShtRng\tShtCnt\tShtDly\tRldDly\tSpeed\tAimRt\tBltTile\tEffType\tEffTile\tExplodeEff\tExplScl\tShell\tFireSound\tUiTile\nFixtureWeapon\tBullet\t3\t0\t250\t2\t0.1\t0.25\t0.1\t1\tMini\t-\t-\t-\t-\t0\tFixtureSound\t0\n"));
}
struct Host:EnemyVehicleHost {
    WeaponTarget playerState{1,{100,0},256};bool hasPlayer=true;
    std::optional<Vec2> forcedContact;
    unsigned reads=0,contacts=0;std::vector<std::uint32_t> values{1,0x40000000u,0};
    std::vector<WeaponEmission> emissions;std::vector<EnemyVehicleEmission> contexts;std::vector<Vec2> sounds;
    const WeaponTarget* player()const override{return hasPlayer?&playerState:nullptr;}
    std::uint32_t random()override{const auto i=reads++;return i<values.size()?values[i]:0x40000000u;}
    void objectContacts(EnemyVehicle& actor)override{++contacts;if(forcedContact)actor.applyContact(*forcedContact);}
    void emit(const WeaponInfo&,const WeaponEmission& shot,const EnemyVehicleEmission& input)override{emissions.push_back(shot);contexts.push_back(input);}
    void fireSound(const WeaponInfo&,Vec2 p)override{sounds.push_back(p);}
};
const PartPose& pose(std::span<const PartPose> poses,std::size_t index){for(const auto& p:poses)if(p.mountIndex==index)return p;throw std::runtime_error("Missing test pose");}
Bytes read(const std::filesystem::path& root,std::string_view file){
    std::ifstream in(root/file,std::ios::binary);if(!in)throw std::runtime_error("Missing original fixture "+std::string(file));
    return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
}
void original(const std::filesystem::path& root){
    const auto models=Models::parse(Chunk::parse(read(root,"PartsInfo.mcb")),Chunk::parse(read(root,"MachInfo.mcb")));
    const auto weapons=Weapons::parse(DataTable::parse(read(root,"WeaponInfo.tble")));
    const auto machines=Machines::parse(DataTable::parse(read(root,"MachList.tble")));
    const auto maps=Maps::parse(Chunk::parse(read(root,"MapInfo.mcb")));TerrainCollision terrain(maps.map("Stage01_01"));
    unsigned count=0;
    for(const auto& spawner:maps.map("Stage01_01").spawners){
        const auto& info=machines.machine(spawner.model);assert(info.name=="E_Tank_01"||info.name=="E_Tank_02");
        Host host;host.values={0,0x40000000u,0};host.playerState.position={spawner.position.x+100,spawner.position.y};
        EnemyVehicle enemy(models,weapons,info,spawner.position,100,host,spawner.position);
        assert(host.reads==3&&enemy.state().hp==info.hp&&enemy.state().attackable&&!enemy.weapons().empty());
        for(const auto& w:enemy.weapons()){assert(w.currentMuzzle==1&&w.aim.state().startTime==0&&w.controller.state().fireLeft==0&&w.controller.state().fireTime==0);}
        for(unsigned i=0;i<120;++i)enemy.tick({100+double(i)*.016599999740719795,false,true},terrain,host);
        assert(enemy.state().targetPlayer&&!host.emissions.empty()&&host.emissions.size()==host.sounds.size());
        assert(!enemy.poses().empty()&&enemy.poses().size()==models.modelMounts(info.name).size());
        for(const auto& e:host.contexts)assert(e.owner==&enemy&&e.hostile);
        enemy.state().hp=0;enemy.setMachineState(2);enemy.removePartsForCorpse();enemy.state().targetPlayer=true;
        const auto emissions=host.emissions.size();const auto reads=host.reads;
        enemy.tick({200,false,true},terrain,host);
        assert(host.emissions.size()==emissions&&host.reads==reads&&enemy.poses().size()==1&&enemy.atlasPoses().size()==1);
        for(const auto& w:enemy.weapons())assert(w.removed);
        ++count;
    }
    assert(count==16);std::cout<<"Stage01_01: all 16 original E_Tank_01/02 spawners initialized, aimed and emitted through real controllers\n";
}
}
int main(int argc,char** argv){
    const auto models=fixtureModels();const auto weapons=fixtureWeapons();MachineInfo info;info.name="FixtureVehicle";info.kind=MachineKind::vehicle;info.hp=200;info.radius=30;
    Map empty;TerrainCollision terrain(empty);
    Host initial;EnemyVehicle enemy(models,weapons,info,{},100,initial);
    assert(initial.reads==3&&enemy.state().hp==200&&!enemy.state().firstAttack);near(enemy.state().createTime,100);near(enemy.state().stopDelay,101);near(enemy.motion().angle,0);
    assert(enemy.weapons().size()==1&&enemy.weapons()[0].currentMuzzle==1);
    assert(enemy.weapons()[0].aim.state().startTime==0&&enemy.weapons()[0].controller.state().fireTime==0&&enemy.weapons()[0].controller.state().fireLeft==0);
    assert(enemy.mounts()[0].layer==1&&enemy.mounts()[1].layer==2);
    // No-target child aim consumes RNG, even while the world is paused.
    enemy.tick({100,true,true},terrain,initial);assert(initial.reads==4&&initial.contacts==0&&initial.emissions.empty());
    const auto aim=enemy.weapons()[0].aim.state().angle;enemy.tick({100,true,true},terrain,initial);assert(initial.reads==5&&enemy.weapons()[0].aim.state().angle!=aim);
    // Entry AI runs before base show guard; hidden root suppresses part ticks.
    Host hidden;EnemyVehicle invisible(models,weapons,info,{},0,hidden);invisible.state().show=false;
    invisible.tick({2,false,true},terrain,hidden);assert(hidden.reads==6&&hidden.contacts==0&&hidden.emissions.empty());
    // First attack and strict radius/time boundaries originate in original AI.
    Host first;first.values={0,0x40000000u,0};first.playerState.position={200,0};EnemyVehicle attacker(models,weapons,info,{},0,first);
    attacker.tick({.5,false,true},terrain,first);assert(!attacker.state().targetPlayer);
    attacker.tick({.6,false,true},terrain,first);assert(!attacker.state().targetPlayer);
    first.playerState.position.x=std::nextafter(200.f,0.f);attacker.tick({.7,false,true},terrain,first);
    assert(attacker.state().targetPlayer&&attacker.state().attackCheckTime==0&&!first.emissions.empty());
    near(attacker.state().targetCheckTime,.7f);assert(!attacker.state().moveToTarget);
    assert(attacker.weapons()[0].currentMuzzle==2&&attacker.weapons()[0].controller.state().fireLeft==1);
    near(attacker.mounts()[1].addPosition.x,-4.5f);near(attacker.mounts()[0].addPosition.x,-2.25f);
    near(pose(attacker.poses(),0).center.x,-1); // Parent recoil takes effect next tick.
    attacker.tick({.7,true,true},terrain,first);near(pose(attacker.poses(),0).center.x,-3.25f);
    // At creation world100, enemy fireTime remains0 (no Player setReload).
    Host late;late.values={0,0x40000000u,0};EnemyVehicle ready(models,weapons,info,{},100,late);
    ready.tick({100,false,true},terrain,late);assert(late.emissions.size()==1&&late.sounds.size()==1);
    assert(late.contexts[0].owner==&ready&&late.contexts[0].mountIndex==1&&late.contexts[0].hostile);
    near(late.contexts[0].partCenter.x,13);near(late.contexts[0].partCenter.y,1);
    // onDamage may retarget player after super.onCrash. Actual part removal,
    // not a new owner-HP guard in WeaponController, prevents corpse firing.
    ready.state().hp=0;ready.setMachineState(2);ready.removePartsForCorpse();ready.state().targetPlayer=true;
    assert(ready.state().weaponReady&&ready.weapons()[0].removed&&ready.partActive(0)&&!ready.partActive(1));
    assert(ready.poses().size()==1&&ready.atlasPoses().size()==1&&ready.poses()[0].layer==1);
    const auto corpseReads=late.reads;const auto corpseFire=ready.weapons()[0].controller.state().fireTime;
    ready.tick({200,false,true},terrain,late);assert(late.reads==corpseReads&&late.emissions.size()==1&&late.sounds.size()==1);
    assert(ready.state().targetPlayer&&ready.weapons()[0].controller.state().fireTime==corpseFire&&ready.poses().size()==1);
    ready.removePartsForCorpse();assert(ready.poses().size()==1); // Idempotent removal.
    // Pre-aim cached center and atlas angle differ from post-child part angle.
    Host vertical;vertical.playerState.position={0,100};EnemyVehicle rotating(models,weapons,info,{},100,vertical);rotating.state().targetPlayer=true;
    rotating.tick({100,true,true},terrain,vertical);const float firstAngle=rotating.weapons()[0].aim.state().angle;
    near(pose(rotating.poses(),1).center.x,13);near(pose(rotating.poses(),1).angle,firstAngle);near(pose(rotating.atlasPoses(),1).angle,0);assert(vertical.emissions.empty());
    rotating.tick({100,true,true},terrain,vertical);
    near(pose(rotating.poses(),1).center.x,11.f-(-2.f*std::cos(firstAngle)+3.f*std::sin(firstAngle)));
    near(pose(rotating.atlasPoses(),1).angle,firstAngle);
    for(unsigned i=0;i<40;++i)rotating.tick({100,true,true},terrain,vertical);
    assert(vertical.contacts==0&&vertical.emissions.size()==1); // Child fire is not globally paused.
    // Patrol timer chooses world coordinates around its real spawner; movement
    // and idle weapon aim consume the same subsequent stream.
    Host patrol;EnemyVehicle wandering(models,weapons,info,{10,20},0,patrol,Vec2{30,40});
    wandering.tick({1,false,true},terrain,patrol);const auto before=patrol.reads;
    wandering.tick({1.1,false,true},terrain,patrol);assert(patrol.reads==before+5);
    near(wandering.motion().destination.x,30);near(wandering.motion().destination.y,40);near(wandering.state().stopDelay,float(1.1+2.0+2.0));assert(wandering.motion().position.x>10);
    // Common tracking runs after movement/collisions, strictly every >.5f.
    Host chasing;EnemyVehicle follower(models,weapons,info,{},0,chasing);follower.state().targetPlayer=follower.state().moveToTarget=true;
    follower.tick({.5,false,true},terrain,chasing);assert(follower.motion().state==0);
    follower.tick({.5001,false,true},terrain,chasing);assert(follower.motion().state==1);near(follower.motion().destination.x,follower.motion().position.x);
    chasing.playerState.position={200,0};follower.tick({1.1,false,true},terrain,chasing);near(follower.motion().destination.x,200);
    const auto prior=follower.motion().position;follower.tick({1.1166,false,true},terrain,chasing);assert(follower.motion().position.x>prior.x);
    Host order;order.forcedContact=Vec2{-200,0};EnemyVehicle corrected(models,weapons,info,{},0,order);
    corrected.state().targetPlayer=corrected.state().moveToTarget=true;corrected.tick({.6,false,true},terrain,order);
    near(corrected.motion().position.x,-200);near(corrected.motion().destination.x,100); // Follow uses the post-object-contact position.
    // Hidden-to-camera root suppresses common move/contact/follow but not AI
    // and child aim; callers must supply the original root isInScreen field.
    const auto contactCount=chasing.contacts;const auto position=follower.motion().position;follower.tick({1.2,false,false},terrain,chasing);
    assert(chasing.contacts==contactCount);near(follower.motion().position.x,position.x);
    // Terrain correction uses the shared original radius16/grid consumer.
    Map blocked;blocked.collisionPolygons={{{0,-100},{0,100},{100,100},{100,-100}}};TerrainCollision wall(blocked);
    Host colliding;EnemyVehicle grounded(models,weapons,info,{-5,0},0,colliding);const auto expected=wall.resolve({-5,0});
    grounded.tick({0,false,true},wall,colliding);near(grounded.motion().position.x,expected.position.x);assert(grounded.contact().current==expected.current&&colliding.contacts==1);
    grounded.setMachineState(2);grounded.state().hp=0;const auto stopped=grounded.motion().position;
    grounded.tick({10,false,true},wall,colliding);near(grounded.motion().position.x,stopped.x);
    if(argc==2)original(argv[1]);else assert(argc==1);
}

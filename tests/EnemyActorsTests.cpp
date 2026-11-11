#include "Core/Simulation/EnemyActors.hpp"
#include "Core/Data/Maps.hpp"
#include "Core/Simulation/EnemySpawner.hpp"
#include "Core/Data/Resources.hpp"
#include <cassert>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <numbers>
using namespace hm2;
namespace {
void near(float a,float b){assert(std::abs(a-b)<.0001f);}
void put(Bytes& b,std::size_t p,float f){const auto n=std::bit_cast<std::uint32_t>(f);for(unsigned i=0;i<4;++i)b[p+i]=std::uint8_t(n>>(8*i));}
void name(Bytes& b,std::string_view s){std::copy(s.begin(),s.end(),b.begin());}
Chunk part(std::string_view id,unsigned type,std::initializer_list<Vec2> sockets){Chunk c;c.type=1;c.data.resize(27+sockets.size()*8);name(c.data,id);c.data[24]=type;c.data[25]=1;c.data[26]=sockets.size();std::size_t p=27;for(auto v:sockets){put(c.data,p,v.x);put(c.data,p+4,v.y);p+=8;}return c;}
Models models(){
    Chunk p;p.type=0;p.children={part("FixtureBody",3,{{1,2},{12,0},{-10,0}}),part("FixtureWeapon",4,{{-2,-3},{5,1},{7,1}}),part("FixtureRotor",7,{{2,0}})};
    Chunk w;w.type=4;w.data.resize(26);name(w.data,"FixtureWeapon");w.data[24]=1;
    Chunk r;r.type=4;r.data.resize(26);name(r.data,"FixtureRotor");r.data[24]=2;
    Chunk body;body.type=4;body.data.resize(26);name(body.data,"FixtureBody");body.children={w,r};
    Chunk machines;machines.type=2;
    for(auto id:{"FixtureTurret","FixturePlane","FixtureVehicle"}){Chunk m;m.type=3;m.data.resize(33);name(m.data,id);m.children={body};m.declaredSize=m.consumedSize=1;machines.children.push_back(m);}
    return Models::parse(p,machines);
}
Weapons weapons(){return Weapons::parse(DataTable::parseText("Parts\tType\tDmg\tRange\tShtRng\tShtCnt\tShtDly\tRldDly\tSpeed\tAimRt\tBltTile\tEffType\tEffTile\tExplodeEff\tExplScl\tShell\tFireSound\tUiTile\nFixtureWeapon\tBullet\t3\t0\t250\t2\t.1\t.25\t.1\t1\tMini\t-\t-\t-\t-\t0\tFixtureSound\t0\n"));}
struct Host:EnemyActorHost {
    WeaponTarget target{1,{100,0},256};bool hasPlayer=true;unsigned calls=0,contacts=0,sounds=0;
    std::vector<std::uint32_t> values{1,0x40000000u,0};std::vector<EnemyActorEmission> emissions;
    std::optional<Vec2> forcedContact;
    std::uint32_t random()override{const auto i=calls++;return i<values.size()?values[i]:0x40000000u;}
    const WeaponTarget* player()const override{return hasPlayer?&target:nullptr;}
    void objectContacts(EnemyActor& a)override{++contacts;if(forcedContact)a.applyContact(*forcedContact);}
    void emit(const WeaponInfo&,const WeaponEmission& s,const EnemyActorEmission& e)override{assert(std::isfinite(s.muzzle.x)&&std::isfinite(s.muzzle.y));emissions.push_back(e);}
    void fireSound(const WeaponInfo&,Vec2)override{++sounds;}
};
MachineInfo info(std::string name,MachineKind kind){MachineInfo i;i.name=std::move(name);i.kind=kind;i.hp=200;i.radius=30;return i;}
const PartPose& pose(std::span<const PartPose> p,std::size_t index){for(const auto& v:p)if(v.mountIndex==index)return v;throw std::runtime_error("No pose");}
Bytes read(const std::filesystem::path& root,std::string_view file){std::ifstream in(root/file,std::ios::binary);if(!in)throw std::runtime_error("Missing original fixture "+std::string(file));return {std::istreambuf_iterator<char>(in),{}};}
void original(const std::filesystem::path& root){
    const auto m=Models::parse(Chunk::parse(read(root,"PartsInfo.mcb")),Chunk::parse(read(root,"MachInfo.mcb")));
    const auto w=Weapons::parse(DataTable::parse(read(root,"WeaponInfo.tble")));const auto machines=Machines::parse(DataTable::parse(read(root,"MachList.tble")));
    const auto maps=Maps::parse(Chunk::parse(read(root,"MapInfo.mcb")));std::map<MachineKind,unsigned> counts,armed,fired;std::map<std::string,unsigned> names,rotorModels;
    assert(machines.all().size()==78&&maps.all().size()==43);
    for(const auto& [mapName,map]:maps.all()){
        (void)mapName;TerrainCollision terrain(map);
        for(const auto& spawn:map.spawners){
            const auto& i=machines.machine(spawn.model);++counts[i.kind];++names[i.name];Host h;h.values={0,0x40000000,0};h.target.position={spawn.position.x+100,spawn.position.y};
            EnemyActor actor(m,w,i,spawn.position,100,h,spawn.position);assert(h.calls==3&&actor.state().hp==i.hp&&!actor.isBoss());
            EnemySpawner spawner(spawn,i.attack!=0);actor.setBoss(spawner.boss());assert(actor.isBoss()==spawner.boss());
            for(const auto& mount:actor.mounts())if(m.part(mount.part).type==7)++rotorModels[i.name];
            for(const auto& weapon:actor.weapons())assert(weapon.currentMuzzle==1&&weapon.controller.state().fireLeft==0&&weapon.controller.state().fireTime==0&&weapon.aim.state().startTime==0);
            if(actor.state().attackable)++armed[i.kind];
            for(unsigned tick=0;tick<120;++tick)actor.tick({100+double(tick)*.016599999740719795,false,true},terrain,h);
            if(!h.emissions.empty())++fired[i.kind];
            assert(h.emissions.size()==h.sounds);for(const auto& emission:h.emissions)assert(emission.owner==&actor&&emission.hostile);
            if(i.kind==MachineKind::aircraft)assert(h.contacts==0);
            actor.state().hp=0;actor.setMachineState(2);actor.removePartsForCorpse();actor.retargetAfterDamage();
            const auto calls=h.calls;const auto shots=h.emissions.size();actor.tick({200,false,true},terrain,h);
            assert(h.calls==calls&&h.emissions.size()==shots&&actor.poses().size()==1&&actor.atlasPoses().size()==1);
            for(const auto& weapon:actor.weapons())assert(weapon.removed);
        }
    }
    assert(counts[MachineKind::vehicle]==385&&counts[MachineKind::turret]==307&&counts[MachineKind::aircraft]==3&&counts[MachineKind::bipod]==0&&counts[MachineKind::train]==0&&names.size()==54);
    assert(armed[MachineKind::turret]==fired[MachineKind::turret]);assert(armed[MachineKind::aircraft]==3&&fired[MachineKind::aircraft]==3);
    for(const auto& [id,count]:rotorModels)std::cout<<"Rotor model "<<id<<" instances="<<count<<"\n";
    std::cout<<"Original actors: vehicle385, turret307 (armed="<<armed[MachineKind::turret]<<",fired="<<fired[MachineKind::turret]<<"), aircraft3/fired3;54 models/43maps;corpse controller removal passed\n";
}
}
int main(int argc,char** argv){
    const auto m=models();const auto w=weapons();Map map;TerrainCollision terrain(map);
    const auto turret=info("FixtureTurret",MachineKind::turret),plane=info("FixturePlane",MachineKind::aircraft),vehicle=info("FixtureVehicle",MachineKind::vehicle);
    Host h;EnemyActor t(m,w,turret,{},100,h);assert(h.calls==3&&t.state().stopDelay==101&&t.state().liveTime==12);
    assert(t.weapons().size()==1&&t.weapons()[0].controller.state().fireTime==0&&t.weapons()[0].currentMuzzle==1);
    t.setDestination({500,500});t.tick({102,false,true},terrain,h);assert(t.motion().position.x==0&&t.motion().position.y==0&&h.contacts==1&&h.calls==4);near(pose(t.poses(),2).angle,.2f);
    t.tick({103,false,true},terrain,h);near(pose(t.poses(),2).angle,.4f); // Rotor does not inherit/reset to body rotation.
    const auto before=h.calls;t.tick({103,true,true},terrain,h);assert(h.calls==before+1&&h.contacts==2);near(pose(t.poses(),2).angle,.6f);
    t.state().hp=0;t.setMachineState(2);t.removePartsForCorpse();t.retargetAfterDamage();assert(t.state().targetPlayer&&t.state().moveToTarget);
    const auto deadCalls=h.calls;t.tick({200,false,true},terrain,h);assert(h.calls==deadCalls&&t.poses().size()==1);
    Host air;EnemyActor a(m,w,plane,{100,100},100,air);a.tick({100,false,false},terrain,air);
    assert(a.state().stopDelay==107.5f&&a.motion().destination.x==0&&a.motion().destination.y==0&&air.contacts==0&&air.calls==7);
    assert(a.motion().position.x!=100||a.motion().position.y!=100);near(a.motion().speed,.003f);
    const auto oldPos=a.motion().position;const auto oldCalls=air.calls;a.tick({100,true,false},terrain,air);assert(a.motion().position.x==oldPos.x&&a.motion().position.y==oldPos.y&&air.calls==oldCalls+1);
    a.retargetAfterDamage();assert(a.state().targetPlayer&&!a.state().moveToTarget);
    // A destination inside inclusive +/-4 stops without zeroing existing speed.
    a.setDestination({a.motion().position.x+4,a.motion().position.y-4});a.tick({101,false,false},terrain,air);assert(a.motion().state==0);near(a.motion().speed,.003f);
    const auto boundary=air.calls;a.tick({101,false,false},terrain,air);assert(a.motion().state==1&&air.calls>=boundary+3); // state0 retriggers AI even before deadline.
    // Hidden machine still executes enemy AI but suppresses child/controller tick.
    Host hidden;EnemyActor hiddenPlane(m,w,plane,{100,100},100,hidden);hiddenPlane.state().show=false;hiddenPlane.tick({100,false,false},terrain,hidden);assert(hidden.calls==6&&hiddenPlane.motion().position.x==100);
    Host deadlines;deadlines.hasPlayer=false;EnemyActor deadline(m,w,plane,{100,100},100,deadlines);
    deadline.tick({100,false,false},terrain,deadlines);const auto afterFirst=deadlines.calls;deadline.state().stopDelay=100;
    deadline.tick({100,false,false},terrain,deadlines);assert(deadlines.calls==afterFirst+1); // equality does not choose a new destination
    const auto equalCalls=deadlines.calls;deadline.tick({std::nextafter(100.0,101.0),false,false},terrain,deadlines);assert(deadlines.calls==equalCalls+4);
    const float expectedAngle=float(std::atan2(-100.0,-100.0))/30.f;
    Host arithmetic;arithmetic.hasPlayer=false;EnemyActor precise(m,w,plane,{100,100},100,arithmetic);precise.tick({100,false,false},terrain,arithmetic);
    assert(precise.motion().angle==expectedAngle&&precise.motion().speed==.3f/100.f);
    assert(precise.motion().position.x==100.f+std::cos(expectedAngle)*(.3f/100.f)&&precise.motion().position.y==100.f+std::sin(expectedAngle)*(.3f/100.f));
    precise.tick({113,false,false},terrain,arithmetic);assert(precise.state().hp==200&&precise.state().liveTime==12); // No guessed12sec despawn.
    Host paused;EnemyActor pausedTurret(m,w,turret,{},100,paused);pausedTurret.state().targetPlayer=true;
    pausedTurret.tick({100,true,true},terrain,paused);assert(paused.emissions.size()==1&&paused.contacts==0&&pausedTurret.weapons()[0].currentMuzzle==2);
    Host dead;EnemyActor notRemoved(m,w,turret,{},100,dead);notRemoved.state().hp=0;notRemoved.state().targetPlayer=true;notRemoved.state().weaponReady=true;notRemoved.setMachineState(2);
    notRemoved.tick({100,true,true},terrain,dead);assert(dead.emissions.size()==1); // Original common controller has no ownerHP guard.
    notRemoved.removePartsForCorpse();const auto deadEmits=dead.emissions.size();notRemoved.tick({200,true,true},terrain,dead);assert(dead.emissions.size()==deadEmits);
    Host offscreen;offscreen.hasPlayer=false;EnemyActor offscreenTurret(m,w,turret,{},100,offscreen);offscreenTurret.setDestination({500,500});offscreenTurret.tick({102,false,false},terrain,offscreen);assert(offscreen.contacts==0&&offscreenTurret.motion().position.x==0&&offscreen.calls==4);
    Host wrap;wrap.hasPlayer=false;wrap.values={1,0x40000000,0x40000000};EnemyActor wrapped(m,w,plane,{},100,wrap);wrapped.setDestination({1000,0});wrapped.tick({100,false,false},terrain,wrap);
    assert(wrapped.motion().angle<0&&wrapped.motion().position.y<0); // float pi is greater than the source double pi.
    Host outside;outside.hasPlayer=false;EnemyActor beyond(m,w,plane,{},100,outside);beyond.setDestination({std::nextafter(4.f,5.f),0});beyond.tick({100,false,false},terrain,outside);assert(beyond.motion().state==1&&beyond.motion().speed>0);
    Host fire;fire.values={0,0x40000000,0};EnemyActor armed(m,w,turret,{},100,fire);
    for(unsigned i=0;i<120;++i)armed.tick({100+double(i)*.016599999740719795,false,true},terrain,fire);
    assert(!fire.emissions.empty()&&fire.emissions.size()==fire.sounds&&armed.state().targetPlayer);
    assert(armed.atlasPoses().size()==armed.poses().size());
    Host delegate;EnemyActor delegated(m,w,vehicle,{},0,delegate);delegated.tick({2,false,true},terrain,delegate);assert(delegate.calls>3&&delegate.contacts==1); // Existing vehicle consumer is active.
    assert(!EnemyActor::supports(MachineKind::train)&&!EnemyActor::supports(MachineKind::bipod));
    auto unsupported=turret;unsupported.kind=MachineKind::train;bool rejected=false;try{EnemyActor bad(m,w,unsupported,{},0,h);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
    if(argc>1)original(argv[1]);
}

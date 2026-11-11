#include "PreparedTextStub.hpp"
#include "Core/Simulation/CombatScene.hpp"
#include "Core/Simulation/DarwinWorldRandom.hpp"
#include "Core/Presentation/VisualLayer.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
using namespace hm2;
struct Files:ResourceAccess {
    std::filesystem::path root;unsigned reads=0;mutable unsigned probes=0;
    explicit Files(std::filesystem::path p):root(std::move(p)){}
    Bytes read(std::string_view name)override{++reads;std::ifstream in(root/std::string(name),std::ios::binary);if(!in)throw std::runtime_error("Missing test asset "+std::string(name));return Bytes(std::istreambuf_iterator<char>(in),{});}
    bool exists(std::string_view name)const override{++probes;return std::filesystem::exists(root/std::string(name));}
};
struct Hooks:CombatSceneHooks {
    ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest& r)override{return test::preparedTextStub(r);}
    CombatScene* scene=nullptr;int bonus=0,reduction=0,maxHP=256;
    unsigned drops=0,rewards=0,deaths=0,playerHits=0,itemTicks=0;
    std::vector<std::string> events;std::vector<unsigned> sounds;
    void sound(std::string_view name,Vec2)override{events.emplace_back(name);}
    void soundID(unsigned id,Vec2)override{sounds.push_back(id);}
    void dropEnemyLoot(const MachineInfo&,Vec2 position,double,float,WorldRandom& random)override{
        ++drops;events.push_back("drop");
        if(scene)for(const auto& e:scene->enemies())if(e.position.x==position.x&&e.position.y==position.y){assert(e.hp==0&&e.corpse&&e.activeParts>1);assert(scene->selectedEnemy()!=e.id);}
        (void)random.random();
    }
    void rewardEnemyKill(const MachineInfo&,Vec2 position,double,float,WorldRandom& random)override{
        ++rewards;events.push_back("reward");assert(drops==rewards);
        if(scene)for(const auto& e:scene->enemies())if(e.position.x==position.x&&e.position.y==position.y)assert(e.hp==0&&e.activeParts==1);
        (void)random.random();
    }
    int playerDamage(int amount,WorldRandom&)override{++playerHits;return std::max(0,amount-reduction);}
    int damageBonus()const override{return bonus;}
    int playerMaximumHP()const override{return maxHP;}
    void playerDied()override{++deaths;}
    void tickItems(double,float,Vec2,WorldRandom&)override{++itemTicks;}
};
static std::uint64_t firstLiving(const CombatScene& scene){
    const auto enemies=scene.enemies();for(const auto& e:enemies)if(e.hp>0)return e.id;return 0;
}
int main(int argc,char** argv){
    if(argc<2){std::cout<<"CombatScene requires original prepared HD assets (not an embedded substitute).\n";return 77;}
    Files files(argv[1]);Hooks hooks;DarwinWorldRandom shared(1);CombatSetup setup;setup.random=&shared;
    CombatScene scene(files,hooks,setup);hooks.scene=&scene;
    assert(scene.playerHP()==256);assert(scene.player().position.x==-275&&scene.player().position.y==-462);
    assert(scene.spawnerCount()==16);assert(scene.enemies().empty());assert(scene.selectedEnemyHpRate()==0);assert(scene.metrics().randomCalls==64&&shared.calls()==64);
    const auto preparation=scene.preparationFrame();assert(preparation.width==768&&preparation.height==1024&&!preparation.sprites.empty());
    const auto reads=files.reads,probes=files.probes;
    scene.tick();assert(!scene.enemies().empty());assert(scene.metrics().enemyTicks==0); // spawned behind listEnemy's slot
    const auto spawned=scene.enemies().size();assert(spawned==16);assert(scene.metrics().randomCalls==64+spawned*3);
    scene.tick();assert(scene.metrics().enemyTicks==spawned);
    const auto callsBeforeBodyDraw=scene.metrics().randomCalls;const auto partFrame=scene.drawFrame();bool directPlayer=false;
    for(const auto& sprite:partFrame.sprites)if(sprite.image=="P_Body_01.png"){
        assert(!sprite.atlasBatch&&sprite.strip.size()==4);directPlayer=true;
        assert(sprite.strip[0].uv.x==0&&sprite.strip[0].uv.y==0&&sprite.strip[3].uv.x==1&&sprite.strip[3].uv.y==1);
        assert(sprite.blend.source==BlendFactor::one&&sprite.blend.destination==BlendFactor::oneMinusSourceAlpha);
        for(const auto& v:sprite.strip)assert(v.color.r==1&&v.color.g==1&&v.color.b==1&&v.color.a==1);
    }
    assert(directPlayer&&scene.metrics().randomCalls==callsBeforeBodyDraw);
    const auto id=firstLiving(scene);assert(id&&scene.selectEnemy(id));
    const auto first=scene.enemies().front();const auto p=scene.camera().toScreen(first.position);
    assert(scene.input(PointerPhase::begin,p,9));assert(scene.selectedEnemy()==first.id);assert(scene.input(PointerPhase::end,p,9));
    // Long battle uses actual shared data/controllers, with a defensive hook so
    // the scene can exercise many reload and corpse cycles in one run.
    hooks.reduction=10000;
    const auto before=scene.metrics();
    for(unsigned i=0;i<1800;++i){
        if(!scene.selectedEnemy())if(const auto next=firstLiving(scene))scene.selectEnemy(next);
        scene.tick();const auto frame=scene.drawFrame();assert(frame.width==768&&frame.height==1024);
    }
    std::cout<<"spawns="<<scene.enemies().size()<<" initial="<<spawned<<" playerShots="<<scene.metrics().playerEmissions<<" enemyShots="<<scene.metrics().enemyEmissions<<" kills="<<scene.metrics().kills<<" hits="<<hooks.playerHits<<"\n";
    assert(scene.metrics().playerEmissions>before.playerEmissions);
    assert(scene.metrics().projectilesCreated>0);assert(scene.metrics().damageCalls>0);assert(scene.metrics().particleAllocations>0);
    assert(hooks.drops>0&&hooks.rewards==hooks.drops&&scene.metrics().kills==hooks.rewards);
    assert(hooks.playerHits>0&&scene.metrics().enemyEmissions>0);
    assert(files.reads==reads&&files.probes==probes);assert(scene.metrics().randomCalls==shared.calls());
    assert(std::find(hooks.events.begin(),hooks.events.end(),"Wpn_Gun01.wav")!=hooks.events.end());
    assert(std::find(hooks.events.begin(),hooks.events.end(),"Wpn_Missile01.wav")!=hooks.events.end());
    const auto t=scene.clock().worldTime();const auto r=scene.clock().renderTime();const auto tick=scene.clock().ticks();
    scene.setPaused(true);for(unsigned i=0;i<10;++i){scene.tick();scene.drawFrame();}
    assert(scene.clock().worldTime()==t&&scene.clock().renderTime()>r&&scene.clock().ticks()==tick);
    const auto pausedFrames=scene.clock().frames(),rng=scene.metrics().randomCalls,draws=scene.metrics().drawCalls;
    scene.setActive(false);for(unsigned i=0;i<10;++i)scene.tick();
    assert(scene.clock().frames()==pausedFrames&&scene.metrics().randomCalls==rng&&scene.metrics().drawCalls==draws);
    // Fixed original-domain RNG avoids first-attack enemies. Player's first
    // ML emission occurs after pool tick: flash is absent from that frame's
    // atlas and first appears on the following tick. New bullet ticks at once.
    struct Fixed:WorldRandom {std::uint64_t calls=0;std::uint32_t random()override{++calls;return 0x40000000;}} fixed;
    Files orderFiles(argv[1]);Hooks orderHooks;CombatSetup orderSetup;orderSetup.random=&fixed;orderSetup.initialRenderTime=1000;
    CombatScene order(orderFiles,orderHooks,orderSetup);assert(order.clock().renderTime()==1000&&order.clock().worldTime()==0);order.tick();assert(order.clock().renderTime()==1000.f+GameClock::increment);assert(order.selectEnemy(firstLiving(order)));
    order.tick();const auto selectedFrame=order.drawFrame();
    assert(!selectedFrame.sprites.empty()&&selectedFrame.sprites.back().image=="Target.png");
    const auto& target=selectedFrame.sprites.back();assert(!target.atlasBatch&&target.strip.size()==4);
    assert(target.blend.source==BlendFactor::sourceAlpha&&target.blend.destination==BlendFactor::one);
    assert(target.strip[0].uv.x==.5f&&target.strip[2].uv.x==1.f);
    assert(target.color.a>.98f&&target.color.a<.99f);
    bool emitted=false;
    const auto fireQuads=[](const Frame& frame){std::size_t count=0;for(const auto& s:frame.sprites)if(s.image=="Efx_Fire.png"){if(s.atlasBatch)count+=s.atlasBatch->quads.size();else ++count;}return count;};
    for(unsigned i=0;i<200&&!emitted;++i){
        order.tick();auto frame=order.drawFrame();
        if(order.metrics().playerEmissions){
            assert(order.metrics().playerEmissions==1&&order.metrics().enemyEmissions==0);
            assert(order.clock().frames()==121&&order.clock().worldTime()==double(GameClock::increment)*121);
            assert(order.metrics().projectileTicks==1&&order.metrics().projectilesCreated==1);
            assert(order.visibleParticles()==1&&fireQuads(frame)==0);assert(order.metrics().particleAllocations==1);
            assert(std::count(orderHooks.events.begin(),orderHooks.events.end(),"Wpn_Missile01.wav")==2);
            order.tick();frame=order.drawFrame();assert(fireQuads(frame)==1&&order.metrics().projectileTicks==2);emitted=true;
        }
    }
    assert(emitted&&order.metrics().randomCalls==fixed.calls);
    const auto beforePauseShots=order.metrics().emissions;const auto pauseWorld=order.clock().worldTime();
    order.setPaused(true);for(unsigned i=0;i<20;++i){order.tick();order.drawFrame();}
    assert(order.clock().worldTime()==pauseWorld&&order.visibleParticles()==0&&order.metrics().emissions==beforePauseShots);
    assert(!order.input(PointerPhase::begin,{384,512},44));
    // Raw HP writer has no synthetic damage/crash or reward side effects.
    order.setPlayerHP(123);assert(order.playerHP()==123&&orderHooks.playerHits==0&&orderHooks.deaths==0);
    order.setPaused(false);bool terminalAtlas=false;
    const auto missileQuads=[](const Frame& frame){std::size_t count=0;for(const auto& s:frame.sprites)if(s.image=="Blt_Missile01.png")count+=s.atlasBatch?s.atlasBatch->quads.size():1;return count;};
    for(unsigned i=0;i<100&&!terminalAtlas;++i){
        const auto removed=order.metrics().projectilesRemoved;order.tick();const auto frame=order.drawFrame();
        if(order.metrics().projectilesRemoved>removed){
            assert(missileQuads(frame)>=1); // Parent was removed, child still ticked.
            if(order.activeProjectiles()==0){
                const auto created=order.metrics().projectilesCreated;order.tick();const auto next=order.drawFrame();
                if(order.metrics().projectilesCreated==created)assert(missileQuads(next)==0);
            }
            terminalAtlas=true;
        }
    }
    assert(terminalAtlas);
    // Inventory.setParts replaces only the selected original Player weapon.
    // Use the actual spare10111 on the left, leaving the loaded right ML alone.
    Files swapFiles(argv[1]);Hooks swapHooks;swapHooks.reduction=10000;Fixed swapRandom;
    CombatSetup swapSetup;swapSetup.random=&swapRandom;CombatScene swapped(swapFiles,swapHooks,swapSetup);
    const auto preparedSwap=swapped.preparationFrame();
    assert(std::any_of(preparedSwap.sprites.begin(),preparedSwap.sprites.end(),[](const auto& sprite){return sprite.image=="P_Wpn_Gun_201.png";}));
    const auto swapReads=swapFiles.reads,swapProbes=swapFiles.probes;
    swapped.tick();assert(swapped.selectEnemy(firstLiving(swapped)));assert(swapped.selectedEnemyHpRate()==1);
    const auto weaponAt=[](const CombatScene& scene,unsigned slot){
        const auto weapons=scene.playerWeapons();const auto found=std::find_if(weapons.begin(),weapons.end(),[&](const auto& weapon){return weapon.inventoryEquipSlot==slot;});
        assert(found!=weapons.end());return *found;
    };
    while(swapped.metrics().playerEmissions==0){swapped.tick();swapped.drawFrame();assert(swapped.clock().frames()<200);}
    const auto rightBefore=weaponAt(swapped,3);assert(rightBefore.fireLeft==1&&rightBefore.fireBound==-4.5f&&rightBefore.addPosition.x==-4.5f);
    EquipmentCodes replacements=PlayerEquipment::startingCodes;replacements[2]="10111";
    // Only the specified slot is consumed, rather than rebuilding unrelated
    // equipment/controllers from a potentially newer inventory snapshot.
    replacements[0]="unused-other-slot";
    const float swapTime=float(swapped.clock().worldTime());const auto swapCalls=swapped.metrics().randomCalls;
    swapped.equipWeapon(replacements,2);
    assert(swapped.metrics().randomCalls==swapCalls&&swapFiles.reads==swapReads&&swapFiles.probes==swapProbes);
    const auto rightAfter=weaponAt(swapped,3),leftNew=weaponAt(swapped,2);
    assert(rightAfter.part==rightBefore.part&&rightAfter.fireLeft==rightBefore.fireLeft&&rightAfter.fireTime==rightBefore.fireTime&&rightAfter.fireBound==rightBefore.fireBound&&rightAfter.currentMuzzle==rightBefore.currentMuzzle);
    assert(rightAfter.addPosition.x==rightBefore.addPosition.x&&rightAfter.addPosition.y==rightBefore.addPosition.y);
    assert(rightAfter.insertionIndex==4&&leftNew.insertionIndex==5);
    assert(leftNew.part=="P_Wpn_Msl_101"&&leftNew.fireTime==swapTime&&leftNew.fireLeft==0&&leftNew.fireBound==0&&leftNew.currentMuzzle==1);
    assert(leftNew.parentSocket==1&&!leftNew.reverseY&&rightAfter.parentSocket==2&&rightAfter.reverseY);
    swapped.tick();const auto swapFrame=swapped.drawFrame();
    std::size_t missileParts=0;bool oldGun=false,orderedMirrors=false;std::vector<float> mirrorWinding;
    for(const auto& sprite:swapFrame.sprites){
        oldGun=oldGun||sprite.image=="P_Wpn_Gun_101.png";
        if(sprite.image=="P_Wpn_Msl_101.png"){
            assert(!sprite.atlasBatch&&sprite.strip.size()==4);++missileParts;
            const auto a=sprite.strip[0].position,b=sprite.strip[1].position,c=sprite.strip[2].position;
            mirrorWinding.push_back((b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x));
        }
    }
    orderedMirrors=mirrorWinding.size()==2&&mirrorWinding[0]<0&&mirrorWinding[1]>0;
    assert(!oldGun&&missileParts==2&&orderedMirrors);
    bool rightContinued=false,leftFired=false;
    for(unsigned i=0;i<200&&!leftFired;++i){
        swapped.tick();swapped.drawFrame();const auto right=weaponAt(swapped,3),left=weaponAt(swapped,2);
        if(right.fireTime!=rightBefore.fireTime)rightContinued=true;
        if(float(swapped.clock().worldTime()-double(swapTime))<2.f)assert(left.fireLeft==0&&left.fireTime==swapTime);
        if(left.fireTime!=swapTime){assert(float(swapped.clock().worldTime()-double(swapTime))>=2.f&&left.fireLeft==1);leftFired=true;}
    }
    assert(rightContinued&&leftFired); // Both actual ML controllers emitted.
    assert(swapped.selectedEnemyHpRate()>0&&swapped.selectedEnemyHpRate()<1);
    const auto leftPreserved=weaponAt(swapped,2);
    replacements=PlayerEquipment::startingCodes;replacements[2]="10111";replacements[3]="10011";
    swapped.equipWeapon(replacements,3);const auto leftAfterRightSwap=weaponAt(swapped,2),rightNew=weaponAt(swapped,3);
    assert(leftAfterRightSwap.insertionIndex==4&&rightNew.insertionIndex==5);
    assert(leftAfterRightSwap.fireLeft==leftPreserved.fireLeft&&leftAfterRightSwap.fireTime==leftPreserved.fireTime&&leftAfterRightSwap.fireBound==leftPreserved.fireBound&&leftAfterRightSwap.addPosition.x==leftPreserved.addPosition.x);
    assert(rightNew.parentSocket==2&&rightNew.reverseY&&rightNew.part=="P_Wpn_Gun_101"&&rightNew.fireLeft==0&&rightNew.currentMuzzle==1);
    swapped.tick();const auto rightSwapFrame=swapped.drawFrame();assert(std::any_of(rightSwapFrame.sprites.begin(),rightSwapFrame.sprites.end(),[](const auto& sprite){return sprite.image=="P_Wpn_Gun_101.png";}));
    bool invalidSlot=false;try{swapped.equipWeapon(replacements,0);}catch(const std::invalid_argument&){invalidSlot=true;}assert(invalidSlot);
    replacements[3]="";const auto beforeNil=weaponAt(swapped,3);swapped.equipWeapon(replacements,3);assert(weaponAt(swapped,3).fireTime==beforeNil.fireTime);
    assert(swapFiles.reads==swapReads&&swapFiles.probes==swapProbes&&swapped.metrics().randomCalls==swapRandom.calls);
    // Adding a previously empty left slot shifts the canonical right index,
    // while the right part's original insertion/controller state survives.
    Files emptyFiles(argv[1]);Hooks emptyHooks;CombatSetup emptySetup;emptySetup.equipment[2]="";
    CombatScene emptyLeft(emptyFiles,emptyHooks,emptySetup);const auto emptyReads=emptyFiles.reads,emptyProbes=emptyFiles.probes;
    const auto rightOnly=weaponAt(emptyLeft,3);replacements=PlayerEquipment::startingCodes;replacements[2]="10111";
    emptyLeft.equipWeapon(replacements,2);assert(weaponAt(emptyLeft,3).insertionIndex==rightOnly.insertionIndex&&weaponAt(emptyLeft,2).insertionIndex>rightOnly.insertionIndex);
    emptyLeft.tick();emptyLeft.drawFrame();assert(emptyFiles.reads==emptyReads&&emptyFiles.probes==emptyProbes);
    assert(!emptyLeft.isSave());emptyLeft.setIsSave(true);emptyLeft.setPlayerHP(0);assert(emptyLeft.isSave());
    emptyLeft.setIsSave(false);emptyLeft.setPlayerHP(256);assert(!emptyLeft.isSave());
    const auto noticeStart=emptyLeft.clock().worldTime();
    emptyLeft.beginPickupNotice();
    emptyLeft.setPickupNoticeText("15 cr",noticeStart);
    emptyLeft.tick();
    const auto noticeSprite=[&]{
        const auto frame=emptyLeft.drawFrame();
        const auto found=std::find_if(frame.sprites.begin(),frame.sprites.end(),[](const Sprite& sprite){return sprite.image=="PickItem.png";});
        return found==frame.sprites.end()?std::optional<Sprite>{}:std::optional<Sprite>(*found);
    };
    const auto bounced=noticeSprite();assert(bounced);
    const auto height=[&](const Sprite& sprite){return sprite.center.y-emptyLeft.camera().toScreen(emptyLeft.player().position).y;};
    assert(height(*bounced)>50.f);
    emptyLeft.setPaused(true);for(unsigned i=0;i<10;++i)emptyLeft.tick();
    const auto frozen=noticeSprite();assert(frozen&&std::abs(height(*frozen)-height(*bounced))<.001f);
    emptyLeft.setPaused(false);
    while(emptyLeft.clock().worldTime()-noticeStart<=2.51)emptyLeft.tick();
    const auto shrinking=noticeSprite();assert(shrinking&&shrinking->strip.size()==4);
    assert(shrinking->strip[1].position.y-shrinking->strip[0].position.y<32.f);
    for(unsigned i=0;i<11;++i)emptyLeft.tick();
    assert(!noticeSprite()&&emptyFiles.reads==emptyReads&&emptyFiles.probes==emptyProbes);
    std::cout<<"Field swap: spare10111 left, append order, preserved right controller/recoil, both emissions and warm IO passed\n";
    // A real untreated player can take damage. This separate shared stream
    // avoids contaminating the long battle's deterministic comparison.
    Files files2(argv[1]);Hooks vulnerable;CombatScene normal(files2,vulnerable);
    normal.tick();normal.selectEnemy(firstLiving(normal));bool hurt=false;
    for(unsigned i=0;i<1800&&normal.playerHP()>0;++i){normal.tick();normal.drawFrame();hurt= hurt||normal.playerHP()<256;}
    assert(hurt&&vulnerable.playerHits>0);
    std::cout<<"Stage01_01: runtime spawners, both equipped weapons, retaliation, HP and synchronous death hooks passed\n";
}

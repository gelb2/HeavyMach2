#include "Core/Simulation/ProjectileVisual.hpp"
#include "Core/Presentation/PreparedTiles.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

using namespace hm2;
namespace {
void near(float a,float b){assert(std::abs(a-b)<.00002f);}
template<class F>void fails(F f){bool failed=false;try{f();}catch(const std::runtime_error&){failed=true;}assert(failed);}
struct Random:WorldRandom {
    unsigned reads=0;std::vector<std::uint32_t> values;
    std::uint32_t random()override{
        const auto result=values.empty()?0x40000000u:values.at(reads);++reads;return result;
    }
    void reset(){reads=0;values.clear();}
};
struct Pool {
    Random rng;ParticlePool particles;
    std::vector<Particle*> occupied;
    Pool():particles(0,rng){rng.reset();}
    void free(unsigned slots){for(unsigned i=0;i<64-slots;++i){auto* p=particles.allocate(0,rng);assert(p);p->start(0);occupied.push_back(p);}rng.reset();}
};
}
int main(){
    TileImage bullet{"Blt_Canon01.png",{32,8},1,1,{}};
    TileImage flame{"Efx_Flame.png",{24,12},1,1,{}};
    WeaponInfo info;info.kind=WeaponKind::bullet;info.effect=WeaponEffect::flame;
    ProjectileState motion;motion.position={100,200};motion.destination={200,200};motion.shotLengthSquared=10000;motion.angle=.5f;
    MapCamera camera;camera.center={100,200};camera.viewport={200,100};PreparedTiles resources("test-common-cache");
    ProjectileVisual visual(info,&bullet,&flame,{},true);
    assert(visual.state().effectDelay==.06f&&visual.state().effectRegenTime==0&&visual.hasEffectChild());
    assert(visual.state().bulletAngle==0&&visual.state().effectVisible&&!visual.state().drop);
    visual.tick(motion);assert(!visual.state().effectVisible);near(visual.state().bulletAngle,.5f);
    VisualLayer layer;visual.collect(layer,camera,motion,{true,true},resources);
    auto first=layer.drain();assert(first.nonAtlas.empty()&&first.atlases.size()==1&&first.atlases[0].image==bullet.name);
    assert(first.atlases[0].tileIdentity==resources.visual(bullet).identity);
    visual.tick(motion,true);assert(!visual.state().effectVisible);
    visual.tick(motion);assert(visual.state().effectVisible);
    visual.collect(layer,camera,motion,{true,true},resources);auto second=layer.drain();
    // Bullet's first-frame atlas stays before the later-created Flame atlas.
    assert(second.atlases.size()==2&&second.atlases[0].image==bullet.name&&second.atlases[1].image==flame.name);
    assert(second.atlases[1].blend.source==BlendFactor::sourceAlpha&&second.atlases[1].blend.destination==BlendFactor::one);
    VisualLayer fresh;visual.collect(fresh,camera,motion,{true,true},resources);auto childOrder=fresh.drain();
    assert(childOrder.atlases[0].image==flame.name&&childOrder.atlases[1].image==bullet.name);
    visual.collect(fresh,camera,motion,{false,true},resources);assert(fresh.drain().atlases.size()==1);
    visual.collect(fresh,camera,motion,{true,false},resources);assert(fresh.drain().atlases.size()==1);
    visual.collect(fresh,camera,motion,{false,false},resources);assert(fresh.drain().atlases.empty());
    auto hidden=motion;hidden.show=false;const bool before=visual.state().effectVisible;
    visual.tick(hidden);assert(visual.state().effectVisible==before);
    visual.collect(fresh,camera,hidden,{true,true},resources);assert(fresh.drain().atlases.empty());
    auto removed=motion;removed.removed=true;visual.collect(fresh,camera,removed,{true,true},resources);assert(fresh.drain().atlases.empty());
    // A nil effect tile still has an actual original child/show toggle; no art
    // substitute is added. A nil bullet tile creates no bullet child.
    ProjectileVisual nil(info,nullptr,nullptr,{});assert(nil.hasEffectChild());nil.tick(motion);assert(!nil.state().effectVisible);
    nil.collect(fresh,camera,motion,{true,true},resources);assert(fresh.drain().atlases.empty());
    info.effect=static_cast<WeaponEffect>(1);ProjectileVisual one(info,nullptr,&flame,{});
    one.tick(motion);assert(one.state().effectVisible);one.collect(fresh,camera,motion,{true,true},resources);assert(fresh.drain().atlases.size()==1);

    // Original drop uses squared remaining distance / squared shot distance,
    // without changing velocity/gravity. Removed objects skip this scale write.
    info.effect=WeaponEffect::none;ProjectileVisual drop(info,&bullet,nullptr,{});drop.setDrop(true);
    auto falling=motion;falling.position={150,200};drop.tick(falling);
    near(drop.state().bulletScale.x,.55f);near(drop.state().bulletScale.y,.775f);
    const auto oldScale=drop.state().bulletScale;falling.removed=true;falling.position={175,200};drop.tick(falling);
    assert(drop.state().bulletScale.x==oldScale.x&&drop.state().bulletScale.y==oldScale.y);
    // Prepared origin/Retina/format must reach atlas geometry without a second
    // per-projectile texture loader or a hand-built replacement resource.
    bullet.origin={2,3};bullet.forRetina=true;bullet.pixelFormat=3;
    ProjectileVisual metadata(info,&bullet,nullptr,{});auto rotated=motion;rotated.angle=std::numbers::pi_v<float>*.5f;
    VisualLayer metadataLayer;metadata.tick(rotated);metadata.collect(metadataLayer,camera,rotated,{false,true},resources);
    auto meta=metadataLayer.drain();assert(meta.atlases.size()==1&&meta.atlases[0].pixelFormat==3);
    assert(meta.atlases[0].blend.source==BlendFactor::sourceAlpha);
    near(meta.atlases[0].quads[0].vertices[0].position.x,96.5f);near(meta.atlases[0].quads[0].vertices[0].position.y,43);

    // Trails are updated at parent draw time, after tick/child collection.
    TileImage trailTile{"Efx_Trail01.png",{16,8},1,1,{}};Random trailRandom;
    auto trail=std::make_shared<Trail>(&trailTile,Vec2{0,0},TrailConfig{4,16,64,.2f,.5f});
    trail->state().blendType=2;trail->state().wind=trail->state().diffuse=0;trail->add({0,0},0,trailRandom);
    info.effect=WeaponEffect::trail;ProjectileVisual trailVisual(info,nullptr,&trailTile,trail);
    ProjectileState flying;flying.position={40,0};flying.destination={100,0};flying.shotLengthSquared=10000;
    trailVisual.tick(flying);assert(trail->lastPosition()==1);
    Frame earlier;trail->append(earlier,camera);assert(earlier.sprites.empty());
    Pool pool;assert(!trailVisual.draw(flying,.1,17.5f,pool.particles,trailRandom));
    assert(trail->lastPosition()==3&&trailRandom.reads==0);
    Frame later;trail->append(later,camera);assert(later.sprites.size()==1&&later.sprites[0].strip.size()==8);
    // No paused argument: original draw continues to add the current endpoint
    // even when tick/worldTime are frozen. Terminal data is rewritten.
    flying.position={45,0};trailVisual.tick(flying,true);
    trailVisual.draw(flying,.1,18,pool.particles,trailRandom);near(trail->rows().back().point.position.x,45);
    const auto rows=trail->rows().size();flying.removed=true;
    trailVisual.draw(flying,.2,18,pool.particles,trailRandom);assert(trail->rows().size()==rows);
    auto bend=std::make_shared<Trail>(&trailTile,Vec2{},TrailConfig{1,64,16,0,.2f});
    bend->state().wind=bend->state().diffuse=0;bend->add({},0,trailRandom);bend->add({64,0},0,trailRandom);
    ProjectileVisual bentVisual(info,nullptr,&trailTile,bend);ProjectileState bent;bent.position={64,-65};
    const auto beforeDraw=trailRandom.reads;bentVisual.tick(bent);
    assert(trailRandom.reads==beforeDraw);bentVisual.draw(bent,.1,18,pool.particles,trailRandom);
    assert(trailRandom.reads==beforeDraw+1); // Join RNG remains in draw, not tick.

    // Effect3 (not present in current WeaponInfo table) still preserves the
    // original draw consumer, exact RNG/reset order and hostile constants.
    for(bool hostile:{false,true}){
        Pool fixture;fixture.free(1);WeaponInfo smoke;smoke.kind=WeaponKind::bullet;smoke.effect=static_cast<WeaponEffect>(3);
        ProjectileVisual regen(smoke,nullptr,&flame,{},hostile);regen.setParentTileNumber(3);
        ProjectileState state;state.toPlayer=hostile;state.position={25,50};
        const double deadline=double(regen.state().effectDelay);
        assert(!regen.draw(state,deadline,10,fixture.particles,fixture.rng)&&fixture.rng.reads==0);
        fixture.rng.values={0x10000000u,0x20000000u,0x40000000u,0x60000000u,0x7fffffffu};
        auto* p=regen.draw(state,std::nextafter(deadline,std::numeric_limits<double>::infinity()),17.5f,fixture.particles,fixture.rng);
        assert(p&&fixture.rng.reads==5&&p==&fixture.particles.particles()[63]);
        const auto& s=p->state();assert(s.tile==&flame&&s.tileNumber==3&&s.show&&s.startTime==17.5f&&s.blendType==2&&s.fadeOut);
        near(s.life,hostile?.5f:.6f);near(s.scale.x,hostile?.15f:.3f);near(s.scale.y,s.scale.x);
        near(s.scaleVelocity,hostile?.75f:1.25f);near(s.angle,std::numbers::pi_v<float>*1.5f);near(s.rotationVelocity,hostile?1.5f:2.5f);
        const double nextDeadline=double(regen.state().effectRegenTime+regen.state().effectDelay);
        assert(!regen.draw(state,nextDeadline,18,fixture.particles,fixture.rng)&&fixture.rng.reads==5);
    }
    Pool full;full.free(0);WeaponInfo smoke;smoke.effect=static_cast<WeaponEffect>(3);
    ProjectileVisual blocked(smoke,nullptr,nullptr,{});ProjectileState state;
    assert(!blocked.draw(state,1,17,full.particles,full.rng)&&full.rng.reads==0&&blocked.state().effectRegenTime==1);
    // Allocation failure advances the deadline before a slot later becomes free.
    full.occupied.front()->state().show=false;
    assert(!blocked.draw(state,1,18,full.particles,full.rng)&&full.rng.reads==0);
    auto* noTile=blocked.draw(state,2,18,full.particles,full.rng);assert(noTile&&noTile->state().tile==nullptr&&full.rng.reads==5);
    fails([&]{(void)blocked.draw(state,std::numeric_limits<double>::quiet_NaN(),19,full.particles,full.rng);});
}

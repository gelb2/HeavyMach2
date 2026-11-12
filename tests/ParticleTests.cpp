#include "Core/Simulation/Particles.hpp"
#include <cassert>
#include <cmath>
using namespace hm2;
struct Random:WorldRandom{unsigned calls=0;std::uint32_t value=0x40000000u;std::uint32_t random()override{++calls;return value;}};
void near(float a,float b){assert(std::abs(a-b)<.00001f);}
int main(){
    Random random;ParticlePool pool(0,random);assert(random.calls==64);
    auto* p=pool.allocate(0,random);assert(p==&pool.particles()[0]&&random.calls==65);near(p->state().waterValue,60);
    TileImage shell{"BulletShell.png",{20,8},2,1};p->state().tile=&shell;
    p->state().scale={.6f,.6f};p->state().scaleVelocity=-.004f;p->state().velocity={2,1};p->state().velocityAdd=-.05f;p->state().rotationVelocity=.4f;p->state().life=.5f;p->start(0);
    p->tick(.0166f);near(p->state().position.x,2);near(p->state().position.y,1);near(p->state().velocity.x,1.9f);near(p->state().scale.x,(.6f-.004f)-.004f);near(p->state().angle,.4f);
    Frame frame;MapCamera camera;camera.viewport={480,320};p->append(frame,camera);assert(frame.sprites.size()==1);assert(frame.sprites[0].blend.source==BlendFactor::sourceAlpha&&frame.sprites[0].blend.destination==BlendFactor::one);near(frame.sprites[0].size.width,10*p->state().scale.x);
    const auto scale=p->state().scale.x;p->tick(.5f);assert(!p->state().show);near(p->state().scale.x,scale-.004f);
    const auto stopped=p->state().position;p->tick(5);near(p->state().position.x,stopped.x);
    // Reset preserves fields that the original does not write.
    p->state().sinRotate=3;p->state().sinRotateValue=2;p->state().easyOutScale=4;
    auto* reuse=pool.allocate(10,random);assert(reuse==p&&reuse->state().tile==&shell);near(reuse->state().sinRotate,3);near(reuse->state().sinRotateValue,2);near(reuse->state().easyOutScale,4);assert(!reuse->state().show);
    // Animation disables alpha fading and Tile2D clamps its final rounded frame.
    p->state().sinRotate=0;p->state().life=.3f;p->animate(6);p->start(10);p->tick(10.29f);near(p->state().alpha,1);assert(p->state().tileNumber==6);
    frame.sprites.clear();p->append(frame,camera);assert(frame.sprites[0].tile==1);
    p->tick(10.3f);assert(!p->state().show);
    // Allocate/start every slot; full pool fails without consuming reset RNG.
    for(unsigned i=0;i<64;++i){auto* q=pool.allocate(20,random);assert(q);q->start(20);}
    const auto calls=random.calls;assert(!pool.allocate(20,random)&&random.calls==calls);
    random.value=0x7fffffffu;Particle max;max.reset(0,random);near(max.state().waterValue,70);
}

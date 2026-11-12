#include "Core/Simulation/Particles.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace hm2 {
void Particle::reset(float time,WorldRandom& random){
    // QobParticle::reset writes these properties, not a memset of the object.
    // In particular retain tile/show/sinRotate/sinRotateValue/easyOutScale.
    auto& s=state_;
    s.useAtlas=true;s.tileNumber=0;s.alpha=s.maxAlpha=1;s.position={};s.velocity={};s.gravity=0;
    s.scale={1,1};s.angle=0;s.radius=10;s.rotationVelocity=s.scaleVelocity=s.easyOutValue=s.velocityAdd=s.life=0;
    s.animate=false;s.animationFrames=0;s.startTime=time;s.blendType=2;s.groundPosition=-1000;
    s.waterValue=randomUnit(random)*20.f+50.f;s.water=false;s.fadeOut=true;s.scaleScale={1,1};s.selfRemove=false;
}
void Particle::tick(float time,float halfHeight){
    auto& s=state_;if(!s.show)return;
    Vec2 v{s.velocity.x,s.velocity.y+s.gravity*-.0166f};
    s.position.x+=v.x;s.position.y+=v.y;s.angle+=s.rotationVelocity;
    s.scale.x+=s.scaleVelocity;s.scale.y+=s.scaleVelocity;
    if(s.velocityAdd!=0){v.x+=v.x*s.velocityAdd;v.y+=v.y*s.velocityAdd;}
    s.velocity=v;
    if(s.life==0)return;
    const float age=time-s.startTime;
    if(age>=s.life){s.show=false;if(s.selfRemove)s.removed=true;return;}
    if(s.animate)s.tileNumber=int((age/s.life)*float(s.animationFrames)+.5f);
    if(s.fadeOut)s.alpha=(1.f-age/s.life)*s.maxAlpha;
    if(s.water){
        s.scaleScale.x=std::cos(age*s.waterValue)*.3f+1.f;
        s.scaleScale.y=std::sin((age*s.waterValue)*1.5f)*.3f+1.f;
        s.angle=std::sin(age*s.waterValue);
        if(s.position.y>halfHeight+40.f){s.show=false;if(s.selfRemove)s.removed=true;}
    }
    if(s.sinRotate!=0)s.angle=std::sin(age*s.sinRotate)*s.sinRotateValue;
    if(s.easyOutValue==0){s.scale.x+=s.scaleVelocity;s.scale.y+=s.scaleVelocity;}
    else{s.scale.x+=(s.easyOutScale-s.scale.x)/s.easyOutValue;s.scale.y+=(s.easyOutScale-s.scale.y)/s.easyOutValue;}
}
void Particle::append(Frame& frame,const MapCamera& camera)const{
    const auto& s=state_;if(!s.show||!s.tile)return;
    const auto& tile=*s.tile;
    if(!tile.columns||!tile.rows||s.tileNumber<0)throw std::runtime_error("Invalid original particle tile");
    Sprite sprite;sprite.image=tile.name;sprite.center=camera.toScreen(s.position);sprite.size={tile.pixels.width/float(tile.columns)*(s.scale.x*s.scaleScale.x),tile.pixels.height/float(tile.rows)*(s.scale.y*s.scaleScale.y)};
    sprite.columns=tile.columns;sprite.rows=tile.rows;sprite.tile=unsigned(std::min<std::uint64_t>(s.tileNumber,std::uint64_t(tile.columns)*tile.rows-1));sprite.angle=s.angle;sprite.color.a=s.alpha;
    switch(s.blendType){
        case 0:break; // Prepared RGBA8888 original Tile2D format1.
        case 1:sprite.blend.source=BlendFactor::sourceColor;break;
        case 2:sprite.blend={BlendFactor::sourceAlpha,BlendFactor::one};break;
        case 3:sprite.blend={BlendFactor::one,BlendFactor::one};break;
        case 4:sprite.blend.source=BlendFactor::sourceAlpha;break;
        default:throw std::runtime_error("Unresolved stateful original particle blend");
    }
    frame.sprites.push_back(std::move(sprite));
}
ParticlePool::ParticlePool(float time,WorldRandom& random){
    for(auto& p:particles_){p.state().show=true;p.reset(time,random);p.state().show=false;}
}
Particle* ParticlePool::allocate(float time,WorldRandom& random){
    for(auto& p:particles_)if(!p.state().show){
        if(p.state().removed)throw std::runtime_error("Removed original pool object cannot be reused");
        p.reset(time,random);++allocations_;return &p;
    }
    return nullptr;
}
void ParticlePool::tick(float time,float halfHeight){for(auto& p:particles_)p.tick(time,halfHeight);}
void ParticlePool::append(Frame& frame,const MapCamera& camera)const{for(const auto& p:particles_)p.append(frame,camera);}
}

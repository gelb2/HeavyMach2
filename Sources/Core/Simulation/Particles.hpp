#pragma once
#include "Core/Presentation/MapPreview.hpp"
#include "Core/Presentation/TileImage.hpp"
#include "Core/Simulation/WorldRandom.hpp"
#include <array>

namespace hm2 {
struct ParticleState {
    const TileImage* tile=nullptr;
    Vec2 position,velocity,scale,scaleScale;
    float gravity=0,angle=0,rotationVelocity=0,scaleVelocity=0,velocityAdd=0;
    float alpha=0,maxAlpha=0,life=0,startTime=0,radius=0,groundPosition=0;
    float waterValue=0,sinRotate=0,sinRotateValue=0,easyOutValue=0,easyOutScale=0;
    int tileNumber=0,animationFrames=0,blendType=0;
    bool show=false,useAtlas=false,animate=false,water=false,fadeOut=false,selfRemove=false,removed=false;
};
class Particle {
    ParticleState state_;
public:
    [[nodiscard]] ParticleState& state(){return state_;}
    [[nodiscard]] const ParticleState& state()const{return state_;}
    void reset(float renderTime,WorldRandom&);
    void start(float renderTime){state_.startTime=renderTime;state_.show=true;}
    void animate(int frames){state_.animate=true;state_.animationFrames=frames;state_.fadeOut=false;}
    // Original particle clock keeps running during an in-game world pause.
    // OS inactivity stops the render loop before this method is called.
    void tick(float renderTime,float cameraHalfHeight=0);
    void append(Frame&,const MapCamera&)const;
};
class ParticlePool {
    std::array<Particle,64> particles_;
    unsigned long allocations_=0;
public:
    ParticlePool(float renderTime,WorldRandom&);
    Particle* allocate(float renderTime,WorldRandom&);
    void tick(float renderTime,float cameraHalfHeight=0);
    void append(Frame&,const MapCamera&)const;
    [[nodiscard]] const auto& particles()const{return particles_;}
    [[nodiscard]] auto& particles(){return particles_;}
    [[nodiscard]] unsigned long allocations()const{return allocations_;}
};
}

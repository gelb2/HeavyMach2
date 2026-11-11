#include "Core/Simulation/AirStrike.hpp"
#include <bit>
#include <cmath>

namespace hm2 {
namespace {
Vec2 normalized(Vec2 v){
    // QVector.normalize0x3f0f4 uses float square/sum/sqrt and a strict1e-5 gate.
    const float length=std::sqrt(v.x*v.x+v.y*v.y);
    if(length<1e-5f)return {};
    return {v.x/length,v.y/length};
}
float unit(WorldRandom& random){return float(random.random())*0x1p-31f;}
}
AirStrike::AirStrike(const WeaponInfo* weapon,float delay,Vec2 target,Vec2 player,double worldTime,float renderTime,AirStrikeHost& host,WorldRandom& random)
:weapon_(weapon),plane_(host.preparedTile("Airplane.png")),host_(host),random_(random){
    state_.target=target;state_.fireDelay=delay;state_.attackCount=weapon?weapon->shotCount:0;
    auto direction=normalized({target.x-player.x,target.y-player.y});
    state_.position={direction.x*-1200.f+target.x,direction.y*-1200.f+target.y};
    state_.angle=float(std::atan2(double(direction.y),double(direction.x)));
    state_.scale=std::bit_cast<float>(0x3fa66666u);
    visibility_.visual=true;
    if(plane_)visibility_.bounds={-plane_->pixels.width*.5f,-plane_->pixels.height*.5f,plane_->pixels.width*.5f,plane_->pixels.height*.5f};
    else visibility_.bounds={0,0,0,0};
    if(auto* marker=host_.allocateParticle(renderTime)){
        auto* targetTile=host_.preparedTile("Target.png");if(targetTile){targetTile->columns=2;targetTile->rows=1;}
        auto& p=marker->state();p.blendType=2;p.life=3;p.tile=targetTile;p.tileNumber=0;p.position=target;
        p.scale={15,15};p.easyOutScale=1.5f;p.easyOutValue=6;p.rotationVelocity=.1f;marker->start(renderTime);
    }
    state_.velocity={direction.x*4.f,direction.y*4.f};host_.sound(9);state_.startTime=float(worldTime);
}
void AirStrike::tick(double worldTime,const MapCamera* camera){
    if(state_.removed)return;
    state_.position.x+=state_.velocity.x;state_.position.y+=state_.velocity.y;
    state_.velocity.x+=state_.velocity.x*.01f;state_.velocity.y+=state_.velocity.y*.01f;
    if(double(float(state_.startTime+state_.fireDelay))<worldTime&&state_.attackCount>0){
        if((random_.random()&3u)==0){
            constexpr float pi=std::bit_cast<float>(0x40490fdbu);
            float angle=unit(random_)*pi;angle+=angle;
            const float aim=weapon_->aimRate;
            const float first=(unit(random_)*aim)*120.f;
            const float second=(unit(random_)*aim)*120.f;
            const float radius=(first+second)+aim*-120.f;
            const Vec2 scatter{std::cos(angle)*radius,std::sin(angle)*radius};
            const Vec2 source{scatter.x*.3f+state_.position.x,scatter.y*.3f+state_.position.y};
            const Vec2 destination{scatter.x+state_.target.x,scatter.y+state_.target.y};
            host_.createBullet({weapon_,state_.angle,source,destination,false,true});
            ++bulletRequests_;--state_.attackCount;
        }
    }else if(double(float(state_.startTime+4.f))<worldTime)state_.removed=true;
    refreshVisibility(visibility_,state_.position,camera);
}
void AirStrike::collect(VisualLayer& layer,const MapCamera& camera,const PreparedTiles& tiles){
    if(state_.removed||!plane_)return;
    VisualImage image;image.tile=tiles.visual(*plane_);image.tileNo=0;image.blendType=0;
    image.screenPosition=camera.toScreen(state_.position);image.scale={state_.scale,state_.scale};image.angle=state_.angle;
    image.useAtlas=false;image.show=true;image.isInScreen=visibility_.inScreen;layer.submitImage(image);
}
}

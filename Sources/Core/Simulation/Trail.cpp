#include "Core/Simulation/Trail.hpp"
#include <cmath>
#include <stdexcept>

namespace hm2 {
namespace {
Vec2 minus(Vec2 a,Vec2 b){return {a.x-b.x,a.y-b.y};}
float squared(Vec2 v){return v.x*v.x+v.y*v.y;}
Vec2 scaledDirection(Vec2 v,float width){
    const float length=std::sqrt(squared(v));
    return {(v.x/length)*width,(v.y/length)*width};
}
void validTime(double time){
    if(!std::isfinite(time)||!std::isfinite(float(time)))throw std::runtime_error("Invalid original trail time");
}
}
Trail::Trail(const TileImage* tile,Vec2 position,TrailConfig config){
    state_.tile=tile;state_.position=position;configure(config);
}
void Trail::configure(TrailConfig config){
    if(!std::isfinite(config.width)||config.width<0||!std::isfinite(config.length)||config.length<=0||
       !config.blocks||!std::isfinite(config.fadeDelay)||config.fadeDelay<0||
       !std::isfinite(config.fadeTime)||config.fadeTime<0)throw std::runtime_error("Invalid original trail configuration");
    // The original allocated blockCount*20+1 bytes for points. Give every
    // committed/terminal record sufficient storage without changing its logic.
    const auto count=std::size_t(config.blocks)+2;
    if(count<config.blocks||count>rows_.max_size())throw std::length_error("Original trail storage overflow");
    rows_.assign(count,{});config_=config;last_=0;odd_=false;hasPoints_=false;
}
void Trail::updateVertices(std::size_t index){
    auto& r=rows_[index];
    r.vertices[0].position={r.point.position.x+r.point.normal.x,r.point.position.y+r.point.normal.y};
    r.vertices[1].position={r.point.position.x-r.point.normal.x,r.point.position.y-r.point.normal.y};
}
void Trail::addValid(Vec2 local,bool commit,double time,WorldRandom& random){
    if(last_>config_.blocks){
        for(std::size_t i=0;i<config_.blocks;++i)rows_[i]=rows_[i+1];
        --last_;
    }
    auto& row=rows_[last_];
    row.point.position=local;row.point.timestamp=float(time);
    // Content-normalized UVs: the renderer applies the original maxS/maxT.
    const float v=odd_?0.f:1.f;
    row.vertices[0].uv={0,v};row.vertices[1].uv={1,v};
    if(last_==0)row.point.normal={};
    else{
        const Vec2 delta=minus(local,rows_[last_-1].point.position);
        row.point.normal=scaledDirection({delta.y,-delta.x},config_.width);
        row.point.normal.x*=.5f;row.point.normal.y*=.5f;
        if(!commit||last_<2){
            // Original 0x1f43a writes record zero, including on a terminal
            // update. It does not select the preceding record here.
            rows_[0].point.normal=row.point.normal;updateVertices(0);
        }else{
            auto& previous=rows_[last_-1];
            const Vec2 back=scaledDirection(minus(rows_[last_-2].point.position,previous.point.position),config_.width);
            const Vec2 forward=scaledDirection(minus(local,previous.point.position),config_.width);
            const float cross=back.x*forward.y-back.y*forward.x;
            // BLE in 0x1f3a2 also skips unordered VFP comparisons.
            if(cross>.1f){
                const float joinWidth=config_.width+randomUnit(random)*6.f;
                const Vec2 sum{back.x+forward.x,back.y+forward.y};
                previous.point.normal=scaledDirection(sum,joinWidth);
                previous.point.normal.x*=.5f;previous.point.normal.y*=.5f;
                updateVertices(last_-1);
            }
        }
    }
    updateVertices(last_);
    row.vertices[0].color={};row.vertices[1].color={};
    hasPoints_=true;
    if(commit){++last_;odd_=!odd_;}
}
void Trail::add(Vec2 world,double time,WorldRandom& random){
    validTime(time);
    const Vec2 local=minus(world,state_.position);
    if(!std::isfinite(local.x)||!std::isfinite(local.y))throw std::runtime_error("Invalid original trail position");
    if(last_==0)addValid(local,true,time,random);
    else{
        Vec2 point=rows_[last_-1].point.position;
        const Vec2 delta=minus(local,point);
        const float distanceSquared=squared(delta);
        if(!std::isfinite(distanceSquared))throw std::runtime_error("Original trail distance overflow");
        if(distanceSquared>=config_.length*config_.length){
            // ARM uses double sqrt/division, then converts the count to float.
            float remaining=float(std::sqrt(double(distanceSquared))/double(config_.length));
            const Vec2 step{delta.x/remaining,delta.y/remaining};
            // Preserve the original float subtraction loop, not integer floor.
            while(remaining>=1.f){
                const float next=remaining-1.f;
                if(next==remaining)throw std::runtime_error("Original trail split count cannot progress");
                point.x+=step.x;point.y+=step.y;
                addValid(point,true,time,random);remaining=next;
            }
        }
    }
    addValid(local,false,time,random);
}
void Trail::tick(double time,bool paused,WorldRandom& random){
    if(paused||!hasPoints_)return;
    validTime(time);bool alive=false;
    for(std::size_t i=0;i<=last_;++i){
        auto& row=rows_[i];const float age=float(time-double(row.point.timestamp));
        if(age>config_.fadeDelay){
            const float sinceDelay=age-config_.fadeDelay;
            float alpha=sinceDelay>=config_.fadeTime?0.f:1.f-sinceDelay/config_.fadeTime;
            if(i==0)alpha=0;
            row.vertices[0].color.a=alpha;row.vertices[1].color.a=alpha;
            if(alpha==0)continue;
        }
        if(state_.diffuse!=0){
            const float amount=randomUnit(random)*state_.diffuse;
            const Vec2 delta{amount*row.point.normal.x,amount*row.point.normal.y};
            row.vertices[0].position.x+=delta.x;row.vertices[0].position.y+=delta.y;
            row.vertices[1].position.x-=delta.x;row.vertices[1].position.y-=delta.y;
        }
        if(state_.wind!=0){
            const float oscillation=std::sin(row.point.timestamp*30.f);
            const float randomWind=(randomUnit(random)*state_.wind)*.5f;
            const float amount=oscillation/(state_.wind+randomWind);
            const Vec2 delta{amount*row.point.normal.x,amount*row.point.normal.y};
            for(auto& vertex:row.vertices){vertex.position.x+=delta.x;vertex.position.y+=delta.y;}
        }
        alive=true;
    }
    if(last_>0&&!alive)state_.removed=true;
}
void Trail::append(Frame& frame,const MapCamera& camera)const{
    if(!state_.show||state_.removed||!state_.tile||last_<2)return;
    Sprite sprite;sprite.image=state_.tile->name;
    switch(state_.blendType){
        case 0:sprite.blend={BlendFactor::sourceAlpha,BlendFactor::oneMinusSourceAlpha};break;
        case 1:sprite.blend={BlendFactor::sourceColor,BlendFactor::oneMinusSourceAlpha};break;
        case 2:sprite.blend={BlendFactor::sourceAlpha,BlendFactor::one};break;
        case 3:sprite.blend={BlendFactor::oneMinusSourceAlpha,BlendFactor::oneMinusSourceAlpha};break;
        default:throw std::runtime_error("Unresolved stateful original trail blend");
    }
    const auto origin=camera.toScreen(state_.position);
    sprite.strip.reserve((last_+1)*2);
    for(std::size_t i=0;i<=last_;++i)for(auto vertex:rows_[i].vertices){
        vertex.position.x+=origin.x;vertex.position.y+=origin.y;
        sprite.strip.push_back(vertex);
    }
    frame.sprites.push_back(std::move(sprite));
}
}

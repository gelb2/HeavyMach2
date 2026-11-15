#include "Core/Presentation/CameraPresentation.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace hm2;
namespace {
bool near(float a,float b){return std::abs(a-b)<.00001f;}
void same(const CameraPresentationState& a,const CameraPresentationState& b){
    assert(a.policy==b.policy&&a.currentZoom==b.currentZoom&&a.targetZoom==b.targetZoom&&a.worldScale==b.worldScale);
    assert(a.viewport.width==b.viewport.width&&a.viewport.height==b.viewport.height);
    assert(a.worldToCanvas.scale==b.worldToCanvas.scale&&a.worldToCanvas.offset.x==b.worldToCanvas.offset.x&&a.worldToCanvas.offset.y==b.worldToCanvas.offset.y);
}
void regularIsolation(){
    for(const auto config:{DisplayConfiguration{},DisplayConfiguration::surface({768,1024}),DisplayConfiguration::surface({1024,1366},{0,20,0,24}),DisplayConfiguration::surface({1366,1024},{24,20,0,24})}){
        CameraPresentation tablet,phone;tablet.configure(config);phone.configure(DisplayConfiguration::surface({912,420},{68,20,68,0}));const auto baseline=tablet.snapshot();
        const float referenceScale=config.enabled?std::max(config.canvas.width/768.f,config.canvas.height/1024.f):1.f;
        assert(near(baseline.worldScale,referenceScale)&&baseline.currentZoom==1);
        const bool hd=std::round(config.canvas.width/referenceScale)==768&&std::round(config.canvas.height/referenceScale)==1024;
        assert(near(baseline.viewport.width,hd?768.f:config.canvas.width/referenceScale)&&near(baseline.viewport.height,hd?1024.f:config.canvas.height/referenceScale));
        if(hd){assert(baseline.policy==CameraPolicy::legacyHD);assert(near(baseline.worldToCanvas.offset.x,(config.canvas.width-768*referenceScale)*.5f)&&near(baseline.worldToCanvas.offset.y,(config.canvas.height-1024*referenceScale)*.5f));}
        else assert(baseline.worldToCanvas.offset.x==0&&baseline.worldToCanvas.offset.y==0);
        for(unsigned i=0;i<1200;++i){phone.advance({{0,0},i%2?std::optional<Vec2>{{720,320}}:std::nullopt,{}});tablet.advance({{0,0},Vec2{720,320},Vec2{-720,-320}});same(tablet.snapshot(),baseline);}
        tablet.configure(DisplayConfiguration::surface({912,420}));for(unsigned i=0;i<300;++i)tablet.advance({{},Vec2{720,0},{}});tablet.configure(config);same(tablet.snapshot(),baseline);
    }
}
void compactIdleAndFocus(){
    for(auto config:{DisplayConfiguration::surface({667,375}),DisplayConfiguration::surface({852,393},{59,21,59,0}),DisplayConfiguration::surface({912,420},{68,20,68,0}),DisplayConfiguration::surface({420,912},{0,34,0,62})}){
        CameraPresentation view;view.configure(config);const auto baseline=view.snapshot();assert(baseline.worldScale==1);
        assert(baseline.viewport.width==config.canvas.width&&baseline.viewport.height==config.canvas.height);
        for(unsigned i=0;i<1200;++i){view.advance({{},std::nullopt,std::nullopt});same(view.snapshot(),baseline);}
        float previous=1;for(unsigned i=0;i<1200;++i){view.advance({{},Vec2{720,320},Vec2{10,10}});const auto& s=view.snapshot();assert(s.worldScale<=previous&&s.worldScale>=s.minimumScale&&s.worldScale<=1);previous=s.worldScale;}
        const auto far=view.snapshot();assert(near(far.worldScale,std::max(.625f,std::max(config.canvas.width,config.canvas.height)/1022.f)));
        assert(far.viewport.width>baseline.viewport.width&&far.viewport.height>baseline.viewport.height);
        for(unsigned i=0;i<1200;++i){view.advance({{},Vec2{10,10},Vec2{720,320}});assert(view.snapshot().worldScale>=previous);previous=view.snapshot().worldScale;}
        assert(near(view.snapshot().worldScale,1));
        view.configure(config);for(unsigned i=0;i<1200;++i)view.advance({{},std::nullopt,Vec2{720,320}});assert(near(view.snapshot().worldScale,far.worldScale));
        const auto& t=view.snapshot().worldToCanvas;for(Vec2 p:{Vec2{0,0},Vec2{100,100},Vec2{512,1024}}){const auto result=t.inverse(t.map(p));assert(std::abs(result.x-p.x)<.0002&&std::abs(result.y-p.y)<.0002);}
    }
}
void wideSurfaceAndValidation(){
    CameraPresentation view;view.configure(DisplayConfiguration::surface({1280,360}));assert(!view.snapshot().edgeOverscan&&view.snapshot().fillSurface&&near(view.snapshot().worldScale,1280.f/1022));
    for(unsigned i=0;i<1200;++i)view.advance({{},Vec2{1000,0},{}});assert(near(view.snapshot().worldScale,1280.f/1022)&&near(view.snapshot().viewport.width,1022));
    view.configure(DisplayConfiguration::surface({912,420}));bool failed=false;try{view.advance({{},Vec2{std::numeric_limits<float>::quiet_NaN(),0},{}});}catch(const std::invalid_argument&){failed=true;}assert(failed);
    auto fixed=DisplayConfiguration::surface({912,420});fixed.focusZoom=false;view.configure(fixed);view.advance({{},Vec2{1000,0},{}});assert(view.snapshot().worldScale==1);
}
}
int main(){regularIsolation();compactIdleAndFocus();wideSurfaceAndValidation();std::cout<<"HM2_CAMERA_POLICY PASS legacy/regular parity; per-instance isolation; idle1200; target priority; path zoom; inverse input; rotation reset; finite source bounds without overscan\n";}

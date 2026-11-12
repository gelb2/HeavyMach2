#include "Core/Presentation/ScreenTransition.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace hm2 {
namespace {
constexpr std::array<std::string_view,5> images={"Loading_Left.png","Loading_Right.png",
    "Loading_Top.png","Loading_Bottom.png","Loading_Center.png"};
// HD clamps before the ease operation. A post-operation clamp shortens it.
void ease(float& current,float target,float divisor){
    if(current==target||std::abs(current-target)<=0.1f)current=target;
    else current+=(target-current)/divisor;
}
void sound(const ScreenTransitionCallbacks& host,std::string_view name){
    if(!host.playSound)throw std::logic_error("Screen transition needs its prepared sound host");
    host.playSound(name);
}
}
ScreenTransition::ScreenTransition(ResourceAccess& resources,int initialCurrentScreen){
    if(initialCurrentScreen<1||initialCurrentScreen>8)throw std::out_of_range("Invalid initial GameMain screen");
    state_.requestedScreen=state_.previousScreen=initialCurrentScreen;
    // GameMain.init writes phase8 after start, whose loading layer is hidden.
    for(const auto name:images)tiles_.prepare(resources,name);
}
bool ScreenTransition::request(int targetScreen){
    if(state_.phase<=6||targetScreen<1||targetScreen>8)return false;
    state_.previousScreen=state_.requestedScreen;state_.requestedScreen=targetScreen;state_.phase=0;return true;
}
void ScreenTransition::tick(const ScreenTransitionCallbacks& host){
    switch(state_.phase){
    case 0:
        state_.visible=true;
        state_.top={384,1408};state_.bottom={384,-384};
        state_.left={-256,512};state_.right={1024,512};
        state_.centerScale={1.5f,1.5f};state_.centerAngle=0;
        state_.phase=1;sound(host,soundResources[1]);break;
    case 1:
        state_.left.x+=(256.f-state_.left.x)/5.f;
        state_.right.x+=(512.f-state_.right.x)/5.f;
        if(state_.left.x>=200.f){state_.phase=2;sound(host,soundResources[1]);}break;
    case 2:
        ease(state_.top.y,640.f,6.f);ease(state_.bottom.y,384.f,6.f);
        ease(state_.left.x,256.f,5.f);ease(state_.right.x,512.f,5.f);
        if(state_.top.y==640.f){state_.bottom.y=384.f;state_.phase=3;}break;
    case 3:
        state_.centerScale.x+=(0.9f-state_.centerScale.x)/5.f;
        state_.centerScale.y=state_.centerScale.x;
        if(state_.centerScale.x<1.f){state_.centerScale={1,1};state_.phase=4;}break;
    case 4:
        if(!host.replaceScreen)throw std::logic_error("Screen transition needs a synchronous screen host");
        host.replaceScreen(state_.requestedScreen);
        state_.phase=5;sound(host,soundResources[0]);break;
    case 5:
        state_.centerScale.x+=(1.5f-state_.centerScale.x)/5.f;
        state_.centerScale.y=state_.centerScale.x;
        if(state_.centerScale.x>1.4f)state_.phase=7;break;
    case 6:
        // Preserved original branch. The verified phase5 path skips phase6;
        // no public phase setter or invented normal-path entry is provided.
        ease(state_.left.x,-256.f,10.f);ease(state_.right.x,1024.f,10.f);
        if(state_.left.x<0.f){state_.phase=7;sound(host,soundResources[0]);}break;
    case 7:
        state_.centerScale.x+=(1.5f-state_.centerScale.x)/5.f;
        state_.centerScale.y=state_.centerScale.x;
        ease(state_.top.y,1408.f,10.f);ease(state_.bottom.y,-384.f,10.f);
        ease(state_.left.x,-256.f,10.f);ease(state_.right.x,1024.f,10.f);
        if(state_.top.y==1408.f){state_.centerScale={1.5f,1.5f};state_.visible=false;state_.phase=8;}break;
    case 8:break;
    }
}
void ScreenTransition::append(VisualLayer& target)const{
    if(!state_.visible)return;
    // QobBase positions add. Bottom scale1.5 does not scale Center's offset or
    // multiply Center's independent scale. Original child order is preserved.
    const std::array<Vec2,5> positions={state_.left,state_.right,state_.top,state_.bottom,
        Vec2{state_.bottom.x,state_.bottom.y+168.f}};
    const std::array<Vec2,5> scales={Vec2{2,2},Vec2{2,2},Vec2{1.5f,1.5f},Vec2{1.5f,1.5f},state_.centerScale};
    VisualLayer prepared;
    for(unsigned i=0;i<images.size();++i){
        const auto* tile=tiles_.lookup(images[i]);if(!tile)continue;
        VisualImage image;image.tile=tiles_.visual(*tile);image.screenPosition=positions[i];image.scale=scales[i];
        image.angle=i==4?state_.centerAngle:0;image.tileNo=0;image.useAtlas=false;prepared.submitImage(image);
    }
    auto commands=prepared.drain();
    for(auto& sprite:commands.nonAtlas){sprite.visualLayer=layer;target.submitNonAtlas(std::move(sprite));}
}
void ScreenTransition::append(std::span<VisualLayer> layers)const{
    if(!state_.visible)return;
    if(layers.size()<=layer)throw std::out_of_range("GameMain loading layer13 is not available");
    append(layers[layer]);
}
void ScreenTransition::append(Frame& frame)const{
    VisualLayer commands;append(commands);std::move(commands.drain()).appendBatches(frame);
}
Frame ScreenTransition::preparationFrame()const{Frame frame;tiles_.appendPreparation(frame);return frame;}
}

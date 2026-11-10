#include "Core/UI/Button.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace hm2::ui {
namespace {
void finite(float x){if(!std::isfinite(x))throw std::runtime_error("Nonfinite button value");}
void validTime(float now){finite(now);if(now<0)throw std::runtime_error("Negative button time");}
unsigned tileCount(const ButtonTile& t){
    if(t.columns==0||t.rows==0||t.columns>std::numeric_limits<unsigned>::max()/t.rows)
        throw std::runtime_error("Invalid button tile grid");
    return t.columns*t.rows;
}
void validTile(const ButtonTile& t){
    tileCount(t);finite(t.textureSize.width);finite(t.textureSize.height);
    if(t.image.empty()||t.textureSize.width<=0||t.textureSize.height<=0)
        throw std::runtime_error("Invalid button texture");
}
}
Button::Button(ButtonKey key,std::optional<ButtonTile> tile,unsigned tileNo):key_(key),defaultTile_(tileNo){
    setTile(std::move(tile));
    validateTile(tileNo);
    releaseTile_=tile_&&tileCount(*tile_)>tileNo+1?tileNo+1:tileNo;
    deactiveTile_=tileNo;
}
void Button::validateTile(unsigned tile)const{
    if(tile_&&tile>=tileCount(*tile_))throw std::runtime_error("Button tile index outside atlas");
}
void Button::setTile(std::optional<ButtonTile> tile){
    if(tile){validTile(*tile);if(defaultTile_>=tileCount(*tile))throw std::runtime_error("Button default tile outside atlas");}
    tile_=std::move(tile);
    if(!tile_)return; // Original setTile:nil preserves the previous bounds.
    const float factor=tile_->forRetina?.25f:.5f;
    const float x=tile_->textureSize.width/float(tile_->columns)*factor;
    const float y=tile_->textureSize.height/float(tile_->rows)*factor;
    bounds_={-x,-y,x,y};
}
void Button::setBoundWidthHeight(float width,float height){
    finite(width);finite(height);finite(scale.x);finite(scale.y);
    if(width<0||height<0)throw std::runtime_error("Negative button dimensions");
    bounds_={(-.5f*width)*scale.x,(-.5f*height)*scale.x,
             (.5f*width)*scale.y,(.5f*height)*scale.y};
}
void Button::setDefaultTile(unsigned n){validateTile(n);defaultTile_=n;}
void Button::setReleaseTile(unsigned n){validateTile(n);releaseTile_=n;}
void Button::setDeactiveTile(unsigned n){validateTile(n);deactiveTile_=n;}
Vec2 Button::screenPosition()const{
    finite(position.x);finite(position.y);Vec2 p=position;
    if(positionSnap==PositionSnap::half){p.x=std::round(p.x*2)*.5f;p.y=std::round(p.y*2)*.5f;}
    else if(positionSnap==PositionSnap::whole){p.x=std::round(p.x);p.y=std::round(p.y);}
    return p;
}
bool Button::contains(Vec2 p)const{
    finite(p.x);finite(p.y);const auto c=screenPosition();
    return p.x>c.x+bounds_.left&&p.x<=c.x+bounds_.right&&
           p.y>c.y+bounds_.bottom&&p.y<=c.y+bounds_.top;
}
void Button::notify(ButtonEventType type,std::uintptr_t pointer,Vec2 p,const ButtonHandler& sink){
    if(sink)sink({type,key_,pointer,p});
}
void Button::setReleaseAction(float now){
    validTime(now);if(!pressed_&&releaseTime_==0)releaseTime_=now;
}
void Button::cancelCapture(float now,const ButtonHandler& sink){
    validTime(now);if(!pointer_)return;
    const auto captured=pointer_;pointer_=0;pressed_=false;clickTime_=0;
    notify(ButtonEventType::cancel,captured,tapPosition_,sink);
}
void Button::discardCapture(){pointer_=0;pressed_=false;clickTime_=releaseTime_=0;tapPosition_={};}
bool Button::input(PointerPhase phase,Vec2 p,std::uintptr_t pointer,float now,const ButtonHandler& sink){
    validTime(now);finite(p.x);finite(p.y);
    if(pointer==0)return false;
    if(phase==PointerPhase::cancel){if(pointer_==pointer)cancelCapture(now,sink);return false;}
    if(!visible||!active)return false;
    const bool inside=contains(p);
    if(phase==PointerPhase::end&&pointer_==pointer){
        pressed_=false;clickTime_=0;pointer_=0;tapPosition_=p;
        if(inside){releaseTime_=now;notify(ButtonEventType::pop,pointer,p,sink);}
        else notify(ButtonEventType::cancel,pointer,p,sink);
        return false;
    }
    bool consumed=false;
    if(!inside){
        if(pointer_!=pointer)return false;
        tapPosition_=p;
        if(pressed_){
            clickTime_=0;releaseTime_=now;
            notify(ButtonEventType::release,pointer,p,sink);
            pressed_=false; // Original ReleaseButton notification precedes this reset.
        }
    }else if(phase==PointerPhase::begin||
             (phase==PointerPhase::move&&!pressed_&&pointer_==pointer)){
        clickTime_=now;releaseTime_=0;pointer_=pointer;pressed_=true;tapPosition_=p;
        notify(ButtonEventType::push,pointer,p,sink);consumed=true;
    }
    if(pointer_==pointer&&phase==PointerPhase::move){
        tapPosition_=p;notify(ButtonEventType::move,pointer,p,sink);
    }
    return consumed;
}
void Button::append(Frame& frame,float now){
    validTime(now);if(!visible||!tile_)return;
    finite(scale.x);finite(scale.y);finite(angle);finite(alpha);
    finite(pushDepth);finite(pushDelay);finite(releaseHeight);finite(releaseDelay);
    if(pushDelay<0||releaseDelay<0)throw std::runtime_error("Negative button animation delay");
    float shift=0;
    if(clickTime_!=0){
        const float elapsed=now-clickTime_;
        shift=elapsed<pushDelay?std::sin((elapsed/pushDelay)*1.5707964f)*pushDepth:pushDepth;
    }else if(releaseTime_!=0){
        const float elapsed=now-releaseTime_;
        if(double(elapsed)<double(releaseDelay)){
            const double phase=(double(elapsed)/double(releaseDelay))*std::numbers::pi;
            shift=float(double(std::sin(float(phase+phase)))*double(releaseHeight)*
                        (double(releaseDelay)-double(elapsed)));
        }else releaseTime_=0;
    }
    unsigned tile=defaultTile_;
    if(!active)tile=deactiveTile_;else if(pressed_)tile=releaseTile_;
    validateTile(tile);
    auto center=screenPosition();center.y+=(!active||pressed_)?-shift:shift;
    const float retina=tile_->forRetina?.5f:1.f;
    Sprite sprite;
    sprite.image=tile_->image;sprite.center=center;
    sprite.size={tile_->textureSize.width/float(tile_->columns)*retina*scale.x,
                 tile_->textureSize.height/float(tile_->rows)*retina*scale.y};
    sprite.angle=angle;sprite.tile=tile;sprite.columns=tile_->columns;sprite.rows=tile_->rows;
    sprite.color={1,1,1,alpha};sprite.blend=tile_->blend;
    frame.sprites.push_back(std::move(sprite));
}
void ButtonDispatcher::add(std::shared_ptr<Button> button){
    if(!button)throw std::runtime_error("Null button receiver");
    if(find(button->key()))throw std::runtime_error("Duplicate owner/buttonId/intData/dataId key");
    buttons_.push_back(std::move(button));
}
std::shared_ptr<Button> ButtonDispatcher::find(ButtonKey key)const{
    const auto i=std::find_if(buttons_.begin(),buttons_.end(),[&](const auto& b){return b->key()==key;});
    return i==buttons_.end()?nullptr:*i;
}
bool ButtonDispatcher::remove(ButtonKey key){
    const auto size=buttons_.size();std::erase_if(buttons_,[&](const auto& b){return b->key()==key;});
    return buttons_.size()!=size;
}
void ButtonDispatcher::removeOwner(Owner owner){
    std::erase_if(buttons_,[&](const auto& b){return b->key().owner==owner;});
    std::erase_if(listeners_,[&](const auto& l){return l.owner&&*l.owner==owner;});
}
std::uint64_t ButtonDispatcher::subscribe(std::optional<Owner> owner,ButtonHandler handler){
    if(!handler)throw std::runtime_error("Empty button handler");
    const auto token=nextToken_++;listeners_.push_back({token,owner,std::move(handler)});return token;
}
bool ButtonDispatcher::unsubscribe(std::uint64_t token){
    const auto size=listeners_.size();std::erase_if(listeners_,[&](const auto& l){return l.token==token;});
    return listeners_.size()!=size;
}
void ButtonDispatcher::emit(const ButtonEvent& event){
    const auto snapshot=listeners_;
    for(const auto& listener:snapshot){
        if(listener.owner&&*listener.owner!=event.key.owner)continue;
        if(std::none_of(listeners_.begin(),listeners_.end(),[&](const auto& l){return l.token==listener.token;}))continue;
        listener.handler(event);
    }
}
bool ButtonDispatcher::input(PointerPhase phase,Vec2 p,std::uintptr_t pointer,float now){
    const auto snapshot=buttons_;
    for(auto i=snapshot.rbegin();i!=snapshot.rend();++i)
        if((*i)->input(phase,p,pointer,now,[this](const auto& e){emit(e);}))return true;
    return false;
}
void ButtonDispatcher::cancelAll(float now){
    const auto snapshot=buttons_;
    for(auto i=snapshot.rbegin();i!=snapshot.rend();++i)(*i)->cancelCapture(now,[this](const auto& e){emit(e);});
}
void ButtonDispatcher::discardAll(){for(const auto& button:buttons_)button->discardCapture();}
void ButtonDispatcher::append(Frame& frame,float now){
    const auto snapshot=buttons_;for(const auto& b:snapshot)b->append(frame,now);
}
}

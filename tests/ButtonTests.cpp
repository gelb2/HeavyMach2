#include "Core/UI/Button.hpp"
#include <cassert>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>
using namespace hm2;
using namespace hm2::ui;
namespace {
void same(float a,float b,float tolerance=.0001f){assert(std::abs(a-b)<tolerance);}
ButtonTile tile(){return {"button.png",{40,120},1,3,false,{}};}
template<class F>void fails(F&& f){bool caught=false;try{f();}catch(const std::runtime_error&){caught=true;}assert(caught);}
}
int main(){
    // QobBase's nil-tile inherited bounds; a nil tile is a working invisible
    // receiver, not a fabricated image or a reason to skip explicit hit bounds.
    Button nil({1,7,0});assert(nil.visible&&nil.active&&!nil.pressed());
    assert(!nil.contains({0,.5f})&&nil.contains({1,1}));Frame frame;nil.append(frame,1);assert(frame.sprites.empty());
    nil.setBoundWidthHeight(100,60);nil.setTile(std::nullopt);same(nil.bounds().left,-50);

    Button button({1,1021,0},tile());
    same(button.bounds().left,-20);same(button.bounds().top,20);
    button.position={100,100};
    assert(!button.contains({80,100}));assert(button.contains({120,100}));
    assert(!button.contains({100,80}));assert(button.contains({100,120}));
    assert(!button.contains({120.001f,100}));assert(button.contains({80.001f,80.001f}));
    button.scale={2,3};button.setBoundWidthHeight(100,60);
    // Original ARM0x37aa8 uses scaleX for the two negative bounds and scaleY
    // for the two positive bounds. Rotation/current draw scale do not re-hit.
    same(button.bounds().left,-100);same(button.bounds().bottom,-60);
    same(button.bounds().right,150);same(button.bounds().top,90);
    button.scale={1,1};button.angle=1;assert(button.contains({249,100}));assert(!button.contains({251,100}));
    button.position={-.25f,1.25f};button.positionSnap=PositionSnap::half;
    same(button.screenPosition().x,-.5f);same(button.screenPosition().y,1.5f);
    button.positionSnap=PositionSnap::whole;same(button.screenPosition().x,0);same(button.screenPosition().y,1);
    auto retina=tile();retina.forRetina=true;button.setTile(retina);
    same(button.bounds().left,-10);same(button.bounds().top,10);
    button.position={};button.positionSnap=PositionSnap::none;button.angle=0;
    button.setTile(tile());button.setBoundWidthHeight(40,40);

    std::vector<ButtonEvent> events;
    auto sink=[&](const ButtonEvent& e){events.push_back(e);};
    assert(button.input(PointerPhase::begin,{0,0},11,10,sink));
    assert(button.pressed()&&button.capturedPointer()==11&&events.back().type==ButtonEventType::push);
    // An unrelated touch cannot move/pop this captured button.
    button.input(PointerPhase::move,{10,0},12,10.01f,sink);
    button.input(PointerPhase::end,{10,0},12,10.02f,sink);assert(events.size()==1&&button.pressed());
    // Dragging out emits Release then Move but keeps touch ownership; entering
    // again emits Push then Move, unlike a permanently cancelled UIKit click.
    assert(!button.input(PointerPhase::move,{21,0},11,10.1f,sink));
    assert(events[1].type==ButtonEventType::release&&events[2].type==ButtonEventType::move);
    assert(!button.pressed()&&button.capturedPointer()==11);
    assert(button.input(PointerPhase::move,{0,0},11,10.2f,sink));
    assert(events[3].type==ButtonEventType::push&&events[4].type==ButtonEventType::move);
    assert(!button.input(PointerPhase::end,{20,20},11,10.3f,sink));
    assert(events.back().type==ButtonEventType::pop&&!button.pressed()&&button.capturedPointer()==0);
    assert(events.back().key==ButtonKey({1,1021,0}));
    events.clear();button.input(PointerPhase::begin,{0,0},1,11,sink);
    button.input(PointerPhase::end,{-20,0},1,11.1f,sink);
    assert(events.size()==2&&events.back().type==ButtonEventType::cancel);
    // A second begin inside replaces the stored ID in the original, without
    // generating a fake CancelButton event for the first touch.
    events.clear();button.input(PointerPhase::begin,{0,0},1,12,sink);
    button.input(PointerPhase::begin,{0,0},2,12.1f,sink);
    assert(events.size()==2&&button.capturedPointer()==2);
    button.input(PointerPhase::end,{0,0},1,12.2f,sink);assert(events.size()==2);
    button.input(PointerPhase::end,{0,0},2,12.3f,sink);assert(events.back().type==ButtonEventType::pop);

    // Original selected/disabled atlas state, push sine and damped release
    // bounce are measured independently at quarter/half periods.
    Button animated({1,2,0},tile());animated.position={50,60};
    animated.pushDepth=3;animated.pushDelay=.2f;animated.releaseHeight=6;animated.releaseDelay=.2f;
    animated.setDeactiveTile(2);animated.alpha=.4f;animated.angle=.25f;
    frame.sprites.clear();animated.append(frame,20);assert(frame.sprites.back().tile==0);
    animated.input(PointerPhase::begin,{50,60},1,20,sink);
    frame.sprites.clear();animated.append(frame,20.1f);
    assert(frame.sprites.back().tile==1);same(frame.sprites.back().center.y,60-3*std::sqrt(.5f),.0002f);
    same(frame.sprites.back().size.width,40);same(frame.sprites.back().size.height,40);
    same(frame.sprites.back().angle,.25f);same(frame.sprites.back().color.a,.4f);
    assert(frame.sprites.back().rows==3&&frame.sprites.back().columns==1);
    animated.active=false;const auto count=events.size();
    animated.input(PointerPhase::end,{50,60},1,20.15f,sink);assert(events.size()==count&&animated.pressed());
    frame.sprites.clear();animated.append(frame,20.2f);assert(frame.sprites.back().tile==2);
    same(frame.sprites.back().center.y,57);
    animated.active=true;animated.input(PointerPhase::end,{50,60},1,20.4f,sink);
    frame.sprites.clear();animated.append(frame,20.45f);assert(frame.sprites.back().tile==0);
    same(frame.sprites.back().center.y,60.9f,.0002f);
    frame.sprites.clear();animated.append(frame,20.7f);same(frame.sprites.back().center.y,60);
    animated.visible=false;frame.sprites.clear();animated.append(frame,21);assert(frame.sprites.empty());
    animated.visible=true;animated.setTile(retina);animated.scale={2,3};
    frame.sprites.clear();animated.append(frame,21);same(frame.sprites.back().size.width,40);same(frame.sprites.back().size.height,60);
    // Explicit host cancellation clears a hidden captured receiver safely.
    animated.input(PointerPhase::begin,{50,60},5,22,sink);animated.visible=false;
    animated.input(PointerPhase::cancel,{},5,22.1f,sink);
    assert(!animated.pressed()&&animated.capturedPointer()==0&&events.back().type==ButtonEventType::cancel);

    // Independent owners may reuse IDs and slots reuse IDs with intData.
    ButtonDispatcher dispatcher;std::vector<ButtonEvent> dispatched;
    auto low=std::make_shared<Button>(ButtonKey{1,1009,0},tile());
    auto high=std::make_shared<Button>(ButtonKey{1,1009,1},tile());
    auto other=std::make_shared<Button>(ButtonKey{2,1009,1},tile());
    dispatcher.add(low);dispatcher.add(high);dispatcher.add(other);
    fails([&]{dispatcher.add(std::make_shared<Button>(ButtonKey{1,1009,1},tile()));});
    dispatcher.subscribe(std::nullopt,[&](const auto& e){dispatched.push_back(e);});
    unsigned owner1=0,owner2=0;
    dispatcher.subscribe(1,[&](const auto&){++owner1;});
    dispatcher.subscribe(2,[&](const auto&){++owner2;});
    assert(dispatcher.input(PointerPhase::begin,{0,0},1,30));
    assert(dispatched.size()==1&&dispatched[0].key.owner==2&&owner1==0&&owner2==1);
    dispatcher.input(PointerPhase::end,{0,0},1,30.1f);assert(owner2==2&&owner1==0);
    dispatcher.removeOwner(2);assert(dispatcher.size()==2);
    dispatcher.input(PointerPhase::begin,{0,0},2,31);assert(dispatched.back().key.intData==1);
    dispatcher.cancelAll(31.1f);assert(dispatched.back().type==ButtonEventType::cancel);

    // Closing a screen from a synchronous PushButton listener can remove its
    // receiver and handlers. The current receiver stays alive until return;
    // buttons added by that listener are only hit on the next input.
    ButtonDispatcher lifetime;std::weak_ptr<Button> weak;unsigned closed=0,newPush=0;
    auto closing=std::make_shared<Button>(ButtonKey{10,1,0},tile());weak=closing;lifetime.add(closing);closing.reset();
    lifetime.subscribe(10,[&](const ButtonEvent& e){
        if(e.type!=ButtonEventType::push)return;
        ++closed;lifetime.removeOwner(10);
        lifetime.add(std::make_shared<Button>(ButtonKey{11,1,0},tile()));
        lifetime.subscribe(11,[&](const ButtonEvent& x){if(x.type==ButtonEventType::push)++newPush;});
        assert(!weak.expired());
    });
    lifetime.input(PointerPhase::begin,{0,0},1,40);assert(closed==1&&newPush==0&&weak.expired());
    lifetime.input(PointerPhase::begin,{0,0},2,41);assert(newPush==1);
    // Removal of a later listener in the current notification is safe and
    // suppresses it. New listeners do not join the current snapshot.
    ButtonDispatcher mutations;mutations.add(std::make_shared<Button>(ButtonKey{3,7,0},tile()));
    std::uint64_t second=0;unsigned secondCalls=0,addedCalls=0;
    mutations.subscribe(3,[&](const auto&){mutations.unsubscribe(second);mutations.subscribe(3,[&](const auto&){++addedCalls;});});
    second=mutations.subscribe(3,[&](const auto&){++secondCalls;});
    mutations.input(PointerPhase::begin,{0,0},1,50);assert(secondCalls==0&&addedCalls==0);
    mutations.input(PointerPhase::end,{0,0},1,50.1f);assert(secondCalls==0&&addedCalls==1);
    // Original setDataObject: assigns Mission/Item payloads to dynamic cells.
    // Persisted IDs distinguish a refreshed cell from a former occupant without
    // retaining an Objective-C pointer or guessing from its screen position.
    ButtonDispatcher payloads;
    const ButtonKey missionA{20,5100,0,"M001010"},missionB{20,5100,0,"M001020"};
    payloads.add(std::make_shared<Button>(missionA,tile()));
    payloads.add(std::make_shared<Button>(missionB,tile()));
    assert(payloads.find(missionA)&&payloads.find(missionB)&&payloads.size()==2);
    assert(payloads.remove(missionA)&&!payloads.find(missionA)&&payloads.find(missionB));
    std::string chosen;
    payloads.subscribe(20,[&](const ButtonEvent& e){chosen=e.key.dataId;});
    payloads.input(PointerPhase::begin,{0,0},1,60);assert(chosen=="M001020");
    fails([&]{payloads.add(std::make_shared<Button>(missionB,tile()));});
    fails([&]{Button bad({1,1,0},ButtonTile{"x",{2,2},0,1,false,{}});});
    // Original DlgInven's one-tile Arrange stores normal/release index1.
    // Button state retains raw indices; only the drawn Tile2D sample clamps.
    Button one({30,6093,0},ButtonTile{"arrange.png",{120,42},1,1,false,{}},1);
    one.setReleaseTile(1);frame.sprites.clear();one.append(frame,70);
    assert(frame.sprites.back().tile==0&&frame.sprites.back().size.width==120&&frame.sprites.back().size.height==42);
    assert(one.input(PointerPhase::begin,{0,0},71,71,sink));frame.sprites.clear();one.append(frame,71);assert(frame.sprites.back().tile==0);
    one.input(PointerPhase::end,{0,0},71,72,sink);one.setDefaultTile(99);one.setTile(tile());frame.sprites.clear();one.append(frame,73);assert(frame.sprites.back().tile==2);
    fails([&]{button.setBoundWidthHeight(-1,2);});
    fails([&]{button.input(PointerPhase::begin,{},1,-1,sink);});
}

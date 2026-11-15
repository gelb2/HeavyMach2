#pragma once
#include "Core/Types.hpp"
#include "Core/Simulation/WorldNavigation.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace hm2::ui {
using Owner = std::uint64_t;
struct ButtonKey {
    Owner owner=0;
    std::uint32_t id=0xffffffffu;
    std::int32_t intData=0;
    // Stable identity for original setDataObject: consumers, e.g. MissionID or
    // ItemCode. A reused list index must not retain its former mission/item.
    // Never store an original Objective-C object address here.
    std::string dataId{};
    bool operator==(const ButtonKey&)const=default;
};
enum class ButtonEventType {push,release,move,pop,cancel};
struct ButtonEvent {
    ButtonEventType type;
    ButtonKey key;
    std::uintptr_t pointer=0;
    Vec2 position;
    std::optional<Vec2> canvasPosition={};
};
using ButtonHandler=std::function<void(const ButtonEvent&)>;
struct ButtonTile {
    std::string image;
    Size textureSize;
    unsigned columns=1,rows=1;
    bool forRetina=false;
    Blend blend;
};
// QobBase boundRect stores min/max offsets, not CGRect origin/extent. HD
// QobButton::onTap uses left/bottom exclusive and right/top inclusive edges.
struct ButtonBounds {float left=0,bottom=0,right=1,top=1;};
enum class PositionSnap {none,half,whole};

class Button {
    ButtonKey key_;
    std::optional<ButtonTile> tile_;
    ButtonBounds bounds_;
    unsigned defaultTile_=0,releaseTile_=0,deactiveTile_=0;
    std::uintptr_t pointer_=0;
    bool pressed_=false;
    float clickTime_=0,releaseTime_=0;
    Vec2 tapPosition_;
    void notify(ButtonEventType,std::uintptr_t,Vec2,const ButtonHandler&);
public:
    // Position is QobBase's resolved screen position, in the same positive-Y-up
    // logical coordinates as GPU sprites. Parent/camera transforms are supplied
    // by the scene; drawing and hit-testing do not use different camera paths.
    Vec2 position,scale{1,1};
    float angle=0,alpha=1;
    bool visible=true,active=true;
    PositionSnap positionSnap=PositionSnap::none;
    float pushDepth=0,pushDelay=0,releaseHeight=0,releaseDelay=0;

    explicit Button(ButtonKey,std::optional<ButtonTile> tile=std::nullopt,unsigned tileNo=0);
    [[nodiscard]] ButtonKey key()const{return key_;}
    [[nodiscard]] ButtonBounds bounds()const{return bounds_;}
    [[nodiscard]] Vec2 screenPosition()const;
    [[nodiscard]] Vec2 tapPosition()const{return tapPosition_;}
    [[nodiscard]] std::uintptr_t capturedPointer()const{return pointer_;}
    [[nodiscard]] bool pressed()const{return pressed_;}
    [[nodiscard]] bool contains(Vec2)const;
    void setTile(std::optional<ButtonTile>);
    // Original 0x37aa8 deliberately preserved: negative bounds use scale.x,
    // positive bounds use scale.y for BOTH axes. Scale changes after this call
    // do not recalculate the stored hit box. Rotation never rotates its hit box.
    void setBoundWidthHeight(float width,float height);
    void setDefaultTile(unsigned);
    void setReleaseTile(unsigned);
    void setDeactiveTile(unsigned);
    void setReleaseAction(float now);
    bool input(PointerPhase,Vec2,std::uintptr_t,float now,const ButtonHandler&);
    // Host touch cancellation is an explicit cleanup path. Original
    // CancelButton is also emitted when an owned touch ends outside its bounds.
    void cancelCapture(float now,const ButtonHandler&);
    // OS interruption cleanup. It must not emit original CancelButton, which
    // can commit an inventory drag/drop action in a game handler.
    void discardCapture();
    void append(Frame&,float now);
};

// QobManager dispatches receivers in reverse insertion order. Strong snapshots
// preserve synchronous notification order without original receiver-list UAF.
// Changes to the button list during a callback take effect on the next input.
class ButtonDispatcher {
    struct Listener {std::uint64_t token;std::optional<Owner> owner;ButtonHandler handler;};
    std::vector<std::shared_ptr<Button>> buttons_;
    std::vector<Listener> listeners_;
    std::uint64_t nextToken_=1;
    void emit(const ButtonEvent&);
public:
    void add(std::shared_ptr<Button>);
    bool remove(ButtonKey);
    void removeOwner(Owner);
    [[nodiscard]] std::shared_ptr<Button> find(ButtonKey)const;
    [[nodiscard]] std::size_t size()const{return buttons_.size();}
    std::uint64_t subscribe(std::optional<Owner>,ButtonHandler);
    bool unsubscribe(std::uint64_t);
    bool input(PointerPhase,Vec2,std::uintptr_t,float now,
               const std::function<Vec2(const Button&,Vec2)>& pointMap={});
    void cancelAll(float now);
    void discardAll();
    void append(Frame&,float now);
};
}

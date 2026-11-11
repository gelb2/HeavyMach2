#pragma once
#include "Core/Progression/Progression.hpp"
#include "Core/Data/Weapons.hpp"
#include "Core/Simulation/Particles.hpp"
#include "Core/Simulation/WorldNavigation.hpp"
#include <optional>

namespace hm2::progression {
struct RepairTarget {
    std::uint64_t id=0;
    bool player=false;
    Vec2 position;
    int hp=0,maximumHP=0,botSlot=-1;
};
struct AirStrikeRequest {
    const Item* item=nullptr;
    const WeaponInfo* weapon=nullptr; // Original nil lookup is not replaced.
    std::string weaponKey;
    Vec2 position;
    double worldTime=0;
    float renderTime=0,fireDelay=0;
    unsigned attackCount=0;
};
// These are actual world/UI consumers, not optional completion callbacks. The
// Scene adapter must supply live actor HP, its existing pool and prepared tiles.
// An inventory BOT registration is not a live RepairTarget.
struct QuickItemsHost {
    virtual ~QuickItemsHost()=default;
    virtual std::optional<RepairTarget> player()const=0;
    virtual std::vector<RepairTarget> bots()const=0; // Original child order.
    virtual void setHP(const RepairTarget&,int)=0;
    // Receives the newly written HP. Player ratio/x: r,21*r-21;
    // BOT ratio/x: .5*r,10.5*r-10.5. Nil original gauge remains nil.
    virtual void refreshActorHP(const RepairTarget&)=0;
    virtual void refreshPlayerHP()=0;
    // Must call the existing world pool.allocate/reset, including its RNG.
    virtual Particle* allocateParticle(float renderTime)=0;
    virtual const TileImage* preparedTile(std::string_view originalName)=0;
    virtual void selectQuick(int)=0;
    virtual void message(std::string_view originalDescriptionKey,int background,bool autoHide)=0;
    virtual void hideMessage()=0;
    virtual void refreshQuickItem(const Item&,int actualCount)=0;
    virtual void sound(unsigned originalSfx)=0;
    // Must create/start the actual GobAirStrike consumer before returning. A
    // no-op implementation would consume inventory without its original effect.
    virtual void startAirStrike(const AirStrikeRequest&)=0;
};
// WndQuickSlot0x34e54 -> World.useItem0x108a0 -> original begin-tap consumer.
// It never writes a file, DB or a new save format. Particle lifetime remains
// owned by the normal world particle clock; no per-item repair cooldown exists.
class QuickItems {
    Inventory& inventory_;
    SlotView slot_;
    const Weapons& weapons_;
    QuickItemsHost& host_;
    const Item* item_=nullptr;
    int selected_=-1;
    short tapState_=0;
    void use(const Item*);
    void clearAfterTap();
    void repair(const Item&,const RepairTarget&,float renderTime);
public:
    QuickItems(Inventory&,SlotView,const Weapons&,QuickItemsHost&);
    // Push only. displayedCount is the original numeric child's current value;
    // registration and count refresh belong to the shared inventory/UI path.
    // Returns whether the original unpaused five-slot handler ran.
    bool press(unsigned quickSlot,int displayedCount,bool paused);
    // Returns true when the original item tap-mode owns this world event.
    // Non-begin phases retain selection; no target still disarms on begin.
    bool input(PointerPhase,Vec2 screen,const MapCamera&,bool paused,
               float renderTime,double worldTime);
    void clear(); // Explicit World.useItem(nil), not an OS cancellation action.
    [[nodiscard]] const Item* inUseItem()const{return item_;}
    [[nodiscard]] int selectedQuick()const{return selected_;}
    [[nodiscard]] short tapState()const{return tapState_;}
};
}

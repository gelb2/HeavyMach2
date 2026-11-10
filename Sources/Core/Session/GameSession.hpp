#pragma once
#include "Core/Data/SaveData.hpp"
#include "Core/Simulation/CombatScene.hpp"
#include "Core/UI/Screens.hpp"
#include <memory>

namespace hm2 {
enum class SaveReason {selectSlot,enterBase,stageTransition,saveAndExit};
struct SessionHost {
    virtual ~SessionHost()=default;
    virtual void sound(std::string_view,Vec2)=0;
    virtual void soundID(unsigned,Vec2)=0;
    virtual void alert(std::string_view originalKey)=0;
    virtual void requestName(unsigned slot)=0;
    virtual void requestDelete(unsigned slot)=0;
    // Called only at confirmed original save events. The platform adapter owns
    // atomic replacement and must report failure instead of claiming a save.
    virtual bool writeSave(std::span<const std::uint8_t>,SaveReason)=0;
    virtual std::uint32_t absoluteTimeLow()=0;
    virtual std::uint32_t unixTime()=0;
};
struct FieldItemSnapshot {std::string code;int type=0,value=0;Vec2 position;bool removed=false;Vec2 pickupPlayer;};
struct SessionMetrics {
    std::uint64_t saveRequests=0,saveCompletions=0,unsupportedActions=0;
};
// Shared game state behind GPU screens and battle. A working slot is a separate
// copy, as in GlobalInfo.selectSlot; the save envelope changes on explicit save.
class GameSession {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    GameSession(ResourceAccess&,SessionHost&,WorldRandom&,std::optional<SaveData> =std::nullopt);
    ~GameSession();
    GameSession(const GameSession&)=delete;
    GameSession& operator=(const GameSession&)=delete;
    void tick();
    void setActive(bool);
    void action(const ui::ScreenAction&);
    bool inputWorld(PointerPhase,Vec2,std::uintptr_t);
    bool createSlot(unsigned,std::string_view name,std::string_view country);
    bool deleteSlot(unsigned); // Caller supplies the original confirmation UI.
    bool selectSlot(unsigned);
    void enterBase();
    bool enterField(std::string_view map,int gate);
    bool acceptMission(std::string_view);
    bool claimMission(std::string_view);
    bool equip(int position,unsigned slot);
    bool save(SaveReason);
    [[nodiscard]] const ui::ScreensState& state();
    [[nodiscard]] const SessionMetrics& metrics()const;
    [[nodiscard]] const SaveData& envelope()const;
    [[nodiscard]] std::span<const std::uint8_t> workingSlot()const;
    [[nodiscard]] CombatScene* battle();
    [[nodiscard]] float renderTime()const;
    [[nodiscard]] std::uint64_t presentationRevision()const;
    [[nodiscard]] Frame preparationFrame()const;
    [[nodiscard]] std::vector<FieldItemSnapshot> fieldItems()const;
    Frame drawWorld();
};
}

#pragma once
#include "Core/Data/SaveData.hpp"
#include "Core/Simulation/CombatScene.hpp"
#include "Core/UI/Screens.hpp"
#include "Core/UI/Help.hpp"
#include "Core/Presentation/ScreenTransition.hpp"
#include "Core/Presentation/MapLoading.hpp"
#include <memory>

namespace hm2 {
enum class SaveReason {selectSlot,enterBase,stageTransition,saveAndExit,deleteSlot};
struct SessionHost {
    virtual ~SessionHost()=default;
    virtual void sound(std::string_view,Vec2)=0;
    virtual void soundID(unsigned,Vec2)=0;
    virtual void alert(std::string_view originalKey)=0;
    virtual void alert(std::string_view originalKey,std::string_view){alert(originalKey);}
    // Restoration notices have literal localized text, not an original key.
    virtual void notice(std::string_view title,std::string_view message){alert(message,title);}
    virtual void requestName(unsigned slot)=0;
    virtual void requestDelete(unsigned slot)=0;
    virtual void requestGameOver(std::uint64_t){alert("DestroyMach","Destroy Your Mach.");}
    // Called only at confirmed original save events. The platform adapter owns
    // atomic replacement and must report failure instead of claiming a save.
    virtual bool writeSave(std::span<const std::uint8_t>,SaveReason)=0;
    virtual ui::PreparedScreenText prepareWorldText(const ui::TextRasterRequest&);
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
    GameSession(ResourceAccess&,SessionHost&,WorldRandom&,std::optional<SaveData> =std::nullopt,std::string_view displayLocale="kr");
    ~GameSession();
    GameSession(const GameSession&)=delete;
    GameSession& operator=(const GameSession&)=delete;
    // GameMain children tick World, GuiGame, then late GuiMapLoading.
    void tick(const std::function<void()>& tickGameUI={});
    void setActive(bool);
    void setDisplay(DisplayConfiguration);
    [[nodiscard]] const DisplayConfiguration& display()const;
    [[nodiscard]] Vec2 worldCanvasPoint(Vec2 logical)const;
    void action(const ui::ScreenAction&);
    // Original GuiGame child receivers run before its HUD/world receivers.
    bool inputOverlay(PointerPhase,Vec2,std::uintptr_t);
    void appendOverlays(Frame&)const;
    [[nodiscard]] const ui::Help* help()const;
    [[nodiscard]] const ScreenTransitionState& screenTransition()const;
    [[nodiscard]] const MapLoading* mapLoading()const;
    bool inputWorld(PointerPhase,Vec2,std::uintptr_t);
    bool createSlot(unsigned,std::string_view name,std::string_view country);
    bool deleteSlot(unsigned); // Caller supplies the original confirmation UI.
    bool confirmGameOver(std::uint64_t token);
    [[nodiscard]] std::uint64_t pendingGameOver()const;
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
    [[nodiscard]] std::string originalText(std::string_view key)const;
    [[nodiscard]] std::span<const std::uint8_t> workingSlot()const;
    [[nodiscard]] CombatScene* battle();
    [[nodiscard]] float renderTime()const;
    [[nodiscard]] std::uint64_t presentationRevision()const;
    [[nodiscard]] Frame preparationFrame()const;
    [[nodiscard]] std::vector<FieldItemSnapshot> fieldItems()const;
    Frame drawWorld();
};
}

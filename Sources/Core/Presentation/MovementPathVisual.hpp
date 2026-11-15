#pragma once
#include "Core/Presentation/PreparedTiles.hpp"
#include "Core/Presentation/Visibility.hpp"
#include <array>

namespace hm2 {
// Read-only copy of the actual WorldNavigation ring, not another navigation
// controller. The owner supplies snapshots at the original mutation sites.
struct MovementPathState {
    std::array<Vec2,12> points{};
    unsigned start=0,count=0;
};
struct MovementMarkObservation {
    Vec2 position,lineEnd;
    Vec2 scale{1,1};
    float alpha=1,angle=0;
    unsigned tile=0;
    bool shown=false,lineShown=false;
};
class MovementPathVisual {
    PreparedTiles* tiles_=nullptr;
    std::array<MovementMarkObservation,12> marks_{};
    struct Line {Vec2 origin;std::vector<StripVertex> vertices;};
    std::array<VisibilityState,12> visibility_{};
    std::array<Line,12> lines_{};
    Line targetLine_;
    Vec2 targetStart_{},targetEnd_{};
    bool targetShown_=false;
    static void validate(const MovementPathState&);
    void updateLine(Line&,std::string_view,Vec2,Vec2,float);
    void submitLine(VisualLayer&,std::string_view,const Line&,const MapCamera*)const;
public:
    // refreshWorldObject0x10174 uses World.tileResMgr for all12 marker/line
    // pairs and TargetPath. These three images are not sharedMgr resources.
    void prepare(ResourceAccess&,PreparedTiles& worldCache);
    // Called AFTER the actual setMovePath mutation, once per accepted point.
    // reset is the actual branch (!add || oldCount==0), not inferred here.
    void acceptedSetMovePath(const MovementPathState& after,Vec2 player,bool reset);
    // Called for the exact physical range hidden by clearMovePath; queue
    // count/start changes remain exclusively the navigation owner's work.
    void clearMovePath(unsigned physicalStart,unsigned count);
    // Capture before at processMyMach entry and after the real arrival/pop.
    // This method never computes arrival or sets an actor destination.
    // Caller preserves original moveType/pause/gameState gates.
    void processMyMach(Vec2 player,double worldTime,const MovementPathState& before,
                       const MovementPathState& after);
    // Only the original user-target selection branch sets show=true. A normal
    // setTarget alone must not manufacture a visible TargetPath.
    void setTargetPathShown(bool shown){targetShown_=shown;}
    void processTarget(Vec2 player,std::optional<Vec2> target);
    // Same registration point as objectBase's 12 line/marker pairs and its
    // TargetPath respectively. Collection never advances animation or queue.
    void appendWorldLayers(VisualLayer& layer1,VisualLayer& layer7,const MapCamera*)const;
    void appendPreparation(Frame&)const;
    [[nodiscard]] const auto& observations()const{return marks_;}
    [[nodiscard]] bool targetPathShown()const{return targetShown_;}
    [[nodiscard]] std::array<std::size_t,13> lineCapacities()const;
};
}

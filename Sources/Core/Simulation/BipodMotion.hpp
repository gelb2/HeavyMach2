#pragma once
#include "Core/Data/PlayerEquipment.hpp"
#include "Core/Simulation/TerrainCollision.hpp"
#include <optional>

namespace hm2 {
struct BipodFoot {float angle=0,addX=0,scaleState=1;};
struct BipodState {
    Vec2 position,destination;
    float angle=0,bodyAngle=0,baseAngle=0,leftWeaponAngle=0,rightWeaponAngle=0;
    BipodFoot left,right;
    int state=0,walkStep=0,walkFoot=0,currentFoot=-1;
    bool blocked=false,weaponReady=false,inScreen=true;
    float stepLen=0,moveLen=0,stepSize=20,bodyAddY=0,bodyScaleState=1;
    float speed=0;
};
// Original normal bipod locomotion/aim. Dash, shots, hit and death are separate
// pending consumers. No time*velocity replacement and no invented blocked write.
class BipodMotion {
    BipodState s_;
    void footChange();
public:
    explicit BipodMotion(Vec2 start={},float direction=0,float stepSize=20);
    explicit BipodMotion(BipodState snapshot);
    [[nodiscard]] const BipodState& state()const{return s_;}
    void setDestination(Vec2);
    void tick(const TerrainEdge* currentWall=nullptr,std::optional<Vec2> aimTarget=std::nullopt);
    void applyContact(Vec2 correctedPosition);
    [[nodiscard]] PlayerPartAngles angles()const;
    [[nodiscard]] PlayerPartOffsets offsets()const;
};
}

#pragma once
#include "Core/Data/Machines.hpp"
#include "Core/Types.hpp"
namespace hm2 {
struct ObjectBody {Vec2 position;int radius=0;MachineKind kind=MachineKind::bipod;};
// GobHvMach::onCollideObject0xef24. The caller supplies the original ordered
// living/camera candidates; this function may move either participant.
bool resolveObjectCollision(ObjectBody& self,ObjectBody& other);
}

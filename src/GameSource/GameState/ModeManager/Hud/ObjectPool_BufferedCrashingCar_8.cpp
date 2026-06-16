#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.h"

// Explicit instantiations of the ObjectPool<BufferedCrashingCar,8,s32> methods (inline in CgsObjectPool.h).
template s32  CgsContainers::ObjectPool<BrnGameState::HUDMessageLogic::BufferedCrashingCar, 8, s32>::AllocateObject();
template void CgsContainers::ObjectPool<BrnGameState::HUDMessageLogic::BufferedCrashingCar, 8, s32>::FreeObject(s32);
template bool CgsContainers::ObjectPool<BrnGameState::HUDMessageLogic::BufferedCrashingCar, 8, s32>::IsObjectAllocated(s32) const;

#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerLeapingData.h"

// Explicit instantiations of the ObjectPool<CarLeapingData,7,s32> methods (inline in CgsObjectPool.h).
template s32  CgsContainers::ObjectPool<BrnGameState::ChallengeManager::CarLeapingData, 7, s32>::AllocateObject();
template bool CgsContainers::ObjectPool<BrnGameState::ChallengeManager::CarLeapingData, 7, s32>::IsObjectAllocated(s32) const;

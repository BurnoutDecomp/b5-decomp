#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerLeapingData.h"

// Explicit instantiations of the ObjectPool<StoredLeapingData,7,s32> methods (inline in CgsObjectPool.h).
// (The "StuntModeScoringOnline::StoredLeapingData" ledger entries are the SAME instantiation -- an
//  IDA enclosing-scope artifact; DWARF names the element BrnGameState::ChallengeManager::StoredLeapingData.)
template s32  CgsContainers::ObjectPool<BrnGameState::ChallengeManager::StoredLeapingData, 7, s32>::AllocateObject();
template bool CgsContainers::ObjectPool<BrnGameState::ChallengeManager::StoredLeapingData, 7, s32>::IsObjectAllocated(s32) const;
template void CgsContainers::ObjectPool<BrnGameState::ChallengeManager::StoredLeapingData, 7, s32>::FreeObject(s32);

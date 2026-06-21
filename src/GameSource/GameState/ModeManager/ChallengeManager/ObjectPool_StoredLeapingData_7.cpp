#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerLeapingData.h"

// Explicit instantiations of the ObjectPool<StoredLeapingData,7,s32> methods (inline in CgsObjectPool.h).
// (The "StuntModeScoringOnline::StoredLeapingData" ledger entries are the SAME instantiation -- an
//  IDA enclosing-scope artifact; DWARF names the element BrnGameState::ChallengeManager::StoredLeapingData.)
template s32  CgsContainers::ObjectPool<BrnGameState::ChallengeManager::StoredLeapingData, 7, s32>::AllocateObject();
template bool CgsContainers::ObjectPool<BrnGameState::ChallengeManager::StoredLeapingData, 7, s32>::IsObjectAllocated(s32) const;
template void CgsContainers::ObjectPool<BrnGameState::ChallengeManager::StoredLeapingData, 7, s32>::FreeObject(s32);
// X360 0x8231A2E8 (class:BrnGameState catch-all TU, ledger "BrnGameState::") = the indexed accessor of
// this same instantiation. ChallengeManager::UpdateLeaptCars indexes the pool by slot; the X360 body
// bounds-checks (capacity 7, CgsObjectPool.h:218) and verifies the slot is allocated (line 219) before
// returning the 32-byte object (stride 32). Non-const overload (UpdateLeaptCars mutates the slot).
template BrnGameState::ChallengeManager::StoredLeapingData& CgsContainers::ObjectPool<BrnGameState::ChallengeManager::StoredLeapingData, 7, s32>::operator[](s32);

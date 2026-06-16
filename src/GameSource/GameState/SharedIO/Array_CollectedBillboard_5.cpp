#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/SharedIO/BrnGameStateLeafContainers.h"

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the CollectedBillboard leaf instantiation -- the committed Array_/EventQueue_ explicit-instantiation pattern.
template void Array<BrnGameState::DeveloperChallengeManager::CollectedBillboard, 5>::Append(const BrnGameState::DeveloperChallengeManager::CollectedBillboard&);
template void Array<BrnGameState::DeveloperChallengeManager::CollectedBillboard, 5>::Erase(u32);

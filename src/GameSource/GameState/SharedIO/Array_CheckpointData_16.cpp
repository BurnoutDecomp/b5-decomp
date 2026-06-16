#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/SharedIO/BrnGameStateLeafContainers.h"
#include "GameSource/GameState/BrnCheckpointData.h"

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the CheckpointData leaf instantiation -- the committed Array_/EventQueue_ explicit-instantiation pattern.
template void Array<BrnGameState::CheckpointData, 16>::Append(const BrnGameState::CheckpointData&);
template BrnGameState::CheckpointData& Array<BrnGameState::CheckpointData, 16>::GetItem(u32);

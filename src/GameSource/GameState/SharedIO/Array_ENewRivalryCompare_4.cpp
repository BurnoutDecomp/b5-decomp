#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/SharedIO/BrnGameStateLeafContainers.h"

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the ENewRivalryCompare leaf instantiation -- the committed Array_/EventQueue_ explicit-instantiation pattern.
template void Array<BrnGameState::OnlineFlybyManager::ENewRivalryCompare, 4>::Append(const BrnGameState::OnlineFlybyManager::ENewRivalryCompare&);

#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"  // BrnGameState::StartLocation element home

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the StartLocation,8 leaf instantiation -- the committed Array_/EventQueue_ explicit-instantiation pattern.
template BrnGameState::StartLocation& Array<BrnGameState::StartLocation, 8>::GetItem(u32);
template void Array<BrnGameState::StartLocation, 8>::Append(const BrnGameState::StartLocation&);

#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/BrnGameStateTypes.h"  // BrnGameState::LandmarkIndex element home

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the BrnGameState::LandmarkIndex,16 leaf instantiation -- the committed
// Array_/EventQueue_ explicit-instantiation pattern.
//   X360 0x82318A98 = Array<...LandmarkIndex,16u>::Append
//   X360 0x8231AB70 = Array<...LandmarkIndex,16u>::FindFirstInstanceOf
//   X360 0x8231AC10 = Array<...LandmarkIndex,16u>::GetItem (non-const)
//   X360 0x82325220 = Array<...LandmarkIndex,16u>::Contains
template void Array<BrnGameState::LandmarkIndex, 16>::Append(const BrnGameState::LandmarkIndex&);
template s32 Array<BrnGameState::LandmarkIndex, 16>::FindFirstInstanceOf(const BrnGameState::LandmarkIndex&) const;
template BrnGameState::LandmarkIndex& Array<BrnGameState::LandmarkIndex, 16>::GetItem(u32);
template bool Array<BrnGameState::LandmarkIndex, 16>::Contains(const BrnGameState::LandmarkIndex&) const;

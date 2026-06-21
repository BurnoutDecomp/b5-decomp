#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/SharedIO/BrnGameStateLeafContainers.h"

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the TargetEventScore leaf instantiation -- the committed Array_/EventQueue_ explicit-instantiation pattern.
template void Array<BrnGameState::GameStateModuleIO::TargetEventScore, 49>::EraseFast(u32);
// X360 0x8235C080 (class:BrnGameState catch-all TU, ledger "BrnGameState::Game") = the reserve-a-slot
// accessor of this same instantiation. Profile::SetTargetEventScore reserves the next free
// TargetEventScore slot then fills it in place: the X360 body asserts Construct/Clear'd + capacity-49
// ("Array container out of space, Length/Capacity", CgsArray.h:289/290) then returns &maElements[count]
// and post-increments the count (stride 40 == 49*40 to the owner+1960 count offset) -- the generic
// Array<T,N>::AddNew body.
template BrnGameState::GameStateModuleIO::TargetEventScore* Array<BrnGameState::GameStateModuleIO::TargetEventScore, 49>::AddNew();

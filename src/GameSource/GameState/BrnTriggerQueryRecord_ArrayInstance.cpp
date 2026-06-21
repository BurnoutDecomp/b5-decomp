#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"   // GameStateModuleIO::TriggerQueryRecord element home

// Explicit instantiation of the generic Array<T,N> const indexed accessor (inline in CgsArray.h)
// for the Array<TriggerQueryRecord, 16> leaf instantiation -- the committed Array_/EventQueue_
// explicit-instantiation pattern.
//
// X360 0x8235FBE0 (class:BrnGameState catch-all TU, ledger "BrnGameState::Ga") = the bounds-checked
// const indexed accessor. The X360 body is the "Array used before Construct/Clear was called"
// (CgsArray.h:556) + "Array index out of bounds" (557) sequence returning &maElements[i]
// (stride 32 == sizeof(TriggerQueryRecord); 16*32 == 512 == the *(this+512) count word the body reads).
// Read by TriggerQueryManager::PreWorldUpdate, which consumes the record's +0x10/+0x14 words.
template const BrnGameState::GameStateModuleIO::TriggerQueryRecord&
    Array<BrnGameState::GameStateModuleIO::TriggerQueryRecord, 16>::operator[](u32) const;

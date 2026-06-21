#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/SharedIO/BrnGameStateLeafContainers.h"

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the ENewRivalryCompare leaf instantiation -- the committed Array_/EventQueue_ explicit-instantiation pattern.
template void Array<BrnGameState::OnlineFlybyManager::ENewRivalryCompare, 4>::Append(const BrnGameState::OnlineFlybyManager::ENewRivalryCompare&);
// X360 0x82360718 (class:BrnGameState catch-all TU, ledger "BrnGameState::Onl") = the bounds-checked
// indexed accessor of this same instantiation. OnlineFlybyManager::AddNewRivalryMessage appends to and
// indexes this 4-element new-rivalry list (the sibling ::Append is at 0x8236DC60's call site). The X360
// body is the "Array used before Construct/Clear" (CgsArray.h:556) + "Array index out of bounds" (557)
// sequence returning &maElements[i] (stride 4; count word at owner+16 == 4*4). CONST overload
// (CgsArray.h line map: 538/539 non-const, 556/557 const -- this instance is attributed to 556/557).
template const BrnGameState::OnlineFlybyManager::ENewRivalryCompare& Array<BrnGameState::OnlineFlybyManager::ENewRivalryCompare, 4>::operator[](u32) const;

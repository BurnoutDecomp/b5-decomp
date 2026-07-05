// Per-instantiation .cpp for Array<s64,7> (X360 __int64 element, 8-byte stride,
// capacity 7). The generic Array<T,N> body (Append / GetItem / operator[] + siblings)
// is fully inline in CgsArray.h, so this TU is just the explicit class instantiation
// (the X360 emits one out-of-line copy per using-TU):
//   Array<__int64,7>::Append  @ 0x8235C7C8  (BrnGameState::GameStateModule::OnPlayerCarChange)
//   Array<__int64,7>::GetItem @ 0x822AE418  (BrnWorld::RaceCarEntityModule::HandleSetPlayerOpponentsAction)
//
// Layout: maElements[7] (7*8 = 56B) + miCount @ +0x38, matching the X360 *(this+0x38)
// count word and the 8*index + base (== &maElements[index]) bounds-checked returns.
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<long long,7u>. s64 == int64_t (types.hpp).
// A primitive element needs no element_home include.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<s64, 7>;

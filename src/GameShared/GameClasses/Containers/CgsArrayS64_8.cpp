// Per-instantiation .cpp for Array<s64, 8>. The generic Array<T,N> body
// (Append / GetItem / GetLength + siblings) is fully inline in CgsArray.h, so this TU
// is just the explicit class instantiation (the X360 emits one out-of-line copy per
// using-TU). The three methods the X360 emitted for this instance:
//   Array<s64,8>::Append    @ 0x8235C428  (BrnGameState::GameStateModule::ProcessGameEvents)
//   Array<s64,8>::GetItem   @ 0x8235C5A0  (BrnGame::BrnGameModule::TranslateGameActionsToGuiEvents)
//   Array<s64,8>::GetLength @ 0x8235C548  (BrnGameState::GameStateModule::ProcessGameEvents)
//
// Layout: maElements[8] (8 * 8 = 64B; each element is one 64-bit word) + miCount
// @ +0x40, matching the X360 count word read/written at *(a1+0x40), the slwi-by-3
// element addressing (8-byte stride), and the single-dword element copy (ld/stdx).
// Capacity N=8 is the `cmplwi 8` / `li r4,8` bound in the Append/GetItem asserts.
//
// The X360 asserts at CgsArray.h:225/226 (Append: unconstructed / out-of-space),
// :538/539 (GetItem: unconstructed / out-of-bounds), and :336 (GetLength: unconstructed).
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<__int64,8u>. s64 == int64_t (types.hpp);
// a primitive element needs no element_home include.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<s64, 8>;

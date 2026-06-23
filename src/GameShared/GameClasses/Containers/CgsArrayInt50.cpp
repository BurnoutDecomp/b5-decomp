// Per-instantiation .cpp for Array<s32,50>. The generic Array<T,N> body (GetItem +
// siblings) is fully inline in CgsArray.h, so this TU is just the explicit class
// instantiation (the X360 emits one out-of-line copy per using-TU):
//   Array<int,50>::GetItem  @ 0x822013F8  (BrnDirector::ShotSelector::Construct,
//                                           BrnDirector::MainDirector::Update)
// Layout: maElements[50] (200B) + miCount @ +0xC8, matching the X360 *(a1+0xC8) count
// word and the `4*index + a1` (== &maElements[index]) bounds-checked return.
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<int,50u>. int == s32 (types.hpp). A
// primitive element needs no element_home include.
//
// NOTE (generic-body parity gap, NOT re-forked here): the X360 Array<int,50>::GetItem
// carries the unconstructed (CgsArray.h:556) + out-of-bounds (CgsArray.h:557, dynamic
// "Array index out of bounds. Index: <i>, length: <n>") asserts. The committed generic
// operator[]/GetItem keeps the unconstructed + bounds checks as static CGS_ASSERT
// strings. Fixing the dynamic-message form must GROW the shared CgsArray.h body so every
// instantiation re-verifies; it is deliberately not specialized in this thin TU.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<s32, 50>;

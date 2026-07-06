// Per-instantiation .cpp for Array<s32,64>. The generic Array<T,N> body (Append +
// operator[] + GetItem + siblings) is fully inline in CgsArray.h, so this TU is just
// the explicit class instantiation (the X360 emits one out-of-line copy per using-TU):
//   Array<int,64>::Append      @ 0x8268F760
//     (BrnSound::Logic::Collision::CollisionStateManager::UpdateParams)
//   Array<int,64>::operator[]  @ 0x8268F880
//     (BrnSound::Logic::Collision::CollisionStateManager::UpdateParams)
// Layout: maElements[64] (256B) + miCount @ +0x100, matching the X360 *(a1+0x100) count
// word and the `4*index + a1` (== &maElements[index]) bounds-checked return / append store.
//
// The Append X360 asserts at CgsArray.h:225 (unconstructed, count == -1 sentinel) and :226
// (out-of-space, count >= 64) -- the ctor/Append line pair. It streamed the dynamic
// 'Array container out of space, Length: <n>, Capacity: 64' message, collapsed here to the
// committed generic static CGS_ASSERT string. operator[] asserts at the distinct :556/:557
// pair. All shared by the inline generic body, so nothing is re-forked in this TU.
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h).
// int == s32 (types.hpp). The concrete element FQN is not attested in this slice
// (UpdateParams / the 64-entry generator-index tables are deferred in
// BrnCollisionStateManager); spelled Array<s32,64> to match the primitive 4-byte stride.
// A primitive element needs no element_home include.
//
// NOTE (generic-body parity gap, NOT re-forked here): the committed generic operator[]
// keeps the unconstructed + bounds checks as static CGS_ASSERT strings rather than the
// X360 dynamic 'Array index out of bounds. Index: <i>, length: <n>' form; fixing that must
// grow the shared CgsArray.h body, so it is deliberately not specialized here (same
// disposition as CgsArrayInt50.cpp).
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<s32, 64>;

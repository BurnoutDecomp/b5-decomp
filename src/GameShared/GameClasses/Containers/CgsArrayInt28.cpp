// Per-instantiation .cpp for Array<s32,28>. The generic Array<T,N> body (Append / GetItem +
// siblings) is fully inline in CgsArray.h, so this TU is just the explicit class instantiation
// (the X360 emits one out-of-line copy per using-TU):
//   Array<int,28>::Append  @ 0x821FCA00  (BrnDirector::Camera::BehaviourManager::Construct)
//   Array<int,28>::GetItem @ 0x821FFCE0  (BehaviourManager::SetBehaviourUsedByHandle /
//                                         UnSetBehaviourUsedByHandle / LockBehaviourForInterpolation
//                                         / NewBehaviour<...> + 21 more BehaviourManager callers)
// Layout: maElements[28] (112B) + miCount @ +0x70, matching the X360 result[28]/*(a1+112) count
// word and the `4*index + a1` (== &maElements[index]) bounds-checked GetItem return.
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<int,28u>. int == s32 (types.hpp). A primitive
// element needs no element_home include.
//
// NOTE (generic-body parity gap, NOT re-forked here): the X360 Array<int,28>::GetItem carries the
// unconstructed (CgsArray.h:556) + dynamic out-of-bounds (CgsArray.h:557, "Array index out of
// bounds. Index: <i>, length: <n>") asserts. The committed generic GetItem/operator[] keeps the
// unconstructed + bounds checks as static CGS_ASSERT strings; matching the dynamic-message form
// must GROW the shared CgsArray.h body so all instantiations re-verify, so it is not specialized
// in this thin TU.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<s32, 28>;

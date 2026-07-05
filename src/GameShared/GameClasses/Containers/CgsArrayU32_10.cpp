// Per-instantiation .cpp for Array<u32,10>. The generic Array<T,N> body (Append /
// operator[] + siblings) is fully inline in CgsArray.h, so this TU is just the explicit
// class instantiation (the X360 emits one out-of-line copy per using-TU):
//   Array<u32,10>::Append      @ 0x82201608  (Selector<uint32_t,10>::AddElement, mOutputArray)
//   Array<u32,10>::operator[]  @ 0x82202998  (Selector<uint32_t,10>::GetSelection/CalculateIntervals)
// Layout: maElements[10] (40B) + miCount @ +0x28, matching the X360 result[10]/*(a1+0x28)
// count word (lwz r11,0x28(r29); -1 sentinel) and the slwi-by-2 u32 store at
// maElements[miCount] (stwx r10,r11,r29; ++count). operator[] returns &maElements[index]
// = 4*index + base (slwi r28,2 / add r29).
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<uint32_t,10u>. uint32_t == u32 (types.hpp).
// A primitive element needs no element_home include.
//
// NOTE (generic-body parity gap, NOT re-forked here): the X360 Array<u32,10> Append/operator[]
// carry the unconstructed + out-of-space/bounds asserts that stream dynamic messages
// ("Array container out of space, Length: <n>, Capacity: 10" @ CgsArray.h:225/226;
// "Array index out of bounds. Index: <i>, length: <n>" @ :538/539). The committed generic
// body keeps those as static CGS_ASSERT strings; that shared-body fix is deliberately not
// specialized in this thin instantiation TU.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<u32, 10>;

// Per-instantiation .cpp for Array<f32,9>. The generic Array<T,N> body (Append /
// GetItem + siblings) is fully inline in CgsArray.h, so this TU is just the explicit
// class instantiation (the X360 emits one out-of-line copy per using-TU):
//   Array<float,9>::Append   @ 0x82202770  (Array<...,10>::CalculateIntervals)
//   Array<float,9>::GetItem  @ 0x82202890  (Array<...,10>::GetSelection)
// Layout: maElements[9] (36B) + miCount @ +0x24, matching the X360 result[9]/*(a1+0x24)
// count word (lwz r11,0x24(r29)) and the slwi-by-2/stfsx float store at maElements[miCount].
// GetItem returns &maElements[index] = 4*index + base (slwi r28,2 / add r29).
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<float,9u>. float == f32 (types.hpp).
// A primitive element needs no element_home include.
//
// NOTE (generic-body parity gap, NOT re-forked here): the X360 Array<float,9> Append/GetItem
// carry the unconstructed + bounds/out-of-space asserts that stream dynamic messages
// ("Array container out of space, Length: <n>, Capacity: 9" @ CgsArray.h:225/226;
// "Array index out of bounds. Index: <i>, length: <n>" @ :556/557). The committed generic
// body keeps those as static CGS_ASSERT strings; that shared-body fix is deliberately not
// specialized in this thin instantiation TU.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<f32, 9>;

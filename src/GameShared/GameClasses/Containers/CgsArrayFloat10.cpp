// Per-instantiation .cpp for Array<f32,10>. The generic Array<T,N> body (Append /
// GetItem + siblings) is fully inline in CgsArray.h, so this TU is just the explicit
// class instantiation (the X360 emits one out-of-line copy per using-TU):
//   Array<float,10>::Append  @ 0x821FDAD0  (BrnDirector::MomentSelector::AddMoment,
//                                            Array<...,10>::AddElement)
// Layout: maElements[10] (40B) + miCount @ +0x28, matching the X360 result[10]/*(a1+0x28)
// count word and the slwi-by-2/stfsx float store at maElements[miCount].
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<float,10u>. float == f32 (types.hpp);
// float32_t is NOT a project type. A primitive element needs no element_home include.
//
// NOTE (generic-body parity gap, NOT re-forked here): the X360 Array<float,10>::Append
// carries the unconstructed + out-of-space asserts that stream a dynamic
// "Array container out of space, Length: <n>, Capacity: 10" message (CgsArray.h:225/226).
// The committed generic Append keeps those as static CGS_ASSERT strings. Fixing the
// dynamic-message form must GROW the shared CgsArray.h body so every Array instantiation
// re-verifies; it is deliberately not specialized in this thin instantiation TU.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<f32, 10>;

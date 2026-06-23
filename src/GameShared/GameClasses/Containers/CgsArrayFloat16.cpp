// Per-instantiation .cpp for Array<f32,16>. The generic Array<T,N> body (Append /
// GetItem + siblings) is fully inline in CgsArray.h, so this TU is just the explicit
// class instantiation (the X360 emits one out-of-line copy per using-TU):
//   Array<float,16>::Append  @ 0x823182B8  (BrnGameState::ModeManager::
//                                            TransmitCheckPointDistancesToFinishLine)
// Layout: maElements[16] (64B) + miCount @ +0x40, matching the X360 result[16]/*(a1+0x40)
// count word and the slwi-by-2/stfsx float store at maElements[miCount].
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<float,16u>. float == f32 (types.hpp);
// float32_t is NOT a project type. A primitive element needs no element_home include.
//
// NOTE (generic-body parity gap, NOT re-forked here): the X360 Array<float,16>::Append
// carries the unconstructed + out-of-space asserts that stream a dynamic
// "Array container out of space, Length: <n>, Capacity: 16" message (CgsArray.h:225/226).
// The committed generic Append keeps those as static CGS_ASSERT strings. Fixing the
// dynamic-message form must GROW the shared CgsArray.h body so every Array instantiation
// re-verifies; it is deliberately not specialized in this thin instantiation TU.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<f32, 16>;

// Per-instantiation .cpp for Array<CgsGui::AnimData::AnimatorChannel,6>. The generic
// Array<T,N> body (Append + siblings) is fully inline in CgsArray.h, so this TU is just the
// explicit member instantiation the X360 emits out-of-line for this element type:
//   Array<AnimData::AnimatorChannel,6>::Append @ 0x8284D478
//       (called by CgsGui::AnimData::AddAnimationChannel)
// Byte-parity check against the X360 Append pseudocode for this instantiation:
//   element stride 4 (AnimatorChannel is the enum -> a 32-bit word), capacity 6, miCount @
//   +0x18 (24 == 6*4) -> matches the X360's `result[6]`/`*(a1+0x18)` count word, the
//   unconstructed (-1) and out-of-space (>= 6) asserts (CgsArray.h:225/226), the single-dword
//   store `v2[v2[6]++] = *a2`, and the post-increment of the count.
//
// Instantiating Append only -- the comparison-dependent members (FindFirstInstanceOf/Contains)
// are not exercised here. (Enums are trivially comparable, but keeping to Append matches the
// single out-of-line method the X360 emitted for this instantiation.)
//
// Spelled unqualified Array<...> to match the committed container convention (CgsArrayInt8.cpp);
// the DWARF spells the type CgsContainers::Array<CgsGui::AnimData::AnimatorChannel,6u>.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimData.h"

template void Array<CgsGui::AnimData::AnimatorChannel, 6>::Append(const CgsGui::AnimData::AnimatorChannel&);

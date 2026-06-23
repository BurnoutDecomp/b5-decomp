// Per-instantiation .cpp for Array<CgsGui::AnimChannelData,2>. The generic Array<T,N> body
// (Append + siblings) is fully inline in CgsArray.h, so this TU is just the explicit member
// instantiation the X360 emits out-of-line for this element type:
//   Array<AnimChannelData,2>::Append @ 0x824E5CF8
//       (called by BrnGui::Animator::AddAnimationChannelToLibrary)
// Byte-parity check against the X360 Append pseudocode for this instantiation:
//   element stride 24 (sizeof AnimChannelData == 0x18: 6 dwords), capacity 2, miCount @ +0x30
//   (48 == 2*24) -> matches the X360's `result[12]`/`*(a1+0x30)` count word, the unconstructed
//   (-1) and out-of-space (>= 2) asserts (CgsArray.h:225/226), the 6-dword element copy into
//   `&v2[6 * v2[12]]`, and the post-increment of the count.
//
// Instantiating Append only -- the comparison-dependent members (FindFirstInstanceOf/Contains)
// are NOT instantiated, so AnimChannelData needs no operator==.
//
// Spelled unqualified Array<...> to match the committed container convention (CgsArrayInt8.cpp);
// the DWARF spells the type CgsContainers::Array<CgsGui::AnimChannelData,2u>.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimData.h"

// Pin the X360-proven element stride: Append @ 0x824E5CF8 copies 6 dwords into
// `&v2[6 * count]`, so sizeof(AnimChannelData) must be 24 (0x18).
static_assert(sizeof(CgsGui::AnimChannelData) == 24, "AnimChannelData stride 24 (X360 0x824E5CF8)");

template void Array<CgsGui::AnimChannelData, 2>::Append(const CgsGui::AnimChannelData&);

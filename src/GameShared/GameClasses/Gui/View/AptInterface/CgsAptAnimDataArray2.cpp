// Per-instantiation .cpp for Array<CgsGui::AnimData,2>. The generic Array<T,N> body
// (Append + siblings) is fully inline in CgsArray.h, so this TU is just the explicit member
// instantiation the X360 emits out-of-line for this element type:
//   Array<AnimData,2>::Append @ 0x824E5BD0
//       (called by BrnGui::Animator::AddAnimationToLibrary)
// Byte-parity check against the X360 Append pseudocode for this instantiation:
//   element stride 176 (sizeof AnimData == 0xB0), capacity 2, miCount @ +0x160 (352 == 2*176)
//   -> matches the X360's `*(a1+0x160)` count word, the unconstructed (-1) and out-of-space
//   (>= 2) asserts (CgsArray.h:225/226), the 0xB0-byte element copy `memcpy(176 * count + a1,
//   a2, 176)`, and the post-increment of the count. The generic Append's copy-assignment of a
//   trivially-copyable AnimData reproduces that 176-byte memcpy.
//
// Instantiating Append only -- the comparison-dependent members (FindFirstInstanceOf/Contains)
// are NOT instantiated, so AnimData needs no operator==.
//
// Spelled unqualified Array<...> to match the committed container convention (CgsArrayInt8.cpp);
// the DWARF spells the type CgsContainers::Array<CgsGui::AnimData,2u>.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimData.h"

// Pin the X360-proven element stride: Append @ 0x824E5BD0 copies the element with
// `memcpy(176 * count + a1, a2, 176)`, so sizeof(AnimData) must be 176 (0xB0). This also
// transitively pins the two embedded Array<>-member count offsets (mChannelData.miCount @
// +0x90, meChannels.miCount @ +0xAC) the AnimData ctor (@ 0x824E8FC8) stores -1 into.
static_assert(sizeof(CgsGui::AnimData) == 176, "AnimData stride 176 (X360 0x824E5BD0)");

template void Array<CgsGui::AnimData, 2>::Append(const CgsGui::AnimData&);

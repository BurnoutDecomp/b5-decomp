#include "GameShared/GameClasses/Containers/CgsArray.h"           // Array<T,N>::operator[] const (inline generic)
#include "GameSource/World/AI/Route/BrnRouteMapModule.h"          // BrnAI::SectionAndPortalIndices (8-byte element)

// Explicit instantiation of the generic Array<T,N> container method (inline in CgsArray.h) for the
// RacingLineGenerator's ExtrapolatedIndexArray leaf -- the committed Array_/EventQueue_ pattern.
//
// X360 0x8276AA08 = the bounds-checked indexed accessor of this instantiation
// (CgsContainers::Array<BrnAI::SectionAndPortalIndices,16u>, DWARF typedef ExtrapolatedIndexArray).
// Hex-Rays attributes the asserts to CgsArray.h:556/557 -- the CONST operator[] overload. The body:
//   * asserts miCount @ byte +0x80 != -1 sentinel  ("Array used before Construct/Clear was called"),
//   * bounds-checks (unsigned index >= miCount)     ("Array index out of bounds"),
//   * returns &maElements[index]  (slwi r11,r28,3 == index*8 stride; add r3,r11,r29 == base+off).
// Stride 8 and count word at +0x80 (== 16*8) confirm N=16 and sizeof(SectionAndPortalIndices) == 8.
// Callers: RacingLineGenerator::ExtrapolateRoute{Forwards,Backwards}/ExtrapolateTwistyRoute,
// ResetOnTrackManager::Scan{Forwards,Backwards}AlongExtrapolatedRoute, RouteMapModule::ProcessExtrapolatedRoute.
template const BrnAI::SectionAndPortalIndices&
Array<BrnAI::SectionAndPortalIndices, 16>::operator[](u32) const;

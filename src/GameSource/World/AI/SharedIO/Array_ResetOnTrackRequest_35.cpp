#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h"

// Out-of-line per-instantiation bodies of Array<BrnAI::AIModuleIO::ResetOnTrackRequest, 35>.
// The X360 build emits each Array<T,N> member used by an instantiation out-of-line; the shared
// generic bodies live in CgsArray.h. ResetOnTrackManager holds an Array<ResetOnTrackRequest, 35>
// (capacity 35 == 0x23) and calls:
//   - Append @0x82769E88 (BrnAI::ResetOnTrackManager::PushResetOnTrackRequest):
//       asserts miCount != -1 (CgsArray.h:225) and miCount < 0x23 (CgsArray.h:226), then stores the
//       four 32-bit element words at maElements[miCount] (`slwi r11,r11,4; add r29` -> 16-byte stride)
//       and bumps miCount. miCount lands at +0x230 (35 * 16 = 560 = 0x230 after maElements).
//   - Erase @0x82769FC0 (BrnAI::ResetOnTrackManager::Update):
//       asserts miCount != -1 (CgsArray.h:380) and luIndex < miCount ("Trying to erase an unused
//       element", CgsArray.h:381), decrements miCount, then order-preserving shifts the 16-byte tail
//       elements down one slot.
// Both match the generic CgsArray.h Append/Erase bodies for a 16-byte element; this TU only forces
// their emission for the ,35> instantiation.

template void Array<BrnAI::AIModuleIO::ResetOnTrackRequest, 35>::Append(
    const BrnAI::AIModuleIO::ResetOnTrackRequest&);
template void Array<BrnAI::AIModuleIO::ResetOnTrackRequest, 35>::Erase(u32);

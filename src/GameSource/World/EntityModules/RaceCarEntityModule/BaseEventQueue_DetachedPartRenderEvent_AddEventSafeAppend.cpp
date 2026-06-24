#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // BaseEventQueue<T>::AddEventSafe/Append (inline generic)
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnDetachedPartRenderEvent.h" // BrnWorld::DetachedPartRenderEvent (80-byte element)

// CgsModule::BaseEventQueue<BrnWorld::DetachedPartRenderEvent>::AddEventSafe @ 0x822C88B0
// CgsModule::BaseEventQueue<BrnWorld::DetachedPartRenderEvent>::Append       @ 0x822CAB70
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic BaseEventQueue<T>::AddEventSafe/Append
// bodies are already inline in CgsBaseEventQueue.h; these are the thin explicit instantiations.
//
// AddEventSafe (0x822C88B0) asserts mpEvents != NULL (CgsBaseEventQueue.h:331), then is
// bounds-gated: `lwz miLength,8; lwz miMaxLength,4; bge => return 0` WITHOUT appending when full;
// otherwise bumps miLength and copies the element to mpEvents[miLength]. Element stride is 80
// bytes: the X360 body does `slwi r9,miLength,2; add r11,miLength,r9` (== miLength*5),
// `slwi r11,r11,4` (*16) == miLength*80, then four 16-byte SIMD stores (lvx128/stvx128 @0/+16/
// +32/+48 = the 64-byte Matrix44Affine) plus `stw r10,0x40` (EntityId dword @ +0x40) and
// `stb r10,0x44` (u8 muPartIndex @ +0x44). The trailing +0x45..+0x4F is alignas(16) tail padding
// the writer leaves untouched; the generic POD `mpEvents[miLength] = lEvent` reproduces the live
// 72-byte field copy (the SIMD lvx/stvx pair is a vector-register block move of the POD, not math).
// Returns 1 on success, 0 when full.
//
// Append (0x822CAB70) asserts mpEvents != NULL (CgsBaseEventQueue.h:413), no overflow
// (:414, "Base event queue overflow"), and the source owns a buffer (:486 via GetQueueStartPointer)
// -- all non-gating -- then block-copies the source's live events (XMemCpy with
// `slwi r9,miLength,2; add r9,miLength,r9; slwi count,r9,4` == 80*count bytes, dest the matching
// 80*miLength offset) and advances miLength by source's miLength. Returns 1.
//
// sizeof(DetachedPartRenderEvent) == 80 (alignas(16): Matrix44Affine(64) + EntityId(4) + u8(1),
// padded to 0x50), matching the 80-byte stride in both bodies. AddEventSafe is called by
// BrnWorld::RaceCarEntityModule::ReadUpdatedActiveRaceCarDataFromPhysics; Append by
// BrnWorld::RaceCarRenderParams::operator=.
template bool
CgsModule::BaseEventQueue<BrnWorld::DetachedPartRenderEvent>::AddEventSafe(
    const BrnWorld::DetachedPartRenderEvent&);

template bool
CgsModule::BaseEventQueue<BrnWorld::DetachedPartRenderEvent>::Append(
    const CgsModule::BaseEventQueue<BrnWorld::DetachedPartRenderEvent>&);

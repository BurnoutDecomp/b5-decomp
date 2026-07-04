#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackDebugComponentTypes.h"
#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"

// Explicit instantiation of
//   CgsContainers::RingBuffer<BrnAI::ResetOnTrackDebugComponent::ResetEntry>::Push.
// X360 @0x82769BA8 (the only RingBuffer member this instantiation emits out-of-line for
// ResetEntry; called by ResetOnTrackDebugComponent::PushResetInfo).
//
// The shared generic body lives in CgsRingBuffer.h: it copies the 80-byte ResetEntry into
// mpData[miWritePos] (the asm's ten 8-byte stores + 80-byte stride `80 * miWritePos`),
// advances and wraps miWritePos, grows miLength, and -- when the buffer is already full --
// clamps miLength to miMaxLength, advances miReadPos with wrap, and asserts read==write
// ("Read pos should equal write pos if buffer is full", CgsRingBuffer.h:137; the X360 folds
// in the StrStream message build + the `tw` divide-trap guards, which the CGS_ASSERT /
// language `%` operator subsume). The owning component embeds it as
// FixedRingBuffer<ResetEntry,16> mResetRingBuffer. Mirrors the sibling
// RingBuffer<HngTestEntry>::Push instantiation (RingBuffer_HngTestEntry.cpp, X360
// @0x82769D60), differing only in the element stride (80 vs 48).

template void CgsContainers::RingBuffer<
    BrnAI::ResetOnTrackDebugComponent::ResetEntry>::Push(
        const BrnAI::ResetOnTrackDebugComponent::ResetEntry* lpEntry);

// operator[](u32) const @0x82769CD0 (caller BrnAI::ResetOnTrackDebugComponent::RenderHUD):
// the checked live-window read of the reset-history ring buffer. Generic body inline in
// CgsRingBuffer.h; range-asserts the index against miLength ('Attempt to access element
// outside range', CgsRingBuffer.h:258) then returns mpData[(miReadPos + luIndex) % miMaxLength]
// as mpData + index*80 (stride 80 == sizeof(ResetEntry): asm index*(4+1)*16). This TU only
// forces the out-of-line emission for the ResetEntry instantiation.
template const BrnAI::ResetOnTrackDebugComponent::ResetEntry&
CgsContainers::RingBuffer<BrnAI::ResetOnTrackDebugComponent::ResetEntry>::operator[](u32) const;

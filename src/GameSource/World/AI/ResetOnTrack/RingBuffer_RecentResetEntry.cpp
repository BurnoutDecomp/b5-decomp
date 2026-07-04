#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManagerTypes.h"
#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"

// Explicit instantiation of
//   CgsContainers::RingBuffer<BrnAI::ResetOnTrackManager::RecentResetEntry>::Push.
// X360 @0x8276A090 (the only RingBuffer member this instantiation emits out-of-line for
// RecentResetEntry; called by ResetOnTrackManager::ProcessResetOnTrackRequest).
//
// The shared generic body lives in CgsRingBuffer.h: it copies the 32-byte RecentResetEntry
// into mpData[miWritePos] (the asm's four 8-byte stores + 32-byte stride `slwi r11,r11,5`),
// advances and wraps miWritePos, grows miLength, and -- when the buffer is already full --
// clamps miLength to miMaxLength, advances miReadPos with wrap, and asserts read==write
// ("Read pos should equal write pos if buffer is full", CgsRingBuffer.h:137; the X360 folds
// in the StrStream message + `tw`-trap divide guards which the CGS_ASSERT / language
// operators subsume). The owning manager embeds it as
// FixedRingBuffer<RecentResetEntry,8> mRecentResets (DWARF BrnResetOnTrackManager.h:330).

template void CgsContainers::RingBuffer<
    BrnAI::ResetOnTrackManager::RecentResetEntry>::Push(
        const BrnAI::ResetOnTrackManager::RecentResetEntry* lpEntry);

// operator[](u32) @0x8276A1B0 (called by ResetOnTrackManager::Update). The generic operator[]
// body is inline in CgsRingBuffer.h: range-checks the index against miLength (CGS_ASSERT
// "Attempt to access element outside range"), then returns mpData[(miReadPos + index) % miMaxLength].
// Member offsets mpData@0, miMaxLength@4, miReadPos@8, miLength@0x10; 32-byte element stride
// (`slwi r11,r11,5`) == sizeof(RecentResetEntry)==32. Sibling to the committed ::Push instantiation
// @0x8276A090 above; this TU only forces the out-of-line emission.
template BrnAI::ResetOnTrackManager::RecentResetEntry&
CgsContainers::RingBuffer<BrnAI::ResetOnTrackManager::RecentResetEntry>::operator[](u32);

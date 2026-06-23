#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackDebugComponentTypes.h"
#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"

// Explicit instantiation of
//   CgsContainers::RingBuffer<BrnAI::ResetOnTrackDebugComponent::HngTestEntry>::Push.
// X360 @0x82769D60 (the only RingBuffer member this instantiation emits out-of-line for
// HngTestEntry; called by ResetOnTrackDebugComponent::PushHngTestInfo).
//
// The shared generic body lives in CgsRingBuffer.h: it copies the 48-byte HngTestEntry
// into mpData[miWritePos] (the asm's six 8-byte stores + 48-byte stride), advances and
// wraps miWritePos, grows miLength, and -- when the buffer is already full -- clamps
// miLength to miMaxLength, advances miReadPos with wrap, and asserts read==write
// ("Read pos should equal write pos if buffer is full", CgsRingBuffer.h:137; the X360
// folds in the StrStream message + `tw`-trap divide guards which the CGS_ASSERT /
// language operators subsume). The owning component embeds it as
// FixedRingBuffer<HngTestEntry,16> mHngTestRingBuffer.

template void CgsContainers::RingBuffer<
    BrnAI::ResetOnTrackDebugComponent::HngTestEntry>::Push(
        const BrnAI::ResetOnTrackDebugComponent::HngTestEntry* lpEntry);

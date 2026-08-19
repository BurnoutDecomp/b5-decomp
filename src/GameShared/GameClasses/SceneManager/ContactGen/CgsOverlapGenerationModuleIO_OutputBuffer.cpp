// ===========================================================================
// CgsSceneManager::OverlapGenerationIO::OutputBuffer -- lifetime + accessors.
//
// The broad phase's product: the candidate overlapping volume-instance pairs the
// SceneSweeper found this frame. OverlapGenerationModule::OutputCollidingPairs
// @0x828C1F58 fills it under a write lock; SceneManagerModule::
// BridgeOverlapGenerationToOverlapCulling @0x828BA538 drains it under a read lock.
// The whole buffer is that one queue.
//
// Class home: CgsOverlapGenerationModuleIO.h (console-attested -- the read accessor's
// own assert bakes that path with line 266). Neither Construct nor Destruct has an
// out-of-line X360 symbol; both are inlined into the IO buffer stack templates, which is
// where the bodies below are lifted from. Sibling of
// CgsOverlapGenerationModuleIO_InputBuffer.cpp (same split as the
// CgsGuiModuleIO_{Input,Output}Buffer.cpp pair).
// ===========================================================================

#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT (the lock tripwires)

namespace CgsSceneManager
{
namespace OverlapGenerationIO
{

// ---------------------------------------------------------------------------------
// OutputBuffer::Construct   (CgsOverlapGenerationModuleIO.h:258,
//                            CgsOverlapGenerationModuleIO.cpp:102)
//   No out-of-line symbol: the X360 inlines it whole into
//   CgsModule::IOBufferStack::CreateIOBuffer<OutputBuffer> @0x828CC780 --
//       lpBuffer = IOBufferStack::Alloc(262160)      -- sizeof(OutputBuffer), X360
//       *(u8*)lpBuffer = 1                           -- IOBuffer::Construct, inlined
//       bl OverlappingPair_16384_::Construct(lpBuffer + 4)
//       *(u32*)(lpBuffer + 12) = 0                   -- the queue's miLength (queue+8)
//   which is IOBuffer::Construct + EventQueue<OverlappingPair,16384>::Construct +
//   BaseEventQueue<OverlappingPair>::Clear -- exactly the three calls the DWARF's own
//   cpp hint lists for this function. The Clear is redundant after Construct and is
//   reproduced because the console makes the store.
//   ⚠️ 262,160 is the CONSOLE sizeof (queue at +4 behind a 4-byte-aligned event pointer);
//   on the x64 host the queue seats at +8 and the total grows by 8. Nothing here depends
//   on either number -- Alloc/Free size the type with sizeof.
// ---------------------------------------------------------------------------------
void OutputBuffer::Construct()
{
    CgsModule::IOBuffer::Construct();

    mOverlappingPairQueue.Construct();
    mOverlappingPairQueue.Clear();
}

// ---------------------------------------------------------------------------------
// OutputBuffer::Destruct   (CgsOverlapGenerationModuleIO.h:262,
//                           CgsOverlapGenerationModuleIO.cpp:123)
//   No out-of-line symbol: inlined into
//   CgsModule::IOBufferStack::DestroyIOBuffer<OutputBuffer> @0x828C5068 --
//       *(u32*)(lpBuffer + 12) = 0                   -- the queue's miLength (Clear)
//       bl CgsModule::IOBuffer::Destruct
//       bl IOBufferStack::Free(lpBuffer, 262160)
//   The DWARF cpp hint lists BaseEventQueue<OverlappingPair>::Destruct ahead of
//   IOBuffer::Destruct; a fixed-capacity queue's Destruct owns no storage and leaves no
//   code, so the surviving instruction is the length store.
// ---------------------------------------------------------------------------------
void OutputBuffer::Destruct()
{
    mOverlappingPairQueue.Clear();

    CgsModule::IOBuffer::Destruct();
}

// ---------------------------------------------------------------------------------
// The two accessors. Each is `return &mOverlappingPairQueue` behind the matching lock
// tripwire; the X360 emits both out of line and bakes this class's header path plus the
// declaration line into each assert:
//     0x828B0428 :266 read-lock  ("Not locked for reading\n")  -> this+4
//     0x828B04D0 :267 write-lock ("Not locked for writing\n")  -> this+4
// (both test the status byte directly: `(*this >> 4) & 1` for read, `(*this >> 3) & 1`
// for write -- eStatusLockedForRead / eStatusLockedForWrite.)
// ---------------------------------------------------------------------------------
const OutOverlappingPairQueue* OutputBuffer::GetOverlappingPairQueue() const
{
    CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
    return &mOverlappingPairQueue;
}

OutOverlappingPairQueue* OutputBuffer::GetOverlappingPairQueue()
{
    CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
    return &mOverlappingPairQueue;
}

} // namespace OverlapGenerationIO
} // namespace CgsSceneManager

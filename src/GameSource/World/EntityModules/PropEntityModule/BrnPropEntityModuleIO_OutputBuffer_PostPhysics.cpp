#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// ============================================================================
// BrnWorld::PropEntityIO::OutputBuffer_PostPhysics member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// ⭐⭐ REWRITTEN 2026-08-18 (wave Q round 2, world-side prop IO-buffer pass). The previous
// version of this TU bound three of its accessors to the WRONG member, because the class was
// a pile of 1-byte opaque placeholders and the binding had been guessed from the getters'
// return offsets alone rather than from Construct's member roll-call. See the big banner on
// the class in BrnPropEntityModuleIO.h for the full decode and the corrected table; in short,
// Construct @0x822EFE08 constructs the members at console +0x10 / +0x160 / +0x7B0 / +0x820 /
// +0x860 / +0xC86B0 / +0xCB2C0 / +0xCB5F0 and zeroes the bool at +0xCB61C, which makes
// +0x860 the SCENE interface, +0xC86B0 the PROP interface and +0xCB5F0 an X360-only
// BrnReplays::ReplayIO::RequestInterface -- one member later than the old labels claimed.
//
// This TU bodies the nine X360-emitted accessors plus Construct:
//   GetSceneInputInterface()             @ 0x822B9B28  W (bit 3)  -> &mSceneInputInterface
//   GetPropInputInterface()              @ 0x822B9BD0  W (bit 3)  -> &mPropInputInterface
//   GetHitOverheadSignQueue()            @ 0x822B9C78  W (bit 3)  -> &mHitOverheadSignQueue
//   GetBrokenPropQueue()                 @ 0x822B9D20  W (bit 3)  -> &mBrokenPropQueue
//   GetPropVFXLocatorQueue()             @ 0x822B9DC8  W (bit 3)  -> &mPropVFXLocatorQueue
//   GetRecordHitPropQueue()              @ 0x822B9E70  W (bit 3)  -> &mRecordHitPropQueue
//   GetPropBecamePhysicalEventQueue()    @ 0x822B9F18  W (bit 3)  -> &mPropBecamePhysicalEventQueue
//   GetReplayRequestInterface()          @ 0x822B9FC0  W (bit 3)  -> &mReplayRequestInterface
//   GetReplayRequestInterface() const    @ 0x827A2000  R (bit 4)  -> &mReplayRequestInterface
//   Construct()                          @ 0x822EFE08
//
// The const (read) handle tests the read-lock bit (`extrwi r11,r11,1,27`); every non-const
// (write) handle tests the write-lock bit (`extrwi r11,r11,1,28`) -- matching
// CgsModule::IOBuffer's IsBufferLockedForReading()/IsBufferLockedForWriting(). Each asserts
// the lock state ("Not locked for reading/writing\n", a non-gating tripwire whose baked line
// is BrnPropEntityModuleIO.h 741..748 in ascending member order), then returns the member's
// address. CGS_ASSERT stamps its own __FILE__/__LINE__, so the X360-baked path/line is
// recorded in the comments only. The rodata strings carry a trailing newline (VERBATIM).
// ============================================================================

namespace BrnWorld
{
namespace PropEntityIO
{
    // ------------------------------------------------------------------------------------
    // Layout gate. Every member is now a real host type, so there is NO host byte offset to
    // pin -- pinning the console offsets here is exactly the gotcha-1 trap the retype removed
    // (a host EventQueue<T,N> is up to 8 bytes wider than its console twin). What IS invariant
    // across both targets, and what the accessors depend on, is the member ORDER and the
    // adjacency Construct walks. That is what this asserts.
    // ------------------------------------------------------------------------------------
    void OutputBuffer_PostPhysics::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer_PostPhysics, mPropBecamePhysicalEventQueue)
                    < offsetof(OutputBuffer_PostPhysics, mRecordHitPropQueue),
                      "console +0x010 < +0x160");
        static_assert(offsetof(OutputBuffer_PostPhysics, mRecordHitPropQueue)
                    < offsetof(OutputBuffer_PostPhysics, mHitOverheadSignQueue),
                      "console +0x160 < +0x7B0");
        static_assert(offsetof(OutputBuffer_PostPhysics, mHitOverheadSignQueue)
                    < offsetof(OutputBuffer_PostPhysics, mBrokenPropQueue),
                      "console +0x7B0 < +0x820");
        static_assert(offsetof(OutputBuffer_PostPhysics, mBrokenPropQueue)
                    < offsetof(OutputBuffer_PostPhysics, mSceneInputInterface),
                      "console +0x820 < +0x860");
        static_assert(offsetof(OutputBuffer_PostPhysics, mSceneInputInterface)
                    < offsetof(OutputBuffer_PostPhysics, mPropInputInterface),
                      "console +0x860 < +0xC86B0");
        static_assert(offsetof(OutputBuffer_PostPhysics, mPropInputInterface)
                    < offsetof(OutputBuffer_PostPhysics, mPropVFXLocatorQueue),
                      "console +0xC86B0 < +0xCB2C0");
        static_assert(offsetof(OutputBuffer_PostPhysics, mPropVFXLocatorQueue)
                    < offsetof(OutputBuffer_PostPhysics, mReplayRequestInterface),
                      "console +0xCB2C0 < +0xCB5F0");
        static_assert(offsetof(OutputBuffer_PostPhysics, mReplayRequestInterface)
                    < offsetof(OutputBuffer_PostPhysics, mbShouldRequestProgression),
                      "console +0xCB5F0 < +0xCB61C");

        // Capacities, each named by its Construct callee's own IDA symbol at 0x822EFE34..
        // 0x822EFE80 -- the one fact about these queues that IS target-independent.
        static_assert(PropBecamePhysicalEventQueue::KI_LENGTH == 20,  "PropBecamePhysicalEvent<20>");
        static_assert(RecordHitPropQueue::KI_LENGTH           == 50,  "RecordPropHitEvent<50>");
        static_assert(HitOverheadSignQueue::KI_LENGTH         == 100, "HitOverheadSignEvent<100>");
        static_assert(BrokenPropQueue::KI_LENGTH              == 50,  "BrokenPropEvent<50>");
        static_assert(PropVFXLocatorQueue::KI_LENGTH          == 10,  "PropVFXLocatorEvent<10>");
    }

    // ========================================================================
    // OutputBuffer_PostPhysics::Construct   @ 0x822EFE08   (DWARF :719)
    // ------------------------------------------------------------------------
    // ⭐ NEW 2026-08-18 (wave Q round 2). It was declared NOWHERE and defined nowhere, so
    // IOBufferStack::CreateIOBuffer<OutputBuffer_PostPhysics> (BrnWorldModule.cpp:2941) bound
    // T::Construct to the inherited CgsModule::IOBuffer::Construct, which writes only the
    // status byte. Since the 2026-08-15 change that made CreateIOBuffer console-faithful (no
    // zero-fill), every embedded queue's mpEvents was therefore raw pool garbage, and the
    // first AddEvent of the frame would have written through it -- the two asserts inside
    // AddEvent are non-gating tripwires, so nothing stopped it. Nothing calls the queue
    // getters today, which is the only reason this has not fired.
    //
    // The asm, statement for statement (r31 == this, r30 == this + 0xC86B0):
    //   stb 1, 0(r31)                                      -> IOBuffer::Construct()
    //   bl InSceneUpdateInterface::Construct(r31+0x860)     -> mSceneInputInterface
    //   bl AddPhysicalPropEvent<50>::Construct(r30+0)       \
    //   bl RemovePhysicalPropEvent<300>::Construct(r30+0x1F60) |  == PropInputInterface::
    //   bl RemovePhysicalPartEvent<100>::Construct(r30+0x28CC) |     Construct(), inlined
    //   bl AddPhysicalPartEvent<50>::Construct(r30+0xFB0)      |
    //   stb 0, 0x2C00(r30)   == mbRemoveAllPropsAndParts     /
    //   bl BrokenPropEvent<50>::Construct(r31+0x820)        -> mBrokenPropQueue
    //   bl HitOverheadSignEvent<100>::Construct(r31+0x7B0)  -> mHitOverheadSignQueue
    //   bl PropVFXLocatorEvent<10>::Construct(r31+0xCB2C0)  -> mPropVFXLocatorQueue
    //   bl RecordPropHitEvent<50>::Construct(r31+0x160)     -> mRecordHitPropQueue
    //   bl PropBecamePhysicalEvent<20>::Construct(r31+0x10) -> mPropBecamePhysicalEventQueue
    //   li r10,0xB ; mtctr ; stw 0,0(r11) ; addi r11,4 ; bdnz   (r11 == r31+0xCB5F0)
    //                                                       -> mReplayRequestInterface's
    //                                                          eleven BaseSerialiser* slots
    //   stbx 0 at r31+0xCB61C                               -> mbShouldRequestProgression
    // The construction ORDER above is the console's own (it is NOT ascending offset order);
    // reproduced call for call.
    //
    // ⚠️ ONE DOCUMENTED DIFFERENCE, not a divergence: the console inlines only
    // PropInputInterface's four sub-Constructs plus its flag store here, whereas the committed
    // PropInputInterface::Construct() (BrnPropInputInterface.cpp:139) additionally runs its own
    // Clear(). Clear() only re-zeroes the four miLength words the queue Constructs just
    // zeroed, so calling the named method is idempotent-equal to the fold. Reaching the four
    // queues directly is impossible from here (they are private to that interface), and
    // forking them would be exactly the "don't padding-fork a type with a real home" break.
    //
    // The eleven-slot pointer clear is spelled out rather than delegated because
    // BrnReplays::ReplayIO::RequestInterface has NO Construct() in the tree (its home,
    // GameSource/Replays/BrnReplayRequestInterface.h, declares only Append and
    // RegisterSerialiser) -- and it is that header's owner's call to add one, not this TU's.
    // The loop is the console's, member-by-name; KI_MAX_SERIALISERS is the committed 11 that
    // RegisterSerialiser's own `cmpwi r11, 0xB` range assert pins.
    // ========================================================================
    void OutputBuffer_PostPhysics::Construct()
    {
        CgsModule::IOBuffer::Construct();

        mSceneInputInterface.Construct();
        mPropInputInterface.Construct();

        mBrokenPropQueue.Construct();
        mHitOverheadSignQueue.Construct();
        mPropVFXLocatorQueue.Construct();
        mRecordHitPropQueue.Construct();
        mPropBecamePhysicalEventQueue.Construct();

        for (s32 liSerialiser = 0; liSerialiser < BrnReplays::ReplayIO::KI_MAX_SERIALISERS; ++liSerialiser)
        {
            mReplayRequestInterface.mapSerialisers[liSerialiser] = NULL;
        }

        mbShouldRequestProgression = false;
    }

    // X360 0x822B9B28 (W, line 741) -- write-lock; return the scene input interface
    // (console this+0x860). Rodata carries a trailing newline (VERBATIM).
    OutputBuffer_PostPhysics::SceneInputInterface* OutputBuffer_PostPhysics::GetSceneInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mSceneInputInterface;
    }

    // X360 0x822B9BD0 (W, line 742) -- write-lock; return the prop-manager input interface
    // (console this+0xC86B0). Its sole decompiled caller is
    // BrnWorld::PropEntityModule::ProcessContacts @0x822FA944, which immediately clears that
    // interface -- the independent confirmation that this getter is the PROP one.
    OutputBuffer_PostPhysics::PropInputInterface* OutputBuffer_PostPhysics::GetPropInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mPropInputInterface;
    }

    // X360 0x822B9C78 (W, line 743) -- write-lock; return the hit-overhead-sign event queue
    // (console this+0x7B0).
    OutputBuffer_PostPhysics::HitOverheadSignQueue* OutputBuffer_PostPhysics::GetHitOverheadSignQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mHitOverheadSignQueue;
    }

    // X360 0x822B9D20 (W, line 744) -- write-lock; return the broken-prop event queue
    // (console this+0x820).
    OutputBuffer_PostPhysics::BrokenPropQueue* OutputBuffer_PostPhysics::GetBrokenPropQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mBrokenPropQueue;
    }

    // X360 0x822B9DC8 (W, line 745) -- write-lock; return the prop-VFX-locator event queue
    // (console this+0xCB2C0).
    OutputBuffer_PostPhysics::PropVFXLocatorQueue* OutputBuffer_PostPhysics::GetPropVFXLocatorQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mPropVFXLocatorQueue;
    }

    // X360 0x822B9E70 (W, line 746) -- write-lock; return the record-hit-prop event queue
    // (console this+0x160).
    OutputBuffer_PostPhysics::RecordHitPropQueue* OutputBuffer_PostPhysics::GetRecordHitPropQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mRecordHitPropQueue;
    }

    // X360 0x822B9F18 (W, line 747) -- write-lock; return the prop-became-physical event queue
    // (console this+0x10).
    OutputBuffer_PostPhysics::PropBecamePhysicalEventQueue* OutputBuffer_PostPhysics::GetPropBecamePhysicalEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mPropBecamePhysicalEventQueue;
    }

    // X360 0x822B9FC0 (W, line 748) -- write-lock; return the X360-only replay request
    // interface (console this+0xCB5F0). PostPhysicsUpdate @0x823032A0 calls exactly this
    // getter and hands the result to ReplayIO::RequestInterface::RegisterSerialiser.
    OutputBuffer_PostPhysics::ReplayRequestInterface* OutputBuffer_PostPhysics::GetReplayRequestInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mReplayRequestInterface;
    }

    // X360 0x827A2000 (R) -- read-lock; the const twin of the getter above, same member.
    const OutputBuffer_PostPhysics::ReplayRequestInterface* OutputBuffer_PostPhysics::GetReplayRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mReplayRequestInterface;
    }
}
}

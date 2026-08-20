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

    // ========================================================================================
    // ⭐ THE READ-LOCK (const) TWINS -- ADDED 2026-08-20 (gateui wave, owner `wire`).
    //
    // These are NOT host inventions. All six are real X360 out-of-line symbols; they were
    // simply left UNNAMED by IDA (`sub_827A1CB8` … `sub_827A1F58`), so the JSON export set --
    // which is keyed on names -- appears not to contain them and a grep concludes "no const
    // twin exists". MEASURED on a private BURNOUT_X360_ARTIST.XEX.i64 copy by enumerating every
    // function start in 0x827A1600..0x827A2100 and decompiling each. Each body is the same
    // three-step shape as its non-const twin, with the READ bit:
    //     lbz r11,0(this) ; extrwi r11,r11,1,27      (IOBuffer::IsBufferLockedForReading)
    //     -> "Not locked for reading\n" against
    //        ..\..\..\GameSource\World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h
    //     -> return this + <offset>
    //   0x827A1CB8 line 732 -> +0x860    mSceneInputInterface
    //   0x827A1D60 line 734 -> +0x7B0    mHitOverheadSignQueue
    //   0x827A1E08 line 736 -> +0xCB2C0  mPropVFXLocatorQueue
    //   0x827A1EB0 line 737 -> +0x160    mRecordHitPropQueue
    //   0x827A1F58 line 738 -> +0x10     mPropBecamePhysicalEventQueue
    //   0x827A2000 line 739 -> +0xCB5F0  mReplayRequestInterface   (already committed, below)
    // Lines 733/735 are absent because the const twins of GetPropInputInterface and
    // GetBrokenPropQueue have no call site; every emitted twin's baked line is its non-const
    // twin's minus 9, which independently re-confirms the 741..748 binding this TU documents.
    //
    // Their only console consumer is WorldModule::BridgeEntityModulesToOutput_PostPhysics
    // @0x827AEEB0, which runs with this buffer READ-locked -- see that bridge's banner in
    // GameSource/World/Bridges/WorldBridgeEntityModulesToOutput.cpp.
    // ========================================================================================

    // X360 0x827A1CB8 (R, line 732) -- read-lock; the scene input interface (console +0x860).
    const OutputBuffer_PostPhysics::SceneInputInterface* OutputBuffer_PostPhysics::GetSceneInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mSceneInputInterface;
    }

    // X360 0x827A1D60 (R, line 734) -- read-lock; the hit-overhead-sign queue (console +0x7B0).
    const OutputBuffer_PostPhysics::HitOverheadSignQueue* OutputBuffer_PostPhysics::GetHitOverheadSignQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mHitOverheadSignQueue;
    }

    // X360 0x827A1E08 (R, line 736) -- read-lock; the prop-VFX-locator queue (console +0xCB2C0).
    const OutputBuffer_PostPhysics::PropVFXLocatorQueue* OutputBuffer_PostPhysics::GetPropVFXLocatorQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mPropVFXLocatorQueue;
    }

    // X360 0x827A1EB0 (R, line 737) -- read-lock; the record-hit-prop queue (console +0x160).
    // ⭐ THE gateui SEAM. PropEntityModule::ProcessContacts fills this queue through the WRITE
    // getter above (PropEntityModule_wQ2_03.cpp :: ProcessContacts); the world->output bridge
    // drains it through THIS one and re-posts every element into the world update-output's
    // GameEventQueue as game event 111 (E_EVENT_RECORD_PROP_HIT), which is the only path by
    // which a smashed prop reaches GameStateModule::ProcessGameEvents -> StuntManager::OnPropHit.
    const OutputBuffer_PostPhysics::RecordHitPropQueue* OutputBuffer_PostPhysics::GetRecordHitPropQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mRecordHitPropQueue;
    }

    // X360 0x827A1F58 (R, line 738) -- read-lock; the prop-became-physical queue (console +0x10).
    const OutputBuffer_PostPhysics::PropBecamePhysicalEventQueue* OutputBuffer_PostPhysics::GetPropBecamePhysicalEventQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mPropBecamePhysicalEventQueue;
    }

    // X360 0x827A2000 (R, line 739) -- read-lock; the const twin of the getter above, same member.
    const OutputBuffer_PostPhysics::ReplayRequestInterface* OutputBuffer_PostPhysics::GetReplayRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mReplayRequestInterface;
    }
}
}

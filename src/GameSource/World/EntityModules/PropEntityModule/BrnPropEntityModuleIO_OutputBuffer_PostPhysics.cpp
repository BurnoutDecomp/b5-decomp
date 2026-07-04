#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnWorld::PropEntityIO::OutputBuffer_PostPhysics member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the 3 X360-emitted accessors:
//
//   GetPropInputInterface() const @ 0x827A2000  -> &mPropInputInterface  (+833008),  read  (bit 4)
//   GetSceneInputInterface()      @ 0x822B9BD0  -> &mSceneInputInterface (+820912),  write (bit 3)
//   GetPropInputInterface()       @ 0x822B9FC0  -> &mPropInputInterface  (+833008),  write (bit 3)
//
// The const (read) handle tests the read-lock bit (((*a1 >> 4) & 1), `extrwi r11,r11,1,27`); the
// non-const (write) handles test the write-lock bit (((*a1 >> 3) & 1), `extrwi r11,r11,1,28`) --
// matching CgsModule::IOBuffer's IsBufferLockedForReading()/IsBufferLockedForWriting(). The X360
// asserts the lock state (streaming "Not locked for reading/writing\n", a non-gating tripwire at
// BrnPropEntityModuleIO.h:739/742/748), then returns the member's address: this + 820912
// (`addis rN,0xD; addi rN,-0x7950`) for mSceneInputInterface, this + 833008
// (`addis rN,0xD; addi rN,-0x4A10`) for mPropInputInterface.
//
// The read-lock and write-lock GetPropInputInterface both return the same +833008 member (the
// const/non-const overload pair); GetSceneInputInterface returns the lower +820912 member. The
// DWARF (BrnPropEntityModuleIO.h:752/753) lays mSceneInputInterface before mPropInputInterface, so
// Scene is the lower offset -- consistent with the asm return offsets.

namespace BrnWorld
{
namespace PropEntityIO
{
    void OutputBuffer_PostPhysics::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer_PostPhysics, mPropBecamePhysicalEventQueue) == 16,     "mPropBecamePhysicalEventQueue @16");
        static_assert(offsetof(OutputBuffer_PostPhysics, mRecordHitPropQueue)  == 0x160,  "mRecordHitPropQueue @0x160");
        static_assert(offsetof(OutputBuffer_PostPhysics, mBrokenPropQueue)     == 2080,   "mBrokenPropQueue @2080");
        static_assert(offsetof(OutputBuffer_PostPhysics, mHitOverheadSignQueue) == 2144,   "mHitOverheadSignQueue @2144");
        static_assert(offsetof(OutputBuffer_PostPhysics, mSceneInputInterface) == 820912, "mSceneInputInterface @820912");
        static_assert(offsetof(OutputBuffer_PostPhysics, mPropVFXLocatorQueue) == 832192, "mPropVFXLocatorQueue @832192");
        static_assert(offsetof(OutputBuffer_PostPhysics, mPropInputInterface)  == 833008, "mPropInputInterface @833008");
    }

    // X360 0x822B9B28 (W, :735) -- write-lock; return the hit-overhead-sign event queue (this+2144).
    // Called by BrnWorld::PropZoneManager::UpdateInstance.
    OutputBuffer_PostPhysics::HitOverheadSignQueueStorage* OutputBuffer_PostPhysics::GetHitOverheadSignQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mHitOverheadSignQueue;
    }

    // X360 0x827A2000: read-lock; return this + 833008.
    const OutputBuffer_PostPhysics::PropInputInterfaceStorage* OutputBuffer_PostPhysics::GetPropInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mPropInputInterface;
    }

    // X360 0x822B9BD0: write-lock; return this + 820912.
    OutputBuffer_PostPhysics::SceneInputInterfaceStorage* OutputBuffer_PostPhysics::GetSceneInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mSceneInputInterface;
    }

    // X360 0x822B9FC0: write-lock; return this + 833008.
    OutputBuffer_PostPhysics::PropInputInterfaceStorage* OutputBuffer_PostPhysics::GetPropInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mPropInputInterface;
    }

    // X360 0x822B9F18 (W, :747) -- write-lock; return the prop-became-physical event queue (this+16).
    // First output-buffer queue member (EventQueue<PropBecamePhysicalEvent,20>, 336B). Called by
    // BrnWorld::PropZoneManager::UpdateInstance. Rodata carries a trailing newline (VERBATIM).
    OutputBuffer_PostPhysics::PropBecamePhysicalEventQueueStorage* OutputBuffer_PostPhysics::GetPropBecamePhysicalEventQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mPropBecamePhysicalEventQueue;
    }

    // X360 0x822B9E70 (W, :746) -- write-lock; return the record-hit-prop event queue (this+0x160).
    // Called by BrnWorld::PropEntityModule::ProcessContacts. Rodata carries a trailing newline
    // (VERBATIM). LOW CONFIDENCE: the exact DWARF member name/type at +0x160 is not separately
    // attested; modelled as opaque queue storage carved at +0x160 (placeholder name).
    OutputBuffer_PostPhysics::RecordHitPropQueueStorage* OutputBuffer_PostPhysics::GetRecordHitPropQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mRecordHitPropQueue;
    }

    // X360 0x822B9D20 (W, :744) -- write-lock; return the broken-prop event queue (this+2080).
    // EventQueue<BrokenPropEvent,50> (span 64 to the next member @+2144). Called by
    // BrnWorld::PropEntityModule::ProcessContacts. Rodata carries a trailing newline (VERBATIM).
    OutputBuffer_PostPhysics::BrokenPropQueueStorage* OutputBuffer_PostPhysics::GetBrokenPropQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mBrokenPropQueue;
    }

    // X360 0x822B9DC8 (W, :745) -- write-lock; return the prop-VFX-locator event queue (this+832192).
    // EventQueue<PropVFXLocatorEvent,10> = 816B; +832192 + 816 = +833008 = mPropInputInterface. Called
    // by PropZoneManager::UpdateInstance / PropEntityModule::ProcessContacts. Rodata carries a trailing
    // newline (VERBATIM).
    OutputBuffer_PostPhysics::PropVFXLocatorQueueStorage* OutputBuffer_PostPhysics::GetPropVFXLocatorQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mPropVFXLocatorQueue;
    }
}
}

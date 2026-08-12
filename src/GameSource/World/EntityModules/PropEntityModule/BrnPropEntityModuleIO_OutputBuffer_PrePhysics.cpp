#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnWorld::PropEntityIO::OutputBuffer_PrePhysics member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the 4 X360-emitted accessors:
//
//   GetPropInputInterface() const     @ 0x827A1B68  -> &mPropInputInterface     (+4),      read  (bit 4)
//   GetPropInputInterface()           @ 0x822B99D8  -> &mPropInputInterface     (+4),      write (bit 3)
//   GetPropToTrafficInterface() const @ 0x827A1C10  -> &mPropToTrafficInterface (+11296),  read  (bit 4)
//   GetPropToTrafficInterface()       @ 0x822B9A80  -> &mPropToTrafficInterface (+11296),  write (bit 3)
//
// The const (read) handles test the read-lock bit (((*a1 >> 4) & 1), `extrwi r11,r11,1,27`); the
// non-const (write) handles test the write-lock bit (((*a1 >> 3) & 1), `extrwi r11,r11,1,28`) --
// matching CgsModule::IOBuffer's IsBufferLockedForReading()/IsBufferLockedForWriting(). The X360
// asserts the lock state, then returns the member's address (this + 4 / this + 11296). The
// Hex-Rays names were truncated (Ge / GetPropI); the read/write lock-bit split + return offsets +
// the OutputBuffer_PreScene/_Prepare/_PostPhysics sibling shape pin the two overload pairs.

namespace BrnWorld
{
namespace PropEntityIO
{
    void OutputBuffer_PrePhysics::_AssertLayout()
    {
        // ⭐⭐ REBASED 2026-08-10 (pre-physics bridge wave). This gate used to pin BOTH members
        // at their console byte offsets (+4 and +11296). mPropInputInterface is the REAL
        // Props::PropInputInterface now (see the header for why the opaque span was unsound),
        // and that type is **8-ALIGNED on the host** -- its four embedded event queues carry an
        // 8-byte mpEvents and its ResourceHandle is two pointers -- so it CANNOT sit at the
        // console's +4; the compiler pads the status-byte run out to 8. That is the correct x64
        // outcome, not a regression, and it is precisely the case the standing rule covers:
        // parity is BY NAMED MEMBER + SEQUENCE, and a console byte offset is only ever a bonus
        // pin on a chain that happens to allow it. This one no longer does.
        // What is still gated, and still has teeth: the member ORDER, that nothing is inserted
        // between the two, and that the interface has not silently shrunk back below its
        // console extent.
        static_assert(offsetof(OutputBuffer_PrePhysics, mPropInputInterface) >= 4,
                      "mPropInputInterface must follow the status byte (console +4)");
        static_assert(sizeof(PropInputInterfaceStorage) >= 11296 - 4 - 12,
                      "prop-input span smaller than the console span -- retype regressed");
        static_assert(offsetof(OutputBuffer_PrePhysics, mPropToTrafficInterface)
                          >= offsetof(OutputBuffer_PrePhysics, mPropInputInterface)
                             + sizeof(PropInputInterfaceStorage),
                      "mPropToTrafficInterface must follow mPropInputInterface (console +11296)");
        // ⭐ ADDED 2026-08-12 (prop-BOOT wave, agent B8): mPropToTrafficInterface is the real
        // PropToTrafficInterface now, not a 1-byte span. Console extent 140 + 332 == the two
        // embedded queues (EventQueue<TrafficLightKnockDownEvent,32> at +0, 12 + 32*4 == 140,
        // and EventQueue<TrafficLightRestoreEvent,80> at +140, 12 + 80*4 == 332) -- pinned by
        // Construct @0x822EFCF0's two calls at buffer+11296 and buffer+11436.
        static_assert(sizeof(PropToTrafficInterfaceStorage) >= 140 + 332,
                      "prop-to-traffic interface smaller than the console span -- retype regressed");
    }

    // ========================================================================
    // OutputBuffer_PrePhysics::Construct   @ 0x822EFCF0   (DWARF :688)
    // ------------------------------------------------------------------------
    // ⭐ NEW 2026-08-12 (prop-BOOT wave, agent B8) -- the same never-written hole as the
    // Prepare / PreScene twins: `Construct()` resolved to the inherited
    // CgsModule::IOBuffer::Construct, so the prop-input interface's four event queues and the
    // prop-to-traffic interface's two kept mpEvents == NULL.
    //
    // Store-for-store from the asm, r31 == this + 16 == &mPropInputInterface:
    //   stb 1, 0(this)                                       -> IOBuffer::Construct()
    //   bl  AddPhysicalPropEvent_50_::Construct(+16)           \
    //   bl  RemovePhysicalPropEvent_300_::Construct(+16+8032)   |  mPropInputInterface.Construct()
    //   bl  RemovePhysicalPartEvent_100_::Construct(+16+10444)  |  (inlined, four queues in member
    //   bl  AddPhysicalPartEvent_50_::Construct(+16+4016)       |   order, then the flag)
    //   stb 0, [+16+11264]                                     /
    //   bl  TrafficLightKnockDownEvent_32_::Construct(+11296)  \ mPropToTrafficInterface.Construct()
    //   bl  TrafficLightRestoreEvent_80_::Construct(+11436)    /
    //   stw 0, [+16+8/+4024/+8040/+10452]; stb 0, [+16+11264]  -> mPropInputInterface.Clear()
    //
    // ⚠️ NOTE ON THE HEADER'S "+4": mPropInputInterface is at console +16, NOT +4. Both
    // GetPropInputInterface overloads end in `addi r3, r28, 0x10` (0x827A1B68 / 0x822B99D8) and
    // Construct's r31 is `this + 16`; the "+4" in the older comments was a transcription slip.
    // It changes nothing here -- the member is reached by name, and on the host it lands at 16
    // anyway because PropInputInterface is 16-aligned (its queues hold alignas(16) elements).
    // The prop-to-traffic interface needs no Clear pass: EventQueue::Construct zeroes miLength.
    // ========================================================================
    void OutputBuffer_PrePhysics::Construct()
    {
        CgsModule::IOBuffer::Construct();

        mPropInputInterface.Construct();
        mPropToTrafficInterface.Construct();

        mPropInputInterface.Clear();
    }

    // X360 0x827A1B68 (own TU): read-lock; return this + 4.
    const OutputBuffer_PrePhysics::PropInputInterfaceStorage* OutputBuffer_PrePhysics::GetPropInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mPropInputInterface;
    }

    // X360 0x822B99D8 (own TU): write-lock; return this + 4.
    OutputBuffer_PrePhysics::PropInputInterfaceStorage* OutputBuffer_PrePhysics::GetPropInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mPropInputInterface;
    }

    // X360 0x827A1C10 (R, :698) -- read-lock; return the prop-to-traffic interface (this+11296).
    // Called by WorldModule::BridgePropModuleToTrafficModule_PrePhysics.
    const OutputBuffer_PrePhysics::PropToTrafficInterfaceStorage* OutputBuffer_PrePhysics::GetPropToTrafficInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mPropToTrafficInterface;
    }

    // X360 0x822B9A80 (W, :699) -- write-lock; return the prop-to-traffic interface (this+11296).
    // Called by BrnWorld::PropZoneManager::SendTrafficLightRestoreEvents,
    //           BrnWorld::PropEntityModule::ChangePropState.
    OutputBuffer_PrePhysics::PropToTrafficInterfaceStorage* OutputBuffer_PrePhysics::GetPropToTrafficInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mPropToTrafficInterface;
    }
}
}

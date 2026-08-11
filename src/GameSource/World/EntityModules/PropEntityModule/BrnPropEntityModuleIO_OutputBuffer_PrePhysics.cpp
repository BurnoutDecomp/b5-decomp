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
                          == offsetof(OutputBuffer_PrePhysics, mPropInputInterface)
                             + sizeof(PropInputInterfaceStorage),
                      "mPropToTrafficInterface must immediately follow mPropInputInterface (console +11296)");
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

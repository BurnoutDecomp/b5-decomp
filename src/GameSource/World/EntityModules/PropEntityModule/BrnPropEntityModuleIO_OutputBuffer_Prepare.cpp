#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnWorld::PropEntityIO::OutputBuffer_Prepare member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the 3 X360-emitted accessors:
//
//   GetResourceRequestInterface() @ 0x822B9690  -> &mResourceRequestInterface (+4),     write (bit 3)
//   GetPropInputInterface() const @ 0x827A1778  -> &mPropInputInterface (+819824),       read  (bit 4)
//   GetPropInputInterface()       @ 0x822B95E8  -> &mPropInputInterface (+819824),       write (bit 3)
//
// The const (read) handle tests the read-lock bit (((*a1 >> 4) & 1), `extrwi r11,r11,1,27`); the
// non-const (write) handles test the write-lock bit (((*a1 >> 3) & 1), `extrwi r11,r11,1,28`) --
// matching CgsModule::IOBuffer's IsBufferLockedForReading()/IsBufferLockedForWriting(). The X360
// asserts the lock state (streaming "Not locked for reading/writing\n", a non-gating tripwire at
// BrnPropEntityModuleIO.h:607/610/613), then returns the member's address (this + 4 / this + 819824
// via `addis rN,0xD; addi rN,-0x7FFC` == +4 and `addis rN,0xD; addi rN,-0x7D90` == +819824).

namespace BrnWorld
{
namespace PropEntityIO
{
    void OutputBuffer_Prepare::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer_Prepare, mResourceRequestInterface) == 4,      "mResourceRequestInterface @4");
        static_assert(offsetof(OutputBuffer_Prepare, mPropInputInterface)       == 819824, "mPropInputInterface @819824");
    }

    // X360 0x822B9690: write-lock; return this + 4.
    OutputBuffer_Prepare::ResourceRequestInterfaceStorage* OutputBuffer_Prepare::GetResourceRequestInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mResourceRequestInterface;
    }

    // X360 0x827A1778: read-lock; return this + 819824.
    const OutputBuffer_Prepare::PropInputInterfaceStorage* OutputBuffer_Prepare::GetPropInputInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mPropInputInterface;
    }

    // X360 0x822B95E8: write-lock; return this + 819824.
    OutputBuffer_Prepare::PropInputInterfaceStorage* OutputBuffer_Prepare::GetPropInputInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mPropInputInterface;
    }
}
}

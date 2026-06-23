#include "GameShared/GameClasses/System/Resource/CgsBundleLoaderModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsResource::BundleLoaderIO::OutputBuffer member function, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the one X360-emitted write-locked accessor:
//
//   GetPool() @ 0x828E21E0  -> &mPool (this + 4), write-locked (bit 3)
//
// The X360 body loads the 1-byte FlagSet status (lbz 0(this)), tests the write-lock bit
// (extrwi r11,r11,1,28 == (status >> 3) & 1, i.e. CgsModule::IOBuffer::eStatusLockedForWrite),
// asserts "Not locked for writing" when clear, then returns this + 4. We return the named
// member's address (semantic parity, not byte-match).

namespace CgsResource
{
namespace BundleLoaderIO
{
    void OutputBuffer::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer, mPool) == 0x0004, "mPool @0x0004");
    }

    // X360 0x828E21E0: write-lock; return this + 4.
    OutputBuffer::PoolStorage* OutputBuffer::GetPool()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");   // :143
        return &mPool;
    }
}
}

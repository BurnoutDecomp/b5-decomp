#include "GameShared/GameClasses/System/Resource/CgsBundleLoaderModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsResource::BundleLoaderIO::OutputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the two X360-emitted accessors:
//
//   GetPool()          @ 0x828E21E0  -> &mPool   (this + 4),       write-locked (bit 3)  (:143)
//   GetStream() const  @ 0x828E2528  -> &mStream (this + 0x1342C), read-locked  (bit 4)  (:151)
//
// Each loads the 1-byte FlagSet status (lbz 0(this)) and tests its lock bit, asserts when
// clear, then returns the payload address. GetStream()'s return is addis r3,this,1 (+0x10000)
// then addi r3,r3,0x342C == this + 0x1342C (78892). We return the named member's address
// (semantic parity, not byte-match).

namespace CgsResource
{
namespace BundleLoaderIO
{
    void OutputBuffer::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer, mPool) == 0x0004, "mPool @0x0004");
        static_assert(offsetof(OutputBuffer, mStream) == 0x1342C, "mStream @0x1342C");
    }

    // X360 0x828E21E0: write-lock; return this + 4.
    OutputBuffer::PoolStorage* OutputBuffer::GetPool()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");   // :143
        return &mPool;
    }

    // X360 0x828E2528: read-lock; return this + 0x1342C.
    const OutputBuffer::StreamStorage* OutputBuffer::GetStream() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");   // :151
        return &mStream;
    }
}
}

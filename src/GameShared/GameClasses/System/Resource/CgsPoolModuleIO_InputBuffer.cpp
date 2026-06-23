#include "GameShared/GameClasses/System/Resource/CgsPoolModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsResource::PoolIO::InputBuffer member function, reconstructed from BURNOUT_X360_ARTIST.XEX:
//
//   GetPoolInputQueue() @ 0x828E1A00 -> &mPoolInputQueue (+4), write-lock (bit 3)
//
// The X360 body tests the write-lock bit (((*a1 >> 3) & 1)) -- matching CgsModule::IOBuffer's
// IsBufferLockedForWriting() -- and on failure fires the "Not locked for writing" assert
// (CgsPoolModuleIO.h:234), then returns the member's address (this + 4).

namespace CgsResource
{
namespace PoolIO
{
    void InputBuffer::_AssertLayoutInput()
    {
        static_assert(offsetof(InputBuffer, mPoolInputQueue) == 0x0004, "mPoolInputQueue @0x0004");
    }

    // X360 0x828E1A00: write-lock; return this + 4.
    InputBuffer::PoolInputQueue* InputBuffer::GetPoolInputQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mPoolInputQueue;
    }
}
}

#include "GameShared/GameClasses/System/Resource/CgsLiveUpdateModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsResource::LiveUpdateIO::OutputBuffer member function, reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//
//   GetOutputQueue() const @ 0x826642D8 -> &mOutputQueue (+4), read-lock (bit 4)
//
// The X360 body tests the read-lock bit (((*a1 >> 4) & 1), via extrwi r11,r11,1,27) -- matching
// CgsModule::IOBuffer's IsBufferLockedForReading() -- and on failure fires the "Not locked for
// reading" assert (CgsLiveUpdateModuleIO.h:111), then returns the member's address (this + 4).

namespace CgsResource
{
namespace LiveUpdateIO
{
    void OutputBuffer::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer, mOutputQueue) == 0x0004, "mOutputQueue @0x0004");
    }

    // X360 0x826642D8: read-lock; return this + 4.
    const OutputBuffer::OutputQueueStorage* OutputBuffer::GetOutputQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mOutputQueue;
    }
}
}

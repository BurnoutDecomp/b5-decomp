#include "GameShared/GameClasses/System/Resource/CgsPoolModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// CgsResource::PoolIO::OutputBuffer member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. This TU bodies the 4 X360-emitted OutputBuffer functions:
//
//   GetPoolOutputQueue() const          @ 0x828E1AA8  -> &mPoolOutputQueue (+4),          read  (bit 4)
//   GetPoolResourceRequestQueue() const @ 0x828E1B50  -> &mPoolResourceRequestQueue (+8212), read  (bit 4)
//   GetPoolOutputQueue()                @ 0x828E1BF8  -> &mPoolOutputQueue (+4),          write (bit 3)
//   GetPoolResourceRequestQueue()       @ 0x828E1CA0  -> &mPoolResourceRequestQueue (+8212), write (bit 3)
//
// The const (read) handles test the read-lock bit (((*a1 >> 4) & 1)); the non-const (write)
// handles test the write-lock bit (((*a1 >> 3) & 1)) -- matching CgsModule::IOBuffer's
// IsBufferLockedForReading()/IsBufferLockedForWriting(). Each returns the member's address.

namespace CgsResource
{
namespace PoolIO
{
    // InputBuffer lifecycle + accessors (mirror of OutputBuffer; one embedded request queue at +4).
    void InputBuffer::Construct() { CgsModule::IOBuffer::Construct(); mPoolInputQueue.Construct(); }
    void InputBuffer::Destruct()  { mPoolInputQueue.Clear(); }
    const InputBuffer::PoolInputQueue* InputBuffer::GetPoolInputQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mPoolInputQueue;
    }
    InputBuffer::PoolInputQueue* InputBuffer::GetPoolInputQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mPoolInputQueue;
    }

    void OutputBuffer::_AssertLayout()
    {
        static_assert(offsetof(OutputBuffer, mPoolOutputQueue)          == 0x0004, "mPoolOutputQueue @0x0004");
        static_assert(offsetof(OutputBuffer, mPoolResourceRequestQueue) == 0x2014, "mPoolResourceRequestQueue @0x2014");
    }

    // Construct the IOBuffer base (sets the constructed status bit) + both embedded event queues.
    void OutputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();
        mPoolOutputQueue.Construct();
        mPoolResourceRequestQueue.Construct();
    }
    void OutputBuffer::Destruct()
    {
        mPoolOutputQueue.Clear();
        mPoolResourceRequestQueue.Clear();
    }

    // X360 0x828E1AA8: read-lock; return this + 4.
    const OutputBuffer::PoolOutputQueue* OutputBuffer::GetPoolOutputQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mPoolOutputQueue;
    }

    // X360 0x828E1B50: read-lock; return this + 8212.
    const OutputBuffer::PoolResourceRequestQueueStorage* OutputBuffer::GetPoolResourceRequestQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading");
        return &mPoolResourceRequestQueue;
    }

    // X360 0x828E1BF8: write-lock; return this + 4.
    OutputBuffer::PoolOutputQueue* OutputBuffer::GetPoolOutputQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mPoolOutputQueue;
    }

    // X360 0x828E1CA0: write-lock; return this + 8212.
    OutputBuffer::PoolResourceRequestQueueStorage* OutputBuffer::GetPoolResourceRequestQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing");
        return &mPoolResourceRequestQueue;
    }
}
}

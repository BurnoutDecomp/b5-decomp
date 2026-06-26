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
//
// PS3 DecFIGS confirms (via the one accessor it did NOT inline,
// CgsResource::PoolIO::InputBuffer::GetPoolInputQueue @0xD175EC): the write-lock test is
// (*this & 8) == 0 and the exact assert message is "Not locked for writing\n"
// (FireAssert path CgsPoolModuleIO.h:234) -- hence the trailing '\n' on the messages below.

namespace CgsResource
{
namespace PoolIO
{
    // InputBuffer lifecycle + accessors (mirror of OutputBuffer; one embedded request queue at +4).
    void InputBuffer::Construct() { CgsModule::IOBuffer::Construct(); mPoolInputQueue.Construct(); }
    void InputBuffer::Destruct()  { mPoolInputQueue.Clear(); }
    const InputBuffer::PoolInputQueue* InputBuffer::GetPoolInputQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mPoolInputQueue;
    }
    InputBuffer::PoolInputQueue* InputBuffer::GetPoolInputQueue()
    {
        // PS3 DecFIGS CgsResource::PoolIO::InputBuffer::GetPoolInputQueue (0xD175EC):
        // exact assert message is "Not locked for writing\n" (CgsPoolModuleIO.h:234).
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
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
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mPoolOutputQueue;
    }

    // X360 0x828E1B50: read-lock; return this + 8212.
    const OutputBuffer::PoolResourceRequestQueueStorage* OutputBuffer::GetPoolResourceRequestQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mPoolResourceRequestQueue;
    }

    // X360 0x828E1BF8: write-lock; return this + 4.
    OutputBuffer::PoolOutputQueue* OutputBuffer::GetPoolOutputQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mPoolOutputQueue;
    }

    // X360 0x828E1CA0: write-lock; return this + 8212.
    OutputBuffer::PoolResourceRequestQueueStorage* OutputBuffer::GetPoolResourceRequestQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mPoolResourceRequestQueue;
    }
}
}

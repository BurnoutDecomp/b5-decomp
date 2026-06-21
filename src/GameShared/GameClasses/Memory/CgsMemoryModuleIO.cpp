#include "GameShared/GameClasses/Memory/CgsMemoryModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

// CgsMemory::MemoryIO::InputBuffer accessors. Recovered from the X360 bodies
// (const @ 0x82869248, non-const @ 0x828E1040): each tests one IOBuffer status bit via
// the inherited query, asserts on failure, then returns the embedded queue (this+4).
namespace CgsMemory
{
namespace MemoryIO
{
    // X360 0x82869248 (const): assert read-locked (status bit 4), then hand back the queue.
    const InputBuffer::MemoryRequestQueue* InputBuffer::GetMemoryRequestQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mMemoryRequestQueue;
    }

    // X360 0x828E1040 (non-const): assert write-locked (status bit 3), then hand back the queue.
    InputBuffer::MemoryRequestQueue* InputBuffer::GetMemoryRequestQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mMemoryRequestQueue;
    }

    // OutputBuffer accessors -- mirror of InputBuffer (the response queue lives at this+4).
    const OutputBuffer::MemoryResponseQueue* OutputBuffer::GetMemoryResponseQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mMemoryResponseQueue;
    }

    OutputBuffer::MemoryResponseQueue* OutputBuffer::GetMemoryResponseQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mMemoryResponseQueue;
    }
}
}

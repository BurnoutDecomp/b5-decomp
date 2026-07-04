#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsSpatialPartitionManagerIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// CgsSceneManager::SpatialPartitionIO accessor bodies, recovered from the X360 bodies. Each
// tests one IOBuffer status bit via the inherited query, asserts on failure, then returns the
// embedded queue/buffer. Mirrors the committed CgsMemoryModuleIO.cpp / CgsGuiModuleIO getters.
//
//   InputBuffer_Update::GetSpatialPartitionUpdateQueue() const @ 0x828AFD98
//       read-lock (bit 4) -> &mSpatialPartitionUpdateQueue (+4)
//   OutputBuffer::GetCoarseResultBuffer()                       @ 0x828AFE40
//       write-lock (bit 3) -> &mCoarseResultBuffer (+20500/0x5014)
//
// The GetCoarseResultBuffer WRITE-bit check is faithful to the asm (a getter that guards on the
// write lock) and is NOT "corrected" to a read check.

namespace CgsSceneManager
{
namespace SpatialPartitionIO
{
    // X360 0x828AFD98: read-lock (bit 4) handle to the embedded spatial-partition update queue.
    const InputBuffer_Update::InSpatialPartitionUpdateQueue*
    InputBuffer_Update::GetSpatialPartitionUpdateQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mSpatialPartitionUpdateQueue;
    }

    // X360 0x828AFE40: write-lock (bit 3) handle to the embedded coarse query-result buffer.
    OutputBuffer::CoarseQueryResultBufferDefault*
    OutputBuffer::GetCoarseResultBuffer()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mCoarseResultBuffer;
    }
}
}

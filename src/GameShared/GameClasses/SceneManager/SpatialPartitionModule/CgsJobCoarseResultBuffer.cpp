// ============================================================================
// GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsJobCoarseResultBuffer.cpp
//
// CgsSceneManager::JobCoarseResultBuffer::Construct @0x828BB128. One caller:
// LooseOctree::Construct @0x828CA0B0, once per frustum-test job.
// ============================================================================

#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsJobCoarseResultBuffer.h"

#include "rw/rwcore_structs.h"   // rw::IResourceAllocator

namespace CgsSceneManager
{
    // The result pool comes from the allocator's DoAllocate (vtable +0x10 @0x828BB198) with the
    // single-lane descriptor { 0x4000, 0x80 } and a null name; the X360 stores the lane-zero
    // pointer at +0x88. Clear is inlined after it.
    void JobCoarseResultBuffer::Construct(rw::IResourceAllocator* lpAllocator)
    {
        mpu16Buffer = static_cast<u16*>(lpAllocator->AllocateMemoryResource(
            KU_JOB_RESULT_BUFFER_SIZE * sizeof(u16), 0x80u, nullptr));

        muNumQueries = 0;
        for (u32 luQuery = 0; luQuery < KU_JOB_BUFFER_MAX_NUM_QUERIES; ++luQuery)
        {
            maQueryOffsets[luQuery]    = 0;
            maQueryNumResults[luQuery] = 0;
        }
        muCurrentWriteOffset = 0;
    }
}

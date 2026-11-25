#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsJobCoarseResultBuffer.h"
#include "rw/rwcore_structs.h"

namespace CgsSceneManager
{
    // ARTIST 0x828BB128: allocate one 0x4000-byte, 128-aligned result run.
    void JobCoarseResultBuffer::Construct(rw::IResourceAllocator* lpAllocator)
    {
        mpu16Buffer = static_cast<u16*>(lpAllocator->AllocateMemoryResource(
            KU_JOB_RESULT_BUFFER_SIZE * sizeof(u16), 128, 0));
        muNumQueries = 0;
        for (u32 luQuery = 0; luQuery < KU_JOB_BUFFER_MAX_NUM_QUERIES; ++luQuery)
        {
            maQueryOffsets[luQuery] = 0;
            maQueryNumResults[luQuery] = 0;
        }
        muCurrentWriteOffset = 0;
    }
}

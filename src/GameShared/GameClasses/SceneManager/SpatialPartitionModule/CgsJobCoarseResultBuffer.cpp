#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsJobCoarseResultBuffer.h"
#include "rw/rwcore_structs.h"

namespace CgsSceneManager
{
    // ARTIST 0x828BB128: allocate one 0x4000-byte, 128-aligned result run.
    void JobCoarseResultBuffer::Construct(rw::IResourceAllocator* lpAllocator)
    {
        rw::ResourceDescriptor lDescriptor;
        for (u32 luLane = 0; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
        {
            lDescriptor.m_baseResourceDescriptors[luLane].m_size = 0;
            lDescriptor.m_baseResourceDescriptors[luLane].m_alignment = 1;
        }
        lDescriptor.m_baseResourceDescriptors[0].m_size = KU_JOB_RESULT_BUFFER_SIZE * sizeof(u16);
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 128;
        rw::Resource lResource = lpAllocator->DoAllocate(lDescriptor, 0);
        mpu16Buffer = static_cast<u16*>(lResource.m_baseResources[0]);
        muNumQueries = 0;
        for (u32 luQuery = 0; luQuery < KU_JOB_BUFFER_MAX_NUM_QUERIES; ++luQuery)
        {
            maQueryOffsets[luQuery] = 0;
            maQueryNumResults[luQuery] = 0;
        }
        muCurrentWriteOffset = 0;
    }
}

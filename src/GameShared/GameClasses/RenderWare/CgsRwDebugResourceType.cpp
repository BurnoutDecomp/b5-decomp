#include "GameShared/GameClasses/RenderWare/CgsRwDebugResourceType.h"
#include "rw/rwcore_structs.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::RwDebugResourceType::GetSerialisedResourceDescriptor @ 0x82667DA0
//   CgsResource::RwDebugResourceType::GetTypeID                       @ 0x826658F8
//
// GetSerialisedResourceDescriptor returns a fixed five-entry descriptor: the first
// entry is {size 4, alignment 4}, the remaining four are {0, 1}. There is no resource
// argument — every entry is constant.

namespace CgsResource
{
    static const uint32_t KU_RW_DEBUG_RESOURCE_TYPE_ID = 22;

    uint32_t RwDebugResourceType::GetTypeID() const
    {
        return KU_RW_DEBUG_RESOURCE_TYPE_ID;
    }

    ResourceDescriptor RwDebugResourceType::GetSerialisedResourceDescriptor(const void*) const
    {
        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = 4;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4;
        for (int li = 1; li < 5; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = 0;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        return lDescriptor;
    }
}

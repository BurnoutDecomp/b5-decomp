#include "GameShared/GameClasses/Sound/Playback/CgsRegistryResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::RegistryResourceType::GetSerialisedResourceDescriptor @ 0x... (CgsRegistryResourceType.h)
//   CgsResource::RegistryResourceType::GetTypeID                       -> return 40960
//
// GetSerialisedResourceDescriptor returns a five-entry descriptor whose serialised
// size = payload + name-data + 4*(stringCount + 7); entries 0/3/4 carry that size
// (entry 0 aligned to 4), the rest are empty.

namespace CgsResource
{
    namespace
    {
        struct Registry
        {
            u32 muUnknown0;
            u32 muStringCount;
            u32 muNameDataSize;
            u32 muUnknown12;
            u32 muPayloadSize;
        };
    }

    static const uint32_t KU_REGISTRY_RESOURCE_TYPE_ID = 40960;

    uint32_t RegistryResourceType::GetTypeID() const
    {
        return KU_REGISTRY_RESOURCE_TYPE_ID;
    }

    ResourceDescriptor RegistryResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const Registry* lpRegistry = static_cast<const Registry*>(lpResource);
        CGS_ASSERT(lpRegistry != nullptr, "lpRegistry");

        const u32 luSerialisedSize =
            lpRegistry->muPayloadSize + lpRegistry->muNameDataSize + 4 * (lpRegistry->muStringCount + 7);

        ResourceDescriptor lDescriptor;
        for (int li = 0; li < 5; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = 0;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        lDescriptor.m_baseResourceDescriptors[3].m_size      = luSerialisedSize;
        lDescriptor.m_baseResourceDescriptors[4].m_size      = luSerialisedSize;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = luSerialisedSize;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4;
        return lDescriptor;
    }
}

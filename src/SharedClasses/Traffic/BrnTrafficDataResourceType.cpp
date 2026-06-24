#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource + rw::BaseResourceDescriptors<5>
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTraffic::TrafficDataResourceType::FixDown   @ 0x82763E68
//   BrnTraffic::TrafficDataResourceType::GetTypeID @ 0x82752560
//
// FixDown forwards to BrnTraffic::TrafficData::FixDown (own TU); that call takes no
// delta, so the rw::Resource argument is unused. GetTypeID returns the lane-data id.

namespace BrnTraffic
{
    static const uint32_t KU_TRAFFIC_LANEDATA_RESOURCE_TYPE_ID = 65538;

    uint32_t TrafficDataResourceType::GetTypeID() const
    {
        return KU_TRAFFIC_LANEDATA_RESOURCE_TYPE_ID;
    }

    // GetSerialisedResourceDescriptor @ 0x82760660 (store-for-store). The serialised
    // traffic-data occupies one 16-byte-aligned block whose size is the
    // TrafficData::muSizeInBytes word read at +4 (`lwz r9, 4(r30)`). The X360 first
    // asserts that word is non-zero ("Uninitialised Traffic Data resource"); the
    // std packs {size = muSizeInBytes, align = 0x10}; entries 1..4 are {0,1}.
    CgsResource::ResourceDescriptor
    TrafficDataResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const TrafficData* lpData = static_cast<const TrafficData*>(lpResource);

        CGS_ASSERT(lpData->muSizeInBytes != 0u, "Uninitialised Traffic Data resource");

        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = lpData->muSizeInBytes;  // entry0 size  (*(a3+4))
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 0x10u;                  // entry0 align
        for (u32 luBlock = 1; luBlock < 5u; ++luBlock)
        {
            lDescriptor.m_baseResourceDescriptors[luBlock].m_size      = 0u;   // entry1..4 {0,1}
            lDescriptor.m_baseResourceDescriptors[luBlock].m_alignment = 1u;
        }
        return lDescriptor;
    }

    void TrafficDataResourceType::FixDown(void* lpResource, const rw::Resource&) const
    {
        static_cast<TrafficData*>(lpResource)->FixDown();
    }
}

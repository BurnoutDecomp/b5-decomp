#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"
#include "rw/rwcore_structs.h"   // rw::Resource (unused here, kept for the base type)

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

    void TrafficDataResourceType::FixDown(void* lpResource, const rw::Resource&) const
    {
        static_cast<TrafficData*>(lpResource)->FixDown();
    }
}

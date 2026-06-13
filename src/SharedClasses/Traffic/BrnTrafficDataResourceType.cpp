#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnTraffic::TrafficDataResourceType::FixDown   @ 0x82763E68
//   BrnTraffic::TrafficDataResourceType::GetTypeID @ 0x82752560
//
// FixDown forwards to BrnTraffic::TrafficData::FixDown (another, not-yet-
// reconstructed TU; forward-declared). GetTypeID returns the lane-data resource
// type id (E_BRN_TRAFFIC_LANEDATA_RESOURCED_TYPE == 65538).

namespace BrnTraffic
{
    struct TrafficData
    {
        int FixDown();
    };

    class TrafficDataResourceType
    {
    public:
        int FixDown(TrafficData* pData) { return pData->FixDown(); }
        int GetTypeID() { return KI_TYPE_ID; }

    private:
        static const int KI_TYPE_ID = 65538;
    };
}

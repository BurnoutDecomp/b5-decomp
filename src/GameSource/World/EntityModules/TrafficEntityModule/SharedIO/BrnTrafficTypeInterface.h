#pragma once

// Traffic-type query response payload. Reconstructed from the DecFIGS DWARF.
#include "BrnCommonTypes.h"                          // CgsID
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"  // BrnTraffic::VehicleClass

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // Response carrying a traffic vehicle's resolved class + type id.
    struct TrafficTypeResponse
    {
        u16          muVehicleIndex;
        VehicleClass meType;
        CgsID        mTypeId;
    };
}
}

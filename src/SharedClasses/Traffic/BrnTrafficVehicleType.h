#pragma once

// Traffic vehicle silhouette class. Recovered from the DecFIGS DWARF.
#include "types.hpp"

namespace BrnTraffic
{
    enum VehicleClass : s32
    {
        E_VEHICLECLASS_CAR    = 0,
        E_VEHICLECLASS_VAN    = 1,
        E_VEHICLECLASS_BUS    = 2,
        E_VEHICLECLASS_BIGRIG = 3,
        E_VEHICLECLASS_COUNT  = 4
    };
}

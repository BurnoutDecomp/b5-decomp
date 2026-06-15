#pragma once

// Vehicle-physics constants/enums. Recovered from the DecFIGS DWARF.
#include "types.hpp"

namespace BrnPhysics
{
namespace Vehicle
{
    // Severity/kind of a vehicle-vs-vehicle impact.
    enum EImpactType : s32
    {
        E_IMPACT_NONE         = 0,
        E_IMPACT_TRADING_PAINT = 1,
        E_IMPACT_NUDGE        = 2,
        E_IMPACT_SLAM         = 3,
        E_IMPACT_SHUNT        = 4,
        E_IMPACT_BOOST_SLAM   = 5,
        E_IMPACT_BOOST_SHUNT  = 6,
        E_IMPACT_GRINDING     = 7,
        E_IMPACT_RUBBING      = 8,
        E_IMPACT_COUNT        = 9
    };

    // Lifecycle state of a traffic vehicle.
    enum ETrafficType : s32
    {
        E_TRAFFIC_TYPE_POTENTIAL = 0,
        E_TRAFFIC_TYPE_CRASHING  = 1,
        E_TRAFFIC_TYPE_PHYSICAL  = 2,
        E_TRAFFIC_TYPE_SLAMMED   = 3,
        E_TRAFFIC_TYPE_COUNT     = 4
    };
}
}

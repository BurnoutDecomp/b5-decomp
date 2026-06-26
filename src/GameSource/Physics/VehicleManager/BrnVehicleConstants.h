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

    // Who-hit-whom classification of a race-car-vs-race-car impact (DWARF BrnVehicleConstants.h).
    // Used by the takedown classifier (VehicleManager::RaceCarResponseInfo::meImpactSitutation).
    enum EImpactSituation
    {
        E_IMPACT_SITUATION_INVALID     = -1,
        E_IMPACT_SITUATION_PLAYER_ON_AI = 0,
        E_IMPACT_SITUATION_AI_ON_PLAYER = 1,
        E_IMPACT_SITUATION_AI_ON_AI    = 2,
        E_IMPACT_SITUATION_NETWORK     = 3,
        E_IMPACT_SITUATION_COUNT       = 4,
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

    // How a traffic vehicle was made to crash. DWARF home BrnTrafficPhysicsConstants.h:32;
    // homed here additively alongside its sibling ETrafficType (the project already keeps
    // the vehicle traffic enums in BrnVehicleConstants.h rather than forking a new header).
    // ADDITIVE GROW (flagged by Vehicle-events group): new enum, no change to existing types.
    // Plain (un-fixed) enum per the DWARF; the X360 copies it as a 4-byte word. Top enumerator
    // 255 forces an int-width underlying type, matching the 4-byte field stride in
    // TrafficSlammedEvent.
    enum eCrashTrafficType
    {
        eCrashTrafficType_Standard    = 0,
        eCrashTrafficType_Checked     = 1,
        eCrashTrafficType_Spontaneous = 2,
        eCrashTrafficType_Slammed     = 3,
        eCrashTrafficType_Invalid     = 255
    };
}
}

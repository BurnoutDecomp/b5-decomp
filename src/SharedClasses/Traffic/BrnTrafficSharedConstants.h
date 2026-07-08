#pragma once

// Shared traffic-subsystem constants. Canonical home per the DecFIGS DWARF
// (SharedClasses/Traffic/BrnTrafficSharedConstants.h). Values grounded by the DWARF
// (KU_MAX_HULLS == 400, KU_MAX_HULLS_IN_PVS == 8) and corroborated by the X360 retail
// XEX (e.g. the hull-bounds assert in TrafficNetworkOutputInterface::ActivateHull compares
// against 0x190 == 400, and Construct seeds an 8-entry active-hull table).
//
// NOTE: KU_INVALID_HULL (0xFFFF) is, for legacy reasons, non-canonically homed in
// GameSource/.../TrafficEntityModule/BrnTrafficStaticParam.h; it is intentionally NOT
// redefined here so the two headers can be included together without an ODR collision.

#include "types.hpp"

namespace BrnTraffic
{
    // Maximum number of traffic collision hulls in the world (DWARF :30).
    static const u32 KU_MAX_HULLS = 400;

    // Maximum number of hulls tracked in the Potentially-Visible-Set per active race car
    // (DWARF :31). Sizes TrafficNetworkOutputInterface::mau16ActiveHulls[].
    static const u32 KU_MAX_HULLS_IN_PVS = 8;

    // Which lateral side of a lane a neighbour / lane-change is on (DWARF
    // BrnTrafficSharedConstants.h:76). Consumed by Section::FindNeighbourForRung (the
    // WorldMap lane walk passes E_LEFT). Additive grow of this canonical home.
    enum Side
    {
        E_LEFT       = 0,
        E_RIGHT      = 1,
        E_SIDE_COUNT = 2,
    };
}

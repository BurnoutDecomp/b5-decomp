#pragma once

// =============================================================================
// The traffic constants both the shared (baked-data) side and the runtime entity
// module need: world sizing, invalid-id sentinels, junction/traffic-light caps.
// Canonical home per the DWARF (dwarfdump/SharedClasses/Traffic/
// BrnTrafficSharedConstants.h); every value carries its DWARF line. The runtime-only
// pool sizes live in GameSource/.../TrafficEntityModule/BrnTrafficConstants.h, where
// the console puts them too.
//
// PARK -- three constants keep non-canonical homes because redefining them here is an
// ODR collision. Each should MOVE here in a later coordinated pass:
//   * KU_INVALID_HULL (0xFFFF)       -> GameSource/.../BrnTrafficStaticParam.h:33
//   * KU_MAX_TRAFFIC_LIGHT_INSTANCES -> GameSource/.../BrnTrafficLightManager.h:43.
//     VALUE DELTA: the DWARF says 2048, but GetLightState @0x8274F9A0 range-asserts the
//     flat instance index against 0x258 == 600, which is what that header carries. The
//     ASM WINS; do not "correct" it to the DWARF.
//   * KU_MAX_VEHICLE_ASSETS_PER_HULL -> also a class static on Hull
//     (BrnTrafficHull.h:107). The namespace-scope copy below is canonical; both say 16.
// =============================================================================

#include "types.hpp"

namespace BrnTraffic
{
    // ---- world sizing ------------------------------------------------------------------

    // Maximum number of traffic collision hulls in the world (DWARF :30). X360:
    // TrafficNetworkOutputInterface::ActivateHull bounds-asserts against 0x190 == 400.
    static const u32 KU_MAX_HULLS = 400;

    // Maximum number of hulls tracked in the Potentially-Visible-Set per active race car
    // (DWARF :31). Sizes TrafficNetworkOutputInterface::mau16ActiveHulls[], which
    // Construct seeds with 8 entries.
    static const u32 KU_MAX_HULLS_IN_PVS = 8;

    // ---- invalid-id sentinels (DWARF :33-:39) ------------------------------------------
    // (KU_INVALID_HULL is deliberately absent -- see the header banner.)

    static const u8  KU_INVALID_SECTION                = 0xFFu;
    static const u16 KU_INVALID_NEIGHBOUR              = 0xFFFFu;
    static const u8  KU_INVALID_STOPLINE               = 0xFFu;
    static const u16 KU_INVALID_FLOWTYPE               = 0xFFFFu;
    static const u16 KU_INVALID_KILLZONEREGION         = 0xFFFFu;
    static const u8  KU_INVALID_TRAFFIC_LIGHT_TRIGGER  = 0xFFu;

    // ---- baked-data caps (DWARF :41-:52) -----------------------------------------------

    static const u32 KU_MAX_KILLZONEREGIONS     = 8192;
    static const u32 KU_MAX_SECTIONS_PER_HULL   = 256;
    static const u32 KU_MAX_RUNGS_PER_SECTION   = 256;
    static const u32 KU_MAX_RUNGS_PER_HULL      = 65536;

    // The traffic vehicle catalogue. X360 corroboration: 64 from
    // TrafficCarStreamer::IsTrafficAssetLoaded @0x82706160 asserting `luAssetId < 0x40`;
    // 96 from TrafficEntityModule::maVehicleTypeRuntime spanning 0x76380..0x79380 at the
    // 128-byte VehicleTypeRuntime stride.
    static const u32 KU_MAX_VEHICLE_ASSETS          = 64;
    static const u32 KU_MAX_VEHICLE_TYPES           = 96;
    static const u32 KU_MAX_VEHICLE_ASSETS_PER_HULL = 16;

    // ---- junctions / traffic lights (DWARF :54-:66) ------------------------------------

    static const u32 KU_MAX_TRAFFIC_LIGHT_STATES            = 16;
    static const u32 KU_MAX_TRAFFIC_LIGHTS                  = 8;
    static const u32 KU_MAX_JUNCTIONS_PER_HULL              = 16;
    static const u32 KU_MAX_STOP_LINES_PER_HULL             = 64;
    static const u32 KU_MAX_STOP_LINES_PER_CONTROLLER       = 6;
    static const u32 KU_MAX_TRAFFIC_LIGHTS_PER_CONTROLLER   = 2;
    static const u32 KU_MAX_RACE_DESTINATIONS_PER_TRAFFIC_LIGHT = 16;

    // DWARF :57 (printed without an initialiser). The per-state timer is a u16 in tenths
    // of a second, so the longest representable time in one state is 65535/10.
    static const f32 KF_TRAFFIC_LIGHT_MAX_TIME_IN_STATE = 65535.0f / 10.0f;

    // ---- enums -------------------------------------------------------------------------

    // Which way a lane split is taken (DWARF :65). Indexes
    // Section::mauForwardSections/mauForwardHulls and their backward twins, and is the
    // first argument of the road runner's lane walkers (assert at 0x821FAE7C compares 3).
    enum Directions
    {
        E_DIR_STRAIGHT_ON  = 0,
        E_DIR_LEFT         = 1,
        E_DIR_RIGHT        = 2,
        E_DIRECTIONS_COUNT = 3,
    };

    // Which lateral side of a lane a neighbour / lane-change is on (DWARF :76). Consumed by
    // Section::FindNeighbourForRung (the WorldMap lane walk passes E_LEFT).
    enum Side
    {
        E_LEFT       = 0,
        E_RIGHT      = 1,
        E_SIDE_COUNT = 2,
    };

    // DWARF :86. The three-phase light cycle every junction controller runs.
    enum ETrafficLightState
    {
        E_TRAFFICLIGHTSTATE_RED   = 0,
        E_TRAFFICLIGHTSTATE_AMBER = 1,
        E_TRAFFICLIGHTSTATE_GREEN = 2,
        E_TRAFFICLIGHTSTATE_COUNT = 3,
    };

    // DWARF :96. How hard a race destination reachable through a junction is graded.
    enum ERaceDesinationType
    {
        E_RACE_DESTINATION_EASY   = 0,
        E_RACE_DESTINATION_MEDIUM = 1,
        E_RACE_DESTINATION_HARD   = 2,
        E_RACE_DESTINATION_COUNT  = 3,
    };
}

#pragma once

// =============================================================================
// SharedClasses/Traffic/BrnTrafficSharedConstants.h
//
// The traffic constants that the SHARED (baked-data) side and the runtime entity module
// both need: world sizing, the invalid-id sentinels, and the junction/traffic-light caps.
// Canonical home per the DecFIGS DWARF (references/DecFIGS/dwarfdump/SharedClasses/
// Traffic/BrnTrafficSharedConstants.h); every value below carries its DWARF source line.
//
// The runtime-only pool sizes (KU_MAX_STANDARD_TRAFFIC, KU_MAX_STATIC_TRAFFIC, ...) live
// in GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h, which is
// where the console puts them too.
//
// X360 corroboration for the two that were already here: the hull-bounds assert in
// TrafficNetworkOutputInterface::ActivateHull compares against 0x190 == 400
// (KU_MAX_HULLS), and Construct seeds an 8-entry active-hull table (KU_MAX_HULLS_IN_PVS).
//
// NOT LANDED HERE (existing non-canonical homes; redefining would be an ODR collision --
// each should MOVE here in a later, coordinated pass):
//   * KU_INVALID_HULL (0xFFFF)              -> GameSource/.../BrnTrafficStaticParam.h:33
//   * KU_MAX_TRAFFIC_LIGHT_INSTANCES        -> GameSource/.../BrnTrafficLightManager.h:43.
//     ⚠️ NOTE THE VALUE DELTA: the DWARF says 2048, but the X360 GetLightState @0x8274F9A0
//     range-asserts the flat instance index against 0x258 == 600, which is what the
//     manager header already carries. The ASM WINS; do not "correct" it to the DWARF.
//   * KU_MAX_VEHICLE_ASSETS_PER_HULL        -> also a class static on Hull
//     (SharedClasses/Traffic/BrnTrafficHull.h:107); the namespace-scope copy below is the
//     canonical one and the two agree (16).
// =============================================================================

#include "types.hpp"

namespace BrnTraffic
{
    // ---- world sizing ------------------------------------------------------------------

    // Maximum number of traffic collision hulls in the world (DWARF :30).
    static const u32 KU_MAX_HULLS = 400;

    // Maximum number of hulls tracked in the Potentially-Visible-Set per active race car
    // (DWARF :31). Sizes TrafficNetworkOutputInterface::mau16ActiveHulls[].
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

    // The traffic vehicle catalogue. KU_MAX_VEHICLE_ASSETS (64) is corroborated by
    // TrafficCarStreamer::IsTrafficAssetLoaded @0x82706160 asserting `luAssetId < 0x40`
    // and by the streamer's two adjacent 64-byte per-asset tables; KU_MAX_VEHICLE_TYPES
    // (96) is corroborated by TrafficEntityModule::maVehicleTypeRuntime spanning
    // 0x76380..0x79380 at the 128-byte VehicleTypeRuntime stride Prepare stage 3 walks.
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

    // DWARF :57 (the dwarfdump prints it without an initialiser). The traffic-light
    // per-state timer is stored as a u16 in tenths of a second, so the longest
    // representable time in one state is 65535/10 -- derived from the on-disk width, not
    // invented, and identical in the Feb-2007 source.
    static const f32 KF_TRAFFIC_LIGHT_MAX_TIME_IN_STATE = 65535.0f / 10.0f;

    // ---- enums -------------------------------------------------------------------------

    // Which way a lane split is taken (DWARF :65). Indexes both
    // Section::mauForwardSections/mauForwardHulls and their backward twins, and is the first
    // argument of the road runner's lane walkers -- whose console assert literal is baked as
    // "lePreferredDirection < BrnTraffic::E_DIRECTIONS_COUNT" (asm 0x821FAE7C compares 3).
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

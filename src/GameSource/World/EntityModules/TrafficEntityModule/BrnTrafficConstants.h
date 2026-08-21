#pragma once

// =============================================================================
// BrnTrafficConstants.h -- the BrnTraffic sizing constants and the id-packing free
// functions. DWARF home: references/DecFIGS/dwarfdump/GameSource/World/EntityModules/
// TrafficEntityModule/BrnTrafficConstants.h, which carries every value and source line
// (cited below as `DWARF :NNN`). The DWARF supplies the names and most values; where the
// X360 asm disagrees, the asm wins.
//
// The pool sizes are pinned by the accessors' element-index arithmetic (each is
// base_element_index + index at a fixed stride, so consecutive pools' base indices differ
// by the preceding pool's capacity):
//   GetStandardVehicle @0x82707A38   -> ((index + 85)  << 7) + this
//   GetStaticVehicle   @0x827079D0   -> ((index + 485) << 7) + this   ; 485-85  == 400
//   GetTrailerVehicle  @0x82707AA0   -> ((index + 684) << 7) + this   ; 684-485 == 199
//   GetVehicleAxles    @0x82707C28   -> ((index + 1370)<< 6) + this   ; 1370*64 == 684*128 + 128
// The static accessors also range-assert `luIndex < 0xC7` against
// "luIndex < KU_MAX_STATIC_TRAFFIC", and GetVehicleSpecies @0x821F4648 asserts `< 0x258`.
//
// Not defined here, to avoid ODR collisions with existing homes: KU_PARAM_NUM_PLANS and
// KU_PARAM_NUM_SEGMENTS_TO_REMEMBER (BrnTrafficParam.h); KU_INVALID_HULL
// (BrnTrafficStaticParam.h, canonical home BrnTrafficSharedConstants.h); and every KF_*
// whose value the DWARF omits, which lives in X360 rodata and is recovered per call site.
// =============================================================================

#include "types.hpp"        // u8/u32/s32/f32
#include "BrnCommonTypes.h" // EntityId (32-bit packed scene-entity handle)

namespace BrnTraffic
{
    // ---- vehicle / param pool capacities (banner has the attestation) ----

    // DWARF :48. The moving-traffic pool.
    static const u32 KU_MAX_STANDARD_TRAFFIC = 400;
    // DWARF :49. One lane-param per standard traffic vehicle.
    static const u32 KU_MAX_PARAMS = KU_MAX_STANDARD_TRAFFIC;
    // DWARF :50.
    static const u32 KU_MAX_PARAMS_UPDATE_ON_NON_DECISION_FRAME = 100;

    // DWARF :51 says 200 (PS3). Ship is 199: every static-pool accessor asserts against 0xC7,
    // and the trailer pool's base element index is exactly 199 above the static pool's.
    static const u32 KU_MAX_STATIC_TRAFFIC = 199;
    // DWARF :52. The single articulated-trailer slot.
    static const u32 KU_MAX_TRAILER_TRAFFIC = 1;

    // DWARF :53 / :54. Where each pool starts in the flat vehicle index space.
    // KU_TRAILER_TRAFFIC_OFFSET is 600 on DecFIGS and 599 on ship; it is derived, so it
    // tracks the KU_MAX_STATIC_TRAFFIC delta.
    static const u32 KU_STATIC_TRAFFIC_OFFSET  = KU_MAX_STANDARD_TRAFFIC;
    static const u32 KU_TRAILER_TRAFFIC_OFFSET = KU_STATIC_TRAFFIC_OFFSET + KU_MAX_STATIC_TRAFFIC;

    // DWARF :55 says 601 (PS3). SHIP == 600 (400 + 199 + 1), asserted as 0x258.
    static const u32 KU_MAX_TOTAL_TRAFFIC =
        KU_MAX_STANDARD_TRAFFIC + KU_MAX_STATIC_TRAFFIC + KU_MAX_TRAILER_TRAFFIC;

    // DWARF :56 / :57. The SoA collidable cache: 64 vehicles held as 16 four-wide packets
    // (Array_CollidableVehicleInfo4_16.cpp is the committed instantiation).
    static const u32 KU_MAX_COLLIDABLE_CACHED_TRAFFIC       = 64;
    static const u32 KU_MAX_COLLIDABLE_CACHED_TRAFFIC_ARRAY = 16;

    // ---- hulls -------------------------------------------------------------------------

    // DWARF :60 / :65. Nine hulls tracked per active race car (Array_short_9.cpp), times the
    // eight active race-car slots (E_ACTIVE_RACE_CAR_INDEX_COUNT) == 72
    // (CgsSetUnsignedShort72.cpp is the committed Set<u16,72> instantiation).
    static const u32 KU_MAX_ACTIVE_HULLS_PER_RACECAR = 9;
    static const u32 KU_MAX_ACTIVE_HULLS             = 72;

    // DWARF :66.
    static const u32 KU_MAX_GENERATORS = 512;

    // ---- crash / physical promotion ----------------------------------------------------

    // DWARF :68.
    static const u32 KI_MAX_NEW_SPONTANEOUS_CRASHING_TRAFFIC = 5;
    // DWARF :70 / :71. 20 promotable bodies + 5 slots of headroom.
    static const u32 KU_MAX_PHYSICAL_TRAFFIC_VEHICLES_HEADROOM = 5;
    static const u32 KU_MAX_PHYSICAL_TRAFFIC_VEHICLES          = 25;
    // DWARF :73 / :74.
    static const u32 KU_MAX_NEW_CRASHED_VEHICLES = 25;
    static const u32 KU_MAX_NEW_REMOVED_VEHICLES = 25;

    // ---- sentinels ---------------------------------------------------------------------

    // DWARF :76 / :77 / :81 / :82 / :84 / :87.
    static const u32 KU_INVALID_PARAM        = 0xFFFFu;
    static const u32 KU_INVALID_VEHICLE      = 0xFFFFu;
    static const u32 KU_UNKNOWN_NEIGHBOUR    = 0xFFFEu;
    static const u32 KU_UNKNOWN_STOPLINE     = 254;
    static const u8  KU_INVALID_HULL_RUNTIME = 255;
    static const u32 KU_INVALID_UPDATE_FRAME = 0xFFFFFFFFu;

    // ---- update cadence ----------------------------------------------------------------

    // DWARF :89 / :90 / :91 / :93. The sim takes a decision every 5th 50 Hz frame or 6th
    // 60 Hz frame, so 10 a second, which is what KF_UPDATE_TIME_DELTA_NO_SLOWMO spells.
    // The DWARF prints the two floats without initialisers; both are derived from the
    // integer cadence above, not invented.
    static const u32 KU_NUM_FRAMES_BETWEEN_DECISIONS_50HZ = 5;
    static const u32 KU_NUM_FRAMES_BETWEEN_DECISIONS_60HZ = 6;
    static const f32 KF_UPDATE_TIME_DELTA_NO_SLOWMO       = 1.0f / 10.0f;
    static const f32 KF_SECONDS_PER_MINUTE                = 60.0f;

    // ---- hull-change prediction --------------------------------------------------------

    // DWARF :99 / :100 / :101 / :102. 50 update frames of look-ahead per player, times the
    // eight active race-car slots == 400 (Array_HullChangeInfo_400.cpp).
    static const u32 KU_UPDATE_FRAMES_PER_HULL_CHANGE = 50;
    static const f32 KF_HULL_CHANGE_PREDICTION_TIME =
        static_cast<f32>(KU_UPDATE_FRAMES_PER_HULL_CHANGE) * KF_UPDATE_TIME_DELTA_NO_SLOWMO;
    static const u32 KU_MAX_HULL_CHANGES_PER_PLAYER = KU_UPDATE_FRAMES_PER_HULL_CHANGE;
    static const u32 KU_MAX_HULL_CHANGES            = 400;

    // ---- purgatory / insertion ---------------------------------------------------------

    // DWARF :104 / :105 / :108 / :109.
    static const u32 KU_PURGATORY_TIME_ONLINE            = 5;
    static const u32 KU_PURGATORY_TIME_OFFLINE           = 5;
    static const u32 KU_MAX_LANE_INSERTIONS_PER_FRAME    = 1;
    static const u32 KU_START_PROTECT_UPDATE_FRAME_ONLINE= 100;

    // ---- near-miss / stomp reporting ---------------------------------------------------

    // DWARF :111 / :112 / :114. These size the TrafficToRaceCarInterface_PreScene
    // collections (Array<NearMissData,16> / <NearMissData,8>).
    static const s32 KI_MAX_NEAR_MISS_TRAFFIC_CARS = 16;
    static const s32 KI_MAX_NEAR_MISS_RACE_CARS    = 8;
    static const s32 KI_MAX_STOMPED_CARS           = 8;

    // ---- avoidance ---------------------------------------------------------------------

    // DWARF :116 / :119. Five steering feelers, scored two at a time per decision frame.
    static const s32 KI_TRAFFIC_AVOIDANCE_FEELERS            = 5;
    static const s32 KI_TRAFFIC_AVOIDANCE_FEELERS_CALC_COUNT = 2;

    // ---- showtime ----------------------------------------------------------------------

    // DWARF :126.
    static const u32 KU_MAX_SHOWTIME_TRAFFIC_VEHICLES = 32;

    // ---- debug -------------------------------------------------------------------------

    // DWARF BrnTrafficEntityModule.h:357. X360-attested: Prepare @0x8274A578 stage 4
    // allocates 2560 bytes for this many DEBUG_VehicleFuzzyLogic records, and 2560/40 == 64,
    // that record's size.
    static const u32 KU_DEBUG_MAX_FUZZY_LOGIC = 40;

    // ---- id packing --------------------------------------------------------------------

    // Pack a traffic vehicle's index into a scene EntityId. X360 @0x827048C0, store-for-store:
    //   slwi  r11, entityIndex, 10     ; entityIndex << 10
    //   oris  r11, r11, 0x200          ; (0x200 << 16) == 0x02000000 == owner(2) << 24
    //   stw   r11, 0(result)
    // guarded by `cmplwi entityIndex, 0x4000` (assert luEntityIndex < (1U << 14)).
    // So a traffic EntityId is owner:[31..24]=2, entityIndex:[23..10] (14 bits),
    // partIndex:[9..0]=0. That 14/10 split is the traffic subsystem's own and differs from
    // CgsSceneManager::EntityId's 8/12/12: this helper packs the raw u32 rather than calling
    // EntityId::Set.
    EntityId MakeTrafficEntityId(u32 luEntityIndex);

    // The scene entity-type flag word traffic vehicles register under. X360
    // CreateNewVehicleEntities @0x8272FA30, 0x8272FF74: `li r29, 0x488` staged straight into
    // AddEntity's r5. Sibling of PropCellManager::KU_PROP_SCENE_ENTITY_TYPE_FLAG == 0x490 and
    // the race car's KU_RACECAR_SCENE_ENTITY_TYPE_FLAG == 0x484, so it is named, not inlined.
    static const u32 KU_TRAFFIC_SCENE_ENTITY_TYPE_FLAG = 0x488;
}

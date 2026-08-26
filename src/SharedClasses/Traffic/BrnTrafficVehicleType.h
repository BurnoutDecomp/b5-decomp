#pragma once

// =============================================================================
// The two serialised per-vehicle-TYPE records of the TrafficData resource:
// VehicleTypeData (the catalogue entry -- asset, traits, class, trailer flow) and
// VehicleTypeUpdateData (the per-frame physics constants), plus the type enums the
// DWARF homes in this file.
//
// Layout and names are DWARF-authoritative
// (dwarfdump/SharedClasses/Traffic/BrnTrafficVehicleType.h); the strides are
// X360-attested per struct. Neither record holds a pointer, so console and host
// footprints are identical.
// =============================================================================

#include "types.hpp"

namespace BrnTraffic
{
    // BrnTrafficVehicleType.h:59 (DWARF) -- the traffic-car silhouette class.
    enum VehicleClass : s32
    {
        E_VEHICLECLASS_CAR    = 0,
        E_VEHICLECLASS_VAN    = 1,
        E_VEHICLECLASS_BUS    = 2,
        E_VEHICLECLASS_BIGRIG = 3,
        E_VEHICLECLASS_COUNT  = 4
    };

    // BrnTrafficVehicleType.h:78 (DWARF) -- the showtime/crash-scoring vehicle category.
    // [boost-msg wave 2026-08-26] The fork that blocked this home is GONE: the five-value
    // E_VEHICLESCORECATEGORY_* enum in GameSource/GameState/ModeManager/Scoring/
    // BrnCrashModeScoringRecentCrash.h was deleted and its one consumer moved onto THIS
    // spelling. The eight values are DWARF-named; X360 attestation for the count:
    // BoostMessageManager::UpdateShowtime @0x8242FAE0 range-asserts
    // `(uint32_t) meHitVehicleCategory < sizeof(KA_VEHICLE_SCORE_CATEGORY_TO_MESSAGE_TYPE)`
    // against an EIGHT-entry table (@dword_82F24A6C = {11..18}) and indexes
    // mpacMessageTypeStrings with it, so categories run 0..7.
    enum VehicleScoreCategory : s32
    {
        E_VEHICLESCORE_CAR           = 0,
        E_VEHICLESCORE_VAN           = 1,
        E_VEHICLESCORE_TRUCK         = 2,
        E_VEHICLESCORE_BUS           = 3,
        E_VEHICLESCORE_BIGRIG        = 4,
        E_VEHICLESCORE_LIMO          = 5,
        E_VEHICLESCORE_TAXI          = 6,
        E_VEHICLESCORE_TARGETVEHICLE = 7,

        E_VEHICLESCORE_COUNT         = 8,
    };

    // BrnTrafficVehicleType.h:107 (DWARF) -- the per-type constants the vehicle update
    // reads every frame. sizeof == 20 (five f32, no padding). The shipped block is
    // 16-byte aligned, which TrafficData::FixUp asserts via
    // Is16Aligned(mpaVehicleTypesUpdate).
    struct VehicleTypeUpdateData
    {
        f32 mfWheelRadius;      // :109  +0x00  (Vehicle::InitialiseAsStatic derives the
                                //               axle Y from this)
        f32 mfSuspensionRoll;   // :112  +0x04
        f32 mfSuspensionPitch;  // :113  +0x08
        f32 mfSuspensionTravel; // :114  +0x0C
        f32 mfMass;             // :115  +0x10

        // :120 DECLARED-ONLY -- no ARTIST symbol, no rw::EndianSwap in this tree, and
        // the shipped PC payload is already little-endian (BrnTrafficStaticTraffic.cpp).
        void EndianSwap();

        static void _AssertLayout();   // never called; body in the .cpp
    };

    // BrnTrafficVehicleType.h:135 (DWARF) -- one serialised vehicle-type record.
    // sizeof == 8, X360-attested: TrafficData::GetVehicleTraitsForVehicleType
    // @0x82705DF0 indexes mpaVehicleTypes at an 8-byte stride (`slwi r10,r29,3`) and
    // reads the traits id at +6 within the element (`lbz r24, 6(r11)`). The DWARF member
    // list sums to 7, so the record carries one byte of trailing pad.
    struct VehicleTypeData
    {
        // :137  +0x00 -- flow type the trailer for this cab is drawn from, or
        // KU_INVALID_FLOWTYPE (0xFFFF) for "no trailer".
        u16 muTrailerFlowTypeId;

        // :138  +0x02 -- vehicle-type flag bits. The byte gates trailer allocation
        // (UpdateVehicles_CreateNewVehicles @0x8273A308):
        //     if (!lpType->mxVehicleFlags || lpType->muTrailerFlowTypeId == 0xFFFF)
        //         no trailer;
        //     else TryAllocateTrailerId(...)
        // FLAG (unnamed bits): the asm tests the byte for NON-ZERO, never an individual
        // bit, so ARTIST attests no bit values and no E_VEHICLETYPEFLAG_* is minted here.
        u8 mxVehicleFlags;

        u8 muVehicleClass;       // :139  +0x03  (a VehicleClass, stored as a byte)
        u8 muInitialDirt;        // :141  +0x04
        u8 muAssetId;            // :143  +0x05  index into TrafficData::mpaVehicleAssets
        u8 muTraitsId;           // :145  +0x06  index into TrafficData::mpaVehicleTraits
        u8 maPad7;               //       +0x07  trailing pad to the attested stride 8

        // :151 DECLARED-ONLY -- same three reasons as above.
        void EndianSwap();

        static void _AssertLayout();   // never called; body in the .cpp
    };
}

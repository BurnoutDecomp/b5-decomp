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

    // PARK -- BrnTraffic::VehicleScoreCategory (DWARF BrnTrafficVehicleType.h:78)
    // belongs in this file, but a fork holds the name at
    // GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h:28, so
    // defining the canonical version here is a C2011 redefinition for every TU that sees
    // both. The fork also diverges: it spells five E_VEHICLESCORECATEGORY_* enumerators
    // and stops at BIGRIG, while the DWARF names eight, E_VEHICLESCORE_CAR / VAN / TRUCK
    // / BUS / BIGRIG / LIMO / TAXI / TARGETVEHICLE (_COUNT = 8).
    // DELETE WHEN the scoring header and its consumers are moved onto the DWARF enum.

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

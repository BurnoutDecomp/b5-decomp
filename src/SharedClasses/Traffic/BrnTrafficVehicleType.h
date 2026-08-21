#pragma once

// =============================================================================
// BrnTrafficVehicleType.h  (OWNING HEADER)
//
// Home for the two serialised per-vehicle-TYPE records of the TrafficData resource:
// VehicleTypeData (the catalogue entry: asset, traits, class, trailer flow) and
// VehicleTypeUpdateData (the physics-ish per-frame constants), plus the two type
// enums the DWARF homes in this file.
//
// LAYOUT / NAMES are DWARF-authoritative
// (references/DecFIGS/dwarfdump/SharedClasses/Traffic/BrnTrafficVehicleType.h);
// the strides are X360-attested (see each struct). The Feb-2007 leak agrees
// member-for-member and pins the same two sizes via
// CheckClassSize<VehicleTypeUpdateData,20,...> / CheckClassSize<VehicleTypeData,8,...>.
// Neither record holds a pointer, so console and host footprints are identical.
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

    // NOT LANDED HERE -- BrnTraffic::VehicleScoreCategory (DWARF
    // BrnTrafficVehicleType.h:78) belongs in THIS file, but a fork already occupies the
    // name at GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h
    // (:28), self-described there as a "minimal home grown for this TU (un-homed
    // elsewhere)". Defining the canonical version here is an immediate C2011 enum
    // redefinition for every TU that sees both. The fork also DIVERGES from the DWARF:
    // it spells five enumerators E_VEHICLESCORECATEGORY_* and stops at BIGRIG, while the
    // DWARF names eight, E_VEHICLESCORE_CAR/VAN/TRUCK/BUS/BIGRIG/LIMO/TAXI/
    // TARGETVEHICLE/(_COUNT = 8). Retiring the fork means editing the scoring header and
    // its consumers, which cluster C2 does not own -- see the C2 report's park list.

    // BrnTrafficVehicleType.h:107 (DWARF) -- the per-type constants the vehicle update
    // reads every frame. sizeof == 20 (five f32, no padding); the shipped block sits
    // 16-byte aligned, which is why TrafficData::FixUp asserts
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
        // the shipped PC payload is already little-endian (evidence in
        // BrnTrafficStaticTraffic.cpp).
        void EndianSwap();

        static void _AssertLayout();   // never called; body in the .cpp
    };

    // BrnTrafficVehicleType.h:135 (DWARF) -- one serialised vehicle-type record.
    // sizeof == 8, X360-authoritative: TrafficData::GetVehicleTraitsForVehicleType
    // @0x82705DF0 indexes mpaVehicleTypes at an 8-byte stride (`slwi r10,r29,3`) and
    // reads the traits id at byte +6 within the element (`lbz r24, 6(r11)`). The DWARF
    // member list sums to 7, so the record carries one byte of trailing pad.
    struct VehicleTypeData
    {
        // :137  +0x00 -- flow type the trailer for this cab is drawn from, or
        // KU_INVALID_FLOWTYPE (0xFFFF) for "no trailer".
        u16 muTrailerFlowTypeId;

        // :138  +0x02 -- vehicle-type flag bits.
        // X360-attested SEMANTIC (UpdateVehicles_CreateNewVehicles @0x8273A308):
        //     if (!lpType->mxVehicleFlags || lpType->muTrailerFlowTypeId == 0xFFFF)
        //         no trailer;
        //     else TryAllocateTrailerId(...)
        // i.e. the byte gates trailer allocation. FLAG: the asm tests it for
        // NON-ZERO, never against an individual bit, so no bit values are attested in
        // ARTIST and no E_VEHICLETYPEFLAG_* enum is minted here (see the C2 report's
        // park list for the Feb-2007 candidate values and what would settle them).
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

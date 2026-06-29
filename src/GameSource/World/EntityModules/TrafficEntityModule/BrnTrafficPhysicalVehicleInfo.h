#ifndef BRN_TRAFFIC_PHYSICAL_VEHICLE_INFO_H
#define BRN_TRAFFIC_PHYSICAL_VEHICLE_INFO_H

#include "types.hpp"
#include "BrnCommonTypes.h" // Vector3, Vector3Plus (16-byte/16-aligned SIMD lanes)

// =============================================================================
// BrnTrafficPhysicalVehicleInfo.h  (NEW OWNING HEADER -- minimal)
//
// Home for the element type BrnTraffic::PhysicalVehicleInfo, the value held by the
// Array<PhysicalVehicleInfo, 33> the TrafficEntityModule fills in
// UpdateParams_DoTimeSlicedLogic (the caller of Array<...>::Append @ 0x8270BA88) and reads
// in UpdateParam_CheckIfNeedToSlow (Array<...>::operator[] @ 0x8270BBC8).
//
// SIZE (asm-attested): 48 bytes. The Append store-for-store proves it -- the count
// word sits at byte +0x630 == 33 * 48 (i.e. right after maElements[33]), and the
// per-element copy loop in Append moves exactly 6 * 8 = 48 bytes (six `std`s) per
// element; the operator[] @ 0x8270BBC8 returns 48*index + base, confirming it again.
//
// FIELDS (GROWN from the DecFIGS DWARF, references/DecFIGS/dwarfdump/GameSource/World/
// EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h:341-343): the earlier slice
// could not see field names (Append only moves the whole element by value) and homed an
// honest 48-byte opaque placeholder, with a FLAG instructing the next slice to grow it. The
// DWARF now names the three 16-byte/16-aligned vector lanes, which sum to exactly 48 and
// preserve the asm-attested footprint:
//   Vector3Plus mPositionAndImportance (:341)  -- position in xyz, an importance scalar in w
//   Vector3     mLinearVelocity        (:342)
//   Vector3     mRight                 (:343)
// =============================================================================

namespace BrnTraffic
{
    // PhysicalVehicleInfo -- 48-byte traffic physical-vehicle info record. Three 16-byte SIMD
    // vector lanes (DWARF :341-343); 48-byte footprint asm-attested by Array<...,33>::Append
    // @ 0x8270BA88 and ::operator[] @ 0x8270BBC8.
    struct PhysicalVehicleInfo
    {
        Vector3Plus mPositionAndImportance; // :341  +0x00  (16; position in xyz, importance in w)
        Vector3     mLinearVelocity;        // :342  +0x10  (16)
        Vector3     mRight;                 // :343  +0x20  (16 -> 48)
    };
}

#endif // BRN_TRAFFIC_PHYSICAL_VEHICLE_INFO_H

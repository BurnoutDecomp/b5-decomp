#ifndef BRN_TRAFFIC_PHYSICAL_VEHICLE_INFO_H
#define BRN_TRAFFIC_PHYSICAL_VEHICLE_INFO_H

#include "types.hpp"

// =============================================================================
// BrnTrafficPhysicalVehicleInfo.h  (NEW OWNING HEADER -- minimal)
//
// Home for the element type BrnTraffic::PhysicalVehicleInfo, the value held by the
// Array<PhysicalVehicleInfo, 33> the TrafficEntityModule fills in
// UpdateParams_DoTimeSlicedLogic (the caller of Array<...>::Append @ 0x8270BA88).
//
// SIZE (asm-attested): 48 bytes. The Append store-for-store proves it -- the count
// word sits at byte +0x630 == 33 * 48 (i.e. right after maElements[33]), and the
// per-element copy loop in Append moves exactly 6 * 8 = 48 bytes (six `std`s) per
// element. The generic Array<T,N>::Append (committed in CgsArray.h) copies the whole
// element by value, so only the 48-byte FOOTPRINT is load-bearing for this slice; no
// individual field is read by Append.
//
// FLAG (un-homed aggregate): PhysicalVehicleInfo has no DWARF and no other reconstructed
// TU in this tree, and its internal fields are NOT touched by the only function that
// references it here (Append's whole-element copy). It is therefore declared as an
// honest 48-byte opaque placeholder, sized exactly to the Append-attested footprint, so
// the Array<PhysicalVehicleInfo,33> instantiation compiles store-for-store faithfully.
// When a TU that actually reads its fields lands, it must GROW this struct (replace the
// placeholder with the real members, preserving sizeof == 48).
// =============================================================================

namespace BrnTraffic
{
    // PhysicalVehicleInfo -- 48-byte traffic physical-vehicle info record (fields opaque;
    // only the 48-byte footprint is attested by Array<...,33>::Append @ 0x8270BA88).
    struct PhysicalVehicleInfo
    {
        u8 maOpaque[48];
    };
}

#endif // BRN_TRAFFIC_PHYSICAL_VEHICLE_INFO_H

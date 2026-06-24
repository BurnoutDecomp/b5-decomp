#ifndef BRN_TRAFFIC_VEHICLE_TYPE_RUNTIME_H
#define BRN_TRAFFIC_VEHICLE_TYPE_RUNTIME_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector4 (== rw::math::vpu::Vector4) -- 16-byte paint colour
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// =============================================================================
// BrnTrafficVehicleTypeRuntime.h  (NEW OWNING HEADER -- partial / additive)
//
// Home for BrnTraffic::VehicleTypeRuntime -- the per-vehicle-type runtime block
// used while spawning/rendering traffic cars. The X360 retail XEX bakes this exact
// header path into PickPaintColourForVehicle's asserts
// ("...trafficentitymodule\\BrnTrafficVehicleTypeRuntime.h", lines 134/135),
// fixing the home.
//
// This slice (brn-traffic2 bodies group) owns ONLY:
//   BrnTraffic::VehicleTypeRuntime::PickPaintColourForVehicle  @ 0x827049A8
//
// The sibling function VehicleTypeRuntime::Prepare (@ 0x82761B10) lives in its own
// file-TU (BrnTrafficVehicleTypeRuntime.cpp) and is NOT owned here; Prepare is the
// function that fills the runtime prefix, so the prefix layout is left to that TU.
//
// LAYOUT (only the fields PickPaintColourForVehicle touches are pinned by its asm):
//   +0x00  opaque runtime prefix  (filled by Prepare; out of scope, honest pad)
//   +0x48  u8  maPaintColourIndices[20]   (lbz at 0x48(this), byte-indexed table)
//   +0x5C  s8  miNumPaintColours          (lbz+extsb at 0x5C(this); asserted != 0)
//
// The [20] count of maPaintColourIndices is the exact gap 0x5C-0x48 == 20; it places
// miNumPaintColours at +0x5C with no synthetic padding. The two fields are the only
// ones this TU reads; everything below +0x48 is an opaque placeholder owned by Prepare.
//
// FLAG: the +0..+0x47 prefix is sized as an opaque byte placeholder only so this slice
// compiles standalone; its real fields belong to the Prepare TU, which must GROW this
// header (replace the placeholder, keep maPaintColourIndices @ +0x48 / miNumPaintColours
// @ +0x5C). Member names maPaintColourIndices / miNumPaintColours are from the asm asserts
// ("lpaPaintColours", "miNumPaintColours != 0"); no DWARF exists for this type here.
// =============================================================================

namespace BrnTraffic
{
    class VehicleTypeRuntime
    {
    public:
        // Highest index into maPaintColourIndices (the gap 0x5C-0x48 == 20 entries).
        static const u32 KU_MAX_PAINT_COLOUR_INDICES = 20;

        // PickPaintColourForVehicle @ 0x827049A8
        // Choose this type's paint colour for one spawned vehicle:
        //   * pick a slot in the per-type index table by (luSeed % miNumPaintColours),
        //   * read the palette index stored there,
        //   * clamp it to at most (liNumAvailableColours - 1),
        //   * return the 16-byte paint colour at that index of lpaPaintColours.
        // Returns the colour BY VALUE (the X360 returns it through the sret pointer in r3
        // via stvx128). asserts lpaPaintColours != null and miNumPaintColours != 0.
        Vector4 PickPaintColourForVehicle(u32 luSeed,
                                          s32 liNumAvailableColours,
                                          const Vector4* lpaPaintColours) const;

    private:
        // --- opaque runtime prefix (+0 .. +0x47), owned by the Prepare TU -------------
        u8 maPrefixPlaceholder[0x48];                       // +0x00

        // --- fields read by PickPaintColourForVehicle --------------------------------
        u8 maPaintColourIndices[KU_MAX_PAINT_COLOUR_INDICES]; // +0x48
        s8 miNumPaintColours;                                 // +0x5C
    };
}

#endif // BRN_TRAFFIC_VEHICLE_TYPE_RUNTIME_H

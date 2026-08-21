#pragma once

// =============================================================================
// BrnTraffic::StaticTrafficVehicle -- one authored PARKED traffic car. A serialised
// record in the TrafficData resource ("BaseTraffic", type 65538, B5TRAFFIC.BNDL);
// Hull::mpaStaticTrafficVehicles is an array of muNumStaticTraffic of these.
// Member names/order: DWARF dwarfdump/.../BrnTrafficStaticTraffic.h:46 (:56-:59).
// Offsets: TrafficEntityModule::FillNewHull @0x82743600.
// Stride 80: Hull::GetStaticVehicle @0x82705C90 is `80 * luIndex + *(hull + 36)`.
// The record holds no pointers, so console and host footprints are identical.
// BEHAVIOUR WARNING: the shipped PC payload is already little-endian (swapped at
// convert time by tools/assets/bundles/lane_transcode.py), so FixUp/FixDown stay
// declared-only. A swap on the load path would corrupt correct data.
// =============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // Matrix44Affine (rw::math::vpu, four 16-byte rows)

namespace BrnTraffic
{
    // BrnTrafficStaticTraffic.h:46 -- one authored parked car.
    struct StaticTrafficVehicle
    {
        // :56  +0x00 .. +0x3F -- world placement. wAxis (+0x30) is the position;
        // FillNewHull's proximity cull loads that row (`lvx128 v12, r11, 48`).
        Matrix44Affine mTransform;

        // :57  +0x40 -- index into TrafficData::mpapFlowTypes. FillNewHull passes it
        // to PickVehicleToSpawn @0x827235F8 to choose the vehicle type.
        u16 mFlowTypeID;

        // :58  +0x42 -- spawn chance as a PERCENTAGE. FillNewHull rolls 1..100 and
        // skips the record when the roll exceeds this.
        u8 mExistsAtAllChance;

        // :59  +0x43 -- ship-only mode gate bits. FillNewHull @0x82743600 tests:
        //     if ((muFlags & 2) != 0 && !mbPlayingShowtimeMode)      skip;
        //     if (!mbAllowDivergentBehaviour && (muFlags & 1) != 0)  skip;
        // Bit 1 marks a showtime-only record, bit 0 a divergent-behaviour-only one.
        // FLAG (unnamed bits): the DWARF member is a plain uint8_t and ARTIST names
        // no enum for the bits, so no E_STATICTRAFFICFLAG_* is minted here.
        u8 muFlags;

        // DWARF :64 / :69. DECLARED-ONLY -- see the endianness note above.
        void FixUp(const void* lpBaseData);
        void FixDown(const void* lpBaseData);

        // Never called. A member function so offsetof reaches every member.
        static void _AssertLayout();
    };

    // Hull::GetStaticVehicle @0x82705C90 indexes this array with a hard-coded
    // 80-byte stride. Also at namespace scope so a TU that includes only the header
    // still fails if the math types change width.
    static_assert(sizeof(StaticTrafficVehicle) == 80,
                  "StaticTrafficVehicle stride (Hull::GetStaticVehicle @0x82705C90 uses 80)");
}

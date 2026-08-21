#pragma once

// =============================================================================
// BrnTrafficStaticTraffic.h  (NEW OWNING HEADER -- wave T1, cluster C2)
//
// Home for BrnTraffic::StaticTrafficVehicle -- one PARKED traffic car authored
// into the lane graph. The record is per-hull: Hull::mpaStaticTrafficVehicles is
// an array of muNumStaticTraffic of these, and Hull::GetStaticVehicle @0x82705C90
// hands one out by index.
//
// It is a SERIALISED ON-DISC record, not a runtime object: it is part of the
// TrafficData resource ("BaseTraffic", type 65538, B5TRAFFIC.BNDL) and therefore
// keeps its console footprint on the host -- see the stride proof below.
//
// -----------------------------------------------------------------------------
// MEMBER NAMES / ORDER: DWARF-authoritative
// (references/DecFIGS/dwarfdump/SharedClasses/Traffic/BrnTrafficStaticTraffic.h,
// struct @ :46) -- mTransform (:56), mFlowTypeID (:57), mExistsAtAllChance (:58),
// muFlags (:59). The Feb-2007 leak (rung 3) carries the first three verbatim; the
// fourth (muFlags) exists in the DWARF and in the shipped data but NOT in the leak,
// so the leak is a strict subset here. DWARF wins.
//
// OFFSETS: X360-authoritative, from TrafficEntityModule::FillNewHull @0x82743600's
// parked-car loop, which walks one record at a time (`_R11 = 80 * v59 + <base>`) and
// touches exactly four places inside it:
//
//     lvx128 v12, r11, r24   with _R24 == 48   -> mTransform's translation row
//                                                 (Matrix44Affine::wAxis, +0x30),
//                                                 differenced against the reference
//                                                 position for the ship-only
//                                                 proximity cull
//     (rng % 0x64 + 1) > *(_R11 + 66)          -> mExistsAtAllChance   @ +0x42
//     (*(_R11 + 67) & 2), (*(_R11 + 67) & 1)   -> muFlags             @ +0x43
//     PickVehicleToSpawn(this, *(_R11 + 64))   -> mFlowTypeID (u16)   @ +0x40
//
// STRIDE 80: Hull::GetStaticVehicle @0x82705C90 is literally
// `return 80 * luIndex + *(hull + 36);` (the X360 encodes it as (idx*5)<<4). 64 + 2
// + 1 + 1 = 68 rounded up to the Matrix44Affine 16-byte alignment == 80, so the
// natural C++ footprint of the DWARF member list already IS the shipped stride --
// no synthetic padding, and the pin below is a real invariant rather than a
// hand-forced number. The record holds NO pointers, so the stride is identical on
// the 4-byte-pointer console and the 8-byte-pointer host; this is one of the few
// places where keeping the console footprint is correct rather than the recurring
// "X360 offsets on x64" bug.
//
// -----------------------------------------------------------------------------
// ENDIANNESS -- THE SHIPPED PC PAYLOAD IS ALREADY LITTLE-ENDIAN. NOTHING SWAPS.
//
// Measured 2026-08-21 against both files (evidence in the C2 report):
//   * build/game/B5TRAFFIC.BNDL is a `bnd2` PLATFORM 4 (PC) bundle, uncompressed,
//     one resource of type 65538. All 875 static-vehicle records in it parse as
//     orthonormal rotation rows plus sane world translations when read LITTLE-endian
//     (875/875) and as garbage when read big-endian (0/875); every mFlowTypeID is
//     < muNumFlowTypes (44) little-endian and none is big-endian.
//   * The X360 original (platform 2, zlib-compressed) decodes to the SAME values
//     read big-endian -- e.g. hull 22's first record is (0.6216, -0.0019, -0.7833),
//     mFlowTypeID 22, mExistsAtAllChance 100, muFlags 1 in BOTH files, with the
//     +0x40 bytes literally reversed (`00 16 64 01` -> `16 00 64 01`).
// So tools/assets/bundles/lane_transcode.py already performed the byte swap at
// convert time. Hull::FixUp must keep doing exactly what it does today -- rebase
// mpaStaticTrafficVehicles and walk NO sub-elements. A swap on the load path would
// corrupt correct data.
//
// FixUp / FixDown (DWARF :64 / :69) are consequently DECLARED-ONLY here, matching
// the established convention for this whole family in this directory
// (BrnTrafficSection.h LaneRung::EndianSwap, BrnTrafficVehicleAsset.h
// VehicleAsset::EndianSwap, CgsSet.h). Reasons, all three independent:
//   1. There is no standalone X360 symbol for either (they fold to nothing on a
//      big-endian target, which is why Hull::FixUp walks no sub-elements at all);
//   2. the Feb-2007 bodies are `rw::EndianSwap(mTransform); rw::EndianSwap(mFlowTypeID);`
//      and rw::EndianSwap is not reconstructed anywhere in this tree yet;
//   3. on the host the data is already native-endian, so the correct body is a
//      no-op and writing one would be indistinguishable from an invented stub.
// =============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // Matrix44Affine (rw::math::vpu, four 16-byte rows)

namespace BrnTraffic
{
    // BrnTrafficStaticTraffic.h:46 -- one authored parked car.
    struct StaticTrafficVehicle
    {
        // :56  +0x00 .. +0x3F -- world placement. wAxis (+0x30) is the position;
        // FillNewHull's proximity cull loads exactly that row (`lvx128 v12, r11, 48`).
        Matrix44Affine mTransform;

        // :57  +0x40 -- index into TrafficData::mpapFlowTypes. FillNewHull passes it
        // straight to PickVehicleToSpawn @0x827235F8 to choose the vehicle type.
        u16 mFlowTypeID;

        // :58  +0x42 -- spawn probability as a PERCENTAGE. FillNewHull rolls
        // `rng % 100 + 1` (i.e. 1..100) and skips the record when the roll exceeds
        // this. The shipped data uses only two values: 100 (867 records) and 75 (8).
        u8 mExistsAtAllChance;

        // :59  +0x43 -- ship-only mode gate bits, absent from the Feb-2007 leak.
        // FillNewHull @0x82743600 tests two of them, in this order (the two module
        // bools are named from the C1 keystone header, X360 +0x717DD / +0x717E7):
        //     if ((muFlags & 2) != 0 && !mbPlayingShowtimeMode)      skip;   // :716
        //     if (!mbAllowDivergentBehaviour && (muFlags & 1) != 0)  skip;   // :726
        // i.e. bit 1 marks a record that only exists in showtime mode, and bit 0 a
        // record that only exists when divergent behaviour is allowed.
        // FLAG (unnamed bits): the individual bit MEANINGS have no name in the DWARF
        // (the member is a plain uint8_t) and no enum exists for them in ARTIST, so
        // no E_STATICTRAFFICFLAG_* enum is minted here -- that would be invention.
        // The shipped distribution is 0:368, 1:473, 2:17, 3:17, i.e. exactly the two
        // bits the asm tests and nothing else.
        u8 muFlags;

        // DWARF :64 / :69. DECLARED-ONLY -- see the endianness banner above.
        // Deliberately not defined: no X360 symbol, no rw::EndianSwap in this tree,
        // and the shipped host payload is already native-endian.
        void FixUp(const void* lpBaseData);
        void FixDown(const void* lpBaseData);

        // Never called. A MEMBER function so the body is a complete-class context
        // (offsetof reaches every member). Body + evidence in
        // BrnTrafficStaticTraffic.cpp.
        static void _AssertLayout();
    };

    // The one pin every consumer depends on: Hull::GetStaticVehicle @0x82705C90
    // indexes this array with a hard-coded 80-byte stride. Kept at namespace scope
    // as well as inside _AssertLayout so a TU that only includes the header still
    // fails loudly if the math types ever change width.
    static_assert(sizeof(StaticTrafficVehicle) == 80,
                  "StaticTrafficVehicle stride (Hull::GetStaticVehicle @0x82705C90 uses 80)");
}

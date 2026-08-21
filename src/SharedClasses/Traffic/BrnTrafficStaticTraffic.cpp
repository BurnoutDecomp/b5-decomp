// =============================================================================
// BrnTrafficStaticTraffic.cpp  (owning .cpp for BrnTraffic::StaticTrafficVehicle)
//
// The record itself is a plain serialised value type; its only out-of-line content
// is the never-called layout pin below. The two DWARF-declared endian shims
// (FixUp :64 / FixDown :69) are deliberately NOT defined here -- the full argument
// is in the header banner, in short:
//
//   * ARTIST has no standalone symbol for either. Hull::FixUp @0x827620A0 rebases the
//     twelve hull pointer slots and walks NO sub-elements, so nothing on the console
//     load path ever calls them; they fold to nothing on a big-endian target.
//   * The Feb-2007 bodies are two rw::EndianSwap calls, and rw::EndianSwap has no
//     reconstruction anywhere in b5-decomp yet (CgsSet.h, BrnTrafficSection.h and
//     BrnTrafficVehicleAsset.h all park their EndianSwap bodies for the same reason).
//   * MEASURED: the shipped PC payload is ALREADY little-endian, so the only correct
//     host body is a no-op -- and an empty body would be indistinguishable from an
//     invented stub. See the evidence block below.
//
// -----------------------------------------------------------------------------
// ENDIAN EVIDENCE (2026-08-21, wave T1 / C2 -- the wire question this cluster owns)
//
//   build/game/B5TRAFFIC.BNDL                    `bnd2` version 2, PLATFORM 4, flags 6
//                                                (uncompressed), 1 resource, type 65538,
//                                                payload 0x2B04E0 at file +0x70
//   D:/bp-staging/x360-retail/.../B5TRAFFIC.BNDL `bnd2` version 2, PLATFORM 2, flags 7
//                                                (zlib), payload inflates to 0x2AC100
//
//   Both carry muDataVersion 44, muNumHulls 315, 113 hulls with parked cars and 875
//   static-vehicle records in total. Reading hull 22's record 0 from each:
//
//     X360 (big-endian, 4-byte pointers)   PC (little-endian, 8-byte pointers)
//       rows  0.6216 -0.0019 -0.7833         rows  0.6216 -0.0019 -0.7833
//             0.0031  1.0000  0.0000               0.0031  1.0000  0.0000
//             0.7833 -0.0024  0.6216               0.7833 -0.0024  0.6216
//            -3424.23 483.11 -3423.99             -3424.23 483.11 -3423.99
//       +0x40 bytes  00 16 64 01                  +0x40 bytes  16 00 64 01
//       mFlowTypeID 22  chance 100  flags 1       mFlowTypeID 22  chance 100  flags 1
//
//   The multi-byte fields are byte-REVERSED between the two files and decode to the
//   same values; the two single-byte fields are carried through verbatim. Whole-set
//   check over all 875 PC records: 875/875 have orthonormal rotation rows and in-range
//   translations read little-endian and 0/875 read big-endian; 875/875 have
//   mFlowTypeID < muNumFlowTypes (44) little-endian and 0/875 big-endian.
//
//   => tools/assets/bundles/lane_transcode.py performs the swap at CONVERT time.
//      Hull::FixUp (BrnTrafficHull.cpp, already live) must keep rebasing
//      mpaStaticTrafficVehicles and must NOT swap anything inside the records.
// =============================================================================

#include "SharedClasses/Traffic/BrnTrafficStaticTraffic.h"
#include <cstddef>   // offsetof

namespace BrnTraffic
{
    // ------------------------------------------------------------------------
    // Never called. Pins the serialised record so a math-type width change or a
    // member reorder is a compile error rather than a silent parked-car garble.
    //
    // Every number below is X360-attested, not chosen:
    //   80  -- Hull::GetStaticVehicle @0x82705C90: `return 80 * luIndex + *(hull + 36);`
    //   64  -- TrafficEntityModule::FillNewHull @0x82743600 reads mFlowTypeID as
    //          *(record + 64) and hands it to PickVehicleToSpawn @0x827235F8
    //   66  -- FillNewHull compares `rng % 100 + 1` against *(record + 66)
    //   67  -- FillNewHull tests *(record + 67) with & 2 then & 1
    //   48  -- FillNewHull's `lvx128 v12, r11, r24` with _R24 == 48 loads the
    //          transform's translation row for the proximity cull
    //
    // These are POINTER-INVARIANT facts: StaticTrafficVehicle contains no pointers,
    // so its console and host footprints are identical and pinning the absolute
    // offsets here is legitimate (contrast Hull, whose pointer block is only pinned
    // by member ORDER because 4-byte console pointers become 8 bytes on the host).
    // ------------------------------------------------------------------------
    void StaticTrafficVehicle::_AssertLayout()
    {
        static_assert(sizeof(Matrix44Affine) == 64, "Matrix44Affine must be four 16-byte rows");
        static_assert(alignof(Matrix44Affine) == 16, "Matrix44Affine must be 16-byte aligned");

        static_assert(offsetof(StaticTrafficVehicle, mTransform)         == 0x00,
                      "StaticTrafficVehicle::mTransform @+0x00");
        static_assert(offsetof(StaticTrafficVehicle, mFlowTypeID)        == 0x40,
                      "StaticTrafficVehicle::mFlowTypeID @+0x40 (FillNewHull *(rec+64))");
        static_assert(offsetof(StaticTrafficVehicle, mExistsAtAllChance) == 0x42,
                      "StaticTrafficVehicle::mExistsAtAllChance @+0x42 (FillNewHull *(rec+66))");
        static_assert(offsetof(StaticTrafficVehicle, muFlags)            == 0x43,
                      "StaticTrafficVehicle::muFlags @+0x43 (FillNewHull *(rec+67))");

        // The proximity cull loads the translation row at +0x30 out of the record.
        static_assert(offsetof(StaticTrafficVehicle, mTransform) + 0x30 == 0x30,
                      "StaticTrafficVehicle transform translation row @+0x30 (FillNewHull lvx +48)");

        // The stride the whole array walk depends on.
        static_assert(sizeof(StaticTrafficVehicle) == 80,
                      "StaticTrafficVehicle stride 80 (Hull::GetStaticVehicle @0x82705C90)");
        static_assert(alignof(StaticTrafficVehicle) == 16,
                      "StaticTrafficVehicle inherits the transform's 16-byte alignment");
    }
}

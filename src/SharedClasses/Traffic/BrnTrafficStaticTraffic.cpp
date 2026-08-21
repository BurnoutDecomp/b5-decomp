// =============================================================================
// BrnTrafficStaticTraffic.cpp -- owning .cpp for BrnTraffic::StaticTrafficVehicle.
// A plain serialised value type; the only out-of-line content is the never-called
// layout pin below.
//
// PARK: the DWARF endian shims FixUp (:64) / FixDown (:69) stay declared-only.
// ARTIST has no standalone symbol for either, rw::EndianSwap has no reconstruction
// in this tree, and the shipped PC payload is already little-endian -- all 875
// records in build/game/B5TRAFFIC.BNDL parse little-endian and match the X360
// original's big-endian read value-for-value, so the swap already happened at
// convert time and the only correct host body is a no-op.
// =============================================================================

#include "SharedClasses/Traffic/BrnTrafficStaticTraffic.h"
#include <cstddef>   // offsetof

namespace BrnTraffic
{
    // ------------------------------------------------------------------------
    // Never called. Pins the serialised record so a math-type width change or a
    // member reorder is a compile error, not a silent parked-car garble. Every
    // number is X360-attested:
    //   80  -- Hull::GetStaticVehicle @0x82705C90: `80 * luIndex + *(hull + 36)`
    //   64  -- FillNewHull @0x82743600 reads mFlowTypeID as *(record + 64)
    //   66  -- FillNewHull compares `rng % 100 + 1` against *(record + 66)
    //   67  -- FillNewHull tests *(record + 67) with & 2 then & 1
    //   48  -- FillNewHull's `lvx128 v12, r11, r24` (_R24 == 48) loads the
    //          transform's translation row for the proximity cull
    // The record contains no pointers, so console and host footprints are identical
    // and pinning absolute offsets here is safe (contrast Hull, whose pointer block
    // is pinned by member order only).
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

#include "GameShared/GameClasses/Geometric/Primitives/CgsSweptSphere.h"

// CgsGeometric::SweptSphere -- out-of-line bodies. ⭐ TU CREATED 2026-08-14 (deformation-mount
// wave): the header has always said the bodies live in "the SweptSphere TU"; none existed. The
// first in-tree caller (DeformableObject::ResetSensors @0x82623D60, phase 3) landed this wave.
//
// Set(Vector3Plus, Vector3Plus) has NO out-of-line X360 export: the console INLINES it at the
// ResetSensors swept-seeding loops, where the two packed-lane stores are fully attested
// (0x82624730/0x826247A8 for the sensor loop, 0x82624864/0x8262487C for the wheel loop):
//   stvx128 <positionAndRadius>  -> maSweptSpheres[i] + 0x00
//   stvx128 <directionAndLength> -> maSweptSpheres[i] + 0x10
// i.e. the two members assigned whole, nothing else. The remaining declared methods stay
// declare-only until a caller lands them with their own asm witness.

namespace CgsGeometric
{
    // Inline-attested at ResetSensors' swept-sphere seeding (see the TU banner).
    void SweptSphere::Set(Vector3Plus lPositionAndRadius, Vector3Plus lDirectionAndLength)
    {
        mPositionAndRadius   = lPositionAndRadius;
        mDirectionAndLength  = lDirectionAndLength;
    }

    // The two packed-lane getters (DWARF CgsSweptSphere.h:56/:59). The console INLINES both
    // -- IntersectTriangle4SweptSphere @0x8283EF50 opens with `lvx128 v11, r0, r3` (+0x00) and
    // `lvx128 v10, r0, r11` where r11 = r3 + 0x10, i.e. the two members read whole, in this
    // order and with no other work. Landed by the swept kernel, which is their first in-tree
    // caller.
    Vector3Plus SweptSphere::GetPositionAndRadius() const
    {
        return mPositionAndRadius;
    }

    Vector3Plus SweptSphere::GetDirectionAndLength() const
    {
        return mDirectionAndLength;
    }
}

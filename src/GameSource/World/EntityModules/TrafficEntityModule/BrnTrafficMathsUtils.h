#ifndef BRN_TRAFFIC_MATHS_UTILS_H
#define BRN_TRAFFIC_MATHS_UTILS_H

// ============================================================================
// BrnTrafficMathsUtils.h -- OWNING HEADER.
//
// The traffic module's small geometry helpers: free functions in namespace BrnTraffic, all
// inline on the console, so none appears in the ARTIST ledger.
//
// The file lands DECLARED-ONLY, in the exact DWARF shape, the same convention this directory
// uses for LaneRung::EndianSwap. The Feb-2007 copy is NOT ported: its one function
// (`GetLineLineIntersectionParamXZ`) appears nowhere in the DWARF, which instead names two
// functions the leak lacks at lines :80 and :191, so the ship file is at least 191 lines
// against the leak's 74. The leaked body also needs Vector2 operations (Normalize, operator-,
// IsSimilar) that this tree's vector2_operation.h does not carry.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "rw/math/vpu/vector3_operation.h"   // Dot, operator-, operator*

#include <cmath>   // std::sqrt

namespace BrnTraffic
{
    // DWARF BrnTrafficMathsUtils.h:80; out-of-line body @0x82714768 (called by
    // UpdateParam_CheckIfNeedToSlow @0x82738948 and by 0x82717110). PARAMETER ORDER is the
    // asm's v1..v6, which corrects this header's earlier guess.
    //
    // A cone from lConeOrigin along lConeDirection, half-angle acos(lfCosAngle), cut off at
    // lfLength, with the vertical axis scaled by lfRecipYScale before the test (the
    // "squish"). Inline here because the console declares it in this header and folds it at
    // most call sites; keeping it header-inline avoids a new unmounted TU.
    inline bool IsPointWithinSquishedCone(Vector3 lConeOrigin,
                                          Vector3 lConeDirection,
                                          VecFloat lfCosAngle,
                                          VecFloat lfLength,
                                          VecFloat lfRecipYScale,
                                          Vector3 lPoint)
    {
        // 0x82714768 vsubfp / vspltw(1) / vmulfp / vrlimi(mask 4 == lane y).
        Vector3 lDiff = lPoint - lConeOrigin;
        lDiff.y = lDiff.y * lfRecipYScale.x;

        // 0x827147AC..0x827147D8 -- behind the apex, or past the cone's length.
        const f32 lfAlong = rw::math::vpu::Dot(lConeDirection, lDiff);
        if (0.0f > lfAlong)
        {
            return false;
        }
        if (lfAlong > lfLength.x)
        {
            return false;
        }

        // 0x827147DC..0x82714838 -- vrsqrtefp + two Newton steps, then dot with the axis.
        // FLAG (host guard, no console equivalent): the console runs the raw rsqrt and returns
        // TRUE for a point coincident with the cone origin. False kept here instead.
        const f32 lfLenSq = rw::math::vpu::Dot(lDiff, lDiff);
        if (lfLenSq <= 0.0f)
        {
            return false;
        }

        const Vector3 lUnit = lDiff * (1.0f / std::sqrt(lfLenSq));
        const f32     lfCos = rw::math::vpu::Dot(lUnit, lConeDirection);

        return !(lfCosAngle.x > lfCos);
    }

    // DWARF BrnTrafficMathsUtils.h:191. DECLARED-ONLY, same reason.
    void Convert3DVectorTo2D(Vector3 lVector, Vector2& lrOut2D);
}

#endif // BRN_TRAFFIC_MATHS_UTILS_H

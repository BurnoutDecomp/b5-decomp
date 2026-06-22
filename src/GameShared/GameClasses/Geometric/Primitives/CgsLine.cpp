#include "GameShared/GameClasses/Geometric/Primitives/CgsLine.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82812370
//   (CgsGeometric::Line::IsValid)
//
// The X360 body is hand-vectorised VMX: it `lvx128`-loads the two endpoints
// (`this+0x00` = mStart, `this+0x10` = mEnd) and runs the per-lane self-compare
// idiom `vcmpeqfp. vR, vR, vR` on each of the x/y/z lanes (splatted one at a
// time with `vspltw`). `x == x` is false iff x is NaN, so each `vcmpeqfp.` is a
// finiteness (not-NaN) predicate; the `mfocrf`/`extrwi r,r,1,24` reads the
// comparison's "all-true" CR6 bit. The control flow ANDs the three lane tests
// for a point; the start point is tested first, and only if it passes is the end
// point tested. The result is true iff BOTH endpoints have finite xyz.
//
// Lowered to portable scalar float math (the lane equality reduces to a per-
// component self-compare). The w lane is never tested (the asm only splats lanes
// 0/1/2). Called only by BaseCollisionGenerator line-vs-polysoup queries.

namespace CgsGeometric
{
    namespace
    {
        // vcmpeqfp self-compare per lane: a component equals itself iff not NaN.
        inline bool PointXyzFinite(const Vector3Plus& lrPoint)
        {
            return lrPoint.x == lrPoint.x
                && lrPoint.y == lrPoint.y
                && lrPoint.z == lrPoint.z;
        }
    }

    bool Line::IsValid() const
    {
        // Start point first; end point only checked if the start is finite
        // (matches the asm's short-circuit branch to the false sink).
        if (!PointXyzFinite(mStart))
        {
            return false;
        }
        if (!PointXyzFinite(mEnd))
        {
            return false;
        }
        return true;
    }
}

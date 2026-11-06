#include "vendor/renderware/collision/LineSegIntersect.hpp"
#include "vendor/renderware/collision/LineSegKernelMath.hpp"
#include <cfloat>
namespace rw { namespace collision {
static const f32 KF_PLANE_LINE_EPSILON = (FLT_MIN * 0.25f);   // flt_821801B0 == 0x00200000 (subnormal, exact)

s32 rwcPlaneLineSegIntersect(Fraction* lpDist, f32 afOrig, f32 afSeg, f32 afSign, f32 afDisp)
{
    const f32 lfNum = linemath::ScalarMadd(afOrig, afSign, -afDisp);     // fmsubs f0, f1, f3, f4
    if (!(lfNum > 0.0f))                                      // fcmpu f0, 0.0 ; bgt @0x82BA8828
    {
        lpDist->num = 0.0f;                                   // stfs f13 (flt_82001CC0)
        lpDist->den = 1.0f;                                   // stfs f0  (flt_82001C98)
        return 1;
    }
    const f32 lfDen = -(afSeg * afSign);                      // fmuls f13, f2, f3 ; fneg f13
    lpDist->num = lfNum;                                      // stfs f0, 0(r3)
    lpDist->den = lfDen;                                      // stfs f13, 4(r3)
    if (lfDen < KF_PLANE_LINE_EPSILON)                        // blt @0x82BA8860
    {
        return -1;
    }
    if (lfDen < lfNum * KF_PLANE_LINE_EPSILON)                // fmuls ; blt @0x82BA886C
    {
        return -1;
    }
    return (lfNum < lfDen) ? 1 : 0;                           // bltlr @0x82BA8878
}
} }

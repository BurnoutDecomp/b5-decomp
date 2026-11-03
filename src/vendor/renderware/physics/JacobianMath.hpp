#pragma once

#include "types.hpp"
#include "rw/math/vpu/types.h"


// X360 Jacobian operations. ARTIST uses four VMX lanes, including packed W
// values, and fused arithmetic. Native operations below must be checked at each
// call site; builders and solver paths are still under reconstruction.

namespace rw
{
namespace physics
{
namespace jacobian_detail
{
    typedef rw::math::vpu::Vector3    V3;
    typedef rw::math::vpu::Vector4    V4;
    typedef rw::math::vpu::Quaternion Quat;
    typedef rw::math::vpu::Matrix33   M33;

    // Native row copies keep the packed W bits. A caller that masks W must
    // do so at an instruction-attested site rather than in this shared copy.
    inline V3 Xyz(const V3& lrV)   { return lrV; }
    inline V3 Xyz(const V4& lrV)   { return V3(lrV.mV); }
    inline V3 Xyz(const Quat& lrQ) { return V3(__lvx(&lrQ, 0)); }

    inline V3 Sub(const V3& lrA, const V3& lrB)
    { return V3(__vsubfp(lrA.mV, lrB.mV)); }

    inline V3 Scale(const V3& lrA, f32 lfS)
    { return V3(__vmulfp(lrA.mV, __vspltw(__lvlx(&lfS, 0), 0))); }

    inline V3 MulLanes(const V3& lrA, const V3& lrB)
    { return V3(__vmulfp(lrA.mV, lrB.mV)); }

    inline f32 Dot3(const V3& lrA, const V3& lrB)
    { return __vmsum3fp(lrA.mV, lrB.mV).v[0]; }

    inline V3 Min3(const V3& lrA, const V3& lrB)
    { return V3(__vminfp(lrA.mV, lrB.mV)); }

    inline V3 Max3(const V3& lrA, const V3& lrB)
    { return V3(__vmaxfp(lrA.mV, lrB.mV)); }

    // ARTIST 0x82BC4908..4940 and 0x82BC5A34..5A70: two
    // reciprocal-root refinements, multiply by the input and select +0 for 0.
    inline f32 Sqrt(f32 lfX)
    {
        const __vector4 value = __vspltw(__lvlx(&lfX, 0), 0);
        const __vector4 root = rw::math::vpu::detail::Vector3NativeReciprocalRoot(value, 2);
        const __vector4 zero = __vspltisw(0);
        return __vsel(__vmulfp(value, root), zero, __vcmpeqfp(value, zero)).v[0];
    }

    // The Hamilton product lrA (x) lrB, in the exact shape both builders' prologues emit
    // (8/8 instructions, hand-verified on X360 and re-read on BurnoutPR):
    //     vmaddfp v0, B, cross, splat(A.w)   == B.xyz*A.w + (A x B)
    //     vmaddfp v0, A, v0,    splat(B.w)   == A.xyz*B.w + B.xyz*A.w + (A x B)
    //     vsubfp  v6, splat(A.w)*splat(B.w), dot3(A,B)
    //     vrlimi128 v0, v6, 1, 1             -- insert the real part into lane w
    inline Quat QuatMul(const Quat& lrA, const Quat& lrB)
    {
        // ARTIST 0x82BC4338..43E8: rounded first product, fused cross,
        // two fused accumulations, then replace the scalar lane.
        const __vector4 a = __lvx(&lrA, 0);
        const __vector4 b = __lvx(&lrB, 0);
        const __vector4 aw = __vspltw(a, 3);
        const __vector4 bw = __vspltw(b, 3);
        const __vector4 product = __vmulfp(a, __vpermwi(b, 0x63));
        const __vector4 cross = __vpermwi(__vnmsubfp(__vpermwi(a, 0x63), b, product), 0x63);
        __vector4 result = __vmaddfp(b, aw, cross);
        result = __vmaddfp(a, bw, result);
        const __vector4 real = __vsubfp(__vmulfp(aw, bw), __vmsum3fp(a, b));
        result = __vrlimi(result, real, 1, 1);
        Quat out;
        __stvx(result, &out, 0);
        return out;
    }

    // The symmetric world inverse inertia, expanded from the body's two packed vectors.
    //     mIfull.xyz = (Ixx, Ixy, Ixz)      mIsplt.xyz = (Izz, Iyy, Iyz)   -- Izz FIRST
    // ⭐ The expansion is READ, not inferred: the X360 `vperm` control vectors are
    //     0x821816B0 = 04050607 14151617 18191a1b 0c0d0e0f = (A.y, B.y, B.z, A.w) = ROW 1
    //     0x821816A0 = 08090a0b 18191a1b 10111213 0c0d0e0f = (A.z, B.z, B.x, A.w) = ROW 2
    // with vA = mIfull and vB = mIsplt, and ROW 0 is mIfull itself -- which is why only two
    // tables exist. Xbox One builds the identical three rows with explicit `vinsertps`
    // immediates out of [body+90h/94h/98h] and [body+A0h/A4h/A8h] (0x1409B1DC5..0x1409B1E23).
    inline M33 UnpackInverseInertia(const V4& lrIfull, const V4& lrIsplt)
    {
        // ARTIST controls 0x821816B0 / 0x821816A0 retain the source W lane.
        static const CGS_ALIGN(16) u32 kRow1[4] = {0x04050607, 0x14151617, 0x18191A1B, 0x0C0D0E0F};
        static const CGS_ALIGN(16) u32 kRow2[4] = {0x08090A0B, 0x18191A1B, 0x10111213, 0x0C0D0E0F};
        return M33(lrIfull.mV, __vperm(lrIfull.mV, lrIsplt.mV, __lvx(kRow1, 0)),
                   __vperm(lrIfull.mV, lrIsplt.mV, __lvx(kRow2, 0)));
    }

    inline M33 ZeroMatrix33()
    {
        M33 lResult;
        lResult.SetZero();
        return lResult;
    }

    // I^-1 . u, built exactly as the console does it -- `u.x*row0 + u.y*row1 + u.z*row2`
    // from three `vspltw` splats, accumulated with the vmaddfp/vmaddcfp128 chain.
    inline V3 Transform(const M33& lrM, const V3& lrU)
    {
        // ARTIST 0x82BC4D04..4D50: three fused accumulations,
        // starting with the zero vector and retaining all four lanes.
        __vector4 value = __vmaddfp(lrM.xAxis.mV, __vspltw(lrU.mV, 0), __vspltisw(0));
        value = __vmaddfp(lrM.yAxis.mV, __vspltw(lrU.mV, 1), value);
        value = __vmaddfp(lrM.zAxis.mV, __vspltw(lrU.mV, 2), value);
        return V3(value);
    }

    inline const V3& Row(const M33& lrM, u32 luRow)
    { return (luRow == 0u) ? lrM.xAxis : ((luRow == 1u) ? lrM.yAxis : lrM.zAxis); }

    inline V3& Row(M33& lrM, u32 luRow)
    { return (luRow == 0u) ? lrM.xAxis : ((luRow == 1u) ? lrM.yAxis : lrM.zAxis); }

    // Project a world vector onto a three-axis frame: (dot(row0,v), dot(row1,v), dot(row2,v)).
    // Built on the console from three `vmsum3fp128` plus a `vperm`/`vrlimi128` gather; lane i
    // is row i on both builders.
    inline V3 Project(const M33& lrFrame, const V3& lrV)
    {
        // ARTIST 0x82BC4670..4688 / 0x82BC5AF8: gather the three
        // dot products with the 0x82CDA350 control, including its W lane.
        static const CGS_ALIGN(16) u32 kGather[4] = {0x00010203, 0x14151617, 0x00010203, 0x00010203};
        const __vector4 x = __vmsum3fp(lrV.mV, lrFrame.xAxis.mV);
        const __vector4 y = __vmsum3fp(lrV.mV, lrFrame.yAxis.mV);
        const __vector4 z = __vmsum3fp(lrV.mV, lrFrame.zAxis.mV);
        return V3(__vrlimi(__vperm(x, y, __lvx(kGather, 0)), z, 2, 0));
    }


}
}
}

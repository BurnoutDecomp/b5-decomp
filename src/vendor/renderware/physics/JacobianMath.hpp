#pragma once

#include "types.hpp"
#include "rw/math/vpu/types.h"

#include <cmath>   // std::sqrt

// ===========================================================================
// Shared scalar helpers for the two rw::physics constraint builders
// (JointJacobian::Build @0x82BC42E8 and DriveJacobian::Build @0x82BC5590).
//
// WHY SCALAR AND NOT INTRINSICS. X360 is VMX128 and the PC is SSE/AVX; neither instruction
// set is portable, and BurnoutPR's Hex-Rays `_mm_*` output is readable but not compilable.
// What carries the fidelity in these two functions is the LANE ASSIGNMENT and the operand
// order, not the instruction set -- and both are recorded at every site in the two bodies.
//
// ⚠️ THE TWO OPERAND-ORDER RULES EVERY EXPRESSION IN THOSE BODIES DEPENDS ON. These are the
// ARCHITECTURE's, not an IDA quirk -- proven from the image word at 0x82BC4E88 = 0x139D1C6E,
// which decodes field for field as `vmaddfp v28,v29,v3,v17` (opcode 4, VD bits 6-10, VA
// 11-15, VB 16-20, VC 21-25, XO 46):
//     vmaddfp  vD,vA,vB,vC  ==  vD = vA*vC + vB     (the THIRD printed operand is the second
//                                                    multiplicand; the SECOND is the addend)
//     vnmsubfp vD,vA,vB,vC  ==  vD = vB - vA*vC
// Read left to right, every expression in these builders comes out transposed.
//
// ⭐ THE TWO SWIZZLE TABLES ARE READ, NOT ARGUED:
//     0x82181670 = 08090a0b 00010203 04050607 0c0d0e0f = (z, x, y, w) = ZXY
//     0x82181680 = 04050607 08090a0b 00010203 0c0d0e0f = (y, z, x, w) = YZX
// so the fused pair `perm(a,YZX)*perm(b,ZXY) - perm(a,ZXY)*perm(b,YZX)` is
// a.yzx*b.zxy - a.zxy*b.yzx == cross(a, b), NOT cross(b, a). BurnoutPR's `pshufd 9` / `12h`
// and Xbox One's `vshufps 9` / `12h` are the same two swizzles -- three witnesses.
// ===========================================================================

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

    inline V3 MakeV3(f32 lfX, f32 lfY, f32 lfZ)
    { V3 r = { lfX, lfY, lfZ, 0.0f }; return r; }

    // Three distinct 16-byte lane types with the same shape; the overload set drops the
    // fourth lane in every case, which is what the console's `vmsum3fp128` / 3-lane
    // arithmetic does and what keeps a packed scalar out of a dot product.
    inline V3 Xyz(const V3& lrV)   { return MakeV3(lrV.x, lrV.y, lrV.z); }
    inline V3 Xyz(const V4& lrV)   { return MakeV3(lrV.x, lrV.y, lrV.z); }
    inline V3 Xyz(const Quat& lrQ) { return MakeV3(lrQ.x, lrQ.y, lrQ.z); }

    inline V3 Add(const V3& lrA, const V3& lrB)
    { return MakeV3(lrA.x + lrB.x, lrA.y + lrB.y, lrA.z + lrB.z); }

    inline V3 Sub(const V3& lrA, const V3& lrB)
    { return MakeV3(lrA.x - lrB.x, lrA.y - lrB.y, lrA.z - lrB.z); }

    inline V3 Scale(const V3& lrA, f32 lfS)
    { return MakeV3(lrA.x * lfS, lrA.y * lfS, lrA.z * lfS); }

    inline V3 MulLanes(const V3& lrA, const V3& lrB)
    { return MakeV3(lrA.x * lrB.x, lrA.y * lrB.y, lrA.z * lrB.z); }

    inline f32 Dot3(const V3& lrA, const V3& lrB)
    { return lrA.x * lrB.x + lrA.y * lrB.y + lrA.z * lrB.z; }

    inline V3 Cross(const V3& lrA, const V3& lrB)
    {
        return MakeV3(lrA.y * lrB.z - lrA.z * lrB.y,
                      lrA.z * lrB.x - lrA.x * lrB.z,
                      lrA.x * lrB.y - lrA.y * lrB.x);
    }

    inline V3 Min3(const V3& lrA, const V3& lrB)
    {
        return MakeV3(lrA.x < lrB.x ? lrA.x : lrB.x,
                      lrA.y < lrB.y ? lrA.y : lrB.y,
                      lrA.z < lrB.z ? lrA.z : lrB.z);
    }

    inline V3 Max3(const V3& lrA, const V3& lrB)
    {
        return MakeV3(lrA.x > lrB.x ? lrA.x : lrB.x,
                      lrA.y > lrB.y ? lrA.y : lrB.y,
                      lrA.z > lrB.z ? lrA.z : lrB.z);
    }

    // X360 `vrsqrtefp` + two Newton-Raphson refinements, with a `vsel` that forces 0 when the
    // input is 0. BurnoutPR uses `sqrtps`. Behaviourally identical; not bit-identical.
    inline f32 Sqrt(f32 lfX) { return (lfX > 0.0f) ? std::sqrt(lfX) : 0.0f; }

    // The Hamilton product lrA (x) lrB, in the exact shape both builders' prologues emit
    // (8/8 instructions, hand-verified on X360 and re-read on BurnoutPR):
    //     vmaddfp v0, B, cross, splat(A.w)   == B.xyz*A.w + (A x B)
    //     vmaddfp v0, A, v0,    splat(B.w)   == A.xyz*B.w + B.xyz*A.w + (A x B)
    //     vsubfp  v6, splat(A.w)*splat(B.w), dot3(A,B)
    //     vrlimi128 v0, v6, 1, 1             -- insert the real part into lane w
    inline Quat QuatMul(const Quat& lrA, const Quat& lrB)
    {
        const V3 lvA = Xyz(lrA);
        const V3 lvB = Xyz(lrB);
        const V3 lvVec = Add(Add(Scale(lvB, lrA.w), Scale(lvA, lrB.w)), Cross(lvA, lvB));
        Quat lResult = { lvVec.x, lvVec.y, lvVec.z, lrA.w * lrB.w - Dot3(lvA, lvB) };
        return lResult;
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
        M33 lResult;
        lResult.xAxis = MakeV3(lrIfull.x, lrIfull.y, lrIfull.z);   // Ixx, Ixy, Ixz
        lResult.yAxis = MakeV3(lrIfull.y, lrIsplt.y, lrIsplt.z);   // Ixy, Iyy, Iyz
        lResult.zAxis = MakeV3(lrIfull.z, lrIsplt.z, lrIsplt.x);   // Ixz, Iyz, Izz
        return lResult;
    }

    inline M33 ZeroMatrix33()
    {
        M33 lResult;
        lResult.xAxis = MakeV3(0.0f, 0.0f, 0.0f);
        lResult.yAxis = MakeV3(0.0f, 0.0f, 0.0f);
        lResult.zAxis = MakeV3(0.0f, 0.0f, 0.0f);
        return lResult;
    }

    // I^-1 . u, built exactly as the console does it -- `u.x*row0 + u.y*row1 + u.z*row2`
    // from three `vspltw` splats, accumulated with the vmaddfp/vmaddcfp128 chain.
    inline V3 Transform(const M33& lrM, const V3& lrU)
    {
        return Add(Add(Scale(lrM.xAxis, lrU.x), Scale(lrM.yAxis, lrU.y)),
                   Scale(lrM.zAxis, lrU.z));
    }

    inline const V3& Row(const M33& lrM, u32 luRow)
    { return (luRow == 0u) ? lrM.xAxis : ((luRow == 1u) ? lrM.yAxis : lrM.zAxis); }

    inline V3& Row(M33& lrM, u32 luRow)
    { return (luRow == 0u) ? lrM.xAxis : ((luRow == 1u) ? lrM.yAxis : lrM.zAxis); }

    // Project a world vector onto a three-axis frame: (dot(row0,v), dot(row1,v), dot(row2,v)).
    // Built on the console from three `vmsum3fp128` plus a `vperm`/`vrlimi128` gather; lane i
    // is row i on both builders.
    inline V3 Project(const M33& lrFrame, const V3& lrV)
    { return MakeV3(Dot3(lrFrame.xAxis, lrV), Dot3(lrFrame.yAxis, lrV), Dot3(lrFrame.zAxis, lrV)); }

    // Gather component `luComp` of the three rows: (row0[c], row1[c], row2[c]).
    //
    // ⭐ THIS IS A STRAIGHT TRANSPOSE, AND IT IS READ RATHER THAN INFERRED (2026-08-04). The
    // three X360 `vperm` control vectors the drive tail uses were previously unread:
    //     0x82CDA3F0 = 00010203 10111213 00010203 00010203 = (A.x, B.x, A.x, A.x)
    //     0x82CDADC0 = 04050607 14151617 00010203 00010203 = (A.y, B.y, A.x, A.x)
    //     0x82CDADE0 = 08090a0b 18191a1b 00010203 00010203 = (A.z, B.z, A.x, A.x)
    // -- lane 0 takes component i of vA and lane 1 component i of vB -- and the following
    // `vrlimi128 v0,v3,mask=z,rot={2,3,0}` inserts component i of the third vector into lane
    // z (a rotate of n on lane z selects vB[(2+n)&3], i.e. .x/.y/.z for n = 2/3/0).
    // So the row really is (v5[i], v4[i], v3[i]) for i = x, y, z.
    inline V3 GatherComponent(const V3& lrR0, const V3& lrR1, const V3& lrR2, u32 luComp)
    {
        if (luComp == 0u) return MakeV3(lrR0.x, lrR1.x, lrR2.x);
        if (luComp == 1u) return MakeV3(lrR0.y, lrR1.y, lrR2.y);
        return MakeV3(lrR0.z, lrR1.z, lrR2.z);
    }
}
}
}

#pragma once

#include <cmath>     // std::fma, std::sqrt

#include "types.hpp"
#include "vendor/renderware/collision/FeatureEdge.hpp"   // Vec4

// ===========================================================================
// The idioms the three volume line kernels are built from, written the way the console computes them:
//   rw::collision::SphereVolume::LineSegIntersect   @ 0x82BA82C8
//   rw::collision::BoxVolume::LineSegIntersect      @ 0x82BA9478
//   rw::collision::CapsuleVolume::LineSegIntersect  @ 0x82BAFCF8
//   rw::collision::rwcPlaneLineSegIntersect         @ 0x82BA8818
// (all four in LineSegIntersect.cpp, beside the rwc* kernels they call)
// (crash parity FX-FOLLOWUPS stage (b), 2026-09-25.)
//
// The rounding is chosen per instruction (scratch/CRASHPARITY_0922/ROUNDING_RULE.md):
//   rule 3  vmaddfp / vnmsubfp / vmaddfp128 / fmadds / fmsubs / fnmsubs round ONCE. std::fma is used with the
//           operands in the console's order: the classic `vmaddfp D,A,B,C` is D = A*C + B,
//           `vnmsubfp D,A,B,C` is D = B - A*C, and `vmaddfp128 D,A,B` is D = A*B + D.
//   rule 4  vmulfp / vaddfp / vsubfp / fmuls / fadds / fsubs / fdivs each round separately, as written.
//   rule 1  vmsum3fp128 is ONE rounding of the f64 sum. FLAG (model): xenia DOT_PRODUCT_3.
//   rule 5  the vrefp / vrsqrtefp estimate is taken at its correctly rounded value. FLAG (model). The console's
//           two Newton-Raphson steps then run as written.
//   rule 6  VMX denormal flushing is not modelled.
// All four lanes are computed wherever the console computes four.
// ===========================================================================

namespace rw
{
namespace collision
{
namespace linemath
{
    // The kernels' .rdata words (tools/re/x360rd.py) and loop counts.
    const f32 KF_LINE_ZERO        = 0.0f;                // flt_82001CC0 == 0x00000000
    const f32 KF_LINE_ONE         = 1.0f;                // flt_82001C98 == 0x3F800000
    const f32 KF_LINE_FLT_MIN     = 0x1p-126f;           // 0x00800000: unk_821800C0 (the sphere kernel's |n| floor)
                                                         //   and flt_8218017C (the box's edge-cap denominator floor)
    const f32 KF_LINE_NEG_FLT_MAX = -0x1.fffffep+127f;   // flt_82035570 == 0xFF7FFFFF (the box's separation seed)
    const u32 KU_BOX_LINE_STEPS     = 6u;                // li r19, 6 @0x82BA94E0 ; addic. -1 @0x82BA9C70
    const u32 KU_CAPSULE_LINE_STEPS = 3u;                // li r24, 3 @0x82BAFD78 ; addic. -1 @0x82BB0088

    // vmsum3fp128 -- FLAG (model): one rounding of the exact f64 sum of the three f32 products.
    inline f32 Dot3(const Vec4& arA, const Vec4& arB)
    {
        return static_cast<f32>(static_cast<f64>(arA.x) * arB.x + static_cast<f64>(arA.y) * arB.y
                                + static_cast<f64>(arA.z) * arB.z);
    }

    // vnmsubfp / fnmsubs: -(a*c - b), rounded ONCE and then negated. An exact cancellation is -0, where
    // std::fma(-a, c, b) gives +0; a QNaN keeps its sign (PowerPC: the fmsub result, negated, NaNs unchanged).
    inline f32 Nmsub(f32 afA, f32 afC, f32 afB)
    {
        const f32 lfDifference = std::fma(afA, afC, -afB);
        return (lfDifference != lfDifference) ? lfDifference : -lfDifference;
    }

    // vrefp + two steps of `vnmsubfp r = 1 - est*x` / `vmaddfp est = est*r + est`.
    inline f32 RefinedRecip(f32 afX)
    {
        f32 lfEstimate = static_cast<f32>(1.0 / static_cast<f64>(afX));
        for (u32 luStep = 0; luStep < 2u; ++luStep)
        {
            const f32 lfResidual = Nmsub(lfEstimate, afX, 1.0f);
            lfEstimate = std::fma(lfEstimate, lfResidual, lfEstimate);
        }
        return lfEstimate;
    }

    // vrsqrtefp + two steps of `vmulfp sq = est*est`, `vmulfp half = est*0.5` (vcfsx 1, 1),
    // `vnmsubfp r = 1 - x*sq`, `vmaddfp est = half*r + est`.
    inline f32 RefinedRsqrt(f32 afX)
    {
        f32 lfEstimate = static_cast<f32>(1.0 / std::sqrt(static_cast<f64>(afX)));
        for (u32 luStep = 0; luStep < 2u; ++luStep)
        {
            const f32 lfSquared  = lfEstimate * lfEstimate;
            const f32 lfHalf     = lfEstimate * 0.5f;
            const f32 lfResidual = Nmsub(afX, lfSquared, 1.0f);
            lfEstimate = std::fma(lfHalf, lfResidual, lfEstimate);
        }
        return lfEstimate;
    }

    inline Vec4 MakeVec4(f32 afX, f32 afY, f32 afZ, f32 afW)
    {
        const Vec4 lvOut = { afX, afY, afZ, afW };
        return lvOut;
    }

    // Lane i (0 = x, 1 = y, 2 = z, 3 = w): the kernels index their vectors by axis, as the console's
    // lfsx / stfsx on the stack copies do.
    inline f32& Lane(Vec4& arV, u32 luLane)             { return (&arV.x)[luLane]; }
    inline f32  Lane(const Vec4& arV, u32 luLane)       { return (&arV.x)[luLane]; }

    // vsubfp: per-lane a - b.
    inline Vec4 Sub(const Vec4& arA, const Vec4& arB)
    {
        return MakeVec4(arA.x - arB.x, arA.y - arB.y, arA.z - arB.z, arA.w - arB.w);
    }

    // vmulfp against a splat: per-lane a * s.
    inline Vec4 MulSplat(const Vec4& arA, f32 afS)
    {
        return MakeVec4(arA.x * afS, arA.y * afS, arA.z * afS, arA.w * afS);
    }

    // vmaddfp against a splat: per-lane a*s + b, ONE rounding.
    inline Vec4 MaddSplat(const Vec4& arA, f32 afS, const Vec4& arB)
    {
        return MakeVec4(std::fma(arA.x, afS, arB.x), std::fma(arA.y, afS, arB.y),
                        std::fma(arA.z, afS, arB.z), std::fma(arA.w, afS, arB.w));
    }

    // The volume's frame composed with the caller's transform -- the four scheduled chains that open the box and
    // capsule kernels (BoxVolume 0x82BA9530..0x82BA9590, CapsuleVolume 0x82BAFDB0..0x82BAFE08): rows 0..2 start
    // from a product (vmulfp128 frame.x*T0), row 3 from the transform's own translation (vmaddfp frame.x*T0 + T3);
    // the y and z terms are fused on.
    inline void ComposeFrame(const Vec4* lpFrame, const Vec4* lpTransform, Vec4 (&arOut)[4])
    {
        for (u32 luRow = 0; luRow < 3u; ++luRow)
        {
            arOut[luRow] = MaddSplat(lpTransform[2], lpFrame[luRow].z,
                                     MaddSplat(lpTransform[1], lpFrame[luRow].y,
                                               MulSplat(lpTransform[0], lpFrame[luRow].x)));
        }
        arOut[3] = MaddSplat(lpTransform[2], lpFrame[3].z,
                             MaddSplat(lpTransform[1], lpFrame[3].y,
                                       MaddSplat(lpTransform[0], lpFrame[3].x, lpTransform[3])));
    }

    // Into the frame (BoxVolume 0x82BA95FC..0x82BA9664, CapsuleVolume 0x82BAFE60..0x82BAFEC4): the transposed
    // rotation (vmrghw / vmrglw with a zero vector, so every column's w lane is +0) and the negated translation
    // (vsubfp 0 - row3) pushed through it: tinv = (-t.x)*c0 + ((-t.y)*c1 + (-t.z)*c2), the last term a product.
    struct LocalFrame
    {
        Vec4 mColumn0;   // (row0.x, row1.x, row2.x, 0)
        Vec4 mColumn1;   // (row0.y, row1.y, row2.y, 0)
        Vec4 mColumn2;   // (row0.z, row1.z, row2.z, 0)
        Vec4 mTranslation;
    };

    inline LocalFrame InvertFrame(const Vec4 (&arFrame)[4])
    {
        LocalFrame lFrame;
        lFrame.mColumn0 = MakeVec4(arFrame[0].x, arFrame[1].x, arFrame[2].x, 0.0f);
        lFrame.mColumn1 = MakeVec4(arFrame[0].y, arFrame[1].y, arFrame[2].y, 0.0f);
        lFrame.mColumn2 = MakeVec4(arFrame[0].z, arFrame[1].z, arFrame[2].z, 0.0f);
        const Vec4 lvNegated = Sub(MakeVec4(0.0f, 0.0f, 0.0f, 0.0f), arFrame[3]);
        lFrame.mTranslation = MaddSplat(lFrame.mColumn0, lvNegated.x,
                                        MaddSplat(lFrame.mColumn1, lvNegated.y,
                                                  MulSplat(lFrame.mColumn2, lvNegated.z)));
        return lFrame;
    }

    // A point into the frame: c0*p.x + tinv first, then c1*p.y, then c2*p.z, all fused.
    inline Vec4 ToLocal(const LocalFrame& arFrame, const Vec4& arPoint)
    {
        return MaddSplat(arFrame.mColumn2, arPoint.z,
                         MaddSplat(arFrame.mColumn1, arPoint.y,
                                   MaddSplat(arFrame.mColumn0, arPoint.x, arFrame.mTranslation)));
    }

    // Out of the frame (BoxVolume 0x82BA9EC4..0x82BA9F4C, CapsuleVolume 0x82BB02BC..0x82BB0344, SphereVolume's
    // centre 0x82BA830C..0x82BA831C): a point opens from the translation row (row0*p.x + row3), a direction from a
    // product (row0*n.x); the y and z terms are fused on.
    inline Vec4 FramePoint(const Vec4* lpFrame, const Vec4& arPoint)
    {
        return MaddSplat(lpFrame[2], arPoint.z,
                         MaddSplat(lpFrame[1], arPoint.y, MaddSplat(lpFrame[0], arPoint.x, lpFrame[3])));
    }

    inline Vec4 FrameDirection(const Vec4* lpFrame, const Vec4& arDirection)
    {
        return MaddSplat(lpFrame[2], arDirection.z,
                         MaddSplat(lpFrame[1], arDirection.y, MulSplat(lpFrame[0], arDirection.x)));
    }

    // The walk's axis successor, `slw r11, 1, axis ; clrlwi r10, r11, 30`: 0 -> 1 -> 2 -> 0.
    inline u32 NextAxis(u32 luAxis)
    {
        return (1u << luAxis) & 3u;
    }
}
}
}

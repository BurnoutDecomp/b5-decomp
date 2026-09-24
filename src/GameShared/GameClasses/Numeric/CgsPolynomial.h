#pragma once

// CgsNumeric polynomial helpers. Home per the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameShared/GameClasses/Numeric/CgsPolynomial.h):
//     extern const MaskScalar SolveQuadratic(VecFloat, VecFloat, VecFloat, VecFloat &, VecFloat &); // :47
// with locals lDenominatorValid (:50), lfSqrtTerm (:53), lSqrtTermValid (:56) and
// lfRecipDenominator (:59) (dwarfdump _compile/BrnTrafficUnity.cpp:7041).
//
// No out-of-line body exists in either build: it is a header inline. The ARTIST instance this was
// read from is the one TrafficEntityModule::GeneratePotentialLeapedAndStompedCarsOutput inlines at
// 0x8271F3DC..0x8271F4C8 (IDA prints classic vmaddfp/vnmsubfp operands in raw field order
// D,A,B,C, i.e. D = A*C + B and D = -(A*C - B)):
//   v0 = vcfsx(splat 2)                      2.0 (GetVecFloat_Two)
//   v7 = ~(a == 0)                           lDenominatorValid   (vcmpeqfp v7,v11,v13 ; vnot)
//   v0 = b*b - ((2*2)*a)*c                   lfSqrtTerm          (vmulfp128 x4 ; vsubfp)
//   v6 = (lfSqrtTerm >= 0)                   lSqrtTermValid      (vcmpgefp v6,v0,v13)
//   v9 = recip(2*a)                          lfRecipDenominator  (vrefp + two Newton steps)
//   v10 = (-1)*b                             -b                  (GetVecFloat_NegativeOne * b)
//   Sqrt(t) = t * rsqrt(t), 0 where t == 0   (vrsqrtefp + two Newton steps ; vsel on t == 0)
//   out0 = (-b + Sqrt(t)) * recip ; out1 = (-b - Sqrt(t)) * recip   (computed unconditionally)
//   return lDenominatorValid & lSqrtTermValid                        (vand v7,v7,v6)
// The caller reduces the mask with MaskScalar::GetBool (`vcmpeqfp. ; mfocrf ; extrwi 1,24`).
//
// PC lowering: every lane is computed on its own (the console operands are splats, so all four
// lanes agree), the estimate-and-refine reciprocal / reciprocal-square-root pairs are
// de-optimised to an exact divide and std::sqrt (the same policy as BrnMathUtils.h), and mask
// lanes follow the tree's float-domain MaskScalar convention (1.0f true / 0.0f false,
// rw/math/vpu/vector4_operation.h).

#include "types.hpp"                        // f32
#include "BrnCommonTypes.h"                 // VecFloat (== rw::math::vpu::Vector4)
#include "rw/math/vpu/types.h"              // rw::math::vpu::MaskScalar
#include "rw/math/vpu/vector4_operation.h"  // rw::math::vpu::And

#include <cmath>                            // std::sqrt

namespace CgsNumeric
{
    namespace SolveQuadraticDetail
    {
        // One lane of SolveQuadratic: both roots, and the lane's two validity tests.
        inline void SolveLane(f32 lfA, f32 lfB, f32 lfC, f32& lrfOutX0, f32& lrfOutX1,
                              bool& lrbDenominatorValid, bool& lrbSqrtTermValid)
        {
            lrbDenominatorValid = (lfA != 0.0f);
            const f32 lfSqrtTerm = lfB * lfB - 2.0f * 2.0f * lfA * lfC;
            lrbSqrtTermValid = (lfSqrtTerm >= 0.0f);
            const f32 lfRecipDenominator = 1.0f / (2.0f * lfA);
            // The console's Sqrt is t * rsqrt(t) with the t == 0 lane selected to 0; std::sqrt
            // is exact and already returns 0 there (and NaN for a negative term, as rsqrte does).
            const f32 lfSqrt = std::sqrt(lfSqrtTerm);
            const f32 lfMinusB = -1.0f * lfB;
            lrfOutX0 = (lfMinusB + lfSqrt) * lfRecipDenominator;
            lrfOutX1 = (lfMinusB - lfSqrt) * lfRecipDenominator;
        }
    }

    // CgsPolynomial.h:47 -- the real roots of lfA*x^2 + lfB*x + lfC = 0. The mask is true where
    // the equation is a quadratic (lfA != 0) with a non-negative discriminant; the two roots are
    // written whatever the mask says.
    inline const rw::math::vpu::MaskScalar SolveQuadratic(VecFloat lfA, VecFloat lfB, VecFloat lfC,
                                                          VecFloat& lfOutX0, VecFloat& lfOutX1)
    {
        bool labDenominatorValid[4];
        bool labSqrtTermValid[4];
        SolveQuadraticDetail::SolveLane(lfA.x, lfB.x, lfC.x, lfOutX0.x, lfOutX1.x,
                                        labDenominatorValid[0], labSqrtTermValid[0]);
        SolveQuadraticDetail::SolveLane(lfA.y, lfB.y, lfC.y, lfOutX0.y, lfOutX1.y,
                                        labDenominatorValid[1], labSqrtTermValid[1]);
        SolveQuadraticDetail::SolveLane(lfA.z, lfB.z, lfC.z, lfOutX0.z, lfOutX1.z,
                                        labDenominatorValid[2], labSqrtTermValid[2]);
        SolveQuadraticDetail::SolveLane(lfA.w, lfB.w, lfC.w, lfOutX0.w, lfOutX1.w,
                                        labDenominatorValid[3], labSqrtTermValid[3]);

        const rw::math::vpu::MaskScalar lDenominatorValid = {
            labDenominatorValid[0] ? 1.0f : 0.0f, labDenominatorValid[1] ? 1.0f : 0.0f,
            labDenominatorValid[2] ? 1.0f : 0.0f, labDenominatorValid[3] ? 1.0f : 0.0f };
        const rw::math::vpu::MaskScalar lSqrtTermValid = {
            labSqrtTermValid[0] ? 1.0f : 0.0f, labSqrtTermValid[1] ? 1.0f : 0.0f,
            labSqrtTermValid[2] ? 1.0f : 0.0f, labSqrtTermValid[3] ? 1.0f : 0.0f };
        return rw::math::vpu::And(lDenominatorValid, lSqrtTermValid);
    }
}

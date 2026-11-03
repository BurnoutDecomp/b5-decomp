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
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "rw/math/vpu/types.h"

namespace CgsNumeric
{
    // ARTIST 0x8271F430..0x8271F4B8: full-lane masks, reciprocal and sqrt
    // estimates with two Newton refinements, and unconditional root stores.
    inline const rw::math::vpu::MaskScalar SolveQuadratic(VecFloat lfA, VecFloat lfB, VecFloat lfC,
                                                          VecFloat& lfOutX0, VecFloat& lfOutX1)
    {
        typedef rw::math::vpu::VectorIntrinsic V;
        const V zero = __vspltisw(0);
        const V one = VecFloat(1.0f).GetVector();
        const V two = VecFloat(2.0f).GetVector();
        const V half = VecFloat(0.5f).GetVector();
        const V negOne = VecFloat(-1.0f).GetVector();
        const V a = lfA.GetVector(), b = lfB.GetVector(), c = lfC.GetVector();
        const V equalZero = __vcmpeqfp(a, zero);
        const V validA = __vnor(equalZero, equalZero);
        const V denominator = __vmulfp(two, a);
        V term = __vmulfp(two, two);
        term = __vmulfp(term, a);
        term = __vmulfp(term, c);
        term = __vsubfp(__vmulfp(b, b), term);
        const V validTerm = __vcmpgefp(term, zero);
        V reciprocal = __vrefp(denominator);
        V error = __vnmsubfp(reciprocal, denominator, one);
        reciprocal = __vmaddfp(reciprocal, error, reciprocal);
        error = __vnmsubfp(reciprocal, denominator, one);
        reciprocal = __vmaddfp(reciprocal, error, reciprocal);
        V rsqrt = __vrsqrtefp(term);
        V square = __vmulfp(rsqrt, rsqrt);
        error = __vnmsubfp(term, square, one);
        rsqrt = __vmaddfp(__vmulfp(rsqrt, half), error, rsqrt);
        square = __vmulfp(rsqrt, rsqrt);
        error = __vnmsubfp(term, square, one);
        rsqrt = __vmaddfp(__vmulfp(rsqrt, half), error, rsqrt);
        const V root = __vsel(__vmulfp(term, rsqrt), zero, __vcmpeqfp(zero, term));
        const V minusB = __vmulfp(negOne, b);
        lfOutX0 = VecFloat(__vmulfp(__vaddfp(minusB, root), reciprocal));
        lfOutX1 = VecFloat(__vmulfp(__vsubfp(minusB, root), reciprocal));
        return rw::math::vpu::MaskScalar(__vand(validA, validTerm));
    }
}

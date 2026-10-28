// ===========================================================================
// rw/math/vpu/Matrix44Operation.cpp -- the rw::math::vpu **Matrix44** (full 4x4)
// operation bodies. Declared in rw/math/vpu/matrix44_operation.h.
//
// Reconstructed from X360 ARTIST:
//   rw::math::vpu::Inverse @ 0x825B2628
//
// X360 shape (0x825B2628, straight-line VMX, no branches):
//   r3 = &result (sret), r4 = &source matrix, r5 = &determinant out.
//   The four 16-byte rows are loaded with lvx128 at +0/+16/+32/+48, then each row is
//   fed through the SAME lane-rotate permute (vperm against the constant at
//   unk_82CDA440) one, two and three times -- giving row^1/row^2/row^3, where
//   row^k[i] == row[(i+k) & 3].
//
//   Each cofactor row is then the lane-parallel 3x3 determinant ("triple product")
//   of the OTHER three rows taken in cyclic order, accumulated with vmaddfp/vnmsubfp:
//
//     A[i] = M1^1*(M2^2*M3^3 - M2^3*M3^2)
//          + M1^2*(M2^3*M3^1 - M2^1*M3^3)
//          + M1^3*(M2^1*M3^2 - M2^2*M3^1)          (cofactor row 0, from rows 1,2,3)
//
//   ...and likewise B from rows (2,3,0), C from (3,0,1), D from (0,1,2). The
//   checkerboard cofactor sign is applied as a vxor against a lane mask built with
//   `vslw v,v,v` (0xFFFFFFFF << 31 == 0x80000000, i.e. the float sign bit) punched
//   out per lane by vrlimi128: rows 0/2 get (+,-,+,-) and rows 1/3 get (-,+,-,+),
//   which is exactly (-1)^(row+col).
//
//   The determinant is `vmsum4fp128 v13, v31, v30` -- the 4-lane dot product of the
//   UNPERMUTED row 0 with cofactor row 0, broadcast to all four lanes -- and is
//   stored whole through r5 (stvx128 v13, r0, r5).
//
//   The adjugate is the TRANSPOSE of the cofactor matrix, formed by the classic
//   vmrghw/vmrglw 4x4 lane-merge transpose, and each transposed row is scaled by the
//   reciprocal of the determinant (vrefp estimate + two Newton-Raphson refinements:
//   e' = e + e*(1 - d*e), spelled vnmsubfp/vmaddfp against the vcfsx-materialised
//   1.0f) before the four stvx128 stores at r3 +0/+16/+32/+48.
//
// => This is the GENERAL 4x4 inverse. There is no affine / orthonormal fast path in
//    this function: all 16 source elements feed the cofactors, the w column is never
//    assumed to be (0,0,0,1), and all four result rows are written. (The cheap
//    transpose-based path is the separate InverseOfMatrixWithOrthonormal3x3, in
//    matrix44affine_operation.h.)
//
// PC lowering: lane rotations are indexed directly; vmaddfp/vnmsubfp retain their
// fused order, and the reciprocal retains BOTH unconditional refinement passes.
// In particular, a zero determinant reaches NaNs through 0*infinity in the first
// refinement; replacing this sequence with a divide changes the shipped result.
// FLAG (model): vrefp starts from the correctly rounded reciprocal; vmsum4fp128
// rounds the left-to-right f64 sum once (the project's Xenia DOT_PRODUCT_4 model).
// VMX denormal operands/results are flushed to signed zero. NaN payloads and the
// hardware estimate's final finite-result ULP are not hardware-verified here.
// ===========================================================================

#include "rw/math/vpu/matrix44_operation.h"

#include <cstddef>   // offsetof
#include <cmath>
#include <cstring>
#include <limits>

namespace rw
{
namespace math
{
namespace vpu
{

namespace
{
    float Flush(float lfValue)
    {
        return std::fpclassify(lfValue) == FP_SUBNORMAL
            ? std::copysign(0.0f, lfValue) : lfValue;
    }

    float Multiply(float lfA, float lfB)
    {
        return Flush(Flush(lfA) * Flush(lfB));
    }

    float MultiplyAdd(float lfA, float lfB, float lfC)
    {
        return Flush(std::fmaf(Flush(lfA), Flush(lfB), Flush(lfC)));
    }

    // vnmsubfp negates the rounded difference, including an exact zero, but
    // leaves a NaN un-negated (the established EffectsModule/XboxMath model).
    float NegativeMultiplySubtract(float lfA, float lfB, float lfC)
    {
        const float lfDifference = Flush(std::fmaf(Flush(lfA), Flush(lfB), -Flush(lfC)));
        return lfDifference != lfDifference ? lfDifference : -lfDifference;
    }

    float FlipSign(float lfValue)
    {
        unsigned int luBits;
        std::memcpy(&luBits, &lfValue, sizeof(luBits));
        luBits ^= 0x80000000u;
        std::memcpy(&lfValue, &luBits, sizeof(lfValue));
        return lfValue;
    }

    // One cofactor row: the lane-parallel 3x3 determinant of rows (A, B, C) -- the
    // three rows OTHER than the one being expanded, in cyclic order. For output lane
    // `i` the three participating columns are the cyclic rotations (i+1, i+2, i+3),
    // which is exactly what the repeated vperm produced on the console.
    //
    // `lbOddRow` selects the checkerboard sign: (-1)^(row+col), i.e. lanes 1 and 3
    // are negated for an even row and lanes 0 and 2 for an odd one (the vxor against
    // the vslw/vrlimi128 sign-bit mask).
    Vector4 CofactorRow(const Vector4& lrRowA, const Vector4& lrRowB, const Vector4& lrRowC,
                        bool lbOddRow)
    {
        const float laA[4] = { lrRowA.x, lrRowA.y, lrRowA.z, lrRowA.w };
        const float laB[4] = { lrRowB.x, lrRowB.y, lrRowB.z, lrRowB.w };
        const float laC[4] = { lrRowC.x, lrRowC.y, lrRowC.z, lrRowC.w };
        float laCofactor[4];
        for (int liLane = 0; liLane < 4; ++liLane)
        {
            const int liP = (liLane + 1) & 3;
            const int liQ = (liLane + 2) & 3;
            const int liS = (liLane + 3) & 3;

            // ARTIST 0x825B2688..0x825B2758: each minor starts with vmaddfp
            // against +0, then vnmsubfp; the first triple product is vmulfp128,
            // followed by the other two vmaddfp terms in cyclic-column order.
            const float lfMinor0 = NegativeMultiplySubtract(laB[liS], laC[liQ],
                MultiplyAdd(laB[liQ], laC[liS], 0.0f));
            const float lfMinor1 = NegativeMultiplySubtract(laB[liP], laC[liS],
                MultiplyAdd(laB[liS], laC[liP], 0.0f));
            const float lfMinor2 = NegativeMultiplySubtract(laB[liQ], laC[liP],
                MultiplyAdd(laB[liP], laC[liQ], 0.0f));
            float lfCofactor = Multiply(laA[liP], lfMinor0);
            lfCofactor = MultiplyAdd(laA[liQ], lfMinor1, lfCofactor);
            lfCofactor = MultiplyAdd(laA[liS], lfMinor2, lfCofactor);

            const bool lbNegate = (((liLane + (lbOddRow ? 1 : 0)) & 1) != 0);
            if (lbNegate)
            {
                lfCofactor = FlipSign(lfCofactor);
            }
            laCofactor[liLane] = lfCofactor;
        }

        { const Vector4 lResult = { laCofactor[0], laCofactor[1], laCofactor[2], laCofactor[3] }; return lResult; }
    }
} // namespace

Matrix44 Inverse(const Matrix44& lrMatrix, Vector4& lrDeterminant)
{
    // -- the four cofactor rows (each from the other three source rows, cyclically) --
    const Vector4 lvCofactor0 = CofactorRow(lrMatrix.yAxis, lrMatrix.zAxis, lrMatrix.wAxis, false);
    const Vector4 lvCofactor1 = CofactorRow(lrMatrix.zAxis, lrMatrix.wAxis, lrMatrix.xAxis, true);
    const Vector4 lvCofactor2 = CofactorRow(lrMatrix.wAxis, lrMatrix.xAxis, lrMatrix.yAxis, false);
    const Vector4 lvCofactor3 = CofactorRow(lrMatrix.xAxis, lrMatrix.yAxis, lrMatrix.zAxis, true);

    // -- determinant: vmsum4fp128 of the unpermuted row 0 with cofactor row 0, in every
    //    lane (the console stores the whole broadcast register through r5) -----------
    double ldDeterminant = static_cast<double>(Flush(lrMatrix.xAxis.x)) * lvCofactor0.x;
    ldDeterminant += static_cast<double>(Flush(lrMatrix.xAxis.y)) * lvCofactor0.y;
    ldDeterminant += static_cast<double>(Flush(lrMatrix.xAxis.z)) * lvCofactor0.z;
    ldDeterminant += static_cast<double>(Flush(lrMatrix.xAxis.w)) * lvCofactor0.w;
    const float lfDeterminant = std::isfinite(ldDeterminant)
            && std::fabs(ldDeterminant) > (std::numeric_limits<float>::max)()
        ? std::numeric_limits<float>::quiet_NaN()
        : Flush(static_cast<float>(ldDeterminant));

    lrDeterminant.x = lfDeterminant;
    lrDeterminant.y = lfDeterminant;
    lrDeterminant.z = lfDeterminant;
    lrDeterminant.w = lfDeterminant;

    // -- adjugate = transpose of the cofactor matrix (the vmrghw/vmrglw transpose),
    //    scaled by 1/det (vrefp + 2x Newton-Raphson) ----------------------------------
    const float lfEstimate = Flush(static_cast<float>(1.0 / static_cast<double>(lfDeterminant)));
    const float lfStep1 = MultiplyAdd(lfEstimate,
        NegativeMultiplySubtract(lfEstimate, lfDeterminant, 1.0f), lfEstimate);
    const float lfInvDeterminant = MultiplyAdd(lfStep1,
        NegativeMultiplySubtract(lfStep1, lfDeterminant, 1.0f), lfStep1);

    Matrix44 lInverse;
    lInverse.xAxis = Vector4{ Multiply(lfInvDeterminant, lvCofactor0.x), Multiply(lfInvDeterminant, lvCofactor1.x),
                              Multiply(lfInvDeterminant, lvCofactor2.x), Multiply(lfInvDeterminant, lvCofactor3.x) };
    lInverse.yAxis = Vector4{ Multiply(lfInvDeterminant, lvCofactor0.y), Multiply(lfInvDeterminant, lvCofactor1.y),
                              Multiply(lfInvDeterminant, lvCofactor2.y), Multiply(lfInvDeterminant, lvCofactor3.y) };
    lInverse.zAxis = Vector4{ Multiply(lfInvDeterminant, lvCofactor0.z), Multiply(lfInvDeterminant, lvCofactor1.z),
                              Multiply(lfInvDeterminant, lvCofactor2.z), Multiply(lfInvDeterminant, lvCofactor3.z) };
    lInverse.wAxis = Vector4{ Multiply(lfInvDeterminant, lvCofactor0.w), Multiply(lfInvDeterminant, lvCofactor1.w),
                              Multiply(lfInvDeterminant, lvCofactor2.w), Multiply(lfInvDeterminant, lvCofactor3.w) };

    return lInverse;
}

// Layout pins (_AssertLayout equivalent -- every member here is public, so the
// asserts need no member-function scope). The console lvx128/stvx128 offsets
// (+0/+16/+32/+48) are only correct here because Matrix44 is four 16-byte,
// 16-aligned float4 rows on the host too -- no pointer widens inside these
// aggregates, so this is one of the rare console strides that survives verbatim.
CGS_STATIC_ASSERT((sizeof(Vector4) == 16), "rw::math::vpu::Vector4 must be one 16-byte lane register");
CGS_STATIC_ASSERT((sizeof(Matrix44) == 64), "rw::math::vpu::Matrix44 must be four 16-byte rows");
CGS_STATIC_ASSERT((offsetof(Matrix44, xAxis) == 0),  "Matrix44::xAxis at +0  (lvx128 r4+0)");
CGS_STATIC_ASSERT((offsetof(Matrix44, yAxis) == 16), "Matrix44::yAxis at +16 (lvx128 r4+0x10)");
CGS_STATIC_ASSERT((offsetof(Matrix44, zAxis) == 32), "Matrix44::zAxis at +32 (lvx128 r4+0x20)");
CGS_STATIC_ASSERT((offsetof(Matrix44, wAxis) == 48), "Matrix44::wAxis at +48 (lvx128 r4+0x30)");

} // namespace vpu
} // namespace math
} // namespace rw

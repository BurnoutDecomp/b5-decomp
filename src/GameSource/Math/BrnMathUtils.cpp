#include "GameSource/Math/BrnMathUtils.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"           // rw::math::vpu::IsValid / Magnitude / MagnitudeSquared

#include <cmath>   // std::sqrt, std::fabs, std::pow

// See BrnMathUtils.h for the recovered native functions and remaining helper scope.
// 2D == the X/Z ground plane (vector lanes 0 and 2 = .x and .z) throughout.

namespace BrnMath
{
    namespace
    {
        // flt_82002138 (x360rd 0x3C23D70A = 0.01f): both IsNormal overloads' tolerance.
        const f32 KF_UNIT_LENGTH_EPSILON = 0.01f;

        // The IsNormal overloads' magnitude: mag^2 * rsqrt(mag^2) (vrsqrtefp + two Newton steps,
        // then vmulfp128), forced to 0 by the vcmpeqfp/vsel zero mask. An infinite mag^2 gives
        // inf * 0 = NaN, as the console's estimate does.
        inline f32 UnitLengthMagnitude(f32 lfLengthSquared)
        {
            return ( lfLengthSquared == 0.0f ) ? 0.0f : lfLengthSquared * ( 1.0f / std::sqrt( lfLengthSquared ) );
        }
    }

    // ---------------------------------------------------------------------------------------
    // Flatten (X360 @ 0x822CB8E8) -> Vector2
    //
    // ASM: vspltw/vcmpeqfp. self-equality cascade over lanes 0,1,2 (the inlined IsValid NaN
    // guard), then on the fall-through `lvx128 v0,&unk_82CDA450; vperm v1,v1,v1,v0` packs the
    // input vector through a static permute mask into the returned Vector2. The Hex-Rays
    // `result` (= EndAssert() return) is artifact noise; the real return is the permuted
    // Vector2 delivered via the hidden sret pointer.
    //
    // FLAG (VMX->portable): the assert cascade is reversed to a single CGS_ASSERT(IsValid).
    // FLAG (Flatten lane mask): the exact vperm mask at unk_82CDA450 is an un-valued .rdata
    //   blob. XZ (drop Y) is INFERRED from the two sibling 2D functions, both of which use
    //   lanes 0 and 2. If the mask ever resolves to XY, swap lVector.z -> lVector.y here.
    Vector2 Flatten(Vector3 lVector)
    {
        CGS_ASSERT(rw::math::vpu::IsValid(lVector), "RwMath::IsValid( lVector )");
        { const Vector2 lResult = { lVector.mV.x, lVector.mV.z }; return lResult; }
    }

    // ---------------------------------------------------------------------------------------
    // IsNormal (X360 @ 0x822B1CF8) -> bool   [EXECUTED in goal trace]
    //
    // ASM: `vmsum3fp128 v0,v1,v1` = dot(xyz,xyz) = magnitude^2. Then vrsqrtefp + two
    // Newton-Raphson refinement steps build 1/sqrt(mag^2); `vmulfp128 v0,v0,v13` = mag^2 *
    // (1/mag) = magnitude, and `vsel v0,v0,v7,v12` (0x822B1DAC) forces it to 0 where
    // `vcmpeqfp` found mag^2 == 0. `vsubfp v0,v0,v6` subtracts v6 (flt_82001C98 = 1.0),
    // `vandc v0,v0,v11` clears the sign bit (fabs), and `vcmpgtfp v0,v0,v8` (0x822B1DB8) tests
    // |magnitude - 1.0| > v8 (flt_82002138, the tolerance). The final vperm/cntlzw/extrwi
    // (0x822B1DBC..DCC) returns TRUE when the compare is NOT greater -- so a NaN magnitude reads
    // as normal, exactly like an in-tolerance one. NO IsValid assert in this function.
    // `return COERCE_INT(1.0) == 0` in the pseudocode is pure Hex-Rays noise.
    //
    // The tolerance is READ from the image (2026-09-24, crash parity G07-D1 leftover):
    // x360rd 0x82002138 = 0x3C23D70A = 0.01f (the old "un-valued .rdata, inferred from
    // CgsTriangle4.cpp" FLAG is retired -- the inference was right).
    // ⚠️ Crash parity 2026-09-24: the PC body used to return `fabs(m - 1) <= eps`, which is FALSE
    // for a NaN (or infinite) vector where the console returns TRUE; it now spells the console's
    // `!(fabs(m - 1) > eps)` and its zero-guarded mag^2 * rsqrt(mag^2) (inf * 0 = NaN, as on the
    // console). Callers that branch on it (BrnAICar.cpp / BrnAICar_Update.cpp) see the console's
    // answer.
    // Retain the native reciprocal-sqrt estimate, two refinements and mask reductions.
    bool IsNormal(Vector3 lVector)
    {
        const __vector4 lvZero = __vspltisw(0);
        const __vector4 lvOne = __vcfsx(__vspltisw(1), 0);
        const __vector4 lvHalf = __vcfsx(__vspltisw(1), 1);
        const __vector4 lvSign = __vslw(__vspltisw(-1), __vspltisw(-1));
        const __vector4 lvTolerance = { KF_UNIT_LENGTH_EPSILON, KF_UNIT_LENGTH_EPSILON,
                                         KF_UNIT_LENGTH_EPSILON, KF_UNIT_LENGTH_EPSILON };
        const __vector4 lvSquared = __vmsum3fp(lVector.mV, lVector.mV);
        __vector4 lvReciprocal = __vrsqrtefp(lvSquared);
        for (s32 liStep = 0; liStep < 2; ++liStep)
        {
            const __vector4 lvSquare = __vmulfp(lvReciprocal, lvReciprocal);
            const __vector4 lvHalfEstimate = __vmulfp(lvReciprocal, lvHalf);
            const __vector4 lvResidual = __vnmsubfp(lvSquared, lvSquare, lvOne);
            lvReciprocal = __vmaddfp(lvHalfEstimate, lvResidual, lvReciprocal);
        }
        const __vector4 lvMagnitude = __vsel(__vmulfp(lvSquared, lvReciprocal), lvZero,
                                             __vcmpeqfp(lvZero, lvSquared));
        const __vector4 lvError = __vandc(__vsubfp(lvMagnitude, lvOne), lvSign);
        return __vcmpgtfp(lvError, lvTolerance).u[0] == 0;
    }

    // ---------------------------------------------------------------------------------------
    // IsNormal (Vector2 overload; X360 sub_8276AC48, DWARF BrnMathUtils.h `bool IsNormal(Vector2)`)
    //
    // Its only caller is ResetOnTrackManager::TestCarHNG (0x82790C40, the :2216 assert
    // "BrnMath::IsNormal( lDirection )"). The same pipeline as the Vector3 overload over lanes
    // 0 and 1 of the register:
    //   0x8276AC4C  vmulfp128 v11 = v*v ; 0x8276ACA0/B8 vspltw lanes 0 and 1 ; 0x8276ACC4 vaddfp
    //               -> x*x + y*y
    //   0x8276ACD4..ACFC  vrsqrtefp + 2 Newton steps, * mag^2 ; 0x8276AD00 vsel -> 0 when mag^2 == 0
    //   0x8276AD0C  - flt_82001C98 (1.0) ; 0x8276AD10 vandc (fabs) ; 0x8276AD14 vcmpgtfp > flt_82002138 (0.01)
    //   0x8276AD18..AD28  vperm / cntlzw / extrwi -> TRUE unless |m - 1| > 0.01 (NaN reads normal)
    bool IsNormal(Vector2 lVector)
    {
        const f32 lfMagnitude = UnitLengthMagnitude(lVector.x * lVector.x + lVector.y * lVector.y);
        return !(std::fabs(lfMagnitude - 1.0f) > KF_UNIT_LENGTH_EPSILON);
    }

    // ---------------------------------------------------------------------------------------
    // Magnitude2D (X360 @ 0x822B1DD8) -> f32
    //
    // ASM: IsValid cascade (assert), then `vspltw v13,v1,2` (lane 2 = Z) and
    // `vspltw v12,v1,0` (lane 0 = X); `vmulfp128 v0,v13,v13` = Z*Z and
    // `vmaddfp v0,v12,v0,v12` -- the Hex-Rays operand order here is garbled, but trusting the
    // function name + the squared sibling, the dot computed is X*X + Z*Z. The vrsqrtefp +
    // two Newton steps then take its sqrt. Net result: sqrt(X^2 + Z^2).
    //
    // FLAG (VMX->portable): rsqrt+NR -> exact std::sqrt.
    // FLAG (2D = XZ): lanes 0 and 2; consistent with the spltw lane selectors in the asm.
    f32 Magnitude2D(Vector3 lVector)
    {
        CGS_ASSERT(rw::math::vpu::IsValid(lVector), "RwMath::IsValid( lVector )");
        return std::sqrt(lVector.mV.x * lVector.mV.x + lVector.mV.z * lVector.mV.z);
    }

    // ---------------------------------------------------------------------------------------
    // MagnitudeSquared2D (X360 @ 0x8276AD30) -> f32
    //
    // ASM: IsValid cascade (assert), then `vspltw v0,v1,2` (Z) and `vspltw v13,v1,0` (X);
    // `vmulfp128 v0,v0,v0` = Z*Z; `vmaddfp v0,v13,v0,v13` ... = X*X + Z*Z. No sqrt.
    //
    // FLAG (2D = XZ): lanes 0 and 2; explicit in the vspltw lane selectors.
    f32 MagnitudeSquared2D(Vector3 lVector)
    {
        CGS_ASSERT(rw::math::vpu::IsValid(lVector), "RwMath::IsValid( lVector )");
        return lVector.mV.x * lVector.mV.x + lVector.mV.z * lVector.mV.z;
    }

    // ---------------------------------------------------------------------------------------
    // RoundWithNumSignificantFigures (X360 @ 0x82361600) -> s32
    //
    // The HARDEST function. The int reg-args a1..a8 in the pseudocode are register-spill
    // NOISE; the two real parameters are the floats (a9 = value, a10 = numFigures).
    //
    // ASM walk-through:
    //   f30 = value;  f31 = fabs(value).
    //   bl sub_82C09970  with f1 = dbl_8202D7F8 (10.0) and f2 = numFigures (a10).
    //     -> sub_82C09970 @ 0x82C09970 is a full libm pow(x,y): it special-cases inf/nan,
    //        calls _decomp / _d_inttype / log / _get_exp / _set_exp / _powhlp, and does the
    //        polynomial log/exp reconstruction. The call here is pow(10.0, numFigures).
    //   f13 = frsp(result) = (float)pow(10, numFigures)  -> the threshold.
    //   f0  = flt_82001C98 (1.0)  -> the running scale, init 1.0.
    //   scale-down loop: while (fabs >= threshold) { fabs *= flt_82004014 (0.1);
    //                                                scale *= flt_8202AC38 (10.0); }
    //   round-to-nearest: the dbl_82001CB8/CB0/CA8/CA0 constants + fsel sequence are the
    //     classic "add then subtract a large power-of-two" round-half trick on the scaled
    //     mantissa; net effect = round(fabs) to the nearest integer-valued mantissa.
    //   sign: if (value != 0) f12 = fsel(value, +1.0 [flt_82001C98], -1.0 [flt_820037C8]).
    //   result = (s32)( sign * roundedMantissa * scale )   via fmuls; fctiwz; stfiwx.
    //
    // FLAG (RoundWith helper): sub_82C09970 @ 0x82C09970 is reconstructed as std::pow. Its
    //   disassembly is an unmistakable libm pow() (inf/nan handling, _decomp/log/_powhlp,
    //   exponent range checks vs 0x400/-0x3FD) -- so std::pow(10.0, numFigures) is the
    //   faithful semantic, NOT an extern stub. Computed in f64 then narrowed to f32 (frsp).
    // FLAG (RoundWith algorithm): the round-to-nearest fsel magic-constant block is
    //   reconstructed as std::floor(x + 0.5) (round-half-up of a non-negative magnitude),
    //   which is what the magic-number trick computes; the literal constants
    //   (dbl_82001CB8 etc.) are un-valued rodata and not transcribed bit-for-bit.
    // FLAG (constants): 10.0 / 0.1 / 1.0 / -1.0 are INFERRED from the loop structure and the
    //   pow(10,n) threshold; the exact rodata floats are not in the exports but these are the
    //   only values consistent with a "round to N significant figures" algorithm.
    s32 RoundWithNumSignificantFigures(f32 lfValue, f32 lfNumFigures)
    {
        const f32 lfSignedValue = lfValue;
        f32 lfMagnitude = std::fabs(lfValue);

        // Threshold = 10^numFigures (helper sub_82C09970 = pow; computed in double, frsp'd).
        const f32 lfThreshold = static_cast<f32>(std::pow(10.0, static_cast<f64>(lfNumFigures)));

        // Scale the magnitude down into [.., threshold), tracking the inverse scale factor.
        f32 lfScale = 1.0f;
        while (lfMagnitude >= lfThreshold)
        {
            lfMagnitude *= 0.1f;
            lfScale     *= 10.0f;
        }

        // Round the scaled magnitude to nearest (the fsel magic-constant round-half block).
        const f32 lfRounded = std::floor(lfMagnitude + 0.5f);

        // Re-apply the original scale and the sign of the input (fsel on f30; only when != 0).
        f32 lfSign = 0.0f;
        if (lfSignedValue != 0.0f)
            lfSign = (lfSignedValue >= 0.0f) ? 1.0f : -1.0f;

        return static_cast<s32>(lfSign * lfRounded * lfScale);
    }

    // ---------------------------------------------------------------------------------------
    // IsPointInsideBox (X360 @ 0x82540AB8)  -- DWARF Math/BrnMathUtils.cpp
    //
    // [gateui] 2026-08-20. THE ORIENTED-BOX NARROWPHASE. StuntManager::OnPropHit
    // @0x8236EE18 runs it after its bounding-sphere pre-cull to decide whether a broken
    // prop really sits inside a SMASH / BILLBOARD generic region, and
    // ChallengeManager::IsPointInTriggerRegion @0x82333368 runs the identical pair.
    // It was declared here and defined NOWHERE (measured UNDEF in BrnStuntManager.obj
    // and BrnChallengeManager_wC_04.obj).
    //
    // ARG SHAPE FROM THE ASM (the header's old "inferred" FLAG is now retired):
    //   r3 = &lBoxTransform -- rows are read at r3+0 / +16 / +32 (`lvx128 v13,r0,r30`,
    //        `... r0,r28` with r28 = r3+16, `... r0,r27` with r27 = r3+32) and the
    //        translation row at r3+48 (`lvx128 v13, r30, r11` with r11 = 48);
    //   v1  = lPoint (saved to v126 in the prologue, `vmr128 v126, v1`);
    //   v2  = lBoxDimensions (spilled to the param save area at sp+256 and re-read from
    //        there as the pseudocode's `a19`).
    // Two SIMD args in v1/v2 and one GPR -- the committed
    // (const Matrix44Affine&, Vector3, Vector3) declaration is exactly right.
    //
    // BODY (asm, 0x82540C60..end):
    //   lOffset = lPoint - lBoxTransform.Pos()                    (vsubfp128 v13, v126, row3)
    //   per axis A in {row0, row1, row2}:
    //     d = vmsum3fp128(lOffset, A)                             -- dot, splatted to 4 lanes
    //     if (!(d >= 0)) d ^= 0x80000000                          -- vcmpgefp. + vxor == fabs
    //     if (!(extent > d)) return false                         -- vcmpgtfp. extent, |d|
    //   return true
    // ⚠️ POLARITY: the surviving compare is `vcmpgtfp. v0, extent, |dot|` and the CR6
    // "all lanes true" bit is what continues, so a point exactly ON a face is OUTSIDE
    // (strict >). Reproduced as `>= extent -> false` below, not `> extent`.
    //
    // The seven asserts are the console's own, in its own order, with its own message
    // strings and BrnMathUtils.cpp line numbers (85..91). The X360 evaluates the
    // orthogonality test with an inlined vmsum3fp/vperm block against
    // `rw::math::vpu::detail::gIVector` and a rodata epsilon (unk_8207BFC4); that is
    // reproduced by the file-local helper below.
    //
    // FLAG (epsilon): `unk_8207BFC4` is an un-valued .rdata float, so the tolerance
    //   below is INFERRED. The test STRUCTURE (M * transpose(M) compared against the
    //   identity, squared error against a tolerance) is transcribed. Every caller feeds
    //   a matrix built by BoxRegion::ComputeTransform from sin/cos, which is orthonormal
    //   to ~1e-7, so the value is comfortably clear of a false assert either way.
    // FLAG (VMX->portable): the lane algebra is lowered to scalar f32 exactly as every
    //   other function in this TU does; std::fabs replaces the vcmpgefp/vxor sign flip.
    namespace
    {
        // The inlined `RwMath::IsOrthogonal3x3(m)` of 0x82540AE0..0x82540C40: form the
        // three rows of M * transpose(M), subtract the identity rows, and reject when the
        // total squared error exceeds the tolerance.
        bool IsOrthogonal3x3(const Matrix44Affine& lrMatrix)
        {
            // FLAG: inferred tolerance -- see the FLAG block above.
            const f32 KF_ORTHOGONAL_EPSILON = 1.0e-3f;

            const f32 lfXX = rw::math::vpu::Dot(lrMatrix.xAxis, lrMatrix.xAxis) - 1.0f;
            const f32 lfYY = rw::math::vpu::Dot(lrMatrix.yAxis, lrMatrix.yAxis) - 1.0f;
            const f32 lfZZ = rw::math::vpu::Dot(lrMatrix.zAxis, lrMatrix.zAxis) - 1.0f;
            const f32 lfXY = rw::math::vpu::Dot(lrMatrix.xAxis, lrMatrix.yAxis);
            const f32 lfXZ = rw::math::vpu::Dot(lrMatrix.xAxis, lrMatrix.zAxis);
            const f32 lfYZ = rw::math::vpu::Dot(lrMatrix.yAxis, lrMatrix.zAxis);

            const f32 lfError = lfXX * lfXX + lfYY * lfYY + lfZZ * lfZZ
                              + 2.0f * (lfXY * lfXY + lfXZ * lfXZ + lfYZ * lfYZ);

            return lfError <= KF_ORTHOGONAL_EPSILON;
        }
    }

    bool IsPointInsideBox(const Matrix44Affine& lBoxTransform, Vector3 lPoint, Vector3 lHalfExtents)
    {
        CGS_ASSERT(IsOrthogonal3x3(lBoxTransform), "RwMath::IsOrthogonal3x3( lBoxTransform )"); // :85
        CGS_ASSERT(IsNormal(lBoxTransform.Right()), "IsNormal( lBoxTransform.XAxis() )");       // :86
        CGS_ASSERT(IsNormal(lBoxTransform.Up()),    "IsNormal( lBoxTransform.YAxis() )");       // :87
        CGS_ASSERT(IsNormal(lBoxTransform.At()),    "IsNormal( lBoxTransform.ZAxis() )");       // :88
        CGS_ASSERT(lHalfExtents.mV.x >= 0.0f, "lBoxDimensions.X() >= 0.0f");                       // :89
        CGS_ASSERT(lHalfExtents.mV.y >= 0.0f, "lBoxDimensions.Y() >= 0.0f");                       // :90
        CGS_ASSERT(lHalfExtents.mV.z >= 0.0f, "lBoxDimensions.Z() >= 0.0f");                       // :91

        // vsubfp128 v13, v126, row3 -- the point in the box's (un-rotated) frame origin.
        const Vector3 lOffset = lPoint - lBoxTransform.Pos();

        // Three independent slab tests along the box's own axes. Each early-outs, exactly
        // as the console's two `goto LABEL_23` do.
        if (std::fabs(rw::math::vpu::Dot(lOffset, lBoxTransform.Right())) >= lHalfExtents.mV.x)
        {
            return false;
        }
        if (std::fabs(rw::math::vpu::Dot(lOffset, lBoxTransform.Up())) >= lHalfExtents.mV.y)
        {
            return false;
        }
        if (std::fabs(rw::math::vpu::Dot(lOffset, lBoxTransform.At())) >= lHalfExtents.mV.z)
        {
            return false;
        }

        return true;
    }

    // ---------------------------------------------------------------------------------------
    // BuildTransform (X360 @ 0x825405A0)  -- DWARF Math/BrnMathUtils.cpp
    //
    // ⭐ THE SPAWN-POSE BUILDER. RaceCarEntityModule::HandleResetPlayerCarAction feeds it the
    // junkyard SpawnLocation's mPosition / mDirection and the world Y up axis (the caller loads
    // unk_82181510 into v3 -- the same rodata row RaceCar::Prepare's identity uses for yAxis,
    // i.e. (0,1,0,0)); the matrix that comes out IS where the player's car ends up.
    //
    // ASM (0x82540828..0x82540938): the console stores lUp VERBATIM at out+0x10 and lPosition
    // VERBATIM at out+0x30, and computes the two remaining axes with the usual pair of
    // vpermwi(0x63 == yzx) cross products, each normalised by vrsqrtefp + two Newton-Raphson
    // refinement steps. out+0x00 is written first, out+0x20 second. It closes with the two
    // matrix sanity asserts (:64 IsOrthogonal3x3, :65 IsNormal3x3).
    //
    // Native VMX operations retain the multiply/FMA association, negative-zero lanes,
    // two reciprocal-square-root refinements and unguarded degenerate normalizations.
    // The eight assertions retain the original message, file and source-line contract.
    Matrix44Affine BuildTransform(Vector3 lPosition, Vector3 lAt, Vector3 lUp)
    {
        Matrix44Affine lTransform;
        const char* const lpcFile = "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Math/BrnMathUtils.cpp";
        const __vector4 lvPositionEqual = __vcmpeqfp(lPosition.mV, lPosition.mV);
        if (!(lvPositionEqual.u[0] && lvPositionEqual.u[1] && lvPositionEqual.u[2]))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("RwMath::IsValid( lPosition )", lpcFile, 50);
            CgsDev::Assert::EndAssert();
        }
        const __vector4 lvAtEqual = __vcmpeqfp(lAt.mV, lAt.mV);
        if (!(lvAtEqual.u[0] && lvAtEqual.u[1] && lvAtEqual.u[2]))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("RwMath::IsValid( lAt )", lpcFile, 51);
            CgsDev::Assert::EndAssert();
        }
        const __vector4 lvUpEqual = __vcmpeqfp(lUp.mV, lUp.mV);
        if (!(lvUpEqual.u[0] && lvUpEqual.u[1] && lvUpEqual.u[2]))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("RwMath::IsValid( lUp )", lpcFile, 52);
            CgsDev::Assert::EndAssert();
        }
        if (!IsNormal(lAt))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("IsNormal( lAt )", lpcFile, 53);
            CgsDev::Assert::EndAssert();
        }
        if (!IsNormal(lUp))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("IsNormal( lUp )", lpcFile, 54);
            CgsDev::Assert::EndAssert();
        }
        const __vector4 lvSign = __vslw(__vspltisw(-1), __vspltisw(-1));
        const __vector4 lvSimilarTolerance = { 0.0000152587890625f, 0.0000152587890625f,
                                                0.0000152587890625f, 0.0000152587890625f };
        const __vector4 lvDifference = __vandc(__vsubfp(lAt.mV, lUp.mV), lvSign);
        const __vector4 lvLarger = __vcmpgtfp(lvDifference, lvSimilarTolerance);
        if (!(lvLarger.u[0] || lvLarger.u[1] || lvLarger.u[2]))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("!RwMath::IsSimilar( lAt, lUp )", lpcFile, 55);
            CgsDev::Assert::EndAssert();
        }

        const __vector4 lvOne = __vcfsx(__vspltisw(1), 0);
        const __vector4 lvHalf = __vcfsx(__vspltisw(1), 1);
        const __vector4 lvAtYzx = __vpermwi(lAt.mV, 0x63);
        const __vector4 lvUpYzx = __vpermwi(lUp.mV, 0x63);
        const __vector4 lvFirstProduct = __vmulfp(lUp.mV, lvAtYzx);
        __vector4 lvRight = __vpermwi(__vnmsubfp(lvUpYzx, lAt.mV, lvFirstProduct), 0x63);
        lTransform.yAxis.mV = lUp.mV;
        lTransform.wAxis.mV = lPosition.mV;
        __vector4 lvSquared = __vmsum3fp(lvRight, lvRight);
        __vector4 lvReciprocal = __vrsqrtefp(lvSquared);
        for (s32 liStep = 0; liStep < 2; ++liStep)
        {
            const __vector4 lvSquare = __vmulfp(lvReciprocal, lvReciprocal);
            const __vector4 lvHalfEstimate = __vmulfp(lvReciprocal, lvHalf);
            const __vector4 lvResidual = __vnmsubfp(lvSquared, lvSquare, lvOne);
            lvReciprocal = __vmaddfp(lvHalfEstimate, lvResidual, lvReciprocal);
        }
        lvRight = __vmulfp(lvRight, lvReciprocal);
        lTransform.xAxis.mV = lvRight;

        const __vector4 lvRightYzx = __vpermwi(lvRight, 0x63);
        const __vector4 lvSecondProduct = __vmulfp(lvRight, lvUpYzx);
        __vector4 lvForward = __vpermwi(__vnmsubfp(lvRightYzx, lUp.mV, lvSecondProduct), 0x63);
        lvSquared = __vmsum3fp(lvForward, lvForward);
        lvReciprocal = __vrsqrtefp(lvSquared);
        for (s32 liStep = 0; liStep < 2; ++liStep)
        {
            const __vector4 lvSquare = __vmulfp(lvReciprocal, lvReciprocal);
            const __vector4 lvHalfEstimate = __vmulfp(lvReciprocal, lvHalf);
            const __vector4 lvResidual = __vnmsubfp(lvSquared, lvSquare, lvOne);
            lvReciprocal = __vmaddfp(lvHalfEstimate, lvResidual, lvReciprocal);
        }
        lvForward = __vmulfp(lvForward, lvReciprocal);
        lTransform.zAxis.mV = lvForward;

        // Original transpose/row-combine/packed-error sequence, including its
        // second squared reduction. Constants are read from 82CDA350/8207BFC4.
        static const CGS_ALIGN(16) u32 lauGather[4] = { 0x00010203, 0x14151617, 0x00010203, 0x00010203 };
        const __vector4 lvGather = __lvx(lauGather, 0);
        const __vector4 lvTolerance = { 0.00000011920928955078125f, 0.00000011920928955078125f,
                                         0.00000011920928955078125f, 0.00000011920928955078125f };
        const __vector4 lvXAxis = { 1.0f, 0.0f, 0.0f, 0.0f };
        const __vector4 lvYAxis = { 0.0f, 1.0f, 0.0f, 0.0f };
        const __vector4 lvZAxis = { 0.0f, 0.0f, 1.0f, 0.0f };
        const __vector4 lvRightForwardHi = __vmrghw(lvRight, lvForward);
        const __vector4 lvRightForwardLo = __vmrglw(lvRight, lvForward);
        const __vector4 lvUpHi = __vmrghw(lUp.mV, lUp.mV);
        const __vector4 lvUpLo = __vmrglw(lUp.mV, lUp.mV);
        const __vector4 lvColumnX = __vmrghw(lvRightForwardHi, lvUpHi);
        const __vector4 lvColumnY = __vmrglw(lvRightForwardHi, lvUpHi);
        const __vector4 lvColumnZ = __vmrghw(lvRightForwardLo, lvUpLo);
        __vector4 lvRightError = __vmulfp(lvColumnX, __vspltw(lvRight, 0));
        lvRightError = __vmaddfp(lvColumnY, __vspltw(lvRight, 1), lvRightError);
        lvRightError = __vmaddfp(lvColumnZ, __vspltw(lvRight, 2), lvRightError);
        lvRightError = __vsubfp(lvRightError, lvXAxis);
        __vector4 lvUpError = __vmulfp(lvColumnX, __vspltw(lUp.mV, 0));
        lvUpError = __vmaddfp(lvColumnY, __vspltw(lUp.mV, 1), lvUpError);
        lvUpError = __vmaddfp(lvColumnZ, __vspltw(lUp.mV, 2), lvUpError);
        lvUpError = __vsubfp(lvUpError, lvYAxis);
        __vector4 lvForwardError = __vmulfp(lvColumnX, __vspltw(lvForward, 0));
        lvForwardError = __vmaddfp(lvColumnY, __vspltw(lvForward, 1), lvForwardError);
        lvForwardError = __vmaddfp(lvColumnZ, __vspltw(lvForward, 2), lvForwardError);
        lvForwardError = __vsubfp(lvForwardError, lvZAxis);
        const __vector4 lvErrorX = __vmsum3fp(lvRightError, lvRightError);
        const __vector4 lvErrorY = __vmsum3fp(lvUpError, lvUpError);
        const __vector4 lvErrorZ = __vmsum3fp(lvForwardError, lvForwardError);
        __vector4 lvPackedError = __vperm(lvErrorX, lvErrorY, lvGather);
        lvPackedError = __vrlimi(lvPackedError, lvErrorZ, 2, 0);
        const __vector4 lvTotalError = __vandc(__vmsum3fp(lvPackedError, lvPackedError), lvSign);
        if (__vcmpgtfp(lvTotalError, lvTolerance).u[0])
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("RwMath::IsOrthogonal3x3( lTransform )", lpcFile, 64);
            CgsDev::Assert::EndAssert();
        }

        const __vector4 lvLengthX = __vmsum3fp(lTransform.xAxis.mV, lTransform.xAxis.mV);
        const __vector4 lvLengthY = __vmsum3fp(lTransform.yAxis.mV, lTransform.yAxis.mV);
        const __vector4 lvLengthZ = __vmsum3fp(lTransform.zAxis.mV, lTransform.zAxis.mV);
        const __vector4 lvLengthErrorX = __vsubfp(lvLengthX, lvOne);
        const __vector4 lvLengthErrorY = __vsubfp(lvLengthY, lvOne);
        const __vector4 lvLengthErrorZ = __vsubfp(lvLengthZ, lvOne);
        __vector4 lvPackedLength = __vperm(lvLengthErrorX, lvLengthErrorY, lvGather);
        lvPackedLength = __vrlimi(lvPackedLength, lvLengthErrorZ, 2, 0);
        const __vector4 lvTotalLength = __vandc(__vmsum3fp(lvPackedLength, lvPackedLength), lvSign);
        if (__vcmpgtfp(lvTotalLength, lvTolerance).u[0])
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("RwMath::IsNormal3x3( lTransform )", lpcFile, 65);
            CgsDev::Assert::EndAssert();
        }
        return lTransform;
    }

}

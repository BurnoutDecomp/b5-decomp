#pragma once

// Canonical RenderWare SDK home for the rw::math::vpu Matrix44Affine operation
// vocabulary (EARenderWare rwmath 1.02.00, rw/math/vpu/matrix44affine_operation.h --
// sibling to types.h and vector3_operation.h). The free-function names and the
// member spellings (xAxis/yAxis/zAxis/wAxis, Pos()) are locked from the public
// rwmath headers and the DecFIGS DWARF call lists (BrnPositionLag.cpp attests live
// instances of InverseOfMatrixWithOrthonormal3x3 / TransformPoint / TransformVector
// adjacent to the Matrix44Affine type).
//
// The console SDK implements these over AltiVec/VMX SIMD on the 16-byte
// `VectorIntrinsic` rows: the affine point/vector transforms are a lane-broadcast
// (vspltw128 of each source component) FMA cascade (vmaddfp) over the four 16-byte
// matrix rows; InverseOfMatrixWithOrthonormal3x3 builds the rotation transpose with
// vmrglw/vmrghw lane-merge permutes and computes the inverse translation as the
// transpose applied to the negated position (a vsubfp against zero then the same
// FMA cascade). The PC reconstruction operates on the named float lanes of the flat
// Matrix44Affine / Vector3 aggregates in types.h; per-lane f32 math gives exact
// semantic parity with the vspltw128 + vmaddfp accumulation.
//
// gen-tool note: tools/renderware/generate_headers.py only writes rwcore_*.h under
// include/rw/; it never touches rw/math/vpu/, so this hand-maintained header (like
// its siblings types.h / vector3_operation.h) is immune to regeneration.

#include <cmath>                           // std::acos / std::sin (SLerp's arc)
#include "rw/math/vpu/types.h"             // rw::math::vpu::Matrix44Affine / Vector3
#include "rw/math/vpu/vector3_operation.h" // Vector3 operator+/-, Mult, Lerp

namespace rw
{
namespace math
{
namespace vpu
{
    // -- affine transforms --------------------------------------------------------------

    // TransformVector(m, v): rotate the vector by the affine's upper 3x3 (rows
    // xAxis/yAxis/zAxis); the translation row (wAxis) is NOT added -- a direction
    // transform. X360 shape: vspltw128(v.x/y/z) broadcast + vmaddfp cascade over the
    // three rotation rows (xAxis*v.x + yAxis*v.y + zAxis*v.z).
    inline Vector3 TransformVector(const Matrix44Affine& lrMatrix, Vector3 lvVector)
    {
        const f32 lfX = lvVector.x;
        const f32 lfY = lvVector.y;
        const f32 lfZ = lvVector.z;

        Vector3 lvResult;
        lvResult.x = lrMatrix.xAxis.x * lfX + lrMatrix.yAxis.x * lfY + lrMatrix.zAxis.x * lfZ;
        lvResult.y = lrMatrix.xAxis.y * lfX + lrMatrix.yAxis.y * lfY + lrMatrix.zAxis.y * lfZ;
        lvResult.z = lrMatrix.xAxis.z * lfX + lrMatrix.yAxis.z * lfY + lrMatrix.zAxis.z * lfZ;
        lvResult.w = 0.0f;
        return lvResult;
    }

    // TransformPoint(m, p): transform the point by the full affine -- rotate by the
    // 3x3 (rows xAxis/yAxis/zAxis) and add the translation row (wAxis). X360 shape:
    // vspltw128(p.x/y/z) broadcast + vmaddfp cascade over all four rows
    // (xAxis*p.x + yAxis*p.y + zAxis*p.z + wAxis).
    inline Vector3 TransformPoint(const Matrix44Affine& lrMatrix, Vector3 lvPoint)
    {
        const f32 lfX = lvPoint.x;
        const f32 lfY = lvPoint.y;
        const f32 lfZ = lvPoint.z;

        Vector3 lvResult;
        lvResult.x = lrMatrix.xAxis.x * lfX + lrMatrix.yAxis.x * lfY + lrMatrix.zAxis.x * lfZ + lrMatrix.wAxis.x;
        lvResult.y = lrMatrix.xAxis.y * lfX + lrMatrix.yAxis.y * lfY + lrMatrix.zAxis.y * lfZ + lrMatrix.wAxis.y;
        lvResult.z = lrMatrix.xAxis.z * lfX + lrMatrix.yAxis.z * lfY + lrMatrix.zAxis.z * lfZ + lrMatrix.wAxis.z;
        lvResult.w = 0.0f;
        return lvResult;
    }

    // -- composition --------------------------------------------------------------------

    // Mult(m, b) / operator*(m, b): the affine matrix product `m * b` (row-major). Each
    // output row i is m.row_i transformed by b: the rotation rows are
    //   out.row_i = m.row_i.x * b.xAxis + m.row_i.y * b.yAxis + m.row_i.z * b.zAxis
    // and the translation row additionally adds b.wAxis (m.wAxis is a point, so its w==1
    // contribution is the +b.wAxis term). X360 shape (matrix44affine_operation_platform_inline.h
    // :84-90): the source rows are vspltw128-broadcast component-by-component (sp0..sp3) and
    // accumulated with a vmaddfp cascade over the loaded b rows (ma0..ma3 / bx/by/bz), the
    // translation row seeded with b.wAxis (the `zero`/w handling). Per-lane f32 math gives exact
    // semantic parity with that broadcast+FMA accumulation.
    inline Matrix44Affine Mult(const Matrix44Affine& lrLhs, const Matrix44Affine& lrRhs)
    {
        Matrix44Affine lResult;
        lResult.xAxis = TransformVector(lrRhs, lrLhs.xAxis);
        lResult.yAxis = TransformVector(lrRhs, lrLhs.yAxis);
        lResult.zAxis = TransformVector(lrRhs, lrLhs.zAxis);
        lResult.wAxis = TransformPoint(lrRhs, lrLhs.wAxis);
        return lResult;
    }
    inline Matrix44Affine operator*(const Matrix44Affine& lrLhs, const Matrix44Affine& lrRhs)
    {
        return Mult(lrLhs, lrRhs);
    }

    // -- elementary rotation builders ----------------------------------------------------

    // MakeRotationX/Y/Z(rads): the affine of a right-handed rotation of `lfAngleRads` about
    // one axis, row-major, with a ZERO translation row (NOT (0,0,0,1) -- the SDK leaves the
    // affine's fourth row at zero, which is what makes Mult(rot, m) leave m's position alone
    // and Mult(m, rot) swing it about the origin).
    //
    // ADDITIVE GROW 2026-08-02 (rotate-helper wave). These are the console SDK's own three
    // inlines -- DecFIGS attributes them to matrix44affine_operation_platform_inline.h
    // :239/:240 (X), :253 (Y) and :269 (Z) -- and the assignment of line to axis is not a
    // guess: it falls straight out of which builders each caller uses.
    //     BehaviourGameplayExternal::CalculateCameraTransform  -> :239/:240 ONLY, and its asm
    //         builds exactly one X rotation (from the authored down-angle).
    //     BehaviourGameplayExternal::ApplyJumpEffects          -> :253 and :269 ONLY, and its
    //         asm builds exactly one Y (yaw drift) and one Z (dutch drift).
    //     Camera::Utils::RotateMatrix44AffineByEulerAnglesZXY  -> ALL THREE, and its asm
    //         builds all three.
    // The row contents below were read off the console's vperm/vrlimi128 packing in those
    // functions, and they independently reproduce the two hand-de-inlined `XMMatrixRotationX`
    // / `XMMatrixRotationY` that BrnBehaviourRotateAboutVehicle.cpp has carried since
    // 2026-07 -- whose comment already said "DELETE-WHEN: an rw/math home for the rotation
    // builders lands". This is that home.
    //
    // FLAG (PC-platform, numeric): the console evaluates sin and cos as one inlined SinCos
    // minimax over a 2pi-reduced argument; std::sin / std::cos are the exact forms. The same
    // de-optimisation this family already applies to rsqrt (Normalize) and reciprocal.
    inline Matrix44Affine MakeRotationX(f32 lfAngleRads)
    {
        const f32 lfSin = std::sin(lfAngleRads);
        const f32 lfCos = std::cos(lfAngleRads);

        Matrix44Affine lResult;
        lResult.xAxis = Vector3{ 1.0f,    0.0f,   0.0f, 0.0f };
        lResult.yAxis = Vector3{ 0.0f,  lfCos, lfSin, 0.0f };
        lResult.zAxis = Vector3{ 0.0f, -lfSin, lfCos, 0.0f };
        lResult.wAxis = Vector3{ 0.0f,    0.0f,   0.0f, 0.0f };
        return lResult;
    }

    inline Matrix44Affine MakeRotationY(f32 lfAngleRads)
    {
        const f32 lfSin = std::sin(lfAngleRads);
        const f32 lfCos = std::cos(lfAngleRads);

        Matrix44Affine lResult;
        lResult.xAxis = Vector3{ lfCos, 0.0f, -lfSin, 0.0f };
        lResult.yAxis = Vector3{  0.0f, 1.0f,    0.0f, 0.0f };
        lResult.zAxis = Vector3{ lfSin, 0.0f,  lfCos, 0.0f };
        lResult.wAxis = Vector3{  0.0f, 0.0f,    0.0f, 0.0f };
        return lResult;
    }

    inline Matrix44Affine MakeRotationZ(f32 lfAngleRads)
    {
        const f32 lfSin = std::sin(lfAngleRads);
        const f32 lfCos = std::cos(lfAngleRads);

        Matrix44Affine lResult;
        lResult.xAxis = Vector3{  lfCos, lfSin, 0.0f, 0.0f };
        lResult.yAxis = Vector3{ -lfSin, lfCos, 0.0f, 0.0f };
        lResult.zAxis = Vector3{   0.0f,  0.0f, 1.0f, 0.0f };
        lResult.wAxis = Vector3{   0.0f,  0.0f, 0.0f, 0.0f };
        return lResult;
    }

    // -- orthonormalisation -------------------------------------------------------------

    // OrthoNormalize3x3(m): re-normalise the three rotation rows (xAxis/yAxis/zAxis) of an
    // affine whose 3x3 is built from a (near-)orthogonal basis, leaving each row a unit
    // vector and the translation row untouched. X360 0x82203B28: per row it computes
    // vmsum3fp (magnitude-squared), a vrsqrtefp estimate refined by two Newton-Raphson
    // steps (vnmsubfp/vmaddfp), then scales the row by that reciprocal magnitude; a degenerate
    // (zero-length) row is selected to zero via vcmpeqfp/vsel. The PC reconstruction
    // de-optimises the rsqrt-estimate-plus-refinement to an exact 1/sqrt (the same convention
    // as Normalize in vector3_operation.h) -- numerically a touch tighter, never a placeholder.
    // The console reads the source rows from one buffer and writes the result to another; here
    // the input is taken by const ref and the orthonormalised matrix returned. Each row is
    // independently normalised (the caller feeds three mutually-orthogonal axes -- a direction,
    // a rotation axis and their cross product -- so per-row normalisation yields an orthonormal
    // 3x3).
    inline Matrix44Affine OrthoNormalize3x3(const Matrix44Affine& lrMatrix)
    {
        Matrix44Affine lResult;
        lResult.xAxis = Normalize(lrMatrix.xAxis);
        lResult.yAxis = Normalize(lrMatrix.yAxis);
        lResult.zAxis = Normalize(lrMatrix.zAxis);
        lResult.wAxis = lrMatrix.wAxis;
        return lResult;
    }

    // -- inverse ------------------------------------------------------------------------

    // InverseOfMatrixWithOrthonormal3x3(m): the affine inverse when the upper 3x3 is
    // orthonormal (a pure rotation). The inverse rotation is the transpose of the 3x3,
    // and the inverse translation is the transpose applied to the negated position:
    //   invR = transpose(R)            (col k of R becomes row k of invR)
    //   invT = invR * (-Pos)  i.e. invT.k = -(Pos . R.row_k)  (lane k = -Pos dot axis k)
    // X360 shape: the transpose is built by vmrglw/vmrghw lane merges; the inverse
    // translation is `vsubfp v,0,Pos` then a vmaddfp cascade of the transposed rows
    // against the negated-position broadcasts. The translation lanes computed there
    // are exactly -(Pos . xAxis), -(Pos . yAxis), -(Pos . zAxis).
    inline Matrix44Affine InverseOfMatrixWithOrthonormal3x3(const Matrix44Affine& lrMatrix)
    {
        Matrix44Affine lInverse;

        // Inverse rotation = transpose of the upper 3x3 (rows become columns).
        lInverse.xAxis.x = lrMatrix.xAxis.x; lInverse.xAxis.y = lrMatrix.yAxis.x; lInverse.xAxis.z = lrMatrix.zAxis.x; lInverse.xAxis.w = 0.0f;
        lInverse.yAxis.x = lrMatrix.xAxis.y; lInverse.yAxis.y = lrMatrix.yAxis.y; lInverse.yAxis.z = lrMatrix.zAxis.y; lInverse.yAxis.w = 0.0f;
        lInverse.zAxis.x = lrMatrix.xAxis.z; lInverse.zAxis.y = lrMatrix.yAxis.z; lInverse.zAxis.z = lrMatrix.zAxis.z; lInverse.zAxis.w = 0.0f;

        // Inverse translation = transpose applied to the negated position, i.e. each
        // lane is the negative dot of the original position with the matching axis row.
        const Vector3& lrPos = lrMatrix.wAxis;
        lInverse.wAxis.x = -(lrPos.x * lrMatrix.xAxis.x + lrPos.y * lrMatrix.xAxis.y + lrPos.z * lrMatrix.xAxis.z);
        lInverse.wAxis.y = -(lrPos.x * lrMatrix.yAxis.x + lrPos.y * lrMatrix.yAxis.y + lrPos.z * lrMatrix.yAxis.z);
        lInverse.wAxis.z = -(lrPos.x * lrMatrix.zAxis.x + lrPos.y * lrMatrix.zAxis.y + lrPos.z * lrMatrix.zAxis.z);
        lInverse.wAxis.w = 0.0f;
        return lInverse;
    }

    // Inverse(m): the GENERAL affine inverse -- the adjugate of the upper 3x3 over its
    // determinant, plus the inverse translation. Unlike the orthonormal fast path above it does
    // NOT assume the 3x3 is a rotation, so it survives a scaled or sheared basis.
    //
    // ADDITIVE GROW 2026-08-15 (post-fx step-6 producers wave). ATTESTED: MotionBlurState::Update
    // @0x823F8490 inlines this routine FOUR times in its lfTimeStep == 0 arm -- over
    // maViewCache[1], maViewCache[2], lView and the constructed camera transform (four
    // `vmsum3fp128` determinants at 0x823F85B0 / 0x823F85F4 / 0x823F8678 / 0x823F8844, each with
    // its own `vrefp`) -- and the DWARF hint listing for that function names
    // `rw::math::vpu::Inverse` exactly four times
    // (dwarfdump/GameSource/Graphics/PostFx/BrnPostFxShader.cpp, MotionBlurState::Update).
    // The X360 shape is the classic VMX adjugate: three cross products built with
    // `vpermwi128 ..., 0x63` (the y,z,x,w swizzle) + `vmulfp128`/`vnmsubfp` pairs, the determinant
    // as one `vmsum3fp128` dot3 of the first row against cross(row1,row2), its reciprocal as
    // `vrefp` refined by TWO Newton-Raphson steps (`vnmsubfp`/`vmaddfp` x2), the transpose of the
    // three crosses assembled with `vmrghw`/`vmrglw`, and the translation negated with a
    // `vxor` against the 0x80000000 splat (`vslw v30, v0, v0` on the all-ones vector) before the
    // same broadcast+FMA cascade.
    //
    // FLAG (VMX -> portable, this vendor home's standing convention): the vrefp + two-Newton
    // reciprocal is reconstructed as an exact divide, exactly as Normalize / OrthoNormalize3x3 /
    // SLerp above already reconstruct vrsqrtefp. Numerically tighter, never a placeholder. There
    // is NO singular-matrix guard because the console has none.
    inline Matrix44Affine Inverse(const Matrix44Affine& lrMatrix)
    {
        const Vector3 lvCrossYZ = Cross(lrMatrix.yAxis, lrMatrix.zAxis);
        const Vector3 lvCrossZX = Cross(lrMatrix.zAxis, lrMatrix.xAxis);
        const Vector3 lvCrossXY = Cross(lrMatrix.xAxis, lrMatrix.yAxis);

        const f32 lfInverseDeterminant = 1.0f / Dot(lrMatrix.xAxis, lvCrossYZ);

        Matrix44Affine lInverse;
        lInverse.xAxis = Vector3{ lvCrossYZ.x * lfInverseDeterminant,
                                  lvCrossZX.x * lfInverseDeterminant,
                                  lvCrossXY.x * lfInverseDeterminant, 0.0f };
        lInverse.yAxis = Vector3{ lvCrossYZ.y * lfInverseDeterminant,
                                  lvCrossZX.y * lfInverseDeterminant,
                                  lvCrossXY.y * lfInverseDeterminant, 0.0f };
        lInverse.zAxis = Vector3{ lvCrossYZ.z * lfInverseDeterminant,
                                  lvCrossZX.z * lfInverseDeterminant,
                                  lvCrossXY.z * lfInverseDeterminant, 0.0f };

        // The inverse translation: -Pos transformed by the inverse rotation just built.
        const Vector3& lrPos = lrMatrix.wAxis;
        lInverse.wAxis = Vector3{
            -(lrPos.x * lInverse.xAxis.x + lrPos.y * lInverse.yAxis.x + lrPos.z * lInverse.zAxis.x),
            -(lrPos.x * lInverse.xAxis.y + lrPos.y * lInverse.yAxis.y + lrPos.z * lInverse.zAxis.y),
            -(lrPos.x * lInverse.xAxis.z + lrPos.y * lInverse.yAxis.z + lrPos.z * lInverse.zAxis.z),
            0.0f };
        return lInverse;
    }

    // IsValid(Matrix44Affine): the affine is valid when every one of its four rows is
    // valid (no NaN in any x/y/z lane). The console build folds this into a per-row,
    // per-lane vcmpeqfp self-equality cascade (the x==x NaN test broadcast over each
    // row, ANDed together); reconstructed here as four IsValid(Vector3) row checks. The
    // RaceCar transform asserts (AddToWorld / UpdatePositioningData) call this on the
    // full transform.
    inline bool IsValid(const Matrix44Affine& lrMatrix)
    {
        return IsValid(lrMatrix.xAxis)
            && IsValid(lrMatrix.yAxis)
            && IsValid(lrMatrix.zAxis)
            && IsValid(lrMatrix.wAxis);
    }

    // SLerp(from, to, amount, &angleOut) -- X360 0x82216858. Spherical-linear blend between
    // two affine transforms. SIGNATURE CORRECTED (the previous `const float* lpfAmount` was a
    // mis-read): the asm takes r3 = the sret, r4 = lrFrom, r5 = lrTo, v1 = the blend amount as
    // a broadcast VecFloat, and r6 = a POINTER the function WRITES -- the remaining rotation
    // angle after the blend. BrnDirector::Camera::TrafficLaneTruck::Update @0x82247AC0 passes a
    // stack slot for it that it never reads (DWARF names that local `lUnusedAngle`).
    //
    // The console's four arms, read off the branch structure:
    //   0x8221688C  amount <= 0            -> out = from, *angleOut = the angle (below)
    //   0x8221694C  amount >= 1            -> out = to,   *angleOut = 0
    //   0x82216A94  angle < flt_82001764   -> out = per-row LERP, the three axis rows
    //                                         re-normalised; *angleOut = angle*(1 - amount)
    //   0x82216BB4  otherwise              -> the true slerp; *angleOut = angle*(1 - amount)
    // where `angle` is computed at 0x822168BC..0x82216910 as
    //   XMVectorACos( clamp( Dot3( Normalize(from.zAxis), Normalize(to.zAxis) ), -1, +1 ) )
    // and flt_82001764 == 0.03490658476948738 == 2 degrees (dumped from the shipped image).
    //
    // FLAG (VMX->portable, the standing convention of this vendor home): the console evaluates
    // the arc with the XNA minimax pipeline -- vrsqrtefp + two Newton-Raphson steps for the
    // normalises, XMVectorACos, and an XMVectorSinCos whose Taylor coefficient block is the
    // shipped rodata at 0x82000BD0..0x82000C6F. Exactly as Normalize/Magnitude/SqrtFast in the
    // sibling header already do, that estimate pipeline is reconstructed as the exact
    // closed-form scalar math (std::acos / std::sin): numerically tighter than the console's
    // approximation, never a placeholder. The BRANCH STRUCTURE, the 2-degree threshold, the
    // "which rows get re-normalised" choice and the angle written back through lpvAngleOut are
    // all transcribed, not invented.
    inline Matrix44Affine SLerp(const Matrix44Affine& lrFrom, const Matrix44Affine& lrTo,
                                float lfAmount, Vector3* lpvAngleOut)
    {
        Matrix44Affine lResult;

        // 0x822168BC..0x82216910 -- the angle between the two forward axes.
        const Vector3 lvFromAt = Normalize(lrFrom.zAxis);
        const Vector3 lvToAt   = Normalize(lrTo.zAxis);
        float lfCos = Dot(lvFromAt, lvToAt);
        if (lfCos < -1.0f) lfCos = -1.0f;      // vmaxfp against vcfsx(-1)
        if (lfCos >  1.0f) lfCos =  1.0f;      // vminfp against vcfsx(+1)
        const float lfAngle = std::acos(lfCos);

        // 0x8221688C -- amount <= 0: hand back `from` untouched, report the whole angle.
        if (lfAmount <= 0.0f)
        {
            if (lpvAngleOut != 0) *lpvAngleOut = Vector3{ lfAngle, lfAngle, lfAngle, lfAngle };
            return lrFrom;
        }

        // 0x8221694C -- amount >= 1: hand back `to`, report no remaining angle.
        if (lfAmount >= 1.0f)
        {
            if (lpvAngleOut != 0) *lpvAngleOut = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
            return lrTo;
        }

        const float lfRemaining = lfAngle - lfAngle * lfAmount;   // 0x82216E20 / 0x82216E3C
        if (lpvAngleOut != 0)
            *lpvAngleOut = Vector3{ lfRemaining, lfRemaining, lfRemaining, lfRemaining };

        // 0x82216AA8 (angle < 2 degrees) -- a straight per-row lerp with the three axis rows
        // re-normalised. 0x82216BB4 (the wider arc) -- the same endpoints distributed along the
        // arc by sin((1-t)*angle)/sin(angle) and sin(t*angle)/sin(angle); at the threshold the
        // two agree to within the console's own estimate error, and for a degenerate (sin ~ 0)
        // arc the slerp weights collapse back to the lerp ones.
        float lfWeightFrom = 1.0f - lfAmount;
        float lfWeightTo   = lfAmount;
        if (lfAngle >= 0.03490658476948738f)
        {
            const float lfSin = std::sin(lfAngle);
            if (lfSin != 0.0f)
            {
                lfWeightFrom = std::sin((1.0f - lfAmount) * lfAngle) / lfSin;
                lfWeightTo   = std::sin(lfAmount * lfAngle) / lfSin;
            }
        }

        lResult.xAxis = Normalize(Mult(lrFrom.xAxis, lfWeightFrom) + Mult(lrTo.xAxis, lfWeightTo));
        lResult.yAxis = Normalize(Mult(lrFrom.yAxis, lfWeightFrom) + Mult(lrTo.yAxis, lfWeightTo));
        lResult.zAxis = Normalize(Mult(lrFrom.zAxis, lfWeightFrom) + Mult(lrTo.zAxis, lfWeightTo));

        // The translation row is ALWAYS a plain lerp (0x82216AE0..0x82216B00 / 0x82216DFC's
        // vmaddcfp128 `from.w + (to.w - from.w)*amount`), never normalised.
        lResult.wAxis = Lerp(lrFrom.wAxis, lrTo.wAxis, lfAmount);
        return lResult;
    }
}
}
}

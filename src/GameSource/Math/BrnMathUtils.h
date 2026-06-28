#pragma once

#include "BrnCommonTypes.h"   // Vector2 / Vector3 (= rw::math::vpu::Vector2/3)
#include "types.hpp"          // f32 / s32

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnMath::Flatten                         @ 0x822CB8E8
//   BrnMath::IsNormal                        @ 0x822B1CF8  (EXECUTED in goal trace)
//   BrnMath::Magnitude2D                     @ 0x822B1DD8
//   BrnMath::MagnitudeSquared2D              @ 0x8276AD30
//   BrnMath::RoundWithNumSignificantFigures  @ 0x82361600  (uses helper sub_82C09970 @ 0x82C09970)
//
// All five are math utilities the X360 build compiled over AltiVec/VMX SIMD (or the
// scalar FPU, for RoundWith). Hex-Rays rendered them as raw __asm: the hidden sret
// pointer, _savegprlr/_restgprlr, lvx128/stvx128, vperm lane permutes, COERCE_INT, and
// the `result = EndAssert()` return-var noise are ALL compiler/decompiler artifacts and
// are dropped. These are reconstructed at SEMANTIC PARITY in portable C++ -- NOT as
// __asm transcriptions -- mirroring BrnTrafficLightTrigger.cpp:
//   * the three-lane vspltw+vcmpeqfp self-equality cascade is the inlined per-lane NaN
//     test = rw::math::vpu::IsValid(lVector), reversed to a single CGS_ASSERT;
//   * the vrsqrtefp + two Newton-Raphson refinement steps that compute 1/sqrt (then x
//     magnitude^2 -> magnitude) are de-optimised to an exact std::sqrt, numerically a
//     touch tighter than the console's estimate, never a placeholder;
//   * float32_t (the SDK type) is reconstructed as the project f32.
//
// "2D" throughout this TU means the X/Z ground plane (lanes 0 and 2 = .x and .z).
// See the .cpp for the per-function FLAGs on every interpretive choice.

namespace BrnMath
{
    // XZ ground-plane flatten: drops the Y (height) lane. @ 0x822CB8E8
    Vector2 Flatten(Vector3 lVector);

    // True iff lVector is unit length within a tolerance. @ 0x822B1CF8
    bool IsNormal(Vector3 lVector);

    // sqrt(x*x + z*z) -- 2D (XZ) magnitude. @ 0x822B1DD8
    f32 Magnitude2D(Vector3 lVector);

    // x*x + z*z -- 2D (XZ) squared magnitude. @ 0x8276AD30
    f32 MagnitudeSquared2D(Vector3 lVector);

    // Round lfValue to lfNumFigures significant figures, returned as an integer. @ 0x82361600
    s32 RoundWithNumSignificantFigures(f32 lfValue, f32 lfNumFigures);

    // ADDITIVE GROW (declare-only; body in its own TU @ X360 0x82540AB8).
    // Exact point-in-oriented-box test: transform lPoint into the box's local frame via
    // lBoxTransform and test each component against +/- the box half-extents. StuntManager::
    // OnPropHit's narrowphase calls this. FLAG: the X360 passes the box transform plus two SIMD
    // args (the prop position and the region dimensions); the exact param order / by-value-vs-ref
    // is inferred from the OnPropHit call site and the asm register setup.
    bool IsPointInsideBox(const Matrix44Affine& lBoxTransform, Vector3 lPoint, Vector3 lHalfExtents);
}

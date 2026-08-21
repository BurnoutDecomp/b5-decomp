// ============================================================================
// GameSource/Director/Camera/Utils/BrnInterpolater.cpp
//
// BrnDirector::Camera::Utils::Interpolater -- the remembered-axis orientation blender.
//
//   Construct        DWARF BrnInterpolater.h:51
//   Interpolate x2   DWARF BrnInterpolater.h:57 / :63
//
// All three are THIN: the console gives none of them a standalone symbol because the whole
// body is Camera::Utils::DirectionPreservingSLerp @0x82205558, which takes this object's two
// members as loose trailing pointers (`&mLastAxis`, `&mbWasInvertedLastTime` -- 16 bytes
// apart, which is what pins the layout). What the class contributes is the STATE: two of
// these live inside CameraInterpolationController, one per matrix being blended, so the
// rotation blend and the look-at blend never share an axis-inversion latch.
//
// The call shape is attested by RotateAboutPivotParams::Interpolate @0x8221E9D0:
//     addi r7, r5, 0x10 ; mr r6, r5 ; ... bl DirectionPreservingSLerp
// i.e. (sret, from, to, &interpolater, &interpolater + 0x10, t).
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnInterpolater.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"   // DirectionPreservingSLerp

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

// ----------------------------------------------------------------------------
// Construct -- DWARF BrnInterpolater.h:51.
//
// Reset the remembered axis state. The console INLINES this at its callers: both
// BehaviourInterpolate::Construct @0x82255FC8 (`stvx 0` at this+0x260 / this+0x280 with a
// `stb 0` at each +0x10) and CameraInterpolationController's own construction zero the pair
// as raw storage. Zero is the right seed rather than an arbitrary axis: the first
// Interpolate's dot against it is 0, which is NOT < 0, so the first frame of any blend takes
// the un-inverted arm -- exactly as a fresh blend should.
// ----------------------------------------------------------------------------
void Interpolater::Construct()
{
    mLastAxis              = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
    mbWasInvertedLastTime  = false;
}

// ----------------------------------------------------------------------------
// Interpolate(Matrix33) -- DWARF BrnInterpolater.h:57.
// ----------------------------------------------------------------------------
Matrix33 Interpolater::Interpolate(Matrix33 lFrom, Matrix33 lTo, VecFloat lvT)
{
    return DirectionPreservingSLerp(lFrom, lTo, mLastAxis, mbWasInvertedLastTime, lvT);
}

// ----------------------------------------------------------------------------
// Interpolate(Matrix44Affine) -- DWARF BrnInterpolater.h:63.
//
// ⛔ CORRECTED 2026-08-20. This used to slerp only the 3x3 and CARRY lFrom.wAxis through,
// on the reasoning that the Matrix33 helper never writes a fourth row. That was wrong: the
// affine overload is its own console function -- DirectionPreservingSLerp @0x82217C08,
// asserting at CameraUtils.h:821 rather than :742 -- and its tail LERPS the translation
// (vsubfp / vmaddcfp128 / stvx to out+0x30). It also has NO small-angle arm, so routing an
// affine through the Matrix33 path would additionally have taken a shortcut the console
// does not take here. Both are now handled by calling the right overload.
// ----------------------------------------------------------------------------
Matrix44Affine Interpolater::Interpolate(Matrix44Affine lFrom, Matrix44Affine lTo, VecFloat lvT)
{
    return DirectionPreservingSLerp(lFrom, lTo, mLastAxis, mbWasInvertedLastTime, lvT);
}
} // namespace Utils
} // namespace Camera
} // namespace BrnDirector

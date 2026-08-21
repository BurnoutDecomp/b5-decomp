#pragma once

// ============================================================================
// GameSource/Director/Shots/ShotControllers/BrnCameraInterpolationController.h
//
// BrnDirector::CameraInterpolationController -- THE PER-FRAME CAMERA BLEND EVALUATOR.
//
// This is the type BehaviourInterpolate embeds at +0x260 and drives as the last statement of
// its PostCollisionUpdate: the thing that turns a parametric time into an actual blended
// camera. Until this header landed it was a 4-byte `OpaqueInterpolationController` stand-in,
// which is why every camera transition in the PC build was a CUT at t == 1 instead of the
// console's eased ramp.
//
// THE OWNING PATH IS THE CONSOLE'S OWN. Every assert in this family names
//   "..\..\..\GameSource\Director/Shots/ShotControllers/BrnCameraInterpolationController.cpp"
// (lines 112, 156 and 212), so the file sits under Director/Shots/ShotControllers/ rather
// than under Director/Camera/ where its callers live.
//
// LAYOUT -- two Utils::Interpolater sub-objects, by value:
//   +0x00  mRotationInterpolater   the orientation blend's remembered-axis state
//   +0x20  mPivotInterpolater      the look-at basis blend's remembered-axis state
// Pinned from the call sites, not guessed: Update @0x822513D8 hands `this` and `this + 32`
// to RotateAboutPivot (asm `a1`, `a1 + 32`), and `this` and `this + 16` to the plain-slerp
// helper sub_82217C08 -- i.e. the SECOND interpolater starts at +0x20 while the FIRST one's
// two members are at +0x00 (mLastAxis) and +0x10 (mbWasInvertedLastTime). That matches
// Utils::Interpolater's committed shape in Camera/Utils/BrnInterpolater.h exactly
// (Vector3 mLastAxis @+0x00, bool mbWasInvertedLastTime @+0x10), and it matches the
// "two 16-byte blocks + a trailing byte, each zeroed" that BehaviourInterpolate::Construct
// @0x82255FC8 zeroes at this+0x260 / this+0x280.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                          // Matrix44Affine / Vector3
#include "GameSource/Director/Camera/Utils/BrnInterpolater.h"        // Utils::Interpolater

namespace BrnDirector
{
namespace Camera { struct Camera; }   // STRUCT, not class -- Camera.h declares it as one,
                                      // and MSVC mangles the two differently (U vs V).

class CameraInterpolationController
{
public:
    // ------------------------------------------------------------------------
    // RotateAboutPivotParams -- a camera expressed RELATIVE TO A PIVOT, in the form that
    // can be blended without the eye swinging through the pivot.
    //
    // ExtractRotateAboutPivotParams @0x8221EAC0 produces one of these; Interpolate
    // @0x8221E9D0 blends two; Matrix44AffineFromRota @0x821F8220 rebuilds a world transform
    // from the result. The decomposition is what makes a rotate-about-car blend orbit the
    // car instead of cutting a chord through it.
    //
    // LAYOUT (every offset read off the three functions' loads/stores):
    //   +0x00..+0x20  mRotation   the camera's 3x3, expressed IN the look-at frame
    //                             (ExtractRotateAboutPivotParams stores M3x3 * lookAt^T)
    //   +0x30..+0x50  mLookAt     the look-at basis built from the pivot-relative eye
    //                             position (Utils::CreateLookAt(eye, origin))
    //   +0x60         mfDistance  |eye| in pivot space -- the orbit radius
    // ------------------------------------------------------------------------
    struct RotateAboutPivotParams
    {
        // @0x8221E9D0. Blend two pivot-relative descriptions by lfT. The two rotations go
        // through DIRECTION-PRESERVING slerps (one Interpolater each, so neither flips to
        // the antipodal axis mid-blend) and the radius is an ordinary scalar lerp.
        static RotateAboutPivotParams Interpolate(const RotateAboutPivotParams& lrFrom,
                                                  const RotateAboutPivotParams& lrTo,
                                                  Camera::Utils::Interpolater& lrRotationInterpolater,
                                                  Camera::Utils::Interpolater& lrLookAtInterpolater,
                                                  f32 lfT);

        Matrix44Affine mRotation;    // +0x00 (rows 0..2 used; row 3 is scratch)
        Matrix44Affine mLookAt;      // +0x30 (rows 0..2 used)
        f32            mfDistance;   // +0x60
    };

    // Reset both remembered-axis states. The console INLINES this at
    // BehaviourInterpolate::Construct @0x82255FC8, which zeroes the pair as raw storage
    // (the two stvx 0 + stb 0 at this+0x260 / this+0x280); reached by name here so the
    // sub-objects own their own reset.
    void Construct() { mRotationInterpolater.Construct(); mPivotInterpolater.Construct(); }

    // @0x822513D8. THE ENTRY POINT -- the whole per-frame blend. Reads the parametric time
    // and the two selector bytes BehaviourInterpolate::PostCollisionUpdate stamped into
    // lrCamera's effects, maps the time through the chosen easing curve, blends the
    // TRANSFORM by the chosen method, then blends CameraState, CameraEffects, the five
    // DepthOfField floats, the FOV and the near-clip distance toward lrTo. Finally CONSUMES
    // the blend field by zeroing it, which is why the producer rewrites it every frame.
    void Update(Camera::Camera& lrCamera, const Camera::Camera& lrTo,
                const Matrix44Affine& lrEyeTarget);

    // @0x8223DA28. Blend lrFrom -> lrTo by lfT as an ORBIT ABOUT lrPivot: invert the pivot
    // transform, describe both cameras relative to it, blend those descriptions, and rebuild
    // a world transform. Asserts lfT in [0, 1] (cpp:212).
    Matrix44Affine RotateAboutPivot(const Matrix44Affine& lrPivot,
                                    const Camera::Camera& lrFrom,
                                    const Camera::Camera& lrTo,
                                    Camera::Utils::Interpolater& lrRotationInterpolater,
                                    Camera::Utils::Interpolater& lrLookAtInterpolater,
                                    f32 lfT);

    // @0x8221EAC0. Describe lrCamera relative to a pivot whose INVERSE is lrInversePivot.
    // (RotateAboutPivot builds that inverse itself and passes it to both extractions.)
    void ExtractRotateAboutPivotParams(const Matrix44Affine& lrCameraTransform,
                                       const Matrix44Affine& lrInversePivot,
                                       RotateAboutPivotParams& lrOut) const;

    // @0x821F8220. The inverse of the extraction: rebuild a world transform from a
    // pivot-relative description and the pivot's own transform.
    Matrix44Affine Matrix44AffineFromRota(const RotateAboutPivotParams& lrParams,
                                          const Matrix44Affine& lrPivot) const;

private:
    Camera::Utils::Interpolater mRotationInterpolater;   // +0x00
    Camera::Utils::Interpolater mPivotInterpolater;      // +0x20
};

} // namespace BrnDirector

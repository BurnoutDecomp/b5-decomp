#ifndef GAMESOURCE_DIRECTOR_CAMERA_UTILS_CAMERA_UTILS_H
#define GAMESOURCE_DIRECTOR_CAMERA_UTILS_CAMERA_UTILS_H

// BrnDirector::Camera::Utils -- shared camera math helpers + small value types.
//
// Provenance: declarations mirror the DecFIGS DWARF for
// GameSource/Director/Camera/Utils/CameraUtils.{h,cpp}. This header is the canonical
// home for the FOV / look-at / screen-fit free functions and the VersionNumber / FOV /
// AABBox value types the camera-utils family shares. BrnLooker.cpp is the first consumer;
// the helper BODIES land with CameraUtils.cpp's own TU (declaration-only here -- the
// per-TU `cl /c` gate does not link, so a real declaration is all a not-yet-done callee
// needs).

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector2/Vector3/Vector4(VecFloat)/Matrix44/Matrix44Affine aliases

namespace BrnDirector
{
namespace Camera
{
    // Axis-aligned bounding box value (two Vector3 corners, 32 bytes). The engine's real
    // type is rw::collision::AABBoxTemplate<...> (EATech cmn/rw/collision/aabbox.h), not
    // yet reconstructed in a vendor home; the camera utils only need the 32-byte min/max
    // storage passed by value into GetSizeOnScreen. FLAG: a storage-only stand-in -- the
    // committed BrnPlayerInfo.h carries an identical BrnDirector::Camera::AABBox slice; a
    // future consolidation should unify both on the real vendor template.
    struct alignas(16) AABBox
    {
        Vector3 mMin;   // aabbox.h: m_min
        Vector3 mMax;   // aabbox.h: m_max
    };

namespace Utils
{
    // DWARF CameraUtils.h:59. A serialiser version tag (a single 32-bit count). Embedded
    // as the head member of every camera-utils Parameters block.
    struct VersionNumber
    {
        u32 muVersion;
    };

    // DWARF CameraUtils.h:65. A single FOV scalar wrapper.
    struct FOV
    {
        f32 mfFOV;
    };

    // ---- FOV <-> zoom / look-at math (DWARF CameraUtils.{h,cpp}; declaration-only) -------

    // DWARF CameraUtils.cpp:54. Map a "zoom" scalar to a field-of-view in degrees.
    f32 GetFOVDegsFromZoom(f32 lfZoom);

    // DWARF CameraUtils.h:60. Clamp an FOV (degrees) into the engine's sensible band.
    f32 ClampToSensibleFOVDegs(f32 lfFOVDegs);

    // DWARF CameraUtils.cpp:14 / :65. Build a world look-at matrix from eye/target (and an
    // optional explicit up). Used by Track.
    Matrix44Affine CreateLookAt(Vector3 lEye, Vector3 lTarget);
    Matrix44Affine CreateLookAt(Vector3 lEye, Vector3 lTarget, Vector3 lUp);

    // DWARF CameraUtils.cpp:56. Apply a screen-space (x,y) look offset to a look-at matrix
    // given the camera FOV / aspect. Used by Track.
    Matrix44Affine CreateAdjustedLookAt(Matrix44Affine lLookAt,
                                        VecFloat lvFOV,
                                        VecFloat lvAspect,
                                        Vector2 lScreenOffset);

    // DWARF CameraUtils.h:66. Rotate an affine in place by ZXY Euler angles (radians).
    // Used by Track when applying the fixed look offset.
    void RotateMatrix44AffineByEulerAnglesZXY(Matrix44Affine& lrMatrix, Vector3 lEulerAngles);

    // DWARF CameraUtils.cpp:44. The projected on-screen size (width/height in screen units)
    // of an AABB as seen by the camera transform. Used by Zoom.
    Vector2 GetSizeOnScreen(Matrix44Affine lCameraTransform,
                            VecFloat lvFOV,
                            VecFloat lvAspect,
                            Matrix44Affine lTargetTransform,
                            AABBox lAABB);

    // DWARF CameraUtils.cpp:50. The FOV (degrees) needed to make an object of the given
    // on-screen size occupy the requested screen AREA at the given distance. Used by Zoom.
    VecFloat GetFOVDegsToFitObjectToScreenArea(VecFloat lvDistance,
                                               Vector2 lSizeOnScreen,
                                               VecFloat lvTargetArea);

    // DWARF CameraUtils.cpp:53. The FOV (degrees) needed to fit an object of the given
    // on-screen size to a requested screen SIZE (x,y) at the given distance. Used by Zoom.
    VecFloat GetFOVDegsToFitObjectToScreenSize(VecFloat lvDistance,
                                               Vector2 lSizeOnScreen,
                                               Vector2 lTargetSize);
// ----------------------------------------------------------------------------
// TransitionSmoother (ADDITIVE GROW: its class TU) -- a lerp-smoothed scalar
// with a self-smoothed lerp amount. Class shape / member names / method set
// verbatim from the DWARF (CameraUtils.h:112/:138-:143). This TU bodies Set;
// Get/GetRef/SetTarget/Update are their own ledger functions (declared-only).
// ----------------------------------------------------------------------------
struct TransitionSmoother
{
    // DWARF :116/:119/:130/:134 -- declared-only.
    f32 Get() const;
    const f32& GetRef() const;
    void SetTarget(f32 lfTarget);
    void Update(f32 lfTimeStep);

    // @0x821F22A0 (class TU; body in CameraUtils.cpp, DWARF :126) -- seed the
    // smoother: value/target snap to lfValue, the live lerp amount restarts at 0
    // and chases lfLerpAmount0to1 (itself smoothed by lfLerpAmountLerpAmount0to1).
    void Set(f32 lfValue, f32 lfLerpAmount0to1, f32 lfLerpAmountLerpAmount0to1,
             f32 lfSimilarityToleranceScale);

private:
    f32 mfData;                       // :138  +0x00
    f32 mfTarget;                     // :139  +0x04
    f32 mfIdealLerpAmount;            // :140  +0x08
    f32 mfLerpAmount;                 // :141  +0x0C
    f32 mfLerpAmountLerpAmount;       // :142  +0x10
    f32 mfSimilarityToleranceScale;   // :143  +0x14
};

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_UTILS_CAMERA_UTILS_H

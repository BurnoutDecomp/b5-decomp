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

// ⛔⛔ ADDED 2026-08-01 (orbit-camera wave) -- THIS INCLUDE IS LOad-BEARING, NOT A CONVENIENCE.
// Every `VecFloat` below sits inside `namespace BrnDirector`, and TWO types answer to that
// name: the global `typedef rw::math::vpu::Vector4 VecFloat` from BrnCommonTypes.h above, and
// `BrnDirector::VecFloat` from BrnDirectorTimestep.h. Unqualified lookup inside the namespace
// picks the SECOND one -- but only in translation units that happened to have included that
// header first. So the declarations here silently meant different types in different TUs, and
// the two spellings mangle differently (`UVecFloat@3@` vs `UVector4@vpu@math@rw@@`).
// It stayed invisible for as long as none of the VecFloat-taking helpers had a body: with
// nothing to link against, no TU ever had to agree with another. The moment
// GetSizeOnScreen / GetFOVDegsToFitObjectToScreenSize / CreateAdjustedLookAt were bodied, the
// caller (which reaches BrnDirectorTimestep.h through Behaviour.h) and the definition (which
// did not) produced two different symbols and an unresolved external that reads like a
// missing body rather than a type mismatch.
// Including it here forces every consumer onto BrnDirector::VecFloat. Timestep pulls only
// types.hpp + CgsAssert.h and does not reach back here, so there is no cycle.
#include "GameSource/Director/Utils/BrnDirectorTimestep.h"   // BrnDirector::VecFloat -- see above

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

    // @0x8220C4F8 (DWARF CameraUtils.cpp:704) / @0x8220C960 (:767). Build a world look-at
    // frame from eye/target -- rows {xAxis, yAxis, zAxis, wAxis=eye} where zAxis is the
    // normalised eye->target direction, xAxis = Normalize(Cross(up, zAxis)) and yAxis =
    // Cross(zAxis, xAxis). The two-argument form uses the world up axis {0,1,0}. Parameter
    // names are the console's own (baked into the assert literals "IsValid(lEyePosition)" /
    // "IsValid(lTargetPosition)"). BODIED in CameraUtils.cpp.
    Matrix44Affine CreateLookAt(Vector3 lEyePosition, Vector3 lTargetPosition);
    Matrix44Affine CreateLookAt(Vector3 lEyePosition, Vector3 lTargetPosition, Vector3 lUpVector);

    // DWARF CameraUtils.cpp:56. Apply a screen-space (x,y) look offset to a look-at matrix
    // given the camera FOV / aspect. Used by Track.
    Matrix44Affine CreateAdjustedLookAt(Matrix44Affine lLookAt,
                                        VecFloat lvFOV,
                                        VecFloat lvAspect,
                                        Vector2 lScreenOffset);

    // @0x82204F98 / PS3 @0xA9780 (DWARF CameraUtils.h:423 -- the console keeps the body
    // inline in this header; the reconstruction puts it in CameraUtils.cpp's own TU like the
    // rest of the family). Rotate an affine in place by ZXY Euler angles in RADIANS, laid out
    // {x = pitch, y = yaw, z = roll}: post-multiplies lrMatrix by Rz*Rx*Ry (row-major), i.e.
    // roll is applied first and yaw last. Used by Track's fixed look offset, by
    // BehaviourDebugFlyWorld's rig, by PerlinShakeController and by CameraShake::Update.
    // ⚠️ It is the PLAIN affine product, so the TRANSLATION ROW IS ROTATED ABOUT THE WORLD
    // ORIGIN as well -- see the block comment at the definition. BODIED 2026-08-02.
    void RotateMatrix44AffineByEulerAnglesZXY(Matrix44Affine& lrMatrix, Vector3 lEulerAngles);

    // @0x82222180 (DWARF CameraUtils.cpp:453,
    // ._ZN11BrnDirector6Camera5Utils32EulerAnglesZXYFromMatrix44AffineEN2rw4math3vpu14Matrix44
    // AffineEPNS4_7Vector3Ef). The ZXY Euler angles (radians) of an affine's rotation -- the
    // inverse of RotateMatrix44AffineByEulerAnglesZXY. lpLastAngles (optional) disambiguates the
    // near-vertical gimbal-degenerate branch toward the previous frame's angles;
    // lfVerticalComparisonEpsilon is the near-vertical tolerance. Used by
    // TrafficLaneTruck::Update (the camera BANK) and BehaviourDebugFlyWorld::WarpToLookAt /
    // ::Update. BODIED in CameraUtils.cpp.
    Vector3 EulerAnglesZXYFromMatrix44Affine(Matrix44Affine lIn, Vector3* lpLastAngles = 0,
                                             f32 lfVerticalComparisonEpsilon = 0.0099999998f);

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

    // @0x821F23E8 (CameraUtils.h:559). Inverse of GetFOVDegsFromZoom: zoom = 1/tan(fov*pi/360).
    f32 GetZoomFromFOVDegs(f32 lfFOVDegs);

    // @0x821F2530. FOV (degrees) + film/sensor size -> 35mm-equivalent lens length (mm).
    // Used by BrnDirector::DebugComponent::RenderHUD.
    f32 ConvertFOVDegsToLensLength(f32 lfFOVDegs, f32 lfFilmSize);

    // @0x821F2378 (CameraUtils.h:340). Wrap a value into [lo, hi) after adding a step (unsigned).
    // Used by BrnDirector::ArbStateCarSelect::StartCarUnlockCam.
    u32 Cycle(u32 luValue, u32 luInclusiveLowerBound, u32 luExclusiveUpperBound, u32 luStep);

    // @0x82205A60 (CameraUtils.h:1205). Pitch angle (radians) of the centre->point direction:
    // asin(Normalize(point-centre).y). Used by the vehicle collision policy.
    f32 GetPitchAboutPointRads(Vector3 lCentre, Vector3 lPoint);

    // DWARF CameraUtils.cpp -- the signed smallest angular delta (degs) from lfFromDegs to
    // lfToDegs, wrapped into (-180, 180]. Used by the 2D / spherical rotation controllers to
    // ease their accumulated rotation angle along the shortest arc (declaration-only here;
    // the body lands with CameraUtils.cpp's own TU).
    f32 GetSmallestDifferenceBetweenDegsAngles(f32 lfFromDegs, f32 lfToDegs);

    // @0x821F8988 / PS3 @0x37EA4 -- the RADIANS sibling of the above: the signed smallest
    // angular delta from lfFromRads to lfToRads, wrapped into [-PI, PI].
    //
    // ⭐ ADDED 2026-08-02. This was the ONE link dependency of
    // BehaviourGameplayExternal::Update that lives OUTSIDE that file: Update calls the
    // Vector3 overload at BehaviourGameplayExternal.cpp:337 and NEITHER overload was
    // declared anywhere in the tree -- only the degrees scalar above. Verified by grep, and
    // now closed at the seam the console draws it at.
    f32 GetSmallestDifferenceBetweenRadAngles(f32 lfFromRads, f32 lfToRads);

    // PS3 @0x382E0 -- the per-component Vector3 overload (the one Update calls). The console
    // body is literally three calls to the scalar overload above, one per X/Y/Z lane,
    // permuted back into a vector; the W lane is NEVER written (the console `lvx`es the
    // uninitialised return buffer and only vperms lanes 0/1/2 into it).
    // ⚠️ NO X360 ADDRESS: this overload does not appear in the X360 ARTIST export at all --
    // it is fully inlined into its callers there. The PS3 build (which carries the DWARF)
    // keeps it out of line, so the shape below is read off @0x382E0 store-for-store.
    Vector3 GetSmallestDifferenceBetweenRadAngles(Vector3 lFromRads, Vector3 lToRads);

    // ------------------------------------------------------------------------------------
    // ⭐⭐ THE "TEND TO LIMIT" FAMILY -- ADDED 2026-08-02 (final-helpers wave).
    // Three scalar response curves BehaviourGameplayExternal::ApplySlideyEffects drives, and
    // NONE of the three was declared anywhere in the tree. Found by walking that helper's
    // callee set and grepping for a DEFINITION rather than a declaration -- the same check
    // that turned up Camera::SetRequestedTimeDilation and the CameraShake::Update stub.
    // SineLerp in particular is cited as a blocker by FOUR other committed camera TUs
    // (BrnBehaviourInterpolate, BrnBehaviourRoadRunner, BrnBehaviourManager, BrnReplayDirector).
    // Bodied here BEFORE their caller lands, deliberately -- the standing rule in this cluster
    // since the shake stub.
    // ------------------------------------------------------------------------------------

    // @0x821F8B78 / PS3 @0x1B410 (DWARF CameraUtils.cpp:616). A saturating response curve:
    // maps [0, inf) onto [0, lrLimit), reaching HALF of lrLimit exactly at
    // lrValueIn == lrValueForHalfway -- which is what names the second parameter and is the
    // cheapest check on the formula.
    //   x = lrValueIn / lrValueForHalfway ;  return (x / (x + 1)) * lrLimit
    // Three asserts, all non-gating (X360/DWARF lines 618 / 643 / 645).
    // ⚠️ PARAMETER NAMES ARE THE DWARF'S OWN (the PS3 export carries lrValueIn /
    //   lrValueForHalfway / lrLimit on f1/f2/f3), and so are the assert texts.
    f32 PositiveValueTendToLimit(f32 lrValueIn, f32 lrValueForHalfway, f32 lrLimit);

    // PS3 @0x1B53C (DWARF CameraUtils.cpp:660). The two-sided form: pick the NEGATIVE pair or
    // the POSITIVE pair by the sign of lrValueIn, take the absolute value, and tail-call the
    // helper above. Both console arms are literal tail branches into it.
    // ⚠️ NO X360 ADDRESS -- the X360 ARTIST export has no such symbol at all: it is inlined
    //   into every caller there (ApplySlideyEffects contains two expansions of it,
    //   @0x822262E4 and @0x822263EC, and reading THOSE is how each caller's five arguments
    //   were separated). The PS3 build keeps it out of line and carries the DWARF names.
    // ⚠️ THE ARGUMENT ORDER IS NEG-PAIR-FIRST and it is easy to get backwards: the console's
    //   `fmr f2, f4` / `fmr f3, f5` in the >= 0 arm is what says f2/f3 are the NEG pair and
    //   f4/f5 the POS pair.
    // FLAG (not transcribed): the DWARF also declares the per-lane Vector3 overload at
    //   CameraUtils.cpp:680 (PS3 @0x1B560). No caller needs it yet; left undeclared rather
    //   than guessed.
    f32 TendToLimits(f32 lrValueIn,
                     f32 lrNegValueForHalfway, f32 lrNegLimit,
                     f32 lrPosValueForHalfway, f32 lrPosLimit);

    // @0x8220CCB0 / PS3 @0x20AE4 (DWARF CameraUtils.cpp:808). A cosine-eased lerp: the
    // parameter is remapped through 1 - (cos(p * PI) + 1) / 2 (0 -> 0, 0.5 -> 0.5, 1 -> 1,
    // with zero slope at both ends) and then used as an ordinary lerp weight.
    // One non-gating assert that the parameter is in [0, 1] (line 810).
    f32 SineLerp(f32 lfFrom, f32 lfTo, f32 lfParameter);

    // @0x822183E0. Rotate a look-at frame about a world pivot by a pitch angle (radians).
    // Used by CollisionPolicyAttachedToVehicle::GenerateSceneQueries.
    // FLAG (declaration-only): NOT YET DONE. ⛔ ITS OLD REASON IS RETIRED (2026-08-02) -- it
    // read "an inline VMX Sin/Cos minimax whose coefficient tables (rodata
    // 82000BD0..82000C60) are not attested as named constants; bodying it would require
    // fabricating the polynomial", which is exactly what was said about
    // RotateMatrix44AffineByEulerAnglesZXY below until that turned out to be no obstacle at
    // all: the coefficients belong to sin/cos, which this family de-optimises to libm, and
    // 0x82000C60 is the DUMPED range-reduction row { pi, 2pi, 1/pi, 1/(2pi) }, not a
    // coefficient table. The real work is the pivot/compose order and nobody has done it.
    Matrix44Affine ApplyPitchAboutPointRads(Vector3 lPoint, VecFloat lvPitchRads);

    // @0x821F25B8. The FOUR near-clip-plane corner positions for a camera transform (a2..a5
    // are the four out-corners, written in asm store order r30/r29/r28/r27). Used by
    // BehaviourRoadRunner::Update.
    // FLAG (declaration-only): heavy XMVectorTan + VMX add/sub corner lattice; the corner
    // packing / lane order is not safely reconstructable store-for-store. Left unbodied.
    void CalcNearClipCorners(Matrix44Affine lrCameraTransform,
                             Vector3& lrCorner0,
                             Vector3& lrCorner1,
                             Vector3& lrCorner2,
                             Vector3& lrCorner3);

    // @0x822171B0. A fixed basis vector guaranteed non-parallel to lVector. Used by SafeSLerp.
    // FLAG (declaration-only): one of the two returned constants is the raw rodata sentinel
    // unk_82181510 (an unrecovered basis vector) -- not fabricated. Left unbodied.
    Vector3 FindNonParallelNormalisedVectorTo(Vector3 lVector);
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

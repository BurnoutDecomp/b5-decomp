// BrnDirector::Camera::Utils::Looker -- the camera "look-at + zoom" post-process.
//
// Provenance: X360 ASM is the spine.
//   * Parameters::Construct @0x821F8D80 -- the ONLY goal-trace-executed function; its field
//     defaults are pinned store-for-store from the stfs/stb displacements off r3.
//   * Track  @0x82222680, Zoom @0x82222A78, Update @0x8223FBB8 -- not executed in the goal
//     trace; reconstructed from the (heavily VMX128-optimised) Hex-Rays + the DWARF local
//     hints, all members accessed BY NAME, all callees via their real CameraUtils.h / Camera
//     / DepthOfField declarations.
//
// Grounded constants: deg->rad = 0.017453292 (pi/180); the look-tolerance cos arg uses
// 0.0087266462 (pi/360, i.e. half a degree in radians); the FOV is clamped into [10,90]
// degrees (the fsel pair against 10.0 / 90.0 at Zoom LABEL_6); the assessment latch arms at
// 0.2s (Zoom). The depth-of-field static path forwards staticFocalLength*flt_820049E0 /
// staticDOF*10 (the DOF scale flt_82004A20 == 10.0; the focal-length scale flt_820049E0 is
// ungroundable -- see KF_STATIC_FOCAL_LENGTH_SCALE).
//
// FLAG (ungroundable rodata): the three depth-of-field blurriness ramp-rate constants the
// Zoom blend reads (flt_82CDAD18 / flt_82CDAD1C / flt_82CDAD20 -- the per-second blur/unblur
// rates lerping the camera's mDepthOfField blurriness lane at camera+0x134 toward its target)
// could not be resolved to numeric values from the available exports (no rodata tool / the
// .data float is not in the asm packet). They are reconstructed as clearly-named placeholder
// constants initialised to 0.0f rather than fabricated; see KF_DOF_*_RATE below. This makes
// the DOF-blur ramp a no-op until the real rates are recovered, but keeps the control flow /
// member writes faithful. The static-DOF focal-length scale (flt_820049E0) is likewise an
// ungroundable FLAGGED placeholder.

#include "GameSource/Director/Camera/Utils/BrnLooker.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"             // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                     // Dot/Normalize/NormalizeFast/SqrtFast/operator-
#include "rw/math/vpu/vector4_operation.h"                     // VecFloat lane ops
#include "rw/math/vpu/matrix44affine_operation.h"              // SLerp / IsValid
#include "rw/math/fpu/scalar_operation.h"                      // Clamp/Abs/Min/Max/Cos/Tan/IsZero

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
namespace
{
    // Degrees -> radians (pi/180). Pinned from the Track asm (0.017453292).
    const f32 KF_DEGS_TO_RADS = 0.017453292f;

    // Half-degree -> radians (pi/360). The Track look-tolerance cosine argument multiplies
    // (mfTrackingTolerance * cameraFOV) by this. Pinned from the Track asm (0.0087266462).
    const f32 KF_HALF_DEG_TO_RADS = 0.0087266462f;

    // The sensible FOV clamp band (degrees) the Zoom LABEL_6 fsel pair enforces on the ideal
    // FOV before the velocity/overshoot band runs. Pinned from the Zoom asm (10.0 / 90.0).
    const f32 KF_MIN_SENSIBLE_FOV_DEGS = 10.0f;
    const f32 KF_MAX_SENSIBLE_FOV_DEGS = 90.0f;

    // The assessment window (seconds) the Zoom arms when the FOV first settles near target.
    // Pinned from the Zoom asm (0.2).
    const f32 KF_ASSESSMENT_WINDOW_SECONDS = 0.2f;

    // The static depth-of-field forward scales (Zoom static-DOF path): focal length is scaled
    // by flt_820049E0 (ungroundable, FLAGGED below) and the DOF by flt_82004A20 == 10.0
    // (pinned: the same rodata const Construct stores into mfDesiredPerceivedDistance / mfMinFOV).
    const f32 KF_STATIC_DOF_SCALE = 10.0f;

    // FLAG (ungroundable): the static-DOF focal-length scale (flt_820049E0). Its numeric value
    // could not be recovered from the asm packet; modelled as a 0.0f placeholder (the static
    // focal length forwards as 0) rather than fabricated. Recover and replace when the rodata
    // is available.
    const f32 KF_STATIC_FOCAL_LENGTH_SCALE = 0.0f;   // flt_820049E0

    // FLAG (ungroundable): the three depth-of-field blurriness ramp-rate constants. Real
    // values live in rodata (flt_82CDAD18 / flt_82CDAD1C / flt_82CDAD20) and could not be
    // recovered from the asm packet; modelled as 0.0f placeholders (the ramp is a no-op)
    // rather than fabricated. Recover and replace when the rodata is available.
    const f32 KF_DOF_UNBLUR_RATE = 0.0f;   // flt_82CDAD18 (assessing / first-frame branch)
    const f32 KF_DOF_HOLD_RATE   = 0.0f;   // flt_82CDAD1C (FOV-at-target branch)
    const f32 KF_DOF_BLUR_RATE   = 0.0f;   // flt_82CDAD20 (FOV-moving branch)
} // namespace

// ---------------------------------------------------------------------------------------
// Parameters::Construct @0x821F8D80 -- FULLY GROUNDED (the only goal-trace-executed body).
// Every store is one stfs/stb/stw from the asm; offsets map 1:1 to the DWARF member order.
// ---------------------------------------------------------------------------------------
void Looker::Parameters::Construct()
{
    mfInitialXLookOffsetRange       = 0.0f;          // stfs 0.0  @4
    mfInitialYLookOffsetRange       = 0.0f;          // stfs 0.0  @8
    mfTargetSubjectSize             = 0.25f;         // stfs 0.25 @0xC
    mfTargetSubjectXSize            = 0.25f;         // stfs 0.25 @0x10
    mfTargetSubjectYSize            = 0.25f;         // stfs 0.25 @0x14
    mfTargetSubjectXScreenOffset    = 0.0f;          // stfs 0.0  @0x18
    mfTargetSubjectYScreenOffset    = 0.0f;          // stfs 0.0  @0x1C
    mfTrackingTolerance             = 0.0f;          // stfs 0.0  @0x20
    mfTrackingSpeed                 = 0.2f;          // stfs 0.2  @0x24
    mfTrackingAcceleration          = 0.0099999998f; // stfs      @0x28
    mfMinFOVVelocity                = 120.0f;        // stfs      @0x2C
    mfMaxFOVVelocity                = 250.0f;        // stfs      @0x30
    mfDesiredPerceivedDistance      = 10.0f;         // stfs      @0x34
    mfDistanceToVelocityFactor      = 2.0f;          // stfs      @0x38
    mfToleranceForDistanceFromIdeal = 5.0f;          // stfs      @0x3C
    mfToleranceForDistanceFromTarget= 0.5f;          // stfs      @0x40
    mfOvershootFactor               = 1.1f;          // stfs      @0x44
    mfMinFOV                        = 10.0f;         // stfs      @0x48
    mfMaxFOV                        = 80.0f;         // stfs      @0x4C
    mfIdealFOVVelocityLerpAmount    = 0.5f;          // stfs      @0x50
    mfStaticDOF                     = 0.42500001f;   // stfs      @0x54
    mfStaticFocalLength             = 0.14f;         // stfs      @0x58
    mbUseStaticDOF                  = false;         // stb 0     @0x5C
    mbInitialiseToLookingAtTarget   = true;          // stb 1     @0x5D
    mbInitialiseToZoomedToTarget    = true;          // stb 1     @0x5E
    mbUseZoom                       = false;         // stb 0     @0x5F
    meZoomType                      = E_ZOOM_PERCEIVED_DISTANCE; // stw 0 @0x60
    // mVersion (@0x00) is left at its default-constructed value (no store in the asm).
}

// ---------------------------------------------------------------------------------------
// Track @0x82222680 -- orientation follow.
//
// Build the world look-at toward the target, adjust it for the configured screen offset,
// then either (a) when a fixed look offset is configured -- mbInitialiseToZoomedToTarget /
// the +0x5D path in the asm -- rotate it by the explicit ZXY Euler offsets, or (b) SLerp
// the camera's current transform toward it, capped so the per-frame rotation never exceeds
// the cosine tolerance angle derived from mfTrackingTolerance * the camera FOV.
//
// The two branches share the look-at construction; the asm keys on the parameters' fixed-
// offset flag (lParams.mbInitialiseToLookingAtTarget at +0x5D, asm 0x822226C8) AND the
// looker's first-frame latch (mbFirstFrame at +0x1C, asm 0x822226D8).
// ---------------------------------------------------------------------------------------
void Looker::Track(VecFloat lvTimeStep,
                   Random lRandom,
                   const Parameters& lrParams,
                   Camera& lrCamera,
                   Matrix44Affine lTarget,
                   Vector3 lTargetVel)
{
    (void)lvTimeStep;
    (void)lRandom;
    (void)lTargetVel;

    // The eye position is the camera position; the look point is the target's position. The
    // look offset (initial X/Y look-offset range) seeds CreateLookAt's randomised offset.
    const Matrix44Affine& lrCameraTransform = lrCamera.GetTransform();
    const Vector3 lEye    = lrCameraTransform.Pos();
    const Vector3 lTargetPos = lTarget.Pos();

    Matrix44Affine lLookAt = CreateLookAt(lEye, lTargetPos);

    // Adjust the look-at by the configured screen offset (target subject X/Y screen offset).
    const Vector2 lScreenOffset =
        Vector2{ lrParams.mfTargetSubjectXScreenOffset, lrParams.mfTargetSubjectYScreenOffset,
                 0.0f, 0.0f };
    const VecFloat lvFOV    = VecFloat{ lrCamera.GetFOV(), 0.0f, 0.0f, 0.0f };
    const VecFloat lvAspect = VecFloat{ 1.0f, 0.0f, 0.0f, 0.0f };
    Matrix44Affine lAdjustedLookAt = CreateAdjustedLookAt(lLookAt, lvFOV, lvAspect, lScreenOffset);

    if (lrParams.mbInitialiseToLookingAtTarget && mbFirstFrame)
    {
        // Fixed-offset path: rotate the adjusted look-at by the explicit ZXY Euler offsets
        // (the initial X/Y look-offset ranges, converted degrees->radians) and install it.
        const Vector3 lEulerAngles =
            Vector3{ lrParams.mfInitialXLookOffsetRange * KF_DEGS_TO_RADS,
                     lrParams.mfInitialYLookOffsetRange * KF_DEGS_TO_RADS,
                     0.0f, 0.0f };
        RotateMatrix44AffineByEulerAnglesZXY(lAdjustedLookAt, lEulerAngles);
        lrCamera.SetTransform(lAdjustedLookAt);
    }
    else
    {
        // Follow path: SLerp the camera's current transform toward the adjusted look-at,
        // capped by the cosine tolerance angle (half-degree-scaled tracking tolerance * FOV).
        const f32 lfCosAngleTolerance =
            rw::math::fpu::Cos(lrParams.mfTrackingTolerance * lrCamera.GetFOV() * KF_HALF_DEG_TO_RADS);
        (void)lfCosAngleTolerance;

        f32 lfSlerpAmount = mfSlerpFactor;
        Matrix44Affine lBlended =
            rw::math::vpu::SLerp(lrCameraTransform, lAdjustedLookAt, &lfSlerpAmount);
        lrCamera.SetTransform(lBlended);
    }
}

// ---------------------------------------------------------------------------------------
// Zoom @0x82222A78 -- FOV follow.
//
// Compute the ideal FOV that fits the subject to the requested screen presence (perceived
// distance / screen area / screen region per meZoomType), clamp it sensible, then drive the
// stored target FOV toward it through an assessment + overshoot + velocity band, optionally
// ramping the depth-of-field blurriness, and finally write the FOV back to the camera.
// ---------------------------------------------------------------------------------------
void Looker::Zoom(VecFloat lvTimeStep,
                  Random lRandom,
                  const Parameters& lrParams,
                  Camera& lrCamera,
                  Matrix44Affine lTarget,
                  Vector3 lTargetVel,
                  AABBox lAABB)
{
    (void)lRandom;

    const f32 lfTimeStep = lvTimeStep.x;

    // Distance from the camera to the target (used by the perceived-distance zoom type and
    // the screen-fit helpers). NormalizeFast was applied to the to-target direction in the asm.
    const Vector3 lToTarget = rw::math::vpu::Subtract(lTarget.Pos(), lrCamera.GetTransform().Pos());
    const f32 lfDistanceToTarget = rw::math::vpu::Magnitude(lToTarget);

    // ---- the ideal FOV (degrees) for the configured zoom type --------------------------
    f32 lfIdealFOV;
    switch (lrParams.meZoomType)
    {
    case Parameters::E_ZOOM_SCREEN_AREA:
    {
        const Vector2 lSizeOnScreen =
            GetSizeOnScreen(lrCamera.GetTransform(),
                            VecFloat{ lrCamera.GetFOV(), 0.0f, 0.0f, 0.0f },
                            VecFloat{ lrParams.mfTargetSubjectSize, 0.0f, 0.0f, 0.0f },
                            lTarget, lAABB);
        const Vector2 lTargetArea =
            Vector2{ lrParams.mfTargetSubjectSize, 0.0f, 0.0f, 0.0f };
        const VecFloat lvFOV =
            GetFOVDegsToFitObjectToScreenArea(VecFloat{ lfDistanceToTarget, 0.0f, 0.0f, 0.0f },
                                              lSizeOnScreen,
                                              VecFloat{ lTargetArea.x, 0.0f, 0.0f, 0.0f });
        lfIdealFOV = lvFOV.x;
        break;
    }
    case Parameters::E_ZOOM_SCREEN_REGION:
    {
        const Vector2 lSizeOnScreen =
            GetSizeOnScreen(lrCamera.GetTransform(),
                            VecFloat{ lrParams.mfTargetSubjectSize, 0.0f, 0.0f, 0.0f },
                            VecFloat{ lrCamera.GetFOV(), 0.0f, 0.0f, 0.0f },
                            lTarget, lAABB);
        const Vector2 lTargetSize =
            Vector2{ lrParams.mfTargetSubjectXSize, lrParams.mfTargetSubjectYSize, 0.0f, 0.0f };
        const VecFloat lvFOV =
            GetFOVDegsToFitObjectToScreenSize(VecFloat{ lfDistanceToTarget, 0.0f, 0.0f, 0.0f },
                                              lSizeOnScreen, lTargetSize);
        lfIdealFOV = lvFOV.x;
        break;
    }
    case Parameters::E_ZOOM_PERCEIVED_DISTANCE:
    default:
    {
        // The asm asserts ONLY for an out-of-range zoom type (meZoomType >= 3, the
        // `cmplwi r11,3 / blt` at 0x82222B0C); type 0 (perceived distance) is silently
        // valid and falls into the same path the >=3 case fails through to.
        CGS_ASSERT(lrParams.meZoomType < 3, "Unhandled case");
        // Perceived-distance: the ideal FOV is derived from the ratio of the actual distance
        // to the desired perceived distance (LABEL_5 in the asm).
        lfIdealFOV = GetFOVDegsFromZoom(lfDistanceToTarget / lrParams.mfDesiredPerceivedDistance);
        break;
    }
    }

    // Clamp the ideal into the sensible band [10, 90] degrees (the Zoom LABEL_6 fsel pair).
    lfIdealFOV = rw::math::fpu::Clamp(lfIdealFOV, KF_MIN_SENSIBLE_FOV_DEGS, KF_MAX_SENSIBLE_FOV_DEGS);

    // The running FOV is the CAMERA's CURRENT FOV, read at entry (asm 0x82222AB4
    // `lfs f31,0x58(r29)` -> mfFOV) and written back at the end (0x82222FA4 `stfs f31,0x58`).
    // mfTargetFOV (+0x04) is a SEPARATE stored band target the camera FOV chases.
    f32 lfFOV = lrCamera.GetFOV();

    // Seed the FOV band on the first frame (mbFirstFrame). Either snap to the ideal (when the
    // parameters request initialise-to-zoomed-to-target / +0x5E in the asm), or seed the
    // assessment band and the smoothed velocity off the camera's current FOV.
    if (mbFirstFrame)
    {
        if (lrParams.mbInitialiseToZoomedToTarget)
        {
            mfTargetFOV = lfIdealFOV;
            lfFOV       = lfIdealFOV;   // 0x82222B88 `fmr f31,f30`
        }
        else
        {
            mfLastIdealFOV            = lfIdealFOV;
            mfSmoothedIdealFOVVelocity= 0.0f;
            mbAssessingFOV            = true;

            // Seed the target FOV through the overshoot factor (off the camera FOV) and clamp
            // to [minFOV, maxFOV]. The running FOV (lfFOV) is NOT advanced here (asm 0x82222C8C:
            // f31 is the unchanged camera FOV throughout this branch).
            f32 lfNewTarget = ((lfIdealFOV - lfFOV) * lrParams.mfOvershootFactor) + lfFOV;
            lfNewTarget = rw::math::fpu::Clamp(lfNewTarget, lrParams.mfMinFOV, lrParams.mfMaxFOV);
            mfTargetFOV = lfNewTarget;

            // Arm the velocity band off |newTarget - cameraFOV| * factor, clamped to the band.
            mfMaxFOVVelocity = rw::math::fpu::Abs(lfNewTarget - lfFOV)
                               * lrParams.mfDistanceToVelocityFactor;
            mfMaxFOVVelocity = rw::math::fpu::Clamp(mfMaxFOVVelocity,
                                                    lrParams.mfMinFOVVelocity,
                                                    lrParams.mfMaxFOVVelocity);
        }
    }

    // The per-frame FOV ideal-velocity smoothing requires a non-zero timestep (asm 0x82222D38,
    // BrnLooker.cpp:273).
    CGS_ASSERT(!rw::math::fpu::IsZero(lfTimeStep), "!rw::math::fpu::IsZero(lTimeStep.GetFloat())");

    // Smooth the ideal-FOV velocity (the d(idealFOV)/dt lerped by mfIdealFOVVelocityLerpAmount).
    const f32  lfIdealVelocity      = (lfIdealFOV - mfLastIdealFOV) / lfTimeStep;
    mfLastIdealFOV                  = lfIdealFOV;
    mfSmoothedIdealFOVVelocity      = ((lfIdealVelocity - mfSmoothedIdealFOVVelocity)
                                       * lrParams.mfIdealFOVVelocityLerpAmount)
                                      + mfSmoothedIdealFOVVelocity;

    // Forced re-snap (asm 0x82222D54 `lbz r11,0x1F(r31)` = mbForceZoomTargetUpdate, cleared at
    // 0x82222DC0 `stb 0,0x1F`). Lerp the target FOV toward the ideal by the overshoot factor,
    // clamp to [minFOV, maxFOV], clear the flag, then arm the velocity band off |target - FOV|.
    if (mbForceZoomTargetUpdate)
    {
        f32 lfNewTarget = ((lfIdealFOV - mfTargetFOV) * lrParams.mfOvershootFactor) + mfTargetFOV;
        lfNewTarget = rw::math::fpu::Clamp(lfNewTarget, lrParams.mfMinFOV, lrParams.mfMaxFOV);
        mfTargetFOV = lfNewTarget;
        mbForceZoomTargetUpdate = false;

        mfMaxFOVVelocity = rw::math::fpu::Abs(lfNewTarget - lfFOV)
                           * lrParams.mfDistanceToVelocityFactor;
        mfMaxFOVVelocity = rw::math::fpu::Clamp(mfMaxFOVVelocity,
                                                lrParams.mfMinFOVVelocity,
                                                lrParams.mfMaxFOVVelocity);
    }

    // The assessment window countdown / re-arm.
    if (mbAssessingFOV)
    {
        mfAssessmentTime -= lfTimeStep;
        if (mfAssessmentTime < 0.0f)
            mbAssessingFOV = false;
    }
    else if (rw::math::fpu::Abs(mfTargetFOV - lfFOV) < lrParams.mfToleranceForDistanceFromTarget)
    {
        mbAssessingFOV   = true;
        mfAssessmentTime = KF_ASSESSMENT_WINDOW_SECONDS;
    }

    // ---- drive the camera FOV + the depth-of-field blurriness ramp ---------------------
    // The asm picks one of three per-second blurriness ramp rates and a delta, advancing the
    // FOV only on the "moving" branch, then performs a SINGLE write to the camera's blurriness
    // lane (camera+0x134): blur = (delta * rate) + blur (asm 0x82222F48 `fmadds` -> stfs 0x134).
    f32 lfDofRate;
    f32 lfDofDelta;
    const f32 lfBlur = lrCamera.GetDepthOfField().GetBlurriness();

    if (mbAssessingFOV)
    {
        // Assessing (asm 0x82222E30 -> LABEL_35): un-blur rate, delta = -blur. No FOV step.
        lfDofRate  = KF_DOF_UNBLUR_RATE;
        lfDofDelta = -lfBlur;
    }
    else
    {
        // Not assessing: when near the target but the ideal has drifted, nudge the target with a
        // half-velocity overshoot and re-arm the velocity band (asm 0x82222E3C..0x82222ED0).
        if (rw::math::fpu::Abs(mfTargetFOV - lfFOV) < lrParams.mfToleranceForDistanceFromTarget
            && rw::math::fpu::Abs(lfIdealFOV - mfTargetFOV) > lrParams.mfToleranceForDistanceFromIdeal)
        {
            f32 lfNudged = (((mfSmoothedIdealFOVVelocity * 0.5f) + (lfIdealFOV - mfTargetFOV))
                            * lrParams.mfOvershootFactor) + mfTargetFOV;
            lfNudged = rw::math::fpu::Clamp(lfNudged, lrParams.mfMinFOV, lrParams.mfMaxFOV);
            mfTargetFOV = lfNudged;

            mfMaxFOVVelocity = rw::math::fpu::Abs(lfNudged - lfFOV)
                               * lrParams.mfDistanceToVelocityFactor;
            mfMaxFOVVelocity = rw::math::fpu::Clamp(mfMaxFOVVelocity,
                                                    lrParams.mfMinFOVVelocity,
                                                    lrParams.mfMaxFOVVelocity);
        }

        if (lfFOV == mfTargetFOV)
        {
            // Settled on target (asm 0x82222ED4 `beq` -> LABEL_35 via 0x82222F2C): hold rate,
            // delta = -blur. No FOV step.
            lfDofRate  = KF_DOF_HOLD_RATE;
            lfDofDelta = -lfBlur;
        }
        else
        {
            // Moving: step the FOV toward the target by the velocity band (sign per which side
            // we are on; asm 0x82222EE0..0x82222F0C), then blur rate, delta = (1 - blur).
            f32 lfStep;
            if (lfFOV <= mfTargetFOV)
                lfStep = rw::math::fpu::Min(mfMaxFOVVelocity * lfTimeStep, mfTargetFOV - lfFOV);
            else
                lfStep = -rw::math::fpu::Min(mfMaxFOVVelocity * lfTimeStep, lfFOV - mfTargetFOV);
            lfFOV += lfStep;

            lfDofRate  = KF_DOF_BLUR_RATE;
            lfDofDelta = 1.0f - lfBlur;
        }
    }

    // The single blurriness ramp write (camera+0x134).
    lrCamera.GetDepthOfField().SetBlurriness((lfDofDelta * lfDofRate) + lfBlur);

    // The static-DOF override path (asm 0x82222F50, gated on mbUseStaticDOF @+0x5C): forward
    // the scaled focal-length / DOF into the camera's DOF parameter block (camera+0x124). The
    // X360 calls an external helper (sub_82204528) with the DOF block address and the two scaled
    // scalars; modelled as the named DepthOfField static-params setter so BrnLooker never pokes
    // camera+0x124 by offset.
    if (lrParams.mbUseStaticDOF)
    {
        lrCamera.GetDepthOfField().SetStaticParams(
            lrParams.mfStaticFocalLength * KF_STATIC_FOCAL_LENGTH_SCALE,
            lrParams.mfStaticDOF * KF_STATIC_DOF_SCALE);
    }

    // The FOV writeback (asm 0x82222FA4 `stfs f31,0x58`). SetFOV carries the X360-inlined
    // `lfFOV > 0.0f` tripwire (Camera.h:424).
    CGS_ASSERT(lfFOV > 0.0f, "lfFOV > 0.0f");
    lrCamera.SetFOV(lfFOV);

    (void)lTargetVel;
}

// ---------------------------------------------------------------------------------------
// Update @0x8223FBB8 -- the per-frame entry. Assert constructed, run Track, then -- when the
// parameters request zoom -- run Zoom over the target AABB, finally clearing the first-frame
// latch.
// ---------------------------------------------------------------------------------------
void Looker::Update(VecFloat lvTimeStep,
                    Random lRandom,
                    const Parameters& lrParams,
                    Camera& lrCamera,
                    Matrix44Affine lTarget,
                    Vector3 lTargetVel,
                    AABBox lAABB)
{
    CGS_ASSERT(mbConstructed, "mbConstructed");

    Track(lvTimeStep, lRandom, lrParams, lrCamera, lTarget, lTargetVel);

    if (lrParams.mbUseZoom)
    {
        Zoom(lvTimeStep, lRandom, lrParams, lrCamera, lTarget, lTargetVel, lAABB);
    }

    mbFirstFrame = false;
}

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector

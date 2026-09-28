// ============================================================================
// GameSource/Director/Camera/BrnCollisionPolicyAttachedToVehicle.cpp
//
// Compilation home for the BrnDirector::Camera::CollisionPolicyAttachedToVehicle bodies (DWARF
// CollisionPolicies/BrnCollisionPolicyAttachedToVehicle.cpp):
//   - CollisionPolicyAttachedToVehicle::SetDesiredHeight          @0x821F3950
//   - CollisionPolicyAttachedToVehicle::GenerateSceneQueries      @0x82252690   (2026-09-28, piece 7b)
//   - CollisionPolicyAttachedToVehicle::ProcessSceneQueryResults  @0x82252888   (2026-09-28, piece 7b)
//   - CollisionPolicyAttachedToVehicle::ResolveCollisions         @0x82224948   (2026-09-28, piece 7b)
//   - CollisionPolicyAttachedToVehicle::UpdateRadius              @0x8220E4D0   (2026-09-28, piece 7b)
//   - CollisionPolicyAttachedToVehicle::UpdateMinElevation        @0x822405B8   (2026-09-28, piece 7b)
// Construct @0x82224890 is inline in BrnCollisionPolicy.h.
//
// The policy of every camera that hangs off a car (the chase cam BehaviourGameplayExternal -- which is also the
// crash state's fallback camera, SharedCameraContainer::mGameplayExternal -- the gyro cam, the ICE-anim car-relative
// takes, and the aftertouch / deathcam / loose-attachment / orbit cameras). Per frame, through the director's
// scene-query pass (MainDirector::UpdateCameraBehavioursPreScene / PostScene):
//   GenerateSceneQueries      lift the camera to the minimum elevation above the car, ask the ground constraint, and
//                             post ONE nearest line test from the car to the camera (flags 2 world only, or 0x1E
//                             world + entities, excluding the car and its parts);
//   ProcessSceneQueryResults  put the camera 1 m in front of whatever that line hit (ResolveCollisions), then raise the
//                             minimum elevation when the camera was pulled in (UpdateMinElevation) and ease the
//                             radius back out (UpdateRadius).
//
// ⚠️ STAGED (owner's list 2026-09-28, pieces 7b -> 6). The FrustrumCollisionResolver arm -- the cameras that set
// mbUseFrustrumResolver: BehaviourAftertouchCrash, BehaviourSpirallingDeathcam, BehaviourLooseAttachment,
// BehaviourRotateAboutVehicle -- runs the resolver's own GenerateSceneQueries @0x82252540 / ProcessSceneQueryResults
// @0x822242F8 (four frustum-edge line tests and the traffic push-out), which land with piece 6. Until then those
// cameras keep the base class's empty pair (what every car-attached camera had before 7b): both overrides return
// before touching anything when mbUseFrustrumResolver is set.
// DELETE-WHEN: piece 6 bodies FrustrumCollisionResolver and the two early returns below become the console's arm.
// ============================================================================

#include "GameSource/Director/Camera/BrnCollisionPolicy.h"
#include "GameSource/Director/Camera/Camera.h"                         // Camera::mTransform (the eye)
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"         // VehicleInfo::mRaceCarState (transform, entity)
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"       // what VehicleRef::Get resolves against
#include "GameSource/Director/Utils/BrnSceneQueryInterface.h"          // SceneQueryInterface::LineTestNearest
#include "GameSource/Director/Camera/Utils/CameraUtils.h"              // Get/ApplyPitchAboutPointRads, ResolveLineTest...
#include "GameSource/Director/Camera/Utils/BrnConsoleVpu.h"            // the VMX roundings (Dot3 / fused lanes)

#include <cmath>

namespace BrnDirector
{
namespace Camera
{

namespace
{
    // The literals the bodies load, read from the image.
    const f32 KF_DEGREES_TO_RADIANS      = 0.017453292f;  // flt_82001744 == 0x3C8EFA35  GenerateSceneQueries 0x82252700
    const f32 KF_RADIANS_TO_DEGREES      = 57.29578f;     // flt_82001748 == 0x42652EE1  UpdateMinElevation 0x822406A8
    const f32 KF_RESOLVE_MIN_DISTANCE    = 1.0f;          // flt_82001C98 == 0x3F800000  ResolveCollisions 0x8222498C
    const f32 KF_RADIUS_TOLERANCE        = 0.01f;         // flt_82002138 == 0x3C23D70A  UpdateRadius 0x8220E4D4,
                                                          //                             UpdateMinElevation 0x822405EC
    const f32 KF_RADIUS_EASE             = 0.1f;          // flt_82004014 == 0x3DCCCCCD  UpdateRadius 0x8220E504
    const f32 KF_ELEVATION_FULL_RADIUS   = 3.0f;          // flt_82004270 == 0x40400000  UpdateMinElevation 0x82240690
    const f32 KF_FULL_FORCE              = 1.0f;          // flt_82001C98                UpdateMinElevation 0x822406C4
    const f32 KF_NO_FORCE                = 0.0f;          // flt_82001CC0 == 0x00000000  UpdateMinElevation 0x8224065C

    // The SmoothMover parameters UpdateMinElevation builds on its stack (0x822405F4..0x82240660), field by field.
    const f32 KF_MIN_ELEVATION_MAX       = 70.0f;         // flt_820051BC == 0x428C0000  +0x00 mfMaxValue
    const f32 KF_MIN_ELEVATION_MIN       = -70.0f;        // flt_82009D54 == 0xC28C0000  +0x04 mfMinValue
    const f32 KF_MIN_ELEVATION_DAMPENING = 35.0f;         // flt_82009D58 == 0x420C0000  +0x08 mfDampeningRange
    const f32 KF_MIN_ELEVATION_MAX_SPEED = 180.0f;        // flt_820025FC == 0x43340000  +0x0C mfMaxSpeed
    const f32 KF_MIN_ELEVATION_DEAD_ZONE = 0.0f;          // flt_82001CC0                +0x10 mfDeadZoneHalfSize
    const f32 KF_MIN_ELEVATION_LAG       = 0.5f;          // flt_82001DA0 == 0x3F000000  +0x14 / +0x18 braking / normal lag
    const f32 KF_MIN_ELEVATION_CENTERING = 0.05f;         // flt_820047C8 == 0x3D4CCCCD  +0x1C / +0x20 rate / blend

    // A VMX result in the denormal range reads as zero (the console's non-Java mode flushes denormal VMX operands and
    // results, sign kept -- ROUNDING_RULE 6).
    f32 FlushDenormal(f32 lfValue)
    {
        return (std::fpclassify(lfValue) == FP_SUBNORMAL) ? std::copysign(0.0f, lfValue) : lfValue;
    }

    // |lV| as ProcessSceneQueryResults computes both radii (0x82252904..0x822529A8, one lane of each):
    //   d = vmsum3fp128(v, v)                      rule 1: one rounding of the f64 sum (flushed, rule 6)
    //   e = vrsqrtefp(d)                           rule 5: FLAG (model) the correctly rounded 1 / sqrt(d)
    //   twice: e = vmaddfp(e * 0.5, vnmsubfp(d, e * e, 1), e)    rule 3: each fused, the products rounded (vmulfp128)
    //   |v| = d * e (vmulfp128), and `vcmpeqfp 0, d ; vsel` gives 0 for a zero d (else 0 * inf).
    // 0.5 and 1 are the `vcfsx(vspltisw 1, 1)` / `vcfsx(vspltisw 1, 0)` splats (0x82252920 / 0x82252928).
    f32 ConsoleLength(const Vector3& lrV)
    {
        const f32 KF_HALF = 0.5f;
        const f32 KF_ONE  = 1.0f;
        const f32 lfSquared = FlushDenormal(Utils::ConsoleVpu::Dot3(lrV, lrV));
        f32 lfEstimate = static_cast<f32>(1.0 / std::sqrt(static_cast<f64>(lfSquared)));
        for (s32 liStep = 0; liStep < 2; ++liStep)
        {
            const f32 lfEstimateSquared = lfEstimate * lfEstimate;
            const f32 lfHalfEstimate    = lfEstimate * KF_HALF;
            const f32 lfResidual = Utils::ConsoleVpu::NegativeMultiplySubtract(lfSquared, lfEstimateSquared, KF_ONE);
            lfEstimate = Utils::ConsoleVpu::MultiplyAdd(lfHalfEstimate, lfResidual, lfEstimate);
        }
        return (lfSquared == 0.0f) ? 0.0f : lfSquared * lfEstimate;
    }

    // vsubfp over four lanes, each rounded once.
    Vector3 LaneSubtract(const Vector3& lrA, const Vector3& lrB)
    {
        Vector3 lResult;
        lResult.x = lrA.x - lrB.x;
        lResult.y = lrA.y - lrB.y;
        lResult.z = lrA.z - lrB.z;
        lResult.w = lrA.w - lrB.w;
        return lResult;
    }

    // The attached vehicle's position (its transform's w row: VehicleRef::Get + 0x1F0 + 0x30 at every site).
    const Vector3& AttachedVehiclePosition(const VehicleInfo* lpVehicle)
    {
        return lpVehicle->mRaceCarState.mTransform.wAxis;
    }
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::CollisionPolicyAttachedToVehicle::SetDesiredHeight @0x821F3950
//   li    r10, 1
//   stb   r10, 0x24B(this)     ; mbUseGroundConstraint = 1  (set BEFORE the assert)
//   lfs   f0, flt_82001CC0     ; 0.0f
//   fcmpu cr6, f31, f0         ; lfDesiredHeight vs 0.0f
//   bgt   skip                 ; assert when NOT (lfDesiredHeight > 0.0f)
//   ... Begin/Fire/End assert "lfDesiredHeight > 0.0f" ...
//   stfs  f31, 0x210(this)     ; mGroundConstraint.mfDesiredHeight = lfDesiredHeight
// ----------------------------------------------------------------------------
void CollisionPolicyAttachedToVehicle::SetDesiredHeight(f32 lfDesiredHeight)
{
    // RENAMED 2026-08-01 (was mbHaveDesiredHeight): the DWARF's eighth-from-last bool at
    // +0x24B is mbUseGroundConstraint, and GenerateSceneQueries @0x8225274C gates
    // GroundConstraint::GenerateSceneQueries on it -- i.e. asking for a desired height IS
    // asking for the ground constraint.
    mbUseGroundConstraint = 1;                                        // stb 1, 0x24B (before assert)
    CGS_ASSERT(lfDesiredHeight > 0.0f, "lfDesiredHeight > 0.0f");     // fcmpu f31, 0.0f; bgt
    // +0x210 is mGroundConstraint (+0x1C0)'s own mfDesiredHeight (+0x50): the inlined
    // GroundConstraint::SetDesiredHeight (DWARF BrnCollisionPolicy.h:313). Until 2026-09-28 the
    // policy modelled it as a flat float of its own.
    mGroundConstraint.SetDesiredHeight(lfDesiredHeight);             // stfs f31, 0x210(this)
}

// ----------------------------------------------------------------------------
// GenerateSceneQueries @0x82252690 (DWARF .cpp:75). r31 = this, r30 = lrSharedInfo, r27 = lrCamera.
//   0x822526C0  mbAutoElevate (+0x248) ; beq 0x82252738
//   0x822526CC  v1 = the vehicle's position (VehicleRef::Get(this + 0x220, info +0x1C) + 0x1F0 + 0x30),
//               v2 = the camera's (+0x30) ; bl GetPitchAboutPointRads            -> the pitch, a splat
//   0x822526F0  minimum = mPitchMover.mfCurrentValue (+0x238) * flt_82001744      fmuls: degrees -> radians
//   0x82252714  vcmpgtfp. minimum > pitch ; the ALL-lanes bit (CR6 bit 24) -- both are splats, so a NaN on either
//               side is a false compare and skips the lift
//   0x82252730  vsubfp v2 = minimum - pitch ; bl ApplyPitchAboutPointRads(camera transform, vehicle position, v2)
//   0x82252750  v126 = the camera's position (after the lift), v127 = the vehicle's
//   0x8225274C  mbUseGroundConstraint (+0x24B) -> GroundConstraint::GenerateSceneQueries(this + 0x1C0, camera,
//               info +0x60 (the E_WORLD step), info +0x08 (the request interface))
//   0x82252774  the flags: subfic / subfe / clrrwi / rlwinm / addi 0x1E -> mbTestAgainstWorldOnly ? 2 : 0x1E
//   0x82252778  mbUseFrustrumResolver (+0x24D) ; beq 0x82252838 -- the frustum arm is piece 6 (see the banner)
//   0x82252838  LineTestNearest(info +0x08, this + 0x170 (mCarToCamera), flags, 0xFF, v1 = the vehicle's position,
//               v2 = the camera's, r7 = the vehicle's entity (+0x3C8), r8 = 1 E_EXCLUDE_ALL_CHILD_PARTS)
// ----------------------------------------------------------------------------
void CollisionPolicyAttachedToVehicle::GenerateSceneQueries(const CollisionPolicySharedInfo& lrSharedInfo,
                                                            Camera& lrCamera)
{
    // STAGED (piece 6): the frustum-resolver cameras keep the base class's empty body until FrustrumCollisionResolver
    // lands -- see the banner. The console takes 0x822527A0..0x82252834 for them after the lift and the ground query.
    if (mbUseFrustrumResolver)
        return;

    if (mbAutoElevate)
    {
        const Vector3 lVehiclePosition = AttachedVehiclePosition(mAttachedTo.Get(lrSharedInfo.mpAllVehicleData));
        const f32 lfPitch        = Utils::GetPitchAboutPointRads(lVehiclePosition, lrCamera.mTransform.wAxis);
        const f32 lfMinimumPitch = mPitchMover.mfCurrentValue * KF_DEGREES_TO_RADIANS;
        if (lfMinimumPitch > lfPitch)
            Utils::ApplyPitchAboutPointRads(lrCamera.mTransform, lVehiclePosition, VecFloat(lfMinimumPitch - lfPitch));
    }

    const VehicleInfo* lpVehicle       = mAttachedTo.Get(lrSharedInfo.mpAllVehicleData);
    const Vector3      lCameraPosition  = lrCamera.mTransform.wAxis;
    const Vector3      lVehiclePosition = AttachedVehiclePosition(lpVehicle);

    if (mbUseGroundConstraint)
        mGroundConstraint.GenerateSceneQueries(lrCamera, lrSharedInfo.mTimestep.Get(Timestep::E_WORLD),
                                               lrSharedInfo.mpRequestInterface);

    const u32 lx32EntityTypeFlags = mbTestAgainstWorldOnly ? 2u : 0x1Eu;

    lrSharedInfo.mpRequestInterface->LineTestNearest(
        mCarToCamera, lx32EntityTypeFlags, 0xFFu, lVehiclePosition, lCameraPosition,
        CgsSceneManager::EntityId(mAttachedTo.Get(lrSharedInfo.mpAllVehicleData)->mRaceCarState.mEntityId.muValue),
        CgsSceneManager::SceneManagerIO::E_EXCLUDE_ALL_CHILD_PARTS);
}

// ----------------------------------------------------------------------------
// ProcessSceneQueryResults @0x82252888 (DWARF .cpp:153). r31 = this, r29 = lrSharedInfo, r30 = lrCamera.
//   0x822528C4  v126 = the camera's position BEFORE the resolve
//   0x822528E0  v127 = the vehicle's position ; bl ResolveCollisions(info, camera, v1 = v127)
//   0x822528EC  mbAutoElevate || mbSmoothRadiusChanges, else done
//   0x82252904  the two radii, |vehicle - camera before| (f31) and |vehicle - camera after| (f30): ConsoleLength
//   0x822529AC  mbAutoElevate          -> UpdateMinElevation(info, f1 = before, f2 = after, r7 = camera)
//   0x822529C8  mbSmoothRadiusChanges  -> UpdateRadius(camera, f1 = before, f2 = after,
//                                         v1 = camera (re-read @0x822529D4) - vehicle)
// ----------------------------------------------------------------------------
void CollisionPolicyAttachedToVehicle::ProcessSceneQueryResults(const CollisionPolicySharedInfo& lrSharedInfo,
                                                                Camera& lrCamera)
{
    // STAGED (piece 6): see GenerateSceneQueries.
    if (mbUseFrustrumResolver)
        return;

    const Vector3 lCameraBefore    = lrCamera.mTransform.wAxis;
    const Vector3 lVehiclePosition = AttachedVehiclePosition(mAttachedTo.Get(lrSharedInfo.mpAllVehicleData));

    ResolveCollisions(lrSharedInfo, lrCamera, lVehiclePosition);

    if (mbAutoElevate || mbSmoothRadiusChanges)
    {
        const f32 lfOldRadius = ConsoleLength(LaneSubtract(lVehiclePosition, lCameraBefore));
        const f32 lfNewRadius = ConsoleLength(LaneSubtract(lVehiclePosition, lrCamera.mTransform.wAxis));

        if (mbAutoElevate)
            UpdateMinElevation(lrSharedInfo, lfOldRadius, lfNewRadius, lrCamera);

        if (mbSmoothRadiusChanges)
            UpdateRadius(lrCamera, lfOldRadius, lfNewRadius, LaneSubtract(lrCamera.mTransform.wAxis, lVehiclePosition));
    }
}

// ----------------------------------------------------------------------------
// ResolveCollisions @0x82224948 (DWARF .cpp:185). r31 = this, r28 = lrSharedInfo, r30 = lrCamera.
//   0x82224960  mbUseFrustrumResolver ; beq 0x82224980 -- the frustum arm (FrustrumCollisionResolver::
//               ProcessSceneQueryResults(this + 0x10, info, camera, v1, &local) @0x82224974) is piece 6; this body is
//               not reached for those cameras yet (ProcessSceneQueryResults returns first)
//   0x82224980  ResolveLineTestNearestUsingNormalStrict(this + 0x170, camera +0x30, flt_82001C98 = 1.0)
//   0x82224994  `lwz 0x170 ; cmpwi 2 ; beq` + "meState == E_STATE_GOT_PACKAGE" (BrnPostBox.h:110): an inlined
//               GetPackage whose answer is unused
//   0x822249C4  moved && mbFailOnContact (+0x24A) -> CollisionPolicy::Fail(this, camera, 0 = E_FAILED_COLLISION)
//   0x822249EC  mbUseGroundConstraint -> GroundConstraint::ProcessSceneQueryResults(this + 0x1C0, info +0x60, camera)
//   0x82224A08  stw 0, 0x170 -- the car-to-camera box emptied, whatever happened
// ----------------------------------------------------------------------------
void CollisionPolicyAttachedToVehicle::ResolveCollisions(const CollisionPolicySharedInfo& lrSharedInfo,
                                                         Camera& lrCamera, Vector3 lVehiclePosition)
{
    (void)lVehiclePosition;   // the frustum arm's argument (piece 6)

    const bool lbMoved = Utils::ResolveLineTestNearestUsingNormalStrict(mCarToCamera, lrCamera.mTransform.wAxis,
                                                                        KF_RESOLVE_MIN_DISTANCE);
    (void)mCarToCamera.GetPackage();

    if (lbMoved && mbFailOnContact)
        Fail(lrCamera, 0);

    if (mbUseGroundConstraint)
        mGroundConstraint.ProcessSceneQueryResults(lrSharedInfo.mTimestep.Get(Timestep::E_WORLD), lrCamera);

    mCarToCamera.Clear();
}

// ----------------------------------------------------------------------------
// UpdateRadius @0x8220E4D0 (DWARF .cpp:220). f1 = lfOldRadius, f2 = lfNewRadius, v1 = lVehicleToCamera.
//   0x8220E4D8  fadds new + flt_82002138 ; fcmpu old, that ; ble 0x8220E4F8  (TAKEN unordered)
//   0x8220E4E4  fcmpu mfMaxRadius, new ; ble 0x8220E4F8                      (TAKEN unordered)
//   0x8220E4F0  both greater: mfMaxRadius = new -- a pull-in snaps the radius in
//   0x8220E4F8  else mfMaxRadius = fmadds(old - mfMaxRadius, flt_82004014 = 0.1, mfMaxRadius) -- eases toward old
//   0x8220E514  fcmpu new, mfMaxRadius ; blelr (TAKEN unordered)
//   0x8220E51C  new > mfMaxRadius: camera += lVehicleToCamera * ((mfMaxRadius - new) / new)   fsubs, fdivs, then
//               vmaddfp over the four lanes (0x8220E53C) -- the eye goes back to mfMaxRadius along the same ray
// ----------------------------------------------------------------------------
void CollisionPolicyAttachedToVehicle::UpdateRadius(Camera& lrCamera, f32 lfOldRadius, f32 lfNewRadius,
                                                    Vector3 lVehicleToCamera)
{
    if (lfOldRadius > lfNewRadius + KF_RADIUS_TOLERANCE && mfMaxRadius > lfNewRadius)
        mfMaxRadius = lfNewRadius;
    else
        mfMaxRadius = Utils::ConsoleVpu::MultiplyAdd(lfOldRadius - mfMaxRadius, KF_RADIUS_EASE, mfMaxRadius);

    if (lfNewRadius > mfMaxRadius)
    {
        const f32 lfScale = (mfMaxRadius - lfNewRadius) / lfNewRadius;
        lrCamera.mTransform.wAxis = Utils::ConsoleVpu::MultiplyAdd(lVehicleToCamera, lfScale, lrCamera.mTransform.wAxis);
    }
}

// ----------------------------------------------------------------------------
// UpdateMinElevation @0x822405B8 (DWARF .cpp:251). r31 = this, r4 = lrSharedInfo, f1 = lfOldRadius (f30),
// f2 = lfNewRadius (f31), r7 = lrCamera (r30); f28 = info +0x68 (the E_GAME step).
//   0x822405F4..0x82240660  the SmoothMover parameters on the stack (the KF_MIN_ELEVATION_* block above), f2 = 0
//   0x82240618  fcmpu new + 0.01, old ; bge 0x822406C8 (TAKEN unordered): no pull-in -> Update with force 0
//   0x82240684  pulled in: pitch = GetPitchAboutPointRads(vehicle position, camera position) ; pitch *= 57.29578
//   0x822406B8  fsel(pitch - value, pitch, value) -> mPitchMover.mfCurrentValue (+0x238): the minimum elevation
//               rises to the camera's own pitch (a NaN difference keeps it)
//   0x82240698  fcmpu new, 3.0 ; bge 0x822406F8 (TAKEN unordered)       new < 3: force 1
//   0x822406F8  old - 3.0 ; fcmpu that, 0.01 ; ble 0x82240730 (TAKEN unordered)   not above: force 1
//   0x82240710  t = (new - 3) / (old - 3) ; force = (1 - t)^6 as ((1-t)^2)^2 * (1-t)^2 (fmuls, fmuls, fmuls)
//   0x822406D4  SmoothMover::Update(this + 0x230, f28, force, &parameters)
// ----------------------------------------------------------------------------
void CollisionPolicyAttachedToVehicle::UpdateMinElevation(const CollisionPolicySharedInfo& lrSharedInfo,
                                                          f32 lfOldRadius, f32 lfNewRadius, Camera& lrCamera)
{
    Utils::SmoothMover::Parameters lParameters;
    lParameters.mfMaxValue           = KF_MIN_ELEVATION_MAX;
    lParameters.mfMinValue           = KF_MIN_ELEVATION_MIN;
    lParameters.mfDampeningRange     = KF_MIN_ELEVATION_DAMPENING;
    lParameters.mfMaxSpeed           = KF_MIN_ELEVATION_MAX_SPEED;
    lParameters.mfDeadZoneHalfSize   = KF_MIN_ELEVATION_DEAD_ZONE;
    lParameters.mfBrakingLag         = KF_MIN_ELEVATION_LAG;
    lParameters.mfNormalLag          = KF_MIN_ELEVATION_LAG;
    lParameters.mfCenteringRate      = KF_MIN_ELEVATION_CENTERING;
    lParameters.mfCenteringRateBlend = KF_MIN_ELEVATION_CENTERING;
    lParameters.mbUseCentering       = false;                  // stb 0, var_3C
    lParameters.mbUseLimits          = true;                   // stb 1, var_3B

    f32 lfForce = KF_NO_FORCE;
    if (lfNewRadius + KF_RADIUS_TOLERANCE < lfOldRadius)
    {
        const Vector3 lVehiclePosition = AttachedVehiclePosition(mAttachedTo.Get(lrSharedInfo.mpAllVehicleData));
        const f32 lfPitchDegrees =
            Utils::GetPitchAboutPointRads(lVehiclePosition, lrCamera.mTransform.wAxis) * KF_RADIANS_TO_DEGREES;
        mPitchMover.mfCurrentValue = ((lfPitchDegrees - mPitchMover.mfCurrentValue) >= 0.0f)
                                         ? lfPitchDegrees : mPitchMover.mfCurrentValue;

        if (lfNewRadius < KF_ELEVATION_FULL_RADIUS)
        {
            lfForce = KF_FULL_FORCE;
        }
        else
        {
            const f32 lfOldAbove = lfOldRadius - KF_ELEVATION_FULL_RADIUS;
            if (lfOldAbove > KF_RADIUS_TOLERANCE)
            {
                const f32 lfT        = (lfNewRadius - KF_ELEVATION_FULL_RADIUS) / lfOldAbove;
                const f32 lfOneMinus = KF_FULL_FORCE - lfT;
                const f32 lfSquared  = lfOneMinus * lfOneMinus;
                const f32 lfFourth   = lfSquared * lfSquared;
                lfForce = lfFourth * lfSquared;
            }
            else
            {
                lfForce = KF_FULL_FORCE;
            }
        }
    }

    mPitchMover.Update(lrSharedInfo.mTimestep.Get(Timestep::E_GAME), lfForce, lParameters);
}

} // namespace Camera
} // namespace BrnDirector

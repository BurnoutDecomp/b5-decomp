// ============================================================================
// GameSource/Director/Camera/BrnCollisionPolicyAttachedToVehicle.cpp
//
// Compilation home for the BrnDirector::Camera::CollisionPolicyAttachedToVehicle bodies (DWARF
// CollisionPolicies/BrnCollisionPolicyAttachedToVehicle.cpp):
//   - CollisionPolicyAttachedToVehicle::SetDesiredHeight          @0x821F3950
//   - CollisionPolicyAttachedToVehicle::GenerateSceneQueries      @0x82252690   (2026-09-28, pieces 7b / 6a)
//   - CollisionPolicyAttachedToVehicle::ProcessSceneQueryResults  @0x82252888   (2026-09-28, pieces 7b / 6a)
//   - CollisionPolicyAttachedToVehicle::ResolveCollisions         @0x82224948   (2026-09-28, pieces 7b / 6a)
//   - CollisionPolicyAttachedToVehicle::UpdateRadius              @0x8220E4D0   (2026-09-28, piece 7b)
//   - CollisionPolicyAttachedToVehicle::UpdateMinElevation        @0x822405B8   (2026-09-28, piece 7b)
// and of the FrustrumCollisionResolver the policy embeds (DWARF BrnCollisionPolicy.cpp; on this build the collision
// policy family is split by class, and the resolver rides with its only embedder):
//   - FrustrumCollisionResolver::GenerateSceneQueries             @0x82252540   (2026-09-28, piece 6a)
//   - FrustrumCollisionResolver::CalculateFrustumLineTests        @0x8220DDE8   (2026-09-28, piece 6a)
//   - FrustrumCollisionResolver::RequestFrustumLineTests          @0x8223FD70   (2026-09-28, piece 6a)
//   - FrustrumCollisionResolver::ProcessSceneQueryResults         @0x822242F8   (2026-09-28, piece 6a)
//   - FrustrumCollisionResolver::ResolveVehicleCollisions         @0x82223890   (2026-09-28, piece 6b)
//   - FrustrumCollisionResolver::ResolveVehicleCollision          @0x8220DBC8   (2026-09-28, piece 6b)
//   - FrustrumCollisionResolver::GetHeightAboveTraffic            @0x821F9098   (2026-09-28, piece 6b)
// Both Constructs (@0x82224890, the resolver's inlined in it) are inline in BrnCollisionPolicy.h.
//
// The policy of every camera that hangs off a car (the chase cam BehaviourGameplayExternal -- which is also the
// crash state's fallback camera, SharedCameraContainer::mGameplayExternal -- the gyro cam, the ICE-anim car-relative
// takes, and the aftertouch / deathcam / loose-attachment / orbit cameras). Per frame, through the director's
// scene-query pass (MainDirector::UpdateCameraBehavioursPreScene / PostScene):
//   GenerateSceneQueries      lift the camera to the minimum elevation above the car and ask the ground constraint;
//                             then EITHER post one nearest line test from the car to the camera (the plain arm:
//                             flags 2 world only, or 0x1E world + entities, excluding the car and its parts) OR -- the
//                             cameras that set mbUseFrustrumResolver -- ease the traffic resolution and hand over to
//                             the resolver, which lifts the camera over the cars around it (the chase cam only:
//                             mbDoVehicleCollision), gives the camera its near clip and posts FOUR world-only nearest
//                             line tests along the edges of the view frustum, from beside the car to past the corners
//                             of the near plane;
//   ProcessSceneQueryResults  pull the camera in front of what they hit (ResolveCollisions: 1 m in front of the plain
//                             arm's hit, or out of each frustum corner's hit), then raise the minimum elevation when
//                             the camera was pulled in (UpdateMinElevation) and ease the radius back out
//                             (UpdateRadius).
// ============================================================================

#include "GameSource/Director/Camera/BrnCollisionPolicy.h"
#include "GameSource/Director/Camera/Camera.h"                         // Camera::mTransform (the eye), the near clip
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"         // VehicleInfo::mRaceCarState (transform, entity)
#include "GameSource/Director/Utils/BrnDirectorAllVehicleData.h"       // what VehicleRef::Get resolves against
#include "GameSource/Director/Utils/BrnSceneQueryInterface.h"          // SceneQueryInterface::LineTestNearest
#include "GameSource/Director/Camera/Utils/CameraUtils.h"              // Get/ApplyPitchAboutPointRads, ResolveLineTest...
#include "GameSource/Director/Camera/Utils/BrnConsoleVpu.h"            // the VMX roundings (Dot3 / fused lanes / Divide)
#include "SDKs/XboxMath/XMVectorTan.h"                                 // XMVectorTan @0x821F0788 (the half field of view)
#include "SDKs/XboxMath/XMVectorCos.h"                                 // XMVectorCos @0x821F06B0 (the roof over a car)
#include "SDKs/XboxMath/XMVectorATan.h"                                // XMVectorATan @0x821F0A70 (the push-out's pitch)
#include "SDKs/XboxMath/XMVectorSinCos.h"                              // the X rotation's sine / cosine, inlined
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficDirectorInterfaces.h"  // the traffic

#include <cmath>

namespace BrnDirector
{
namespace Camera
{

namespace
{
    // The literals the bodies load, read from the image.
    const f32 KF_DEGREES_TO_RADIANS      = 0.017453292f;  // flt_82001744 == 0x3C8EFA35  GenerateSceneQueries 0x82252700,
                                                          //   CalculateFrustumLineTests 0x8220DE44,
                                                          //   the resolver's ProcessSceneQueryResults 0x822243D0
    const f32 KF_RADIANS_TO_DEGREES      = 57.29578f;     // flt_82001748 == 0x42652EE1  UpdateMinElevation 0x822406A8
    const f32 KF_RESOLVE_MIN_DISTANCE    = 1.0f;          // flt_82001C98 == 0x3F800000  ResolveCollisions 0x8222498C
    const f32 KF_RADIUS_TOLERANCE        = 0.01f;         // flt_82002138 == 0x3C23D70A  UpdateRadius 0x8220E4D4,
                                                          //                             UpdateMinElevation 0x822405EC
    const f32 KF_RADIUS_EASE             = 0.1f;          // flt_82004014 == 0x3DCCCCCD  UpdateRadius 0x8220E504
    const f32 KF_ELEVATION_FULL_RADIUS   = 3.0f;          // flt_82004270 == 0x40400000  UpdateMinElevation 0x82240690
    const f32 KF_FULL_FORCE              = 1.0f;          // flt_82001C98                UpdateMinElevation 0x822406C4
    const f32 KF_NO_FORCE                = 0.0f;          // flt_82001CC0 == 0x00000000  UpdateMinElevation 0x8224065C
    const f32 KF_NO_TRAFFIC_RESOLUTION   = 0.0f;          // flt_82001CC0                GenerateSceneQueries 0x822527AC
    const f32 KF_FULL_TRAFFIC_RESOLUTION = 1.0f;          // flt_82001C98                GenerateSceneQueries 0x822527E0
    const f32 KF_ASPECT_NUMERATOR        = 1.0f;          // flt_82001C98  1 / mfAspectRatio (fdivs):
                                                          //   CalculateFrustumLineTests 0x8220DED8,
                                                          //   the resolver's ProcessSceneQueryResults 0x822243AC
    const f32 KF_NEAR_CLIP_DOUBLING      = 2.0f;          // flt_82001D9C == 0x40000000
                                                          //   the resolver's ProcessSceneQueryResults 0x8222439C

    // The splats the VMX bodies build in registers (no load): 0.5 and 1 are vcfsx / vcsxwfp128 of vspltisw 1 scaled
    // by 2^-1 / 2^0 (ProcessSceneQueryResults 0x82252920 / 0x82252928, CalculateFrustumLineTests 0x8220DE24 /
    // 0x8220DEA4, RequestFrustumLineTests 0x8223FDAC / 0x8223FDB4, the resolver's ProcessSceneQueryResults
    // 0x82224398), 2 is vcfsx(vspltisw 2, 0) (CalculateFrustumLineTests 0x8220DE18).
    const f32 KF_VPU_HALF = 0.5f;
    const f32 KF_VPU_ONE  = 1.0f;
    const f32 KF_VPU_TWO  = 2.0f;

    // The four frustum line tests' entity types: world only (`li r5, 2` before each of the four calls).
    const u32 KU_FRUSTUM_TEST_ENTITY_TYPES = 2u;

    // rw::math's splat of pi / 2: the .bss VecFloat 0x830180F0 its CRT thunk 0x82C6D200 fills from the .data float
    // 0x82F3192C == 0x3FC90FDB (GetHeightAboveTraffic 0x821F90E4).
    const f32 KF_HALF_PI = 1.5707964f;

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

    // The refined vrsqrtefp of a squared length, as both |v| (ProcessSceneQueryResults 0x82252904..0x822529A8) and
    // Normalize (RequestFrustumLineTests, e.g. 0x8223FDFC..0x8223FE64) run it:
    //   e = vrsqrtefp(d)                           rule 5: FLAG (model) the correctly rounded 1 / sqrt(d)
    //   twice: e = vmaddfp(e * 0.5, vnmsubfp(d, e * e, 1), e)    rule 3: each fused, the products rounded (vmulfp128)
    f32 RefinedReciprocalSquareRoot(f32 lfSquared)
    {
        f32 lfEstimate = static_cast<f32>(1.0 / std::sqrt(static_cast<f64>(lfSquared)));
        for (s32 liStep = 0; liStep < 2; ++liStep)
        {
            const f32 lfEstimateSquared = lfEstimate * lfEstimate;
            const f32 lfHalfEstimate    = lfEstimate * KF_VPU_HALF;
            const f32 lfResidual = Utils::ConsoleVpu::NegativeMultiplySubtract(lfSquared, lfEstimateSquared, KF_VPU_ONE);
            lfEstimate = Utils::ConsoleVpu::MultiplyAdd(lfHalfEstimate, lfResidual, lfEstimate);
        }
        return lfEstimate;
    }

    // |lV| as ProcessSceneQueryResults computes both radii (0x82252904..0x822529A8, one lane of each):
    //   d = vmsum3fp128(v, v)                      rule 1: one rounding of the f64 sum (flushed, rule 6)
    //   |v| = d * RefinedReciprocalSquareRoot(d) (vmulfp128), and `vcmpeqfp 0, d ; vsel` gives 0 for a zero d (else
    //   0 * inf).
    f32 ConsoleLength(const Vector3& lrV)
    {
        const f32 lfSquared  = FlushDenormal(Utils::ConsoleVpu::Dot3(lrV, lrV));
        const f32 lfEstimate = RefinedReciprocalSquareRoot(lfSquared);
        return (lfSquared == 0.0f) ? 0.0f : lfSquared * lfEstimate;
    }

    // vsubfp / vaddfp over four lanes, each rounded once.
    Vector3 LaneSubtract(const Vector3& lrA, const Vector3& lrB)
    {
        Vector3 lResult;
        lResult.x = lrA.x - lrB.x;
        lResult.y = lrA.y - lrB.y;
        lResult.z = lrA.z - lrB.z;
        lResult.w = lrA.w - lrB.w;
        return lResult;
    }

    Vector3 LaneAdd(const Vector3& lrA, const Vector3& lrB)
    {
        Vector3 lResult;
        lResult.x = lrA.x + lrB.x;
        lResult.y = lrA.y + lrB.y;
        lResult.z = lrA.z + lrB.z;
        lResult.w = lrA.w + lrB.w;
        return lResult;
    }

    // vmulfp128 of four lanes by a splat, each rounded once.
    Vector3 LaneScale(const Vector3& lrV, f32 lfScale)
    {
        Vector3 lResult;
        lResult.x = lrV.x * lfScale;
        lResult.y = lrV.y * lfScale;
        lResult.z = lrV.z * lfScale;
        lResult.w = lrV.w * lfScale;
        return lResult;
    }

    // rw::math::vpu::Normalize as RequestFrustumLineTests runs it, eight times (e.g. 0x8223FDEC..0x8223FE6C):
    // d = vmsum3fp128(v, v) (flushed), the refined estimate above, then v * e (vmulfp128). NO zero guard: a zero v
    // gives e = +inf and NaN lanes, as on the console.
    Vector3 NormalizeRefined(const Vector3& lrV)
    {
        return LaneScale(lrV, RefinedReciprocalSquareRoot(FlushDenormal(Utils::ConsoleVpu::Dot3(lrV, lrV))));
    }

    // rw::math::vpu::IsValid of a transform, as CalculateFrustumLineTests inlines it (0x8220DFAC..0x8220E13C): the x,
    // y and z lanes of all four rows compare equal to themselves (vspltw ; vcmpeqfp.) -- no NaN; an infinity passes
    // and w is not read.
    bool IsValidRows(const rw::math::vpu::Matrix44Affine& lrTransform)
    {
        const Vector3* const lapRows[4] = { &lrTransform.xAxis, &lrTransform.yAxis, &lrTransform.zAxis,
                                            &lrTransform.wAxis };
        for (const Vector3* lpRow : lapRows)
        {
            if (lpRow->x != lpRow->x || lpRow->y != lpRow->y || lpRow->z != lpRow->z)
                return false;
        }
        return true;
    }

    Vector3 LaneAbs(const Vector3& lrV)   // vandc of the sign mask: the sign bit cleared (a NaN's too)
    {
        Vector3 lResult;
        lResult.x = std::fabs(lrV.x);
        lResult.y = std::fabs(lrV.y);
        lResult.z = std::fabs(lrV.z);
        lResult.w = std::fabs(lrV.w);
        return lResult;
    }

    // vmaxfp / vminfp lanes as the AltiVec PEM defines them: the larger (smaller) value, +0 above -0, and a NaN
    // operand gives a NaN (the first one met is returned).
    // FLAG (model): the emulator's vmax / vmin return the SECOND operand of two equal zeros; the PEM's +0 / -0 rule is
    // taken here. Its callers below do not meet two zeros of opposite sign: a length against +0 (itself +0 or
    // positive), a cosine against 0, a camera height against a roof, a lift against 0 (a difference of positions
    // is never -0 unless an operand is).
    f32 VmxMax(f32 lfA, f32 lfB)
    {
        if (lfA != lfA)
            return lfA;
        if (lfB != lfB)
            return lfB;
        if (lfA == 0.0f && lfB == 0.0f)
            return std::signbit(lfA) ? lfB : lfA;
        return (lfA > lfB) ? lfA : lfB;
    }

    f32 VmxMin(f32 lfA, f32 lfB)
    {
        if (lfA != lfA)
            return lfA;
        if (lfB != lfB)
            return lfB;
        if (lfA == 0.0f && lfB == 0.0f)
            return std::signbit(lfA) ? lfA : lfB;
        return (lfA < lfB) ? lfA : lfB;
    }

    Vector3 LaneMax(const Vector3& lrV, f32 lfFloor)
    {
        Vector3 lResult;
        lResult.x = VmxMax(lrV.x, lfFloor);
        lResult.y = VmxMax(lrV.y, lfFloor);
        lResult.z = VmxMax(lrV.z, lfFloor);
        lResult.w = VmxMax(lrV.w, lfFloor);
        return lResult;
    }

    // rw::math::vpu::Matrix44AffineFromXRotationAngle as ResolveVehicleCollisions inlines it (0x82223ECC..
    // 0x822240AC): the XMVectorSinCos inline (SDKs/XboxMath/XMVectorSinCos.h -- the same tables, the same powers
    // and terms), then the rows packed by the vperm control 0x82CDA350 (lanes a.x, b.y, a.x, a.x) and vrlimi128 into
    // z:  xAxis {1, 0, 0, 1}  yAxis {0, cos, sin, 0}  zAxis {0, -sin, cos, 0}  wAxis 0   (-sin: a vxor of the sign).
    // xAxis.w is the 1 the vperm leaves there; the product below never reads a rotation row's w.
    rw::math::vpu::Matrix44Affine XRotation(f32 lfAngle)
    {
        f32 lfSin = 0.0f;
        f32 lfCos = 0.0f;
        XboxMath::XMVectorSinCos(&lfSin, &lfCos, lfAngle);
        rw::math::vpu::Matrix44Affine lRotation;
        lRotation.xAxis.x = 1.0f;  lRotation.xAxis.y = 0.0f;    lRotation.xAxis.z = 0.0f;   lRotation.xAxis.w = 1.0f;
        lRotation.yAxis.x = 0.0f;  lRotation.yAxis.y = lfCos;   lRotation.yAxis.z = lfSin;  lRotation.yAxis.w = 0.0f;
        lRotation.zAxis.x = 0.0f;  lRotation.zAxis.y = -lfSin;  lRotation.zAxis.z = lfCos;  lRotation.zAxis.w = 0.0f;
        lRotation.wAxis.x = 0.0f;  lRotation.wAxis.y = 0.0f;    lRotation.wAxis.z = 0.0f;   lRotation.wAxis.w = 0.0f;
        return lRotation;
    }

    // The attached vehicle's position (its transform's w row: VehicleRef::Get + 0x1F0 + 0x30 at every site).
    const Vector3& AttachedVehiclePosition(const VehicleInfo* lpVehicle)
    {
        return lpVehicle->mRaceCarState.mTransform.wAxis;
    }
}

// ----------------------------------------------------------------------------
// The FrustrumCollisionResolver tunables (DWARF BrnCollisionPolicy.h:157..:172). Each VecFloat is a .bss splat a CRT
// thunk fills at start-up -- `lfs f0, <rodata> ; stfs ; lvlx ; vspltw 0 ; stvx128` -- so it is the splat of that
// float, read from the image (the .bss slot <- its thunk, the float):
VecFloat FrustrumCollisionResolver::sMaxViewportHalfWidth(0.6f);       // 0x82FAA7B0 <- 0x82C491B0, flt_82004D00 0x3F19999A
VecFloat FrustrumCollisionResolver::sMaxViewportHalfHeight(0.5f);      // 0x82FAAAB0 <- 0x82C491D8, flt_82001DA0 0x3F000000
VecFloat FrustrumCollisionResolver::sTestStartWidthPadding(0.1f);      // 0x82FAAB50 <- 0x82C49200, flt_82004014 0x3DCCCCCD
VecFloat FrustrumCollisionResolver::sTestLengthPadding(1.0f);          // 0x82FAA910 <- 0x82C49228, flt_82001C98 0x3F800000
VecFloat FrustrumCollisionResolver::kvfHorizontalExtentsScale(1.0f);           // 0x82FAA9B0 <- 0x82C49250, flt_82001C98 0x3F800000
VecFloat FrustrumCollisionResolver::kvfVehicleCollisionCurveTopScale(2.5f);    // 0x82FAAA90 <- 0x82C49278, flt_82005548 0x40200000
VecFloat FrustrumCollisionResolver::kvfVehicleCollisionCurveBottomScale(2.0f); // 0x82FAA970 <- 0x82C492A0, flt_82001D9C 0x40000000
VecFloat FrustrumCollisionResolver::kvfVehicleCollisionReturnSpeed(0.01f);     // 0x82FAAB30 <- 0x82C492C8, flt_82002138 0x3C23D70A
VecFloat FrustrumCollisionResolver::kvfDistBelowCarToIgnoreCollision(0.5f);    // 0x82FAA980 <- 0x82C492F0, flt_82001DA0 0x3F000000
VecFloat FrustrumCollisionResolver::kvfMinVehicleResolveAmount(0.01f); // 0x82FAA700 <- 0x82C49318, flt_82002138 0x3C23D70A
// A .data float, read where it is used (CalculateFrustumLineTests 0x8220DE28, ProcessSceneQueryResults 0x8222438C).
f32      FrustrumCollisionResolver::kfFOVBodgeAmount = 2.0f;            // flt_82CDA6E0 == 0x40000000

// DWARF BrnCollisionPolicy.cpp:161 `const VecFloat KVF_MIN_CAMERA_DIST` (namespace BrnDirector::Camera): the floor
// under the camera-to-target distance ResolveVehicleCollisions divides by. The .bss splat 0x82FAA720, filled by the
// CRT thunk 0x82C49340 -- the one after kvfMinVehicleResolveAmount's, in definition order -- from flt_82002540 ==
// 0x38D1B717.
const VecFloat KVF_MIN_CAMERA_DIST(1.0e-4f);

// The policy's traffic ramp (DWARF BrnCollisionPolicyAttachedToVehicle.cpp:27..:29), folded into literals by the
// console's compiler (GenerateSceneQueries 0x822527C8 / 0x822527E4 / 0x822527F8):
const f32 CollisionPolicyAttachedToVehicle::kfSpeedLimitForTrafficCollision = 35.0f;  // flt_82009D58 == 0x420C0000
const f32 CollisionPolicyAttachedToVehicle::kfTrafficCollisionRampUp        = 0.05f;  // flt_820047C8 == 0x3D4CCCCD
const f32 CollisionPolicyAttachedToVehicle::kfTrafficCollisionRampDown      = 0.01f;  // flt_82002138 == 0x3C23D70A

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
//   0x82252778  mbUseFrustrumResolver (+0x24D) ; beq 0x82252838 (the plain arm)
//   the frustum arm:
//   0x82252798  mbResetVehicleCollision (+0x24E) -> cleared (stb 0) and the resolution is 0 (flt_82001CC0); else
//   0x822527BC  VehicleRef::Get again ; its mfSpeedMPH (+0x3CC) < 35 (flt_82009D58; fcmpu, bge TAKEN unordered, so a
//               NaN speed eases DOWN): t = fmadds(1 - t, 0.05, t) (fsubs flt_82001C98 - t ; flt_820047C8), else
//               t = fmadds(-t, 0.01, t) (fneg ; flt_82002138)
//   0x8225280C  stfs -> mfTrafficCollisionResolution (+0x244)
//   0x82252830  FrustrumCollisionResolver::GenerateSceneQueries(this + 0x10, info, camera, v1 = the vehicle's
//               position, v2 = splat(+0x244), v3 = splat(mfDesiredNearClip +0x23C), r6 = mbDoVehicleCollision
//               (+0x24F)) -> done
//   the plain arm:
//   0x82252838  LineTestNearest(info +0x08, this + 0x170 (mCarToCamera), flags, 0xFF, v1 = the vehicle's position,
//               v2 = the camera's, r7 = the vehicle's entity (+0x3C8), r8 = 1 E_EXCLUDE_ALL_CHILD_PARTS)
// ----------------------------------------------------------------------------
void CollisionPolicyAttachedToVehicle::GenerateSceneQueries(const CollisionPolicySharedInfo& lrSharedInfo,
                                                            Camera& lrCamera)
{
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

    if (mbUseFrustrumResolver)
    {
        if (mbResetVehicleCollision)
        {
            mbResetVehicleCollision      = 0;
            mfTrafficCollisionResolution = KF_NO_TRAFFIC_RESOLUTION;
        }
        else
        {
            const f32 lfResolution = mfTrafficCollisionResolution;
            if (mAttachedTo.Get(lrSharedInfo.mpAllVehicleData)->mRaceCarState.mfSpeedMPH
                < kfSpeedLimitForTrafficCollision)
                mfTrafficCollisionResolution = Utils::ConsoleVpu::MultiplyAdd(
                    KF_FULL_TRAFFIC_RESOLUTION - lfResolution, kfTrafficCollisionRampUp, lfResolution);
            else
                mfTrafficCollisionResolution = Utils::ConsoleVpu::MultiplyAdd(-lfResolution, kfTrafficCollisionRampDown,
                                                                              lfResolution);
        }

        mFrustrumCollisionResolver.GenerateSceneQueries(lrSharedInfo, lrCamera, lVehiclePosition,
                                                        VecFloat(mfTrafficCollisionResolution),
                                                        VecFloat(mfDesiredNearClip), mbDoVehicleCollision != 0);
        return;
    }

    lrSharedInfo.mpRequestInterface->LineTestNearest(
        mCarToCamera, lx32EntityTypeFlags, 0xFFu, lVehiclePosition, lCameraPosition,
        CgsSceneManager::EntityId(mAttachedTo.Get(lrSharedInfo.mpAllVehicleData)->mRaceCarState.mEntityId.muValue),
        CgsSceneManager::SceneManagerIO::E_EXCLUDE_ALL_CHILD_PARTS);
}

// ----------------------------------------------------------------------------
// ProcessSceneQueryResults @0x82252888 (DWARF .cpp:153). r31 = this, r29 = lrSharedInfo, r30 = lrCamera.
//   0x822528C4  v126 = the camera's position BEFORE the resolve
//   0x822528E0  v127 = the vehicle's position ; bl ResolveCollisions(info, camera, v1 = v127)  -- both arms
//   0x822528EC  mbAutoElevate || mbSmoothRadiusChanges, else done
//   0x82252904  the two radii, |vehicle - camera before| (f31) and |vehicle - camera after| (f30): ConsoleLength
//   0x822529AC  mbAutoElevate          -> UpdateMinElevation(info, f1 = before, f2 = after, r7 = camera)
//   0x822529C8  mbSmoothRadiusChanges  -> UpdateRadius(camera, f1 = before, f2 = after,
//                                         v1 = camera (re-read @0x822529D4) - vehicle)
// ----------------------------------------------------------------------------
void CollisionPolicyAttachedToVehicle::ProcessSceneQueryResults(const CollisionPolicySharedInfo& lrSharedInfo,
                                                                Camera& lrCamera)
{
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
// ResolveCollisions @0x82224948 (DWARF .cpp:185). r31 = this, r28 = lrSharedInfo, r30 = lrCamera, v1 = lVehiclePosition.
//   0x82224960  mbUseFrustrumResolver ; beq 0x82224980
//   0x82224974  the frustum arm: FrustrumCollisionResolver::ProcessSceneQueryResults(this + 0x10, info, camera, v1,
//               r6 = a stack Vector3 nothing reads) -> the answer ; b 0x822249C4
//   0x82224980  the plain arm: ResolveLineTestNearestUsingNormalStrict(this + 0x170, camera +0x30,
//               flt_82001C98 = 1.0)
//   0x82224994  `lwz 0x170 ; cmpwi 2 ; beq` + "meState == E_STATE_GOT_PACKAGE" (BrnPostBox.h:110): an inlined
//               GetPackage whose answer is unused (plain arm only)
//   0x822249C4  moved && mbFailOnContact (+0x24A) -> CollisionPolicy::Fail(this, camera, 0 = E_FAILED_COLLISION)
//   0x822249EC  mbUseGroundConstraint -> GroundConstraint::ProcessSceneQueryResults(this + 0x1C0, info +0x60, camera)
//   0x82224A08  stw 0, 0x170 -- the car-to-camera box emptied, whatever happened (both arms)
// ----------------------------------------------------------------------------
void CollisionPolicyAttachedToVehicle::ResolveCollisions(const CollisionPolicySharedInfo& lrSharedInfo,
                                                         Camera& lrCamera, Vector3 lVehiclePosition)
{
    bool lbMoved;
    if (mbUseFrustrumResolver)
    {
        Vector3 lPlaneNormal;
        lbMoved = mFrustrumCollisionResolver.ProcessSceneQueryResults(lrSharedInfo, lrCamera, lVehiclePosition,
                                                                      lPlaneNormal);
    }
    else
    {
        lbMoved = Utils::ResolveLineTestNearestUsingNormalStrict(mCarToCamera, lrCamera.mTransform.wAxis,
                                                                 KF_RESOLVE_MIN_DISTANCE);
        (void)mCarToCamera.GetPackage();
    }

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

// ============================================================================
// FrustrumCollisionResolver (DWARF BrnCollisionPolicy.cpp) -- piece 6a.
// Every VecFloat these bodies take or read is a splat (the policy's lvlx / vspltw at 0x82252820..0x8225282C, the
// tunables' start-up vspltw, the resolver's own lvlx / vspltw of mfMinDistance), so each is taken as its scalar and
// the ALL / ANY lane tests are the scalar tests. FLAG (rule 6): the VMX flush of denormal operands and results is
// modelled only on the squared lengths (Normalize's zero case); the camera's distances and extents are metres.
// ============================================================================

// ----------------------------------------------------------------------------
// FrustrumCollisionResolver::GenerateSceneQueries @0x82252540 (DWARF BrnCollisionPolicy.cpp:97). r31 = this,
// r29 = lSharedInfo, r30 = lCamera, r6 = lbResolveVehicleCollisions; v126 = lTarget, v127 = lVehicleResolveAmount,
// v125 = lDesiredNearClip.
//   0x82252584  lbResolveVehicleCollisions ; beq 0x82252604
//   0x82252588..0x822525D8  !IsZero(lVehicleResolveAmount, kvfMinVehicleResolveAmount): |amount| (vandc of the sign
//               mask vslw(-1, -1)) > the minimum (vcmpgtfp), the four lanes' top bytes gathered by the vperm control
//               0x0004080C and tested as one word -- ANY lane; a NaN lane counts as zero
//   0x822525EC  ResolveVehicleCollisions(this, info +0x1C (mpAllVehicleData), v1 = the camera's position (+0x30),
//               r5 = lCamera, v2 = lTarget)
//   0x822525F0  mVehicleResolveVector *= lVehicleResolveAmount (vmulfp128), called or not
//   0x82252604  else mVehicleResolveVector = 0 (vspltisw 0 ; stvx128)
//   0x82252634  CalculateFrustumLineTests(v1 = lDesiredNearClip, lCamera, v2 = lTarget, &lForward, &lUp, &lLeft,
//               &lStart, &lEnd)
//   0x82252668  RequestFrustumLineTests(info +0x08 (mpRequestInterface), lStart, lLeft, lUp, lEnd, lForward)
// ----------------------------------------------------------------------------
void FrustrumCollisionResolver::GenerateSceneQueries(const CollisionPolicySharedInfo& lSharedInfo, Camera& lCamera,
                                                     Vector3 lTarget, VecFloat lVehicleResolveAmount,
                                                     VecFloat lDesiredNearClip, bool lbResolveVehicleCollisions)
{
    if (lbResolveVehicleCollisions)
    {
        const f32 lfResolveAmount = static_cast<f32>(lVehicleResolveAmount);
        if (std::fabs(lfResolveAmount) > static_cast<f32>(kvfMinVehicleResolveAmount))
            ResolveVehicleCollisions(lSharedInfo.mpAllVehicleData, lCamera.mTransform.wAxis, lCamera, lTarget);
        mVehicleResolveVector = LaneScale(mVehicleResolveVector, lfResolveAmount);
    }
    else
    {
        mVehicleResolveVector.SetZero();
    }

    Vector3 lForward;
    Vector3 lUp;
    Vector3 lLeft;
    Vector3 lStart;
    Vector3 lEnd;
    CalculateFrustumLineTests(lDesiredNearClip, lCamera, lTarget, lForward, lUp, lLeft, lStart, lEnd);
    RequestFrustumLineTests(lSharedInfo.mpRequestInterface, lStart, lLeft, lUp, lEnd, lForward);
}

// ----------------------------------------------------------------------------
// FrustrumCollisionResolver::CalculateFrustumLineTests @0x8220DDE8 (DWARF BrnCollisionPolicy.cpp:358). r30 = lCamera,
// r29 / r28 / r27 / r26 / r25 = &lForward / &lUp / &lLeft / &lStart / &lEnd; v1 = lDesiredNearClip, v124 = lTarget.
//   0x8220DE48  lNewClipDistance = lDesiredNearClip * 2                                        vmulfp128
//   0x8220DE30  the field of view in radians: (mfFOV (+0x58) + kfFOVBodgeAmount) * flt_82001744   fadds, fmuls
//   0x8220DE70  tan = XMVectorTan(0.5 * that)                                                   vmulfp128, bl 0x821F0788
//   0x8220DE74  lViewportHalfWidth = tan * lNewClipDistance                                    vmulfp128
//   0x8220DE88  > sMaxViewportHalfWidth (vcmpgtfp., ALL lanes: a NaN fails): lNewClipDistance *=
//               sMaxViewportHalfWidth / lViewportHalfWidth (the SDK's operator/: vrefp and two Newton-Raphson steps,
//               ConsoleVpu::Divide) and the width becomes the maximum
//   0x8220DEE0  1 / mfAspectRatio (+0x5C)                                                       fdivs flt_82001C98
//   0x8220DF0C  lViewportHalfHeight = (tan * that) * lNewClipDistance                          vmulfp128 x2
//   0x8220DF10  > sMaxViewportHalfHeight: the same clamp
//   0x8220DF68  Camera::SetCustomNearClipDistance, inlined: mbHasCustomNearClipDistance (+0x15D) = 1 and
//               mfCustomNearClipDistance (+0x150) = lane 0 of lNewClipDistance * 0.5
//   0x8220DF84..0x8220DFA8  lForward = the z axis * lNewClipDistance ; lUp = the y axis * lViewportHalfHeight ;
//               lLeft = the x axis * lViewportHalfWidth (vmulfp128) ; lStart = lTarget ; lEnd = the w axis
//   0x8220DFAC..0x8220E1A0  the IsValid assert on the camera's transform (BrnCollisionPolicy.cpp:390, a formatted
//               matrix message)
// ----------------------------------------------------------------------------
void FrustrumCollisionResolver::CalculateFrustumLineTests(VecFloat lDesiredNearClip, Camera& lCamera, Vector3 lTarget,
                                                          Vector3& lForward, Vector3& lUp, Vector3& lLeft,
                                                          Vector3& lStart, Vector3& lEnd)
{
    f32 lfNewClipDistance = static_cast<f32>(lDesiredNearClip) * KF_VPU_TWO;
    const f32 lfFieldOfView = (lCamera.mfFOV + kfFOVBodgeAmount) * KF_DEGREES_TO_RADIANS;
    const f32 lfTanHalfFOV  = XboxMath::XMVectorTan(KF_VPU_HALF * lfFieldOfView);

    f32 lfViewportHalfWidth = lfTanHalfFOV * lfNewClipDistance;
    if (lfViewportHalfWidth > static_cast<f32>(sMaxViewportHalfWidth))
    {
        lfNewClipDistance   = lfNewClipDistance * Utils::ConsoleVpu::Divide(sMaxViewportHalfWidth, lfViewportHalfWidth);
        lfViewportHalfWidth = sMaxViewportHalfWidth;
    }

    const f32 lfInverseAspectRatio = KF_ASPECT_NUMERATOR / lCamera.mfAspectRatio;
    f32 lfViewportHalfHeight = (lfTanHalfFOV * lfInverseAspectRatio) * lfNewClipDistance;
    if (lfViewportHalfHeight > static_cast<f32>(sMaxViewportHalfHeight))
    {
        lfNewClipDistance    = lfNewClipDistance
                             * Utils::ConsoleVpu::Divide(sMaxViewportHalfHeight, lfViewportHalfHeight);
        lfViewportHalfHeight = sMaxViewportHalfHeight;
    }

    lCamera.mbHasCustomNearClipDistance = true;
    lCamera.mfCustomNearClipDistance    = lfNewClipDistance * KF_VPU_HALF;

    lForward = LaneScale(lCamera.mTransform.zAxis, lfNewClipDistance);
    lUp      = LaneScale(lCamera.mTransform.yAxis, lfViewportHalfHeight);
    lLeft    = LaneScale(lCamera.mTransform.xAxis, lfViewportHalfWidth);
    lStart   = lTarget;
    lEnd     = lCamera.mTransform.wAxis;

    CGS_ASSERT(IsValidRows(lCamera.mTransform), "IsValid(lCamera.GetTransform())");
}

// ----------------------------------------------------------------------------
// FrustrumCollisionResolver::RequestFrustumLineTests @0x8223FD70 (DWARF BrnCollisionPolicy.cpp:402). r31 = this,
// r30 = lpRequestInterface; v124 = lStart, v123 = lLeft, v125 = lUp, v4 = lEnd, v5 = lForward.
//   0x8223FD88  the near plane's centre, lEnd + lForward (v122)
//   per corner, in box order -- mTopLeft (+lLeft +lUp), mTopRight (-lLeft +lUp), mBottomLeft (+lLeft -lUp),
//   mBottomRight (-lLeft -lUp); each sum a vaddfp / vsubfp, the two terms applied left to right:
//     lLineStart = (lStart +- lLeft) +- lUp ; lLineEnd = ((lEnd + lForward) +- lLeft) +- lUp
//     lLineStart' = Normalize(lLineStart - lStart) * sTestStartWidthPadding + lLineStart      (vmaddfp: one rounding)
//     lLineEnd'   = Normalize(lLineEnd - lLineStart) * sTestLengthPadding + lLineEnd          -- the direction is
//                   taken from the UNPADDED start
//     LineTestNearest(lpRequestInterface, box, 2 (world only), 0xFF, lLineStart', lLineEnd',
//                     dword_82CDA790 == 0xFFFFFFFF (K_INVALID_ENTITY_ID), 0 (E_EXCLUDE_ENTITY_ONLY))
//   at 0x8223FE74 / 0x8223FF30 / 0x8223FFE4 / 0x82240098.
// ----------------------------------------------------------------------------
void FrustrumCollisionResolver::RequestFrustumLineTests(const SceneQueryInterface* lpRequestInterface, Vector3 lStart,
                                                        Vector3 lLeft, Vector3 lUp, Vector3 lEnd, Vector3 lForward)
{
    const Vector3 lNearPlaneCentre = LaneAdd(lEnd, lForward);

    const auto RequestCorner = [&](LineTestNearestPostBox& lrBox, const Vector3& lLineStart, const Vector3& lLineEnd)
    {
        const Vector3 lPaddedStart = Utils::ConsoleVpu::MultiplyAdd(NormalizeRefined(LaneSubtract(lLineStart, lStart)),
                                                                    sTestStartWidthPadding, lLineStart);
        const Vector3 lPaddedEnd   = Utils::ConsoleVpu::MultiplyAdd(
            NormalizeRefined(LaneSubtract(lLineEnd, lLineStart)), sTestLengthPadding, lLineEnd);
        lpRequestInterface->LineTestNearest(lrBox, KU_FRUSTUM_TEST_ENTITY_TYPES, 0xFFu, lPaddedStart, lPaddedEnd,
                                            CgsSceneManager::K_INVALID_ENTITY_ID,
                                            CgsSceneManager::SceneManagerIO::E_EXCLUDE_ENTITY_ONLY);
    };

    RequestCorner(mTopLeft,     LaneAdd(LaneAdd(lStart, lLeft), lUp),
                                LaneAdd(LaneAdd(lNearPlaneCentre, lLeft), lUp));
    RequestCorner(mTopRight,    LaneAdd(LaneSubtract(lStart, lLeft), lUp),
                                LaneAdd(LaneSubtract(lNearPlaneCentre, lLeft), lUp));
    RequestCorner(mBottomLeft,  LaneSubtract(LaneAdd(lStart, lLeft), lUp),
                                LaneSubtract(LaneAdd(lNearPlaneCentre, lLeft), lUp));
    RequestCorner(mBottomRight, LaneSubtract(LaneSubtract(lStart, lLeft), lUp),
                                LaneSubtract(LaneSubtract(lNearPlaneCentre, lLeft), lUp));
}

// ----------------------------------------------------------------------------
// FrustrumCollisionResolver::ProcessSceneQueryResults @0x822242F8 (DWARF BrnCollisionPolicy.cpp:443). r29 = this,
// r28 = lCamera, v124 = lTarget, r6 = &lPlaneNormalOut; lSharedInfo (r4) is never read.
//   0x82224320  lPlaneNormalOut = 0 (vspltisw 0 ; stvx128)
//   0x82224324..0x8222436C  kNearClip = lCamera.GetNearClipDistance(), inlined (+0x15D / +0x150 / the +0x140 flag)
//   0x822243A4  twice it (fmuls flt_82001D9C == 2.0) -- the doubled near clip CalculateFrustumLineTests sized
//   0x82224394 / 0x822243D8  (mfFOV + kfFOVBodgeAmount) * flt_82001744 ; 0x822243B8  1 / mfAspectRatio (fdivs)
//   0x822243E8  lForward = the z axis * that (vmulfp128) ; 0x82224404  tan = XMVectorTan(0.5 * the field of view)
//   0x8222440C  lViewportWidth = tan * 2 kNearClip ; 0x82224448  lViewportHeight = (tan * 1 / aspect) * 2 kNearClip
//   0x8222444C  lLeft = the x axis * lViewportWidth ; 0x82224454  lUp = the y axis * lViewportHeight
//   per corner -- lPos RE-READ from the camera's w row before each, since each resolve can move it:
//     lPosToTarget = lTarget - lPos ; the corner = ((lPos + lForward) +- lLeft) +- lUp
//     ResolveLineTestNearestUsingDisplacementAndVector(box, the corner, the camera's w row, lPosToTarget,
//                                                     splat(mfMinDistance +0x150))
//   at 0x82224460 (mTopLeft +lLeft +lUp), 0x8222448C (mTopRight -lLeft +lUp), 0x822244C4 (mBottomLeft +lLeft -lUp),
//   0x822244FC (mBottomRight -lLeft -lUp) -- all four always made; the answers OR-ed (lbHasResolvedAnything)
//   0x82224504..0x82224518  the four boxes emptied (stw 0)
// ----------------------------------------------------------------------------
bool FrustrumCollisionResolver::ProcessSceneQueryResults(const CollisionPolicySharedInfo& lSharedInfo, Camera& lCamera,
                                                         Vector3 lTarget, Vector3& lPlaneNormalOut)
{
    (void)lSharedInfo;
    lPlaneNormalOut.SetZero();

    const f32 lfTwiceNearClip      = lCamera.GetNearClipDistance() * KF_NEAR_CLIP_DOUBLING;
    const f32 lfFieldOfView        = (lCamera.mfFOV + kfFOVBodgeAmount) * KF_DEGREES_TO_RADIANS;
    const f32 lfInverseAspectRatio = KF_ASPECT_NUMERATOR / lCamera.mfAspectRatio;
    const Vector3 lForward         = LaneScale(lCamera.mTransform.zAxis, lfTwiceNearClip);
    const f32 lfTanHalfFOV         = XboxMath::XMVectorTan(KF_VPU_HALF * lfFieldOfView);
    const f32 lfViewportWidth      = lfTanHalfFOV * lfTwiceNearClip;
    const f32 lfViewportHeight     = (lfTanHalfFOV * lfInverseAspectRatio) * lfTwiceNearClip;
    const Vector3 lLeft            = LaneScale(lCamera.mTransform.xAxis, lfViewportWidth);
    const Vector3 lUp              = LaneScale(lCamera.mTransform.yAxis, lfViewportHeight);
    const VecFloat lvMinDistance(mfMinDistance);

    bool lbHasResolvedAnything = false;
    {
        const Vector3 lPos = lCamera.mTransform.wAxis;
        lbHasResolvedAnything |= Utils::ResolveLineTestNearestUsingDisplacementAndVector(
            mTopLeft, LaneAdd(LaneAdd(LaneAdd(lPos, lForward), lLeft), lUp), lCamera.mTransform.wAxis,
            LaneSubtract(lTarget, lPos), lvMinDistance);
    }
    {
        const Vector3 lPos = lCamera.mTransform.wAxis;
        lbHasResolvedAnything |= Utils::ResolveLineTestNearestUsingDisplacementAndVector(
            mTopRight, LaneAdd(LaneSubtract(LaneAdd(lPos, lForward), lLeft), lUp), lCamera.mTransform.wAxis,
            LaneSubtract(lTarget, lPos), lvMinDistance);
    }
    {
        const Vector3 lPos = lCamera.mTransform.wAxis;
        lbHasResolvedAnything |= Utils::ResolveLineTestNearestUsingDisplacementAndVector(
            mBottomLeft, LaneSubtract(LaneAdd(LaneAdd(lPos, lForward), lLeft), lUp), lCamera.mTransform.wAxis,
            LaneSubtract(lTarget, lPos), lvMinDistance);
    }
    {
        const Vector3 lPos = lCamera.mTransform.wAxis;
        lbHasResolvedAnything |= Utils::ResolveLineTestNearestUsingDisplacementAndVector(
            mBottomRight, LaneSubtract(LaneSubtract(LaneAdd(lPos, lForward), lLeft), lUp), lCamera.mTransform.wAxis,
            LaneSubtract(lTarget, lPos), lvMinDistance);
    }

    mTopLeft.Clear();
    mTopRight.Clear();
    mBottomLeft.Clear();
    mBottomRight.Clear();
    return lbHasResolvedAnything;
}

// ----------------------------------------------------------------------------
// FrustrumCollisionResolver::ResolveVehicleCollisions @0x82223890 (DWARF BrnCollisionPolicy.cpp:167). r19 = this,
// r14 = lpAllVehicles, r22 = lCamera; v126 = lCameraPos, v124 = lTarget. Only the chase cam reaches it (its policy is
// the one constructed with mbDoVehicleCollision), and only while the eased traffic resolution is above
// kvfMinVehicleResolveAmount.
//   0x822238C4  lCurrentCameraPos = lCameraPos: the stack copy every ResolveVehicleCollision below lifts
//   0x822238C0..0x82223924  AllVehicleData::GetTraffic, inlined -- `lwz 0xD0` and its "mpTrafficVehicleArray !=
//               NULL" (BrnDirectorAllVehicleData.h:96) -- then GetLength's "Array used before Construct/Clear was
//               called" (CgsArray.h:336)
//   0x8222393C  each traffic vehicle, in array order: GetItem (@0x821FB940), ResolveVehicleCollision(this, its
//               mLocalTransform (+0x00), v1 = its mHalfExtents (+0x50), &lCurrentCameraPos)
//   0x82223968..0x82223BF4  each set bit of the used race cars (+0xC8), ascending -- the attached car included:
//               AllVehicleData::GetRaceCar @0x82205DE8, ResolveVehicleCollision(this, its mRaceCarState.mTransform
//               (+0x1F0), v1 = its mRaceCarState.mHalfExtent (+0x350), &lCurrentCameraPos)
//   0x82223C20  lResolveVector = lCurrentCameraPos - lCameraPos
//   0x82223C34  mVehicleResolveVector += Max(lResolveVector - mVehicleResolveVector, 0)  (vsubfp, vmaxfp128, vaddfp):
//               a lift is taken at once ...
//   0x82223C54  ... and let go at kvfVehicleCollisionReturnSpeed a frame: mVehicleResolveVector =
//               (lResolveVector - mVehicleResolveVector) * that + mVehicleResolveVector (one vmaddfp)
//   0x82223C60  the camera's position += mVehicleResolveVector
//   0x82223C84  lvfDist = Max(the z axis . (lTarget - the camera's position), KVF_MIN_CAMERA_DIST)  vmsum3fp128, vmaxfp
//   0x82223C90..0x82223CB4  lvfAngle = XMVectorATan(mVehicleResolveVector.y / lvfDist) -- the SDK's operator/ (vrefp
//               and two Newton-Raphson steps, ConsoleVpu::Divide)
//   0x82223CC4 / 0x82223D80 / 0x82223E28  three tripwires on the angle (BrnCollisionPolicy.cpp:207 / :208 / :209,
//               a formatted float message), each `vcmpeqfp128. angle, angle`: all three fire on a NaN
//   0x82223ECC..0x822240AC  Matrix44AffineFromXRotationAngle(lvfAngle) (XRotation above)
//   0x822240A4..0x8222411C  the camera's transform = that rotation * the transform (ConsoleVpu::Mult): the eye pitched
//               about its own x axis by the angle the lift subtends at the target, and its position carried with it
//   0x82224104..0x822242E0  the IsValid assert on the new transform (BrnCollisionPolicy.cpp:214)
// ----------------------------------------------------------------------------
void FrustrumCollisionResolver::ResolveVehicleCollisions(const AllVehicleData* lpAllVehicles, Vector3 lCameraPos,
                                                         Camera& lCamera, Vector3 lTarget)
{
    Vector3 lCurrentCameraPos = lCameraPos;

    const Array<BrnTraffic::BrnTrafficIO::TrafficDirectorEntity, 32u>* lpAllTraffic = lpAllVehicles->GetTraffic();
    CGS_ASSERT(lpAllTraffic != 0, "mpTrafficVehicleArray != NULL");
    const s32 liNumTrafficVehicles = static_cast<s32>(lpAllTraffic->GetLength());
    for (s32 liTrafficIndex = 0; liTrafficIndex < liNumTrafficVehicles; ++liTrafficIndex)
    {
        const BrnTraffic::BrnTrafficIO::TrafficDirectorEntity& lrTrafficVehicle =
            lpAllTraffic->GetItem(static_cast<u32>(liTrafficIndex));
        ResolveVehicleCollision(lrTrafficVehicle.mLocalTransform, lrTrafficVehicle.mHalfExtents, lCurrentCameraPos);
    }

    const CgsContainers::BitArray<8u>& lrUsedRaceCars = lpAllVehicles->GetUsedRaceCarsBitArray();
    for (s32 liRaceCarIndex = lrUsedRaceCars.GetFirstNonZeroBit(); liRaceCarIndex != -1;
         liRaceCarIndex = lrUsedRaceCars.GetNextNonZeroBit(liRaceCarIndex))
    {
        const VehicleInfo& lrRaceCar = lpAllVehicles->GetRaceCar(static_cast<EActiveRaceCarIndex>(liRaceCarIndex));
        ResolveVehicleCollision(lrRaceCar.mRaceCarState.mTransform, lrRaceCar.mRaceCarState.mHalfExtent,
                                lCurrentCameraPos);
    }

    const Vector3 lResolveVector = LaneSubtract(lCurrentCameraPos, lCameraPos);
    mVehicleResolveVector = LaneAdd(mVehicleResolveVector,
                                    LaneMax(LaneSubtract(lResolveVector, mVehicleResolveVector), 0.0f));
    mVehicleResolveVector = Utils::ConsoleVpu::MultiplyAdd(LaneSubtract(lResolveVector, mVehicleResolveVector),
                                                           kvfVehicleCollisionReturnSpeed, mVehicleResolveVector);

    lCamera.mTransform.wAxis = LaneAdd(lCamera.mTransform.wAxis, mVehicleResolveVector);
    const f32 lfDist = VmxMax(FlushDenormal(Utils::ConsoleVpu::Dot3(lCamera.mTransform.zAxis,
                                                                    LaneSubtract(lTarget, lCamera.mTransform.wAxis))),
                              KVF_MIN_CAMERA_DIST);
    const f32 lfAngle = XboxMath::XMVectorATan(Utils::ConsoleVpu::Divide(mVehicleResolveVector.y, lfDist));
    CGS_ASSERT(lfAngle == lfAngle, "IsValid(lvfAngle)");                                   // BrnCollisionPolicy.cpp:207
    CGS_ASSERT(lfAngle == lfAngle, "IsValid(lvfAngle)");                                   // :208
    CGS_ASSERT(lfAngle == lfAngle, "IsValid(lvfAngle)");                                   // :209

    lCamera.mTransform = Utils::ConsoleVpu::Mult(XRotation(lfAngle), lCamera.mTransform);
    CGS_ASSERT(IsValidRows(lCamera.mTransform), "IsValid(lCamera.GetTransform())");        // :214
}

// ----------------------------------------------------------------------------
// FrustrumCollisionResolver::ResolveVehicleCollision @0x8220DBC8 (DWARF BrnCollisionPolicy.cpp:223). r4 =
// &lTrafficTransform, v1 = lTrafficExtentsIn, r5 = &lCurrentCameraPos.
//   0x8220DC18..0x8220DC34  (Pos().y - lTrafficExtentsIn.y) - kvfDistBelowCarToIgnoreCollision > the camera's y
//               (vsubfp, vsubfp, vcmpgtfp.): a camera that far under the vehicle's underside is left alone
//   0x8220DC84  lTrafficAxisUp = the y column {x.y, y.y, z.y, w.y} (vmrghw / vmrglw)
//   0x8220DC8C  lvfDistanceSq = |Pos() - the camera|^2 (vsubfp128, vmsum3fp128)
//   0x8220DC94  lTrafficExtents = lTrafficExtentsIn * ((1 - lTrafficAxisUp) * kvfHorizontalExtentsScale + 1)
//               (vsubfp, vmaddfp, vmulfp128) -- an axis lying flat is widened by the scale
//   0x8220DCA4  lvfTrafficRadiusSq = |lTrafficExtents * 2|^2 ; lvfDistanceSq < it (vcmpgtfp., ALL lanes: a NaN
//               fails), else done
//   0x8220DCE0  lCameraPosFlattened = the camera with y = Pos().y (vrlimi128)
//   0x8220DD3C  lInverseTrafficTransform = InverseOfMatrixWithOrthonormal3x3 (ConsoleVpu's: the transposed 3x3, the
//               translation accumulated z first)
//   0x8220DD64  lLocalCameraPosFlattened = TransformPoint(that, lCameraPosFlattened), then /= lTrafficExtents
//               (0x8220DD68: times the refined vrefp of 0x8220DCBC / 0x8220DD44, ConsoleVpu::Divide lane by lane)
//   0x8220DD6C..0x8220DD9C  lvfDist = Magnitude(that): vmsum3fp128, the refined vrsqrtefp, 0 for a zero length
//               (ConsoleLength)
//   0x8220DD24  lTrafficExtentAlongY = Abs(lWorldYInTrafficSpace: the inverse's y row) . lTrafficExtents
//   0x8220DDA0  lvfHeight = GetHeightAboveTraffic(that * kvfVehicleCollisionCurveTopScale,
//               that * kvfVehicleCollisionCurveBottomScale, lvfDist)
//   0x8220DDBC  the camera's y = Max(the camera's y, Pos().y + lvfHeight) (vaddfp, vmaxfp, vrlimi128 into the camera
//               as loaded at 0x8220DC50): the camera is lifted onto the roof, never lowered
// ----------------------------------------------------------------------------
void FrustrumCollisionResolver::ResolveVehicleCollision(const Matrix44Affine& lTrafficTransform,
                                                        Vector3 lTrafficExtentsIn, Vector3& lCurrentCameraPos)
{
    const Vector3& lTrafficPos = lTrafficTransform.wAxis;
    if ((lTrafficPos.y - lTrafficExtentsIn.y) - static_cast<f32>(kvfDistBelowCarToIgnoreCollision) > lCurrentCameraPos.y)
        return;

    Vector3 lTrafficAxisUp;
    lTrafficAxisUp.x = lTrafficTransform.xAxis.y;
    lTrafficAxisUp.y = lTrafficTransform.yAxis.y;
    lTrafficAxisUp.z = lTrafficTransform.zAxis.y;
    lTrafficAxisUp.w = lTrafficTransform.wAxis.y;

    const Vector3 lToTraffic   = LaneSubtract(lTrafficPos, lCurrentCameraPos);
    const f32     lfDistanceSq = FlushDenormal(Utils::ConsoleVpu::Dot3(lToTraffic, lToTraffic));

    const f32 lfHorizontalScale = kvfHorizontalExtentsScale;
    Vector3 lTrafficExtents;
    lTrafficExtents.x = lTrafficExtentsIn.x
                      * Utils::ConsoleVpu::MultiplyAdd(KF_VPU_ONE - lTrafficAxisUp.x, lfHorizontalScale, KF_VPU_ONE);
    lTrafficExtents.y = lTrafficExtentsIn.y
                      * Utils::ConsoleVpu::MultiplyAdd(KF_VPU_ONE - lTrafficAxisUp.y, lfHorizontalScale, KF_VPU_ONE);
    lTrafficExtents.z = lTrafficExtentsIn.z
                      * Utils::ConsoleVpu::MultiplyAdd(KF_VPU_ONE - lTrafficAxisUp.z, lfHorizontalScale, KF_VPU_ONE);
    lTrafficExtents.w = lTrafficExtentsIn.w
                      * Utils::ConsoleVpu::MultiplyAdd(KF_VPU_ONE - lTrafficAxisUp.w, lfHorizontalScale, KF_VPU_ONE);

    const Vector3 lTwiceExtents      = LaneScale(lTrafficExtents, KF_VPU_TWO);
    const f32     lfTrafficRadiusSq  = FlushDenormal(Utils::ConsoleVpu::Dot3(lTwiceExtents, lTwiceExtents));
    if (!(lfDistanceSq < lfTrafficRadiusSq))
        return;

    Vector3 lCameraPosFlattened = lCurrentCameraPos;
    lCameraPosFlattened.y = lTrafficPos.y;
    const rw::math::vpu::Matrix44Affine lInverseTrafficTransform =
        Utils::ConsoleVpu::InverseOfMatrixWithOrthonormal3x3(lTrafficTransform);
    const Vector3 lLocal = Utils::ConsoleVpu::TransformPoint(lInverseTrafficTransform, lCameraPosFlattened);
    Vector3 lLocalCameraPosFlattened;
    lLocalCameraPosFlattened.x = Utils::ConsoleVpu::Divide(lLocal.x, lTrafficExtents.x);
    lLocalCameraPosFlattened.y = Utils::ConsoleVpu::Divide(lLocal.y, lTrafficExtents.y);
    lLocalCameraPosFlattened.z = Utils::ConsoleVpu::Divide(lLocal.z, lTrafficExtents.z);
    lLocalCameraPosFlattened.w = Utils::ConsoleVpu::Divide(lLocal.w, lTrafficExtents.w);
    const f32 lfDist = ConsoleLength(lLocalCameraPosFlattened);

    const f32 lfTrafficExtentAlongY =
        FlushDenormal(Utils::ConsoleVpu::Dot3(LaneAbs(lInverseTrafficTransform.yAxis), lTrafficExtents));
    const f32 lfHeight = GetHeightAboveTraffic(lfTrafficExtentAlongY * static_cast<f32>(kvfVehicleCollisionCurveTopScale),
                                               lfTrafficExtentAlongY * static_cast<f32>(kvfVehicleCollisionCurveBottomScale),
                                               lfDist);
    lCurrentCameraPos.y = VmxMax(lCurrentCameraPos.y, lTrafficPos.y + lfHeight);
}

// ----------------------------------------------------------------------------
// FrustrumCollisionResolver::GetHeightAboveTraffic @0x821F9098 (DWARF BrnCollisionPolicy.cpp:333). r3 = the returned
// VecFloat's slot; v1 = lvfTopCurveFactor, v2 = lvfBottomCurveFactor, v3 = lvfDistanceFromCentre.
//   0x821F90DC..0x821F90E8  lvfClampedDistanceFromCentre = Clamp(lvfDistanceFromCentre, 0, 2): vmaxfp128(0, d) then
//               vminfp(2, that) (2 = vcfsx of vspltisw 2)
//   0x821F90EC  lvfCosDistance = XMVectorCos(that * pi / 2) (vmulfp128 by the splat 0x830180F0; bl 0x821F06B0)
//   0x821F9100  lvfEllipsoidCurveHeight = Max(cos, 0) * lvfTopCurveFactor (vmaxfp128, vmulfp128)
//   0x821F9104  + Min(cos, 0) * lvfBottomCurveFactor (vminfp128, one vmaddfp128): the roof over the vehicle's middle
//               (up to the top factor), falling below zero past its edge (down to minus the bottom factor at 2)
// ----------------------------------------------------------------------------
VecFloat FrustrumCollisionResolver::GetHeightAboveTraffic(VecFloat lvfTopCurveFactor, VecFloat lvfBottomCurveFactor,
                                                          VecFloat lvfDistanceFromCentre)
{
    const f32 lfClampedDistanceFromCentre = VmxMin(KF_VPU_TWO, VmxMax(0.0f, static_cast<f32>(lvfDistanceFromCentre)));
    const f32 lfCosDistance               = XboxMath::XMVectorCos(lfClampedDistanceFromCentre * KF_HALF_PI);
    const f32 lfEllipsoidCurveHeight      = VmxMax(lfCosDistance, 0.0f) * static_cast<f32>(lvfTopCurveFactor);
    return VecFloat(Utils::ConsoleVpu::MultiplyAdd(VmxMin(lfCosDistance, 0.0f), lvfBottomCurveFactor,
                                                   lfEllipsoidCurveHeight));
}

} // namespace Camera
} // namespace BrnDirector

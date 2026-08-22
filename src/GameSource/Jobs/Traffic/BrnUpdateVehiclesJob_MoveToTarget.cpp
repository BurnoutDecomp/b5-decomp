// GameSource/Jobs/Traffic/UpdateVehiclesJob.cpp:1341 -- BrnTraffic::UpdateVehiclesJob::
// MoveToTarget @0x8291BEE0. Kinematic mover: walk the front axle toward the target, shape the
// speed, filter steering / roll / pitch, spin the wheels, seat both axles, republish the matrix.
//
// Structural key: TrafficEntityModule::UpdateVehicles_MoveToTarget, Feb-2007
// BrnTrafficEntityModule.cpp:7476. Ship divergences, each asm-attested:
//   - No early exit. One epilogue (0x8291CE3C/0x8291CE44) and no branch reaches it; the leak
//     small-error return became a branchless distance clamp.
//   - No too-close recovery arm. lMoveVector is written once @0x8291C334, read after.
//   - Param::mfSpeedDiff is gone; ParamTransform::GetSpeed @0x827128E0 feeds the stopped test,
//     not a pitch term. Pitch is only re-clamped @0x8291CA18..0x8291CA24.
//   - VehicleAxles::UpdateForRoadCollision became PlaceVehicleOnRoad @0x82918A20.
//   - Vehicle::UpdateMatrix grew the lOldUp fourth argument.
//   - every KF_ constant the leak names is now a tuning lane, seeded by Initialise @0x82919AD0.
//
// VMX128 operand note: xxx128 forms accumulate into the dest (product = operands 1 and 2);
// plain vmaddfp / vnmsubfp print as vD, vA, vB, vC with product vA * vC and addend vB.

#include "GameSource/Jobs/Traffic/BrnUpdateVehiclesJob.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/vector4_operation.h"

#include <cmath>
#include <cstdlib>

namespace BrnTraffic
{
namespace
{
    inline f32 ClampF(f32 lfValue, f32 lfMin, f32 lfMax)
    {
        return lfValue < lfMin ? lfMin : (lfValue > lfMax ? lfMax : lfValue);
    }

    // 0x8291C83C..0x8291C860: strictly-greater / greater-equal pair, so exact zero gives zero.
    inline f32 SgnF(f32 lfValue)
    {
        return lfValue > 0.0f ? 1.0f : (lfValue < 0.0f ? -1.0f : 0.0f);
    }

    // The two wheel-radius sanity streams the console prints (leak :7604 / :7605).
    const f32 KF_MIN_SANE_WHEEL_RADIUS = 0.1f;
    const f32 KF_MAX_SANE_WHEEL_RADIUS = 2.0f;

    // [T2-move] diagnostics. DELETE-WHEN-STABLE.
    CgsDev::Log::DebugPrint* MoveDiagStream()
    {
        static const bool sbEnabled = (getenv("BRN_TRAFFIC_DIAG") != 0);
        if (!sbEnabled || CgsDev::Log::gpDebugPrint == 0)
        {
            return 0;
        }
        return CgsDev::Log::gpDebugPrint;
    }
}

// @0x8291BEE0. lTargetPos arrives in v1, lbPartialUpdate in r4.
void UpdateVehiclesJob::MoveToTarget(Vector3 lTargetPos, bool lbPartialUpdate)
{
    Vehicle* lpVehicle = GetCurrentVehicle();
    VehicleAxles* lpVehicleAxles = GetCurrentVehicleAxles();
    Matrix44Affine lVehicleTransform = GetCurrentVehicleTransform();

    CGS_ASSERT(lpVehicle->IsAlive(), "lpVehicle->IsAlive()");
    CGS_ASSERT(lpVehicleAxles, "lpVehicleAxles");
    CGS_ASSERT(rw::math::vpu::IsValid(lVehicleTransform), "RwMath::IsValid( lVehicleTransform )");

    const VehicleTypeRuntime* lpVehicleTypeRuntime = GetCurrentVehicleTypeRuntime();
    CGS_ASSERT(lpVehicleTypeRuntime, "lpVehicleTypeRuntime");

    // 0x8291C264: lane 2 of mCabPivot_TrailerPivot_BackAxle_FwdAxle.
    const f32 lfBackAxleOffset = lpVehicleTypeRuntime->GetBackAxleOffset();

    const Vector3 lOldFrontAxlePos = lpVehicleAxles->mFrontAxle.mPosAndWheelRadius.GetVector3();
    CGS_ASSERT(rw::math::vpu::IsValid(lOldFrontAxlePos), "RwMath::IsValid( lOldFrontAxlePos )");

    // 0x8291C334. Written once; every later reference is a read.
    const Vector3 lMoveVector = lTargetPos - lOldFrontAxlePos;
    CGS_ASSERT(rw::math::vpu::IsValid(lMoveVector), "RwMath::IsValid( lMoveVector )");

    // 0x8291C3F8 + 0x8291C594: |lMoveVector| via vmsum3fp128 and a two-step rsqrt refinement,
    // vsel to 0 when the squared length is exactly 0.
    const f32 lfMaxDistanceToMove = rw::math::vpu::Magnitude(lMoveVector);

    Vector4 lPitch_Roll_Steering_WheelRot = lpVehicle->GetPitch_Roll_Steering_WheelRot();

    const Param* lpParam = GetCurrentParam();
    const ParamTransform* lpParamTransform = GetCurrentParamTransform();
    const Vector3 lParamDir = lpParamTransform->GetDirection();

    // 0x8291C46C: 0.1f (flt_82004014, the same value as E_TUNE_LIMITS.w) over Param+0x3C.
    const f32 lfPitchFactor = maTuning[E_TUNE_LIMITS].w / lpParam->mfFrontDist;

    const Vector3 lAt = lVehicleTransform.At();

    // 0x8291C464/0x8291C4B4: Dot(lMoveVector, At) minus E_TUNE_SWERVE.z.
    const f32 lfSpeedModifier =
        rw::math::vpu::Dot(lMoveVector, lAt) - maTuning[E_TUNE_SWERVE].z;

    const f32 lfSimTimeStep = maTuning[E_TUNE_TIME].z;

    // 0x8291C4C0..0x8291C524: cubic shaping, E_TUNE_SWERVE.w gain, integrated over one step.
    const f32 lfSpeedShaped =
        (lfSpeedModifier * lfSpeedModifier * lfSpeedModifier) + lfSpeedModifier;
    const f32 lfNewSpeed = lpVehicle->GetSpeed().x
                           + (lfSpeedShaped * maTuning[E_TUNE_SWERVE].w) * lfSimTimeStep;

    // 0x8291C514..0x8291C584: both speeds below E_TUNE_MOVE.x hold the vehicle still. Both
    // compares splat LANE 0 (lvsl with r31 == 0 at 0x8291C4D4 and 0x8291C4E0).
    const f32 lfStoppedSpeed = maTuning[E_TUNE_MOVE].x;
    const bool lbStopped = (lfNewSpeed < lfStoppedSpeed)
                           && (lpParamTransform->GetSpeed().x < lfStoppedSpeed);

    // 0x8291C59C..0x8291C5A8: the length ceiling collapses to 0 when stopped, and the whole
    // distance collapses to 0 under mpParams->mbDEBUGStopTrafficMoving (params+0x77).
    const f32 lfDistanceCeiling = lbStopped ? 0.0f : lfMaxDistanceToMove;
    f32 lfDistanceToMove = ClampF(lfNewSpeed * lfSimTimeStep, 0.0f, lfDistanceCeiling);
    if (mpParams->mbDEBUGStopTrafficMoving)
    {
        lfDistanceToMove = 0.0f;
    }

    // 0x8291C5B0..0x8291C5F8: the amortised param direction. E_TUNE_LANE.x, the NEW speed.
    const f32 lfDot = rw::math::vpu::Dot(lParamDir, lAt);
    const f32 lfTurningFactor = lfDot < 0.0f ? -lfDot : lfDot;
    const Vector3 lAmortizedParamDir =
        lParamDir * (lfDistanceToMove * lfNewSpeed * maTuning[E_TUNE_LANE].x * lfTurningFactor);

    CGS_ASSERT(rw::math::vpu::IsValid(lMoveVector), "RwMath::IsValid( lMoveVector )");
    CGS_ASSERT(rw::math::vpu::IsValid(lAmortizedParamDir), "RwMath::IsValid( lAmortizedParamDir )");

    // 0x8291C768..0x8291C7E0
    const Vector3 lNewVelocity = lMoveVector + lAmortizedParamDir;
    Vector3 lNewDirection = rw::math::vpu::IsZero(lNewVelocity)
                                ? lParamDir
                                : rw::math::vpu::Normalize(lNewVelocity);

    // 0x8291C7E4..0x8291C898: sin(theta) is the signed magnitude of Cross(newDir, At); the
    // DELTA against the previous steering is rate-limited to +/- E_TUNE_ANGLES.w.
    const Vector3 lCrossProduct = rw::math::vpu::Cross(lNewDirection, lAt);
    const f32 lfSinThetaRaw =
        rw::math::vpu::Magnitude(lCrossProduct) * SgnF(lCrossProduct.y);

    const f32 lfOldSteering = lPitch_Roll_Steering_WheelRot.z;
    const f32 lfMaxSteeringDelta = maTuning[E_TUNE_ANGLES].w;   // 0.025
    const f32 lfSteeringDelta =
        ClampF(lfSinThetaRaw - lfOldSteering, -lfMaxSteeringDelta, lfMaxSteeringDelta);

    // 0x8291C89C..0x8291C918: the second GetCurrentVehicleTypeRuntime call. The absolute
    // steering limit lerps E_TUNE_ANGLES.x -> .y by (mBBoxHalfSize.z * E_TUNE_ANGLES.z).
    const VehicleTypeRuntime* lpSteeringTypeRuntime = GetCurrentVehicleTypeRuntime();
    const f32 lfLerp = lpSteeringTypeRuntime->GetBBoxHalfSize().z * maTuning[E_TUNE_ANGLES].z;
    const f32 lfMaxSteering = maTuning[E_TUNE_ANGLES].x
                              + (maTuning[E_TUNE_ANGLES].y - maTuning[E_TUNE_ANGLES].x) * lfLerp;

    const f32 lfSinTheta =
        ClampF(lfOldSteering + lfSteeringDelta, -lfMaxSteering, lfMaxSteering);
    lPitch_Roll_Steering_WheelRot.z = lfSinTheta;

    // 0x8291C930..0x8291C9CC: cos from sqrt(1 - sin^2), then the heading rebuilt in XZ.
    const f32 lfCosTheta = std::sqrt(1.0f - (lfSinTheta * lfSinTheta));
    lNewDirection.x = (lAt.x * lfCosTheta) - (lAt.z * lfSinTheta);
    lNewDirection.z = (lAt.x * lfSinTheta) + (lAt.z * lfCosTheta);

    // 0x8291C988..0x8291C9F0: roll. Steering doubled and clamped, scaled by
    // newSpeed * E_TUNE_FILTERS.x and clamped again, weighted by E_TUNE_FILTERS.y, then
    // filtered toward the old roll by E_TUNE_FILTERS.z.
    const f32 lfDesiredRoll0 = ClampF(lfSinTheta * 2.0f, -1.0f, 1.0f);
    const f32 lfDesiredRoll1 =
        ClampF(lfDesiredRoll0 * (lfNewSpeed * maTuning[E_TUNE_FILTERS].x), -1.0f, 1.0f);
    const f32 lfDesiredRoll = lfDesiredRoll1 * maTuning[E_TUNE_FILTERS].y;
    const f32 lfOldRoll = lPitch_Roll_Steering_WheelRot.y;
    lPitch_Roll_Steering_WheelRot.y =
        lfOldRoll + (lfDesiredRoll - lfOldRoll) * maTuning[E_TUNE_FILTERS].z;

    // 0x8291CA18..0x8291CA24: pitch is only re-clamped. The ship computes no desired pitch;
    // ParamTransform::GetSpeed feeds the stopped test above instead.
    lPitch_Roll_Steering_WheelRot.x =
        ClampF(lPitch_Roll_Steering_WheelRot.x, -lfPitchFactor, lfPitchFactor);

    CGS_ASSERT(rw::math::vpu::IsValid(lNewDirection), "RwMath::IsValid( lNewDirection )");
    CGS_ASSERT(lfDistanceToMove == lfDistanceToMove, "RwMath::IsValid( lfDistanceToMove )");

    // 0x8291CB50..0x8291CB98: seat both axles. The vsel pair holds both when lbStopped.
    if (!lbStopped)
    {
        const Vector3 lFrontAxleDiff = lNewDirection * lfDistanceToMove;
        const Vector3 lBackAxlePos = lVehicleTransform.Pos() + (lAt * lfBackAxleOffset);

        lpVehicleAxles->mFrontAxle.mPosAndWheelRadius.SetVector3(lOldFrontAxlePos + lFrontAxleDiff);
        lpVehicleAxles->mBackAxle.mPosAndWheelRadius.SetVector3(lBackAxlePos);
    }

    if (!lbPartialUpdate)
    {
        PlaceVehicleOnRoad();
    }

    // Console streams "Stupidly small/large wheels on traffic vehicle: <r>" (leak :7604/:7605).
    const f32 lfWheelRadius = lpVehicleAxles->mFrontAxle.mPosAndWheelRadius.GetPlus();
    CGS_ASSERT(lfWheelRadius > KF_MIN_SANE_WHEEL_RADIUS, "Stupidly small wheels on traffic vehicle");
    CGS_ASSERT(lfWheelRadius < KF_MAX_SANE_WHEEL_RADIUS, "Stupidly large wheels on traffic vehicle");

    // 0x8291CDF8..0x8291CE0C: distance over radius, wrapped once at E_TUNE_LIMITS.y (2*pi).
    const f32 lfNewRotationUnclamped =
        lPitch_Roll_Steering_WheelRot.w + (lfDistanceToMove / lfWheelRadius);
    const f32 lfTwoPi = maTuning[E_TUNE_LIMITS].y;
    lPitch_Roll_Steering_WheelRot.w = lfNewRotationUnclamped > lfTwoPi
                                          ? lfNewRotationUnclamped - lfTwoPi
                                          : lfNewRotationUnclamped;

    // 0x8291CE14..0x8291CE30: rebuild and publish. lOldUp is the pre-move Up row (v111).
    const Vector3 lOldUp = lVehicleTransform.Up();
    lpVehicle->UpdateMatrix(lpVehicleAxles, lVehicleTransform, lpVehicleTypeRuntime, lOldUp);
    SetCurrentVehicleTransform(lVehicleTransform);
    lpVehicle->SetPitch_Roll_Steering_WheelRot(lPitch_Roll_Steering_WheelRot);

    if (CgsDev::Log::DebugPrint* lpDiag = MoveDiagStream())
    {
        // [T2-move] one-shot + a value-latched repeat past 0.05 m. DELETE-WHEN-STABLE.
        const f32 lfDelta = lfDistanceToMove < 0.0f ? -lfDistanceToMove : lfDistanceToMove;
        static bool sbFirst = true;
        static bool sbBigDelta = false;
        if (sbFirst || (!sbBigDelta && lfDelta > 0.05f))
        {
            const bool lbWasFirst = sbFirst;
            sbFirst = false;
            if (lfDelta > 0.05f)
            {
                sbBigDelta = true;
            }
            *lpDiag << (lbWasFirst ? "[T2-move] FIRST vehicle=" : "[T2-move] DELTA>0.05 vehicle=")
                    << static_cast<s32>(muCurrentVehicle)
                    << " dPos=" << lfDelta
                    << " speed=" << lfNewSpeed
                    << " steering=" << lfSinTheta
                    << (lbStopped ? " stopped=1\n" : " stopped=0\n");
        }
    }
}

} // namespace BrnTraffic

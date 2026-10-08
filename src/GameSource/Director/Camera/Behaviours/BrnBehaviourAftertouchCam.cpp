// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCam.cpp
//
// BrnDirector::Camera::BehaviourAftertouchCam -- all six of the aftertouch camera's Behaviour
// overrides (Construct, Prepare, Update, GetCollisionPolicy, SetupTweaker, GetName) and its two
// private rival helpers (CalculateDesiredTargetPos, AssignIfBetterRival).
//
// Every vector operation below is rounded the way the console rounds it (the campaign rounding
// rule): vmsum3fp128 dots (Utils::ConsoleVpu::Dot3), vrsqrtefp estimates refined by two Newton
// steps, fused vmaddfp lanes and products (Utils::ConsoleVpu), and the pitch rotation's sine and
// cosine from the XDK's XMVectorSinCos. The console's vector IsZero counts a NaN lane as zero; so
// does the file-local IsZeroVmx.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCam.h"

#include "GameSource/Director/Camera/Camera.h"                     // Camera (Update publishes into it)
#include "GameSource/Director/Camera/BrnCameraState.h"             // CameraState::E_FLAG_VALID
#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"     // Utils::Tweaker::Construct
#include "GameSource/Director/Camera/Utils/CameraUtils.h"          // CreateLookAt / SafeSLerp / PointsWillPassEachOther
#include "GameSource/Director/Camera/Utils/BrnConsoleVpu.h"        // Utils::ConsoleVpu (the fused VMX forms)
#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"       // Utils::CameraShake::Update
#include "SDKs/XboxMath/XMVectorSinCos.h"                          // XboxMath::XMVectorSinCos
#include "GameShared/GameClasses/Development/Log/CgsLog.h"         // [FLAG PC witness] BRN_CAMBODY_DIAG

#include <cmath>     // std::fabs / std::sqrt
#include <cstdlib>   // getenv ([FLAG PC witness] BRN_CAMBODY_DIAG)

namespace
{
    using rw::math::vpu::Matrix44Affine;
    using rw::math::vpu::Vector3;

    const f32 KF_EPSILON      = 1.1920929e-07f;      // the vector IsZero tolerance (0x34000000)
    const f32 KF_SMALL_FLOAT  = 1.52587890625e-05f;  // the IsSimilar tolerance (0x37800000)
    const f32 KF_DEGS_TO_RADS = 0.017453292f;        // 0x3C8EFA35

    // The console's vector IsZero: the three lanes' magnitudes compared `> tolerance` by one
    // vcmpgtfp. (x copied into w), true when no lane is above it -- a NaN lane counts as zero.
    bool IsZeroVmx(const Vector3& lrVector, f32 lfTolerance)
    {
        return !(std::fabs(lrVector.x) > lfTolerance
                 || std::fabs(lrVector.y) > lfTolerance
                 || std::fabs(lrVector.z) > lfTolerance);
    }

    // The same test on a Vector2 taken from lanes x and z (a vperm of x and z), one lane at a time.
    bool IsZeroXZ(f32 lfX, f32 lfZ, f32 lfTolerance)
    {
        return !(std::fabs(lfX) > lfTolerance) && !(std::fabs(lfZ) > lfTolerance);
    }

    // The vrsqrtefp estimate refined by TWO Newton-Raphson steps:
    //   e2 = e * e ; h = e * 0.5 ; r = -(x * e2 - 1) fused ; e = h * r + e fused.
    // FLAG (model, rule 5): the hardware estimate is taken as the correctly rounded 1 / sqrt(x).
    f32 RefinedReciprocalSqrt(f32 lfValue)
    {
        f32 lfEstimate = static_cast<f32>(1.0 / std::sqrt(static_cast<f64>(lfValue)));
        for (s32 liStep = 0; liStep < 2; ++liStep)
        {
            const f32 lfEstimateSquared = lfEstimate * lfEstimate;
            const f32 lfHalfEstimate    = lfEstimate * 0.5f;
            const f32 lfResidual = BrnDirector::Camera::Utils::ConsoleVpu::NegativeMultiplySubtract(
                lfValue, lfEstimateSquared, 1.0f);
            lfEstimate = BrnDirector::Camera::Utils::ConsoleVpu::MultiplyAdd(lfHalfEstimate, lfResidual, lfEstimate);
        }
        return lfEstimate;
    }

    // x * the refined 1 / sqrt(x), with the vcmpeqfp / vsel guard that turns a zero x into 0.
    f32 GuardedRoot(f32 lfValue)
    {
        return (lfValue == 0.0f) ? 0.0f : lfValue * RefinedReciprocalSqrt(lfValue);
    }

    // vmulfp of every lane by one scalar.
    Vector3 Scale(const Vector3& lrVector, f32 lfScalar)
    {
        return Vector3{ lrVector.x * lfScalar, lrVector.y * lfScalar, lrVector.z * lfScalar, lrVector.w * lfScalar };
    }

    // rw::math::vpu::Matrix44AffineFromXRotationAngle with the XDK sine and cosine: rows
    // (1,0,0) / (0,c,s) / (0,-s,c) / 0, packed with vperm / vrlimi128 (no arithmetic). The
    // console's w lanes carry copies; Mult reads x, y and z only.
    Matrix44Affine RotationX(f32 lfAngleRads)
    {
        f32 lfSin;
        f32 lfCos;
        XboxMath::XMVectorSinCos(&lfSin, &lfCos, lfAngleRads);
        Matrix44Affine lResult;
        lResult.xAxis = Vector3{ 1.0f,   0.0f,  0.0f, 0.0f };
        lResult.yAxis = Vector3{ 0.0f,  lfCos, lfSin, 0.0f };
        lResult.zAxis = Vector3{ 0.0f, -lfSin, lfCos, 0.0f };
        lResult.wAxis = Vector3{ 0.0f,   0.0f,  0.0f, 0.0f };
        return lResult;
    }

    // [FLAG PC witness] BRN_CAMBODY_DIAG -- NOT IN THE CONSOLE BUILD. One line per Construct /
    // Prepare and the first frames of every Update run, capped per session. Prints only.
    bool CamBodyDiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_CAMBODY_DIAG") != 0);
        return sbEnabled && CgsDev::Log::gpDebugPrint != 0;
    }

    s32 siCamBodyDiagLinesLeft = 64;
    s32 siCamBodyDiagFrame     = 0;
}

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// Construct -- seed a freshly pooled instance: the base head (the seven stores that are
// Behaviour::Construct), CollisionPolicyAttachedToVehicle::Construct(policy, false), then the
// three embedded utilities' own Constructs, all inlined on the console: the position lag's two
// flags, the random stream's default seed and primed ring, and the shake's four zeroed words.
// mpParameters is SetParameters', the height / distance / blend are Prepare's.
// ----------------------------------------------------------------------------
void BehaviourAftertouchCam::Construct()
{
    Behaviour::Construct();

    mCollisionPolicy.Construct(false);

    mPositionLag.Construct();
    mRandom.Construct();
    mShake.Construct();

    if (CamBodyDiagEnabled() && siCamBodyDiagLinesLeft > 0)
    {
        --siCamBodyDiagLinesLeft;
        *CgsDev::Log::gpDebugPrint << "[cambody] AftertouchCam Construct [FLAG PC witness]\n";
    }
}

// ----------------------------------------------------------------------------
// Prepare -- drop the prepared latch (Update's first frame seeds the directions and the target),
// then start from the block's fast height and distance and its minimum blend factor. The shared
// prepare/release info is not read. Always true.
// ----------------------------------------------------------------------------
bool BehaviourAftertouchCam::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    SetNotPrepared();

    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");                                              // :99

    mfBlendFactor = mpParameters->mfMinimumBlendFactor;
    mfHeight      = mpParameters->mfMaxHeight;
    mfDistance    = mpParameters->mfMaxDistance;

    if (CamBodyDiagEnabled() && siCamBodyDiagLinesLeft > 0)
    {
        --siCamBodyDiagLinesLeft;
        siCamBodyDiagFrame = 0;
        *CgsDev::Log::gpDebugPrint << "[cambody] AftertouchCam Prepare height " << mfHeight
                                   << " distance " << mfDistance << " blend " << mfBlendFactor
                                   << " [FLAG PC witness]\n";
    }
    return true;
}

// ----------------------------------------------------------------------------
// AssignIfBetterRival -- weigh race car luRivalToConsiderForBest against the best rival so far.
//
// The rival "will pass" when the two cars close on each other (Utils::PointsWillPassEachOther)
// sooner than the crash time remaining plus the uncertainty padding; failing that it "will pass
// if the heading is adjusted" when the gap divided by the difference of the two speeds is within
// the same window. A better rival is, in order: one that will pass over one that will not (two
// that will pass: the sooner, unless the two times are within the timing threshold); then the
// same on the heading-adjusted test; then, with neither, a rival behind the camera direction
// whose predecessor was ahead of it and that is nearer by more than the distance threshold.
// ----------------------------------------------------------------------------
void BehaviourAftertouchCam::AssignIfBetterRival(const BehaviourSharedInfo& lrSharedInfo,
                                                 AftertouchRival& lCurrentBestRivalInOut,
                                                 u32 luRivalToConsiderForBest)
{
    const VehicleInfo& lrRival = lrSharedInfo.mpRaceCars[luRivalToConsiderForBest];

    const Vector3 lPlayerPos = lrSharedInfo.mPlayerInfo.mRaceCarState.mTransform.wAxis;
    const Vector3 lPlayerVel = lrSharedInfo.mPlayerInfo.mRaceCarState.mLinearVelocity;
    const Vector3 lRivalPos  = lrRival.mRaceCarState.mTransform.wAxis;
    const Vector3 lRivalVel  = lrRival.mRaceCarState.mLinearVelocity;

    // NormalizeReturnMagnitude: one refined estimate gives the unit vector and the length.
    const Vector3 lRivalToPlayer          = lPlayerPos - lRivalPos;
    const f32     lfRivalToPlayerSquared  = Utils::ConsoleVpu::Dot3(lRivalToPlayer, lRivalToPlayer);
    const f32     lfRivalToPlayerRecip    = RefinedReciprocalSqrt(lfRivalToPlayerSquared);
    const Vector3 lRivalToPlayerNormalised = Scale(lRivalToPlayer, lfRivalToPlayerRecip);
    const f32     lfDistanceToPlayer = (lfRivalToPlayerSquared == 0.0f)
                                     ? 0.0f : lfRivalToPlayerSquared * lfRivalToPlayerRecip;

    AftertouchRival& lrBest = lCurrentBestRivalInOut;
    if (!lrBest.mbIsValid)
    {
        lrBest.mbWillPass                  = false;
        lrBest.mbWillPassIfHeadingAdjusted = false;
        lrBest.mfDotWithCurrentHeading     = -1.0f;
        lrBest.mfDistance                  = mpParameters->mfMaximumDistanceForConsiderationOfRivals;
    }

    const f32 lfImpactWindow = mpParameters->mfTimeToRivalImpactUncertaintyPadding
                             + lrSharedInfo.mfCrashTimeRemaining;

    // FLAG PC-platform: the console leaves this unwritten when the points do not close; it is
    // stored into the rival either way but only read back where mbWillPass holds.
    f32 lfTimeToPassing = 0.0f;
    const bool lbPointsWillPassEachOther =
        Utils::PointsWillPassEachOther(lPlayerPos, lPlayerVel, lRivalPos, lRivalVel, lfTimeToPassing);
    const bool lbWillPass = lbPointsWillPassEachOther && lfTimeToPassing < lfImpactWindow;

    bool lbWillPassIfHeadingAdjusted = false;
    f32  lfTimeToPassingIfAdjusted   = 0.0f;
    if (lbWillPass)
    {
        lfTimeToPassingIfAdjusted   = lfTimeToPassing;
        lbWillPassIfHeadingAdjusted = true;
    }
    else
    {
        const f32 lfRivalSpeed   = GuardedRoot(Utils::ConsoleVpu::Dot3(lRivalVel, lRivalVel));
        const f32 lfPlayerSpeed  = GuardedRoot(Utils::ConsoleVpu::Dot3(lPlayerVel, lPlayerVel));
        const f32 lfClosingSpeed = lfPlayerSpeed - lfRivalSpeed;
        if (lfClosingSpeed > KF_EPSILON)
        {
            lfTimeToPassingIfAdjusted = lfDistanceToPlayer / lfClosingSpeed;
            if (lfTimeToPassingIfAdjusted < lfImpactWindow)
            {
                lbWillPassIfHeadingAdjusted = true;
            }
        }
    }

    const f32 lfDotWithHeading = Utils::ConsoleVpu::Dot3(lRivalToPlayerNormalised, mWorldSpaceNormalizedVectorFromCar);

    bool lbNewRivalIsBetter = false;
    if (lrBest.mbWillPass || lbWillPass)
    {
        if (!lrBest.mbWillPass)
        {
            lbNewRivalIsBetter = true;
        }
        else if (lbWillPass)
        {
            const bool lbSimilarTiming = std::fabs(lrBest.mfTimeToPassing - lfTimeToPassing)
                                       < mpParameters->mfTimingSimilarityThreshold;
            lbNewRivalIsBetter = !lbSimilarTiming && lrBest.mfTimeToPassing > lfTimeToPassing;
        }
    }
    else if (lrBest.mbWillPassIfHeadingAdjusted || lbWillPassIfHeadingAdjusted)
    {
        if (!lrBest.mbWillPassIfHeadingAdjusted)
        {
            lbNewRivalIsBetter = true;
        }
        else if (lbWillPassIfHeadingAdjusted)
        {
            const bool lbSimilarTiming =
                std::fabs(lrBest.mfTimeToPassingIfHeadingAdjusted - lfTimeToPassingIfAdjusted)
                < mpParameters->mfTimingSimilarityThreshold;
            lbNewRivalIsBetter = !lbSimilarTiming
                              && lrBest.mfTimeToPassingIfHeadingAdjusted > lfTimeToPassingIfAdjusted;
        }
    }
    else if (lfDotWithHeading < 0.0f && lrBest.mfDotWithCurrentHeading > 0.0f)
    {
        const bool lbSimilarDistance = std::fabs(lfDistanceToPlayer - lrBest.mfDistance)
                                     < mpParameters->mfDistanceSimilarityThreshold;
        lbNewRivalIsBetter = !lbSimilarDistance && lfDistanceToPlayer < lrBest.mfDistance;
    }

    if (lbNewRivalIsBetter)
    {
        lrBest.mfTimeToPassing                  = lfTimeToPassing;
        lrBest.mfTimeToPassingIfHeadingAdjusted = lfTimeToPassingIfAdjusted;
        lrBest.mfDotWithCurrentHeading          = lfDotWithHeading;
        lrBest.mbWillPass                       = lbWillPass;
        lrBest.mfDistance                       = lfDistanceToPlayer;
        lrBest.mbWillPassIfHeadingAdjusted      = lbWillPassIfHeadingAdjusted;
        lrBest.mPosition                        = lRivalPos;
        lrBest.mbIsValid                        = true;
    }
}

// ----------------------------------------------------------------------------
// CalculateDesiredTargetPos -- sweep every used race car but the player's for the best rival.
// Choosing one restarts the decision clock and returns its position; with none the current
// target stands.
// ----------------------------------------------------------------------------
Vector3 BehaviourAftertouchCam::CalculateDesiredTargetPos(const BehaviourSharedInfo& lrSharedInfo,
                                                          Vector3 lCurrentTargetPos)
{
    AftertouchRival lPreferredRival;
    lPreferredRival.mbIsValid = false;

    for (EActiveRaceCarIndex leRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
         leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; leRaceCarIndex++)
    {
        if (lrSharedInfo.mUsedRaceCars.IsBitSet(static_cast<u32>(leRaceCarIndex))
            && leRaceCarIndex != lrSharedInfo.mePlayerCarIndex)
        {
            AssignIfBetterRival(lrSharedInfo, lPreferredRival, static_cast<u32>(leRaceCarIndex));
        }
    }

    Vector3 lDesiredTargetPos;
    if (lPreferredRival.mbIsValid)
    {
        mfTimeSinceLastDecision = 0.0f;
        lDesiredTargetPos = lPreferredRival.mPosition;
    }
    else
    {
        lDesiredTargetPos = lCurrentTargetPos;
    }
    return lDesiredTargetPos;
}

// ----------------------------------------------------------------------------
// Update.
//   1. The player's speed and direction of travel (NormalizeReturnMagnitude), and the player's
//      frame lagged by the block's position lag; its translation is the framing origin.
//   2. The first frame after Prepare looks along the car's nose and aims one metre below it.
//   3. Every mfTimeBetweenDecisions the target is re-chosen among the rivals.
//   4. The desired direction is away from the target, never above the car (|y|), and the height
//      and distance ease toward the shot's slow-to-fast values by speed.
//   5. Unless the current direction is already there, it swings toward the desired one by the
//      eased blend factor; when the aftertouch input agrees with the car's travel as seen by the
//      camera, part of that swing goes toward the car's travel first.
//   6. A look-at from the direction * distance at the origin, lifted by the height, pitched by the
//      shot's pitch, shaken, published with the shot's FOV and marked valid.
// ----------------------------------------------------------------------------
bool BehaviourAftertouchCam::Update(Camera& lrCamera, const BehaviourSharedInfo& lrSharedInfo)
{
    CGS_ASSERT(mpParameters != 0, "mpParameters != NULL");                                              // :310
    CGS_ASSERT(mSourceShot.IsValid(), "mParameters.IsValid()");                                         // :311

    const BrnPhysics::Vehicle::RaceCarState& lrCarState = lrSharedInfo.mPlayerInfo.mRaceCarState;
    const f32 lfTimestep = lrSharedInfo.GetTimestep(BrnDirector::Timestep::E_WORLD);

    const f32     lfPlayerSpeedSquared = Utils::ConsoleVpu::Dot3(lrCarState.mLinearVelocity, lrCarState.mLinearVelocity);
    const f32     lfPlayerSpeedRecip   = RefinedReciprocalSqrt(lfPlayerSpeedSquared);
    const f32     lfPlayerSpeed        = (lfPlayerSpeedSquared == 0.0f) ? 0.0f : lfPlayerSpeedSquared * lfPlayerSpeedRecip;
    const Vector3 lNormalisedPlayerVel = Scale(lrCarState.mLinearVelocity, lfPlayerSpeedRecip);

    Matrix44Affine lPlayerTransform = lrCarState.mTransform;
    mPositionLag.Update(mpParameters->mLagParams, lfTimestep, lPlayerTransform);
    const Vector3 lCarPosition = lPlayerTransform.wAxis;

    const bool lbDiagFirstFrame = !IsPrepared();   // [FLAG PC witness] BRN_CAMBODY_DIAG only

    if (!IsPrepared())
    {
        mDesiredWorldSpaceNormalizedVectorFromCar = lPlayerTransform.zAxis;
        mWorldSpaceNormalizedVectorFromCar        = lPlayerTransform.zAxis;

        const Vector3 lOneMetreDown = { 0.0f, -1.0f, 0.0f, 0.0f };
        mCrashPoint       = lCarPosition + lOneMetreDown;
        mCurrentTargetPos = lCarPosition + lOneMetreDown;

        SetPrepared();
        mfTimeSinceLastDecision = mpParameters->mfTimeBetweenDecisions;
    }

    // `fcmpu ; blt` past the call: a NaN clock re-decides too.
    if (!(mfTimeSinceLastDecision < mpParameters->mfTimeBetweenDecisions))
    {
        mCurrentTargetPos = CalculateDesiredTargetPos(lrSharedInfo, mCurrentTargetPos);
    }

    Vector3 lDesiredVector = mCurrentTargetPos - lCarPosition;
    mfTimeSinceLastDecision = lrSharedInfo.GetTimestep(BrnDirector::Timestep::E_WORLD_NO_SLOMO)
                            + mfTimeSinceLastDecision;
    lDesiredVector.y = std::fabs(lDesiredVector.y);

    if (!IsZeroVmx(lDesiredVector, KF_EPSILON))
    {
        mDesiredWorldSpaceNormalizedVectorFromCar = -Scale(lDesiredVector,
            RefinedReciprocalSqrt(Utils::ConsoleVpu::Dot3(lDesiredVector, lDesiredVector)));
        CGS_ASSERT(!IsZeroVmx(mDesiredWorldSpaceNormalizedVectorFromCar, KF_EPSILON),
                   "!IsZero(mDesiredWorldSpaceNormalizedVectorFromCar)");                                // :348
    }

    if (lfPlayerSpeed < mpParameters->mfHeightDistanceVelocityRange)
    {
        // value = ((max - min) / range * speed + min - value) * blend + value: fsubs, fdivs, fmadds,
        // fsubs, fmadds.
        const f32 lfRange = mpParameters->mfHeightDistanceVelocityRange;
        const f32 lfBlend = mpParameters->mfHeightDistanceBlendFactor;

        const f32 lfHeightTarget = Utils::ConsoleVpu::MultiplyAdd(
            (mSourceShot.MaxHeight() - mSourceShot.MinHeight()) / lfRange, lfPlayerSpeed, mSourceShot.MinHeight());
        mfHeight = Utils::ConsoleVpu::MultiplyAdd(lfHeightTarget - mfHeight, lfBlend, mfHeight);

        const f32 lfDistanceTarget = Utils::ConsoleVpu::MultiplyAdd(
            (mSourceShot.MaxDistance() - mSourceShot.MinDistance()) / lfRange, lfPlayerSpeed, mSourceShot.MinDistance());
        mfDistance = Utils::ConsoleVpu::MultiplyAdd(lfDistanceTarget - mfDistance, lfBlend, mfDistance);
    }

    const f32 lfDesiredVectorLength = GuardedRoot(Utils::ConsoleVpu::Dot3(lDesiredVector, lDesiredVector));

    if (!IsZeroVmx(mWorldSpaceNormalizedVectorFromCar - mDesiredWorldSpaceNormalizedVectorFromCar, KF_SMALL_FLOAT))
    {
        // `fcmpu len, 2.0 ; bge` -> the maximum (a NaN length too), else the minimum.
        const f32 lfTargetBlend = (lfDesiredVectorLength < 2.0f) ? mpParameters->mfMinimumBlendFactor
                                                                 : mpParameters->mfMaximumBlendFactor;
        mfBlendFactor = Utils::ConsoleVpu::MultiplyAdd(lfTargetBlend - mfBlendFactor,
                                                       mpParameters->mfBlendFactorBlendFactor, mfBlendFactor);
        const f32 lfT = mfBlendFactor;

        // The car's velocity in the camera's frame (the camera's orthonormal inverse), and both
        // velocities as (x, z) pairs.
        const Vector3 lCameraSpaceCarVelocity = Utils::ConsoleVpu::TransformVector(
            Utils::ConsoleVpu::InverseOfMatrixWithOrthonormal3x3(lrCamera.GetTransform()), lrCarState.mLinearVelocity);

        if (IsZeroXZ(lCameraSpaceCarVelocity.x, lCameraSpaceCarVelocity.z, KF_EPSILON)
            || IsZeroXZ(lrCarState.mLinearVelocity.x, lrCarState.mLinearVelocity.z, KF_EPSILON))
        {
            mWorldSpaceNormalizedVectorFromCar = Utils::SafeSLerp(
                mWorldSpaceNormalizedVectorFromCar, mDesiredWorldSpaceNormalizedVectorFromCar, VecFloat(lfT));
            CGS_ASSERT(!IsZeroVmx(mWorldSpaceNormalizedVectorFromCar, KF_EPSILON),
                       "!IsZero(mWorldSpaceNormalizedVectorFromCar)");                                   // :425
        }
        else
        {
            // Normalize(Vector2): x * x + z * z (two vmulfp, one vaddfp), the refined estimate.
            const f32 lfLengthSquared = lCameraSpaceCarVelocity.x * lCameraSpaceCarVelocity.x
                                      + lCameraSpaceCarVelocity.z * lCameraSpaceCarVelocity.z;
            const f32 lfRecip  = RefinedReciprocalSqrt(lfLengthSquared);
            const f32 lfNormX  = lCameraSpaceCarVelocity.x * lfRecip;
            const f32 lfNormZ  = lCameraSpaceCarVelocity.z * lfRecip;
            const f32 lfControlAgreementWithCamera = lrSharedInfo.mCarModifier.x * lfNormX
                                                   + lrSharedInfo.mCarModifier.y * lfNormZ;

            if (lfControlAgreementWithCamera > 0.0f)
            {
                const f32 lfAgreement = (lfControlAgreementWithCamera > 1.0f) ? 1.0f : lfControlAgreementWithCamera;

                CGS_ASSERT(!IsZeroVmx(lNormalisedPlayerVel, KF_EPSILON), "!IsZero(lNormalisedPlayerVel)");     // :404
                mWorldSpaceNormalizedVectorFromCar = Utils::SafeSLerp(
                    mWorldSpaceNormalizedVectorFromCar, -lNormalisedPlayerVel, VecFloat(lfT * lfAgreement));
                mWorldSpaceNormalizedVectorFromCar = Utils::SafeSLerp(
                    mWorldSpaceNormalizedVectorFromCar, mDesiredWorldSpaceNormalizedVectorFromCar,
                    VecFloat(lfT * (1.0f - lfAgreement)));
                CGS_ASSERT(!IsZeroVmx(mWorldSpaceNormalizedVectorFromCar, KF_EPSILON),
                           "!IsZero(mWorldSpaceNormalizedVectorFromCar)");                               // :412
            }
            else
            {
                mWorldSpaceNormalizedVectorFromCar = Utils::SafeSLerp(
                    mWorldSpaceNormalizedVectorFromCar, mDesiredWorldSpaceNormalizedVectorFromCar, VecFloat(lfT));
                CGS_ASSERT(!IsZeroVmx(mWorldSpaceNormalizedVectorFromCar, KF_EPSILON),
                           "!IsZero(mWorldSpaceNormalizedVectorFromCar)");                               // :418
            }
        }

        mWorldSpaceNormalizedVectorFromCar = Scale(mWorldSpaceNormalizedVectorFromCar,
            RefinedReciprocalSqrt(Utils::ConsoleVpu::Dot3(mWorldSpaceNormalizedVectorFromCar,
                                                          mWorldSpaceNormalizedVectorFromCar)));
    }

    CGS_ASSERT(!IsZeroVmx(mWorldSpaceNormalizedVectorFromCar, KF_EPSILON),
               "!IsZero(mWorldSpaceNormalizedVectorFromCar)");                                           // :432
    // `fcmpu d, eps ; bgt` then `fcmpu d, -eps ; bge`: a NaN distance fires it, as on the console.
    CGS_ASSERT(mfDistance > KF_EPSILON || mfDistance < -KF_EPSILON, "!rw::math::fpu::IsZero(mfDistance)"); // :433
    CGS_ASSERT(!IsZeroVmx(Scale(mWorldSpaceNormalizedVectorFromCar, mfDistance), KF_EPSILON),
               "!IsZero(mWorldSpaceNormalizedVectorFromCar * mfDistance)");                             // :434

    Matrix44Affine lTransform = Utils::CreateLookAt(
        Utils::ConsoleVpu::MultiplyAdd(mWorldSpaceNormalizedVectorFromCar, mfDistance, lCarPosition), lCarPosition);
    lTransform.wAxis.y += mfHeight;
    lTransform = Utils::ConsoleVpu::Mult(RotationX(mSourceShot.Pitch() * KF_DEGS_TO_RADS), lTransform);

    mShake.Update(lTransform, mpParameters->mShakeParams, mRandom, lfTimestep, 1.0f);

    lrCamera.SetTransform(lTransform);
    lrCamera.ValidateTransformWithDebugInfo();
    lrCamera.SetFOV(mSourceShot.FOV());
    lrCamera.GetState().SetFlag(CameraState::E_FLAG_VALID, true);

    if (CamBodyDiagEnabled() && siCamBodyDiagLinesLeft > 0)
    {
        if (siCamBodyDiagFrame < 8 || siCamBodyDiagFrame % 60 == 0)
        {
            --siCamBodyDiagLinesLeft;
            const Vector3& lrPublished = lrCamera.GetTransform().wAxis;
            *CgsDev::Log::gpDebugPrint
                << "[cambody] AftertouchCam Update frame " << siCamBodyDiagFrame
                << " first " << (lbDiagFirstFrame ? 1 : 0)
                << " car " << lCarPosition.x << " " << lCarPosition.y << " " << lCarPosition.z
                << " target " << mCurrentTargetPos.x << " " << mCurrentTargetPos.y << " " << mCurrentTargetPos.z
                << " cam " << lrPublished.x << " " << lrPublished.y << " " << lrPublished.z
                << " dir " << mWorldSpaceNormalizedVectorFromCar.x << " " << mWorldSpaceNormalizedVectorFromCar.y
                << " " << mWorldSpaceNormalizedVectorFromCar.z
                << " dist " << mfDistance << " height " << mfHeight << " blend " << mfBlendFactor
                << " pitch " << mSourceShot.Pitch() << " fov " << lrCamera.GetFOV() << " [FLAG PC witness]\n";
        }
        ++siCamBodyDiagFrame;
    }
    return true;
}

// The vehicle-attached policy Construct seeded.
CollisionPolicy* BehaviourAftertouchCam::GetCollisionPolicy()
{
    return &mCollisionPolicy;
}

void BehaviourAftertouchCam::SetupTweaker(Utils::Tweaker& lrTweaker)
{
    lrTweaker.Construct();
}

const char* BehaviourAftertouchCam::GetName() const
{
    return "BehaviourAftertouchCam";
}

} // namespace Camera
} // namespace BrnDirector

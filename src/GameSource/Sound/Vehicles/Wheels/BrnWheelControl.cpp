#include "GameSource/Sound/Vehicles/Wheels/BrnWheelControl.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnEngineControl.h"
#include "GameSource/Sound/Vehicles/Brn3dCarPosition.h"

#include <algorithm>
#include <cmath>

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

namespace
{
const f32 KF_DRIFT_DRIFT_RPM_SCALE_FACTOR = 0.7f;
const f32 KF_DRIFT_RPM_SHIFT_FACTOR = 0.8f;
const f32 KF_DRIFT_RPM_UP_SHIFT_FACTOR = 1.2f;
const f32 KF_DRIFT_RPM_POST_SHIFT_FACTOR = 1.3f;
const f32 KF_DRIFT_TIME_BETWEEN_SHIFTS = 3.0f;
const f32 KF_DRIFT_SHIFT_RISE_FROM_RPM = 3000.0f;
const f32 KF_DRIFT_UP_SHIFT_TARGET_RPM_OFFSET = 1000.0f;

const f32 KF_IN_AIR_RPM_BOOST = 3000.0f;
const f32 KF_IN_AIR_MAX_VOLUME = 1.4f;
const f32 KF_IN_AIR_ASCENDING_TIME = 150.0f;
const f32 KF_IN_AIR_DESCENDING_TIME = 3500.0f;
const f32 KF_IN_AIR_THROTTLE_TRANSITION = 100.0f;

const s32 KAE_FLIPPED_WHEELS_LOOKUP[4] = { 1, 0, 3, 2 };

f32 Clamp(f32 afValue, f32 afMinimum, f32 afMaximum)
{
    return (std::max)(afMinimum, (std::min)(afMaximum, afValue));
}

f32 Approach(f32 afCurrent, f32 afTarget, f32 afRise, f32 afFall)
{
    if (afTarget - afCurrent > afRise)
        return afCurrent + afRise;
    if (afCurrent - afTarget > afFall)
        return afCurrent - afFall;
    return afTarget;
}
}

WheelControl::WheelControl()
    : mWheelAttribs()
    , mWheelData()
    , mWheelStatus()
    , mpPhysicsControl(nullptr)
    , mpShiftControl(nullptr)
    , mpEngineControl(nullptr)
    , mpRight3dControl(nullptr)
    , mpLeft3dControl(nullptr)
    , meInAirRevState(E_IN_AIR_REV_STATE_NONE)
    , mIsOnGround(false)
    , mfAudioRPM(0.0f)
    , mfRPMDueToDrift(0.0f)
    , mfDriftingRPMFactor(0.0f)
    , mfDriftingShiftOccured(0.0f)
    , mbPerformedDriftShift(false)
    , mbIsDriftUpShift(false)
    , mfRPMDueToPeel(0.0f)
    , mfPeelingRPMFactor(0.0f)
    , mPeelOscillator()
    , mRandomGenerator()
    , mInAirRevThrottlePath()
    , mInAirRevRpmInterpolate()
    , mInAirRevVolumeInterpolate()
    , mfTimeInAir(0.0f)
    , mfTimeSinceLanding(0.0f)
{
    mWheelAttribs.mPeelSlow.x = 0.2f;
    mWheelAttribs.mPeelSlow.y = 4.0f;
    mWheelAttribs.mPeelFast.x = 0.2f;
    mWheelAttribs.mPeelFast.y = 4.0f;
    mWheelAttribs.mLateralSlow.x = 1.0f;
    mWheelAttribs.mLateralSlow.y = 7.0f;
    mWheelAttribs.mLateralFast.x = 2.5f;
    mWheelAttribs.mLateralFast.y = 13.0f;
    mWheelAttribs.mBrakeSlow.x = 0.1f;
    mWheelAttribs.mBrakeSlow.y = 0.5f;
    mWheelAttribs.mBrakeFast.x = 0.1f;
    mWheelAttribs.mBrakeFast.y = 0.5f;
    mWheelAttribs.mafSideRightLateralMultipler[0] = 0.8f;
    mWheelAttribs.mafSideRightLateralMultipler[1] = 1.1f;
    mWheelAttribs.mafSideLeftLateralMultipler[0] = 1.2f;
    mWheelAttribs.mafSideLeftLateralMultipler[1] = 0.95f;
    mWheelAttribs.mfSlowFastThreshold = 35.0f;
    mRandomGenerator.Construct();
}

WheelControl::~WheelControl()
{
}

s32 WheelControl::GetController(s32 aiSlot)
{
    static const s32 kaiControllers[] = { 0, 2, 4, 8, 9 };
    return aiSlot >= 0 && aiSlot < 5 ? kaiControllers[aiSlot] : -1;
}

void WheelControl::AttachController(CgsSound::Logic::EffectBase* apController)
{
    switch (apController->GetEffectID())
    {
    case 0: mpPhysicsControl = static_cast<Engines::PhysicsControl*>(apController); return;
    case 2: mpShiftControl = static_cast<Engines::ShiftControl*>(apController); return;
    case 4: mpEngineControl = static_cast<Engines::EngineControl*>(apController); return;
    case 8: mpLeft3dControl = static_cast<LeftSide3dControl*>(apController); return;
    case 9: mpRight3dControl = static_cast<RightSide3dControl*>(apController); return;
    default:
        CGS_ASSERT(false, "Bad attach");
        return;
    }
}

bool WheelControl::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;
    mfTimeInAir = 0.0f;
    mfTimeSinceLanding = 0.0f;
    for (s32 liWheel = 0; liWheel < 4; ++liWheel)
        mWheelStatus[liWheel].mIsOnGround.Flush(true);
    mWheelData.Construct();
    mbPerformedDriftShift = false;
    return true;
}

void WheelControl::UpdateWheelStatus(f32)
{
    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    const BrnSound::Vehicles::VehicleData* lpRaw = mpPhysicsControl->GetRawPhysicsData();
    CGS_ASSERT(lpRaw != nullptr, "mpVehiclePhysicsData");

    s32 liGroundedWheels = 0;
    s32 liAttachedWheels = 0;
    for (s32 liStatus = 0; liStatus < 4; ++liStatus)
    {
        const s32 liWheel = KAE_FLIPPED_WHEELS_LOOKUP[liStatus];
        const BrnPhysics::Vehicle::WheelLite& lrWheel = lpRaw->maWheels[liWheel];
        const bool lbOnGround = lrWheel.mRoadContact.mbIsOnGround &&
            std::fabs(lrWheel.mfSuspensionHeight - -1.0f) >= 0.05f;
        mWheelStatus[liStatus].mIsOnGround.Update(lbOnGround);
        mWheelStatus[liStatus].mSurfaceType.Update(static_cast<u8>(
            (lrWheel.mRoadContact.mCollisionTag.muValue >> 4) & 0x3Fu));
        if (lbOnGround)
            ++liGroundedWheels;
        if (lrWheel.mbAttached)
            ++liAttachedWheels;
    }

    SetMixerInputValue(2, liGroundedWheels == 0 ? 0x7FFF : 0);
    SetMixerInputValue(5, liAttachedWheels == 4 ? 0x7FFF : 0);
    mIsOnGround.Update(liGroundedWheels != 0);
}

f32 WheelControl::LerpedNormalise(const Vector2& arSlow, const Vector2& arFast,
                                  f32 afValue, f32 afFraction) const
{
    const f32 lfMinimum = arSlow.x + (arFast.x - arSlow.x) * afFraction;
    const f32 lfMaximum = arSlow.y + (arFast.y - arSlow.y) * afFraction;
    if (lfMaximum <= lfMinimum)
        return 0.0f;
    return Clamp((std::fabs(afValue) - lfMinimum) / (lfMaximum - lfMinimum), 0.0f, 1.0f);
}

void WheelControl::UpdateSkidValues(f32)
{
    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    const BrnSound::Vehicles::VehicleData* lpRaw = mpPhysicsControl->GetRawPhysicsData();
    CGS_ASSERT(lpRaw != nullptr, "mpVehiclePhysicsData");
    const Engines::PhysicsControl::PhysicsData& lrPhysics = mpPhysicsControl->GetPhysicsData();

    f32 lfSpeedMPS = lrPhysics.mSpeedMPS.GetCurrent();
    if (lfSpeedMPS >= -0.1f && lfSpeedMPS <= 0.1f)
        lfSpeedMPS = 0.0f;
    const f32 lfAbsSpeed = std::fabs(lfSpeedMPS);
    mWheelData.mbReverse = lfAbsSpeed > 0.0f && lrPhysics.mGear.GetCurrent() == 0;
    const f32 lfSpeedFraction =
        Clamp(lfAbsSpeed, 0.0f, mWheelAttribs.mfSlowFastThreshold) /
        mWheelAttribs.mfSlowFastThreshold;

    for (s32 liSide = 0; liSide < E_MAX_SIDES; ++liSide)
    {
        f32 lfSidePeel = 0.0f;
        f32 lfSideBrake = 0.0f;
        f32 lfSideLateral = 0.0f;
        s32 liGrounded = 0;
        for (s32 liWheelSlot = liSide; liWheelSlot < 4; liWheelSlot += 2)
        {
            const BrnPhysics::Vehicle::WheelLite& lrWheel =
                lpRaw->maWheels[KAE_FLIPPED_WHEELS_LOOKUP[liWheelSlot]];
            if (!lrWheel.mRoadContact.mbIsOnGround)
                continue;

            const f32 lfRoadLong = lrWheel.mfRoadLongSpeed;
            const f32 lfWheelLong = lrWheel.mfWheelLongSpeed;
            const f32 lfLongDenominator = (std::max)(std::fabs(lfRoadLong), 1.0f);
            const f32 lfLongSlip = std::fabs((lfWheelLong - lfRoadLong) / lfLongDenominator);
            f32 lfLateral = 0.0f;
            if (!lrPhysics.IsCrashing.GetCurrent())
            {
                const f32* lpMultiplier = lrWheel.mfRoadLatSpeed >= 0.0f
                    ? mWheelAttribs.mafSideRightLateralMultipler
                    : mWheelAttribs.mafSideLeftLateralMultipler;
                lfLateral = lpMultiplier[liSide] * std::fabs(lrWheel.mfRoadLatSpeed);
            }

            ++liGrounded;
            const f32 lfWheelSign = lfWheelLong == 0.0f ? 0.0f : (lfWheelLong > 0.0f ? 1.0f : -1.0f);
            const f32 lfRoadSign = lfRoadLong == 0.0f ? 0.0f : (lfRoadLong > 0.0f ? 1.0f : -1.0f);
            if (std::fabs(lfWheelSign - lfRoadSign) <= 0.00000011920929f &&
                lfWheelLong < lfRoadLong)
                lfSideBrake = (std::max)(lfSideBrake, lfLongSlip);
            else
                lfSidePeel = (std::max)(lfSidePeel, lfLongSlip);
            lfSideLateral = (std::max)(lfSideLateral, lfLateral);
        }

        WheelSide& lrSide = mWheelData.maSide[liSide];
        lrSide.mbIsOnGround.Update(liGrounded != 0);
        lrSide.mfBrake = Approach(lrSide.mfBrake, lfSideBrake, 100.0f, 100.0f);
        lrSide.mfPeel = Approach(lrSide.mfPeel, lfSidePeel, 100.0f, 100.0f);
        lrSide.mfLateral = lfSideLateral >= 1.0f
            ? lfSideLateral : Approach(lrSide.mfLateral, lfSideLateral, 100.0f, 100.0f);
        lrSide.mfBrakeNormalized = LerpedNormalise(
            mWheelAttribs.mBrakeSlow, mWheelAttribs.mBrakeFast, lrSide.mfBrake, lfSpeedFraction);
        lrSide.mfPeelNormalized = LerpedNormalise(
            mWheelAttribs.mPeelSlow, mWheelAttribs.mPeelFast, lrSide.mfPeel, lfSpeedFraction);
        lrSide.mfLateralNormalized = LerpedNormalise(
            mWheelAttribs.mLateralSlow, mWheelAttribs.mLateralFast, lrSide.mfLateral, lfSpeedFraction);
        SetMixerInputValue(liSide, liGrounded == 0 ? 0x7FFF : 0);
    }
}

void WheelControl::UpdateDriftingRPM()
{
    const Engines::PhysicsControl::PhysicsData& lrPhysics = mpPhysicsControl->GetPhysicsData();
    mfDriftingShiftOccured -= mfDeltaTime;
    const f32 lfPhysicsRPM = lrPhysics.mNormalizedRpm.GetCurrent();
    mfDriftingRPMFactor = 1.0f -
        (1.0f - KF_DRIFT_DRIFT_RPM_SCALE_FACTOR) * lrPhysics.mDrifting.GetCurrent();
    if (mbPerformedDriftShift)
        mfDriftingRPMFactor *= KF_DRIFT_RPM_POST_SHIFT_FACTOR;

    if (lrPhysics.mGear.GetCurrent() >= 4)
    {
        if (mfDriftingRPMFactor < KF_DRIFT_RPM_SHIFT_FACTOR)
        {
            const bool lbShiftActive =
                mpShiftControl->GetShiftingState() != Engines::ShiftControl::E_SHFT_NONE;
            if (!lbShiftActive && mfDriftingShiftOccured < 0.0f)
            {
                mbIsDriftUpShift = false;
                mpShiftControl->BeginDownShift(this);
                mbPerformedDriftShift = true;
                mfDriftingShiftOccured = KF_DRIFT_TIME_BETWEEN_SHIFTS;
            }
        }
        else if (mfDriftingRPMFactor > KF_DRIFT_RPM_UP_SHIFT_FACTOR && mbPerformedDriftShift)
        {
            mbIsDriftUpShift = true;
            mbPerformedDriftShift = false;
            mpShiftControl->BeginUpShift(this);
        }
    }
    mfRPMDueToDrift = lfPhysicsRPM * mfDriftingRPMFactor - lfPhysicsRPM;
}

void WheelControl::UpdatePeelRPM(f32)
{
    // ARTIST/DecFIGS compile this method to a bare return.
}

void WheelControl::UpdateWheelsInAirRPM(f32 afTimeStep)
{
    const BrnSound::Vehicles::VehicleData* lpRaw = mpPhysicsControl->GetRawPhysicsData();
    const Engines::PhysicsControl::PhysicsData& lrPhysics = mpPhysicsControl->GetPhysicsData();
    switch (meInAirRevState)
    {
    case E_IN_AIR_REV_STATE_NONE:
        if (!mIsOnGround.GetCurrent() &&
            (lrPhysics.mSpeedMPH.GetCurrent() > 5.0f || lpRaw->mbCrashing))
        {
            meInAirRevState = E_IN_AIR_REV_STATE_ASCENDING;
            const f32 lfThrottleHighTime = (std::max)(
                KF_IN_AIR_ASCENDING_TIME - KF_IN_AIR_THROTTLE_TRANSITION, 0.0f);
            mInAirRevThrottlePath.ClearStages();
            mInAirRevThrottlePath.AddStage(mpEngineControl->GetAudioThrottle().GetCurrent(),
                1.0f, KF_IN_AIR_THROTTLE_TRANSITION, CgsSound::Utils::Curve::E_LINEAR);
            mInAirRevThrottlePath.AddStage(1.0f, 1.0f, lfThrottleHighTime,
                CgsSound::Utils::Curve::E_LINEAR);
            mInAirRevThrottlePath.AddStage(1.0f, 0.0f, KF_IN_AIR_THROTTLE_TRANSITION,
                CgsSound::Utils::Curve::E_LINEAR);
            const f32 lfRpm = mpEngineControl->GetAudioRPM().GetCurrent();
            mInAirRevRpmInterpolate.Initialize(lfRpm,
                Clamp(lfRpm + KF_IN_AIR_RPM_BOOST, 1000.0f, 10000.0f),
                KF_IN_AIR_ASCENDING_TIME, CgsSound::Utils::Curve::E_EQ_PWR_SQ);
            mInAirRevVolumeInterpolate.Initialize(
                mpEngineControl->GetAudioEngVolume().GetCurrent(), KF_IN_AIR_MAX_VOLUME,
                KF_IN_AIR_ASCENDING_TIME, CgsSound::Utils::Curve::E_ONE_MINUS_EQPWR);
        }
        break;

    case E_IN_AIR_REV_STATE_ASCENDING:
        mInAirRevThrottlePath.Update(afTimeStep);
        mInAirRevRpmInterpolate.Update(afTimeStep);
        mInAirRevVolumeInterpolate.Update(afTimeStep);
        if (mInAirRevRpmInterpolate.IsFinished())
        {
            meInAirRevState = E_IN_AIR_REV_STATE_DESCENDING;
            mInAirRevRpmInterpolate.Initialize(mpEngineControl->GetAudioRPM().GetCurrent(),
                1000.0f, KF_IN_AIR_DESCENDING_TIME, CgsSound::Utils::Curve::E_EQ_PWR_SQ);
            mInAirRevVolumeInterpolate.Initialize(
                mpEngineControl->GetAudioEngVolume().GetCurrent(), 1.0f,
                KF_IN_AIR_DESCENDING_TIME, CgsSound::Utils::Curve::E_POWER);
            break;
        }
        if (mIsOnGround.GetCurrent())
        {
            meInAirRevState = E_IN_AIR_REV_STATE_MERGING;
            const f32 lfMergeTimeMs = mInAirRevRpmInterpolate.GetElapsedTime() * 1000.0f;
            mInAirRevRpmInterpolate.Initialize(mInAirRevRpmInterpolate.GetValueFloat(),
                mpEngineControl->GetTargetRPM(), lfMergeTimeMs,
                CgsSound::Utils::Curve::E_EQ_PWR_SQ);
            mInAirRevVolumeInterpolate.Initialize(mInAirRevVolumeInterpolate.GetValueFloat(),
                1.0f, lfMergeTimeMs, CgsSound::Utils::Curve::E_POWER);
        }
        break;

    case E_IN_AIR_REV_STATE_DESCENDING:
        mInAirRevThrottlePath.Update(afTimeStep);
        mInAirRevRpmInterpolate.Update(afTimeStep);
        mInAirRevVolumeInterpolate.Update(afTimeStep);
        if (mIsOnGround.GetCurrent())
        {
            meInAirRevState = E_IN_AIR_REV_STATE_MERGING;
            mInAirRevRpmInterpolate.Initialize(mInAirRevRpmInterpolate.GetValueFloat(),
                mpEngineControl->GetTargetRPM(), KF_IN_AIR_ASCENDING_TIME,
                CgsSound::Utils::Curve::E_EQ_PWR_SQ);
            mInAirRevVolumeInterpolate.Initialize(mInAirRevVolumeInterpolate.GetValueFloat(),
                1.0f, KF_IN_AIR_ASCENDING_TIME, CgsSound::Utils::Curve::E_POWER);
        }
        break;

    case E_IN_AIR_REV_STATE_MERGING:
        mInAirRevThrottlePath.Update(afTimeStep, lrPhysics.mThrottle.GetCurrent());
        mInAirRevRpmInterpolate.Update(afTimeStep, mpEngineControl->GetTargetRPM());
        mInAirRevVolumeInterpolate.Update(afTimeStep);
        if (mInAirRevRpmInterpolate.IsFinished())
            meInAirRevState = E_IN_AIR_REV_STATE_NONE;
        break;
    }
}

void WheelControl::UpdateParams(f32 afTimeStep)
{
    UpdateWheelStatus(afTimeStep);
    UpdateWheelsInAirRPM(afTimeStep);
    if (GetStateId() == 1)
    {
        UpdateDriftingRPM();
        UpdateSkidValues(afTimeStep);
    }
    mfAudioRPM = mfRPMDueToPeel + mfRPMDueToDrift;
    if (mIsOnGround.GetCurrent())
    {
        mfTimeSinceLanding += afTimeStep;
        mfTimeInAir = 0.0f;
    }
    else
    {
        mfTimeInAir += afTimeStep;
        mfTimeSinceLanding = 0.0f;
    }
}

f32 WheelControl::GetStartRPM()
{
    return mpEngineControl->GetAudioRPM().GetCurrent();
}

f32 WheelControl::GetTargetRPM()
{
    f32 lfTarget = Clamp(mpEngineControl->GetTargetRPM(), 1000.0f, 10000.0f);
    if (mbIsDriftUpShift && mpPhysicsControl->GetPhysicsData().mGear.GetCurrent() == 5)
        lfTarget -= KF_DRIFT_UP_SHIFT_TARGET_RPM_OFFSET;
    return lfTarget;
}

f32 WheelControl::GetRiseFromRPM()
{
    return mpEngineControl->GetAudioRPM().GetCurrent() - KF_DRIFT_SHIFT_RISE_FROM_RPM;
}

AIWheelControl::AIWheelControl()
    : WheelControl()
{
}

AIWheelControl::~AIWheelControl()
{
}

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound

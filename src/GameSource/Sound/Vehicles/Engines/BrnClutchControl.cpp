#include "GameSource/Sound/Vehicles/Engines/BrnClutchControl.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"
#include "GameSource/Sound/Vehicles/Engines/BrnEngineControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnHybridExhaustControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Vehicles/Wheels/BrnWheelControl.h"

#include <algorithm>
#include <cmath>

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

namespace
{
f32 ClampRpm(f32 lfRpm)
{
    return (std::max)(1000.0f, (std::min)(10000.0f, lfRpm));
}

f32 Component(const Vector4& lrValue, s32 liIndex)
{
    switch (liIndex)
    {
    case 1: return lrValue.y;
    case 2: return lrValue.z;
    case 3: return lrValue.w;
    default: return lrValue.x;
    }
}
}

ClutchControl::ClutchControl()
    : mpPhysicsControl(nullptr)
    , mpEngineControl(nullptr)
    , mpShiftControl(nullptr)
    , mpHybridExhaustControl(nullptr)
    , mpWheelControl(nullptr)
    , mVehicleEngineAttributes(nullptr, nullptr)
    , meClutchState(E_CLUTCH_STATE_NONE)
    , mfLastClutchAttack(0.0f)
    , mfLastIdleClutch(0.0f)
    , mfElapsedTimeOfInfiniteGears(0.0f)
    , meDrivingState(E_DRIVING_STATE_REGULAR)
    , mfMaxIncrement(0.0f)
    , mfRandomTarget(0.0f)
    , mfRPMBeforeShift(0.0f)
    , mfDamageEngineAmount(0.0f)
    , mfDamageThrottleAmount(1.0f)
{
    mRandom.Construct();
}

ClutchControl::~ClutchControl() {}

CgsSound::Logic::EffectControl* ClutchControl::CreateObject(u32 /*luType*/)
{
    return new ClutchControl();
}

s32 ClutchControl::GetController(s32 aiSlot)
{
    static const s32 kaiControllers[] = { 0, 4, 2, 5, 1 };
    return aiSlot >= 0 && aiSlot < 5 ? kaiControllers[aiSlot] : -1;
}

void ClutchControl::AttachController(CgsSound::Logic::EffectBase* apController)
{
    switch (apController->GetEffectID())
    {
    case 0: mpPhysicsControl = static_cast<PhysicsControl*>(apController); break;
    case 1: mpWheelControl = static_cast<BrnSound::Vehicles::Wheels::WheelControl*>(apController); break;
    case 2: mpShiftControl = static_cast<ShiftControl*>(apController); break;
    case 4: mpEngineControl = static_cast<EngineControl*>(apController); break;
    case 5: mpHybridExhaustControl = static_cast<HybridExhaustControl*>(apController); break;
    default: CGS_ASSERT(false, "Couldn't attach clutch controller"); break;
    }
}

bool ClutchControl::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;
    CGS_ASSERT(mpPhysicsControl && mpEngineControl && mpShiftControl,
               "mpPhysicsControl && mpEngineControl && mpShiftControl");
    if (!mpPhysicsControl || !mpEngineControl || !mpShiftControl)
        return false;

    const u64 luKey = mpPhysicsControl->GetEngineComponentKey(
        BrnSound::Vehicles::VehicleState::E_EXHAUST);
    mVehicleEngineAttributes.Change(Attrib::FindCollectionWithDefault(
        0x7F161D94482CB3BFull, luKey));

    meClutchState = E_CLUTCH_STATE_NONE;
    mfDamageEngineAmount = 0.0f;
    mfDamageThrottleAmount = 0.0f;
    mfRPMBeforeShift = 0.0f;
    mDamagedThrottle.Construct(0xC87CD8C91AD0891Bull,
        CgsSound::Utils::MinMax(0.01f, 0.2f),
        CgsSound::Utils::MinMax(0.01f, 0.1f));
    GenerateDamagedWindow();
    return true;
}

bool ClutchControl::ShouldBeginClutchAttack() const
{
    if (GetInstanceId() != 1 || !mpPhysicsControl || !mpShiftControl)
        return false;
    const PhysicsControl::PhysicsData& lrPhysics = mpPhysicsControl->GetPhysicsData();
    return lrPhysics.mDeltaThrottle.GetAverage() >= 0.1f &&
           !mpShiftControl->IsActive() &&
           lrPhysics.mNormalizedRpm.GetCurrent() >= 3000.0f &&
           mfRunningTime - mfLastClutchAttack >= 2.0f;
}

bool ClutchControl::ShouldBeginIdleClutch() const
{
    if (GetInstanceId() != 1 || !mpPhysicsControl || !mpShiftControl)
        return false;
    const PhysicsControl::PhysicsData& lrPhysics = mpPhysicsControl->GetPhysicsData();
    return lrPhysics.mDeltaThrottle.GetAverage() >= 0.1f &&
           !mpShiftControl->IsActive() &&
           lrPhysics.mNormalizedRpm.GetCurrent() <= 3000.0f &&
           mfRunningTime - mfLastIdleClutch >= 2.0f;
}

bool ClutchControl::ShouldBeginBoostAttack() const
{
    if (GetInstanceId() != 1 || !mpPhysicsControl || !mpShiftControl)
        return false;
    const PhysicsControl::PhysicsData& lrPhysics = mpPhysicsControl->GetPhysicsData();
    return lrPhysics.IsBoosting.GetCurrent() && !lrPhysics.IsBoosting.GetPrevious() &&
           lrPhysics.mDeltaThrottle.GetAverage() <= 0.1f &&
           lrPhysics.mNormalizedRpm.GetCurrent() >= 3000.0f &&
           !mpShiftControl->IsActive();
}

bool ClutchControl::ShouldBeginInfiniteGearRise() const
{
    if (!mpPhysicsControl || !mpShiftControl)
        return false;
    const PhysicsControl::PhysicsData& lrPhysics = mpPhysicsControl->GetPhysicsData();
    const f32 lfStart = Component(mVehicleEngineAttributes.INF_MaxRpmBeforeStart(),
                                  static_cast<s32>(meDrivingState));
    if (lrPhysics.mNormalizedRpm.GetCurrent() < lfStart ||
        !lrPhysics.mIsAccelerating.GetCurrent() ||
        lrPhysics.mGear.GetCurrent() < 5)
        return false;
    return !mpShiftControl->IsActive() || IsInfiniteGears();
}

void ClutchControl::UpdateParams(f32 afTimeStep)
{
    if (!mpPhysicsControl || !mpEngineControl || !mpShiftControl)
        return;

    const PhysicsControl::PhysicsData& lrPhysics = mpPhysicsControl->GetPhysicsData();
    meDrivingState = lrPhysics.IsBoosting.GetCurrent()
        ? E_DRIVING_STATE_BOOST : E_DRIVING_STATE_REGULAR;
    SetMixerInputValue(0, 0);
    SetMixerInputValue(1, 0);
    SetMixerInputValue(2, IsInfiniteGears() ? 0x7FFF : 0);

    if (meClutchState == E_CLUTCH_STATE_NONE)
    {
        if (ShouldBeginClutchAttack())
        {
            meClutchState = E_CLUTCH_STATE_ATTACK_BEGIN;
            SetMixerInputValue(1, 0x7FFF);
        }
        else if (ShouldBeginIdleClutch())
        {
            meClutchState = E_CLUTCH_STATE_IDLE_BEGIN;
        }
    }

    if (ShouldBeginBoostAttack())
    {
        meClutchState = E_CLUTCH_STATE_BOOST;
        mpShiftControl->BeginDownShift(this);
        SetMixerInputValue(0, 0x7FFF);
    }

    const bool lbInfinite = ShouldBeginInfiniteGearRise();
    if (lbInfinite && meClutchState == E_CLUTCH_STATE_NONE)
    {
        meClutchState = E_CLUTCH_STATE_INIFINITE_GEAR;
        const s32 liDrive = static_cast<s32>(meDrivingState);
        const f32 lfTimeMultiplier = mRandom.RandomFloat(1.0f,
            Component(mVehicleEngineAttributes.INF_TimeMulti(), liDrive));
        const f32 lfReachTime = Component(
            mVehicleEngineAttributes.INF_TimeBeforeReachingMaxSpeed(), liDrive);
        mfMaxIncrement = lfTimeMultiplier * lfReachTime * 3000.0f;
        mfRandomTarget = Component(mVehicleEngineAttributes.INF_MaxRpmBeforeShift(), liDrive);
        const f32 lfRpm = lrPhysics.mNormalizedRpm.GetCurrent();
        mInterpRPM.Initialize(lfRpm, lfRpm, 10.0f, CgsSound::Utils::Curve::E_LINEAR);
        mfElapsedTimeOfInfiniteGears = 0.0f;
    }
    else if (!lbInfinite && IsInfiniteGears())
    {
        meClutchState = E_CLUTCH_STATE_NONE;
    }

    UpdateClutchState(afTimeStep);
    UpdateDamagedEngine(afTimeStep);
}

void ClutchControl::UpdateClutchState(f32 afTimeStep)
{
    const PhysicsControl::PhysicsData& lrPhysics = mpPhysicsControl->GetPhysicsData();
    switch (meClutchState)
    {
    case E_CLUTCH_STATE_ATTACK_BEGIN:
        mfLastClutchAttack = mfRunningTime;
        mInterpThrottle.Initialize(1.0f, 0.0f, 10.0f, CgsSound::Utils::Curve::E_LINEAR);
        mInterpRPM.Initialize(ClampRpm(mpEngineControl->GetAudioRPM().GetCurrent() + 1000.0f),
            lrPhysics.mNormalizedRpm.GetCurrent(), 190.0f, CgsSound::Utils::Curve::E_POWER);
        mInterpVol.Initialize(1.4f, 1.0f, 190.0f, CgsSound::Utils::Curve::E_ONE_MINUS_EQPWR);
        meClutchState = E_CLUTCH_STATE_ATTACK_UPDATE;
        // ARTIST deliberately falls through and advances the newly-created ramps.
    case E_CLUTCH_STATE_ATTACK_UPDATE:
        mInterpRPM.Update(afTimeStep, lrPhysics.mNormalizedRpm.GetCurrent());
        mInterpThrottle.Update(afTimeStep);
        mInterpVol.Update(afTimeStep);
        if (mInterpRPM.IsFinished() && mInterpThrottle.IsFinished() && mInterpVol.IsFinished())
            meClutchState = E_CLUTCH_STATE_NONE;
        break;

    case E_CLUTCH_STATE_IDLE_BEGIN:
        mfLastIdleClutch = mfRunningTime;
        mInterpThrottle.Initialize(lrPhysics.mThrottle.GetCurrent(), 1.0f, 200.0f,
            CgsSound::Utils::Curve::E_LINEAR);
        mInterpRPM.Initialize(mpEngineControl->GetAudioRPM().GetCurrent(),
            ClampRpm(mpEngineControl->GetAudioRPM().GetCurrent() + 7000.0f),
            350.0f, CgsSound::Utils::Curve::E_EQ_PWR_SQ);
        mInterpVol.Initialize(1.3f, 1.0f, 100.0f, CgsSound::Utils::Curve::E_LINEAR);
        meClutchState = E_CLUTCH_STATE_IDLE_REVING;
        // fall through
    case E_CLUTCH_STATE_IDLE_REVING:
        mInterpRPM.Update(afTimeStep);
        mInterpThrottle.Update(afTimeStep);
        mInterpVol.Update(afTimeStep);
        if (mInterpRPM.IsFinished() && mInterpThrottle.IsFinished() && mInterpVol.IsFinished())
        {
            mInterpThrottle.Initialize(1.0f, lrPhysics.mThrottle.GetCurrent(), 100.0f,
                CgsSound::Utils::Curve::E_LINEAR);
            mInterpRPM.Initialize(mInterpRPM.GetValueFloat(),
                lrPhysics.mNormalizedRpm.GetCurrent(), 700.0f,
                CgsSound::Utils::Curve::E_EQ_PWR_SQ);
            mInterpVol.Initialize(mInterpVol.GetValueFloat(), 1.0f, 100.0f,
                CgsSound::Utils::Curve::E_LINEAR);
            meClutchState = E_CLUTCH_STATE_IDLE_DISENGAGE;
        }
        break;

    case E_CLUTCH_STATE_IDLE_DISENGAGE:
        mInterpRPM.Update(afTimeStep, lrPhysics.mNormalizedRpm.GetCurrent());
        mInterpThrottle.Update(afTimeStep, lrPhysics.mThrottle.GetCurrent());
        mInterpVol.Update(afTimeStep);
        if (mInterpRPM.IsFinished() && mInterpThrottle.IsFinished() && mInterpVol.IsFinished())
            meClutchState = E_CLUTCH_STATE_NONE;
        break;

    case E_CLUTCH_STATE_INTERRUPT:
        break;

    case E_CLUTCH_STATE_INIFINITE_GEAR:
    {
        mfElapsedTimeOfInfiniteGears += afTimeStep;
        f32 lfRpm = mInterpRPM.GetValueFloat();
        if (mpShiftControl->IsActive())
        {
            lfRpm = mpShiftControl->GetShiftingRPM();
        }
        else
        {
            const f32 lfIncrement = (1.0f - lrPhysics.mDrifting.GetCurrent()) *
                                    mfMaxIncrement * afTimeStep;
            lfRpm += (std::min)(999.0f, (std::max)(0.0f, lfIncrement));
        }
        mInterpRPM.Reset(lfRpm);
        mInterpThrottle.Reset(lrPhysics.mThrottle.GetCurrent());
        mInterpVol.Reset(1.0f);

        if (lfRpm > mfRandomTarget && !mpShiftControl->IsActive() &&
            mpShiftControl->GetLastUpShiftTime() + 3.0f < mfRunningTime &&
            lrPhysics.mDrifting.GetCurrent() < 1.0f)
        {
            const s32 liDrive = static_cast<s32>(meDrivingState);
            const f32 lfTimeMultiplier = mRandom.RandomFloat(1.0f,
                Component(mVehicleEngineAttributes.INF_TimeMulti(), liDrive));
            const f32 lfReachTime = meDrivingState == E_DRIVING_STATE_BOOST
                ? 0.08f
                : Component(mVehicleEngineAttributes.INF_TimeBeforeReachingMaxSpeed(), liDrive);
            mfMaxIncrement = lfTimeMultiplier * lfReachTime * 3000.0f;

            const f32 lfDropPercent = Component(
                mVehicleEngineAttributes.INF_RpmDropPercentage(), liDrive);
            const f32 lfDropRpm = Component(mVehicleEngineAttributes.INF_RpmDrop(), liDrive);
            const f32 lfMaxRpm = Component(
                mVehicleEngineAttributes.INF_MaxRpmBeforeShift(), liDrive);

            // ARTIST @ 0x826CDEE4..0x826CE084.  The artificial upshift falls to
            // a randomized point in the authored drop band, then starts the next
            // rise at another randomized point near INF_MaxRpmBeforeShift.  Wheel
            // audio RPM is added to the fall point exactly as in the original.
            const f32 lfDropToMin = lfMaxRpm - lfDropRpm;
            const f32 lfDropToMax = lfDropToMin + lfDropRpm * lfDropPercent;
            const f32 lfWheelRpm = mpWheelControl ? mpWheelControl->GetAudioRPM() : 0.0f;
            mfRPMBeforeShift = lfWheelRpm +
                mRandom.RandomFloat(lfDropToMin, lfDropToMax);
            mfRandomTarget = mRandom.RandomFloat(
                lfMaxRpm - lfDropRpm * lfDropPercent, lfMaxRpm);
            mInterpRPM.Reset(mfRPMBeforeShift);
            mpShiftControl->BeginUpShift(this);
        }
        break;
    }

    case E_CLUTCH_STATE_BOOST:
        if (!mpShiftControl->IsActive())
            meClutchState = E_CLUTCH_STATE_NONE;
        break;
    default:
        break;
    }
}

f32 ClutchControl::GetStartRPM()
{
    return mpEngineControl ? mpEngineControl->GetAudioRPM().GetCurrent() : 1000.0f;
}

f32 ClutchControl::GetTargetRPM()
{
    if (IsInfiniteGears())
        return mfRandomTarget;
    return mpEngineControl ? mpEngineControl->GetTargetRPM() : 1000.0f;
}

f32 ClutchControl::GetRiseFromRPM()
{
    return GetStartRPM() - 3000.0f;
}

void ClutchControl::GenerateDamagedWindow()
{
    const f32 lfDamage = (std::max)(0.0f, (std::min)(1.0f, mfDamageEngineAmount));
    const f32 lfUp = 5.0f + (0.2f - 5.0f) * lfDamage;
    const f32 lfDown = 7.0f + (0.1f - 7.0f) * lfDamage;
    mDamagedWindow.Construct(0xD1B54A32D192ED03ull,
        CgsSound::Utils::MinMax(lfUp, lfUp),
        CgsSound::Utils::MinMax(lfDown, lfDown));
}

void ClutchControl::UpdateDamagedEngine(f32 afTimeStep)
{
    if (mfDamageEngineAmount <= 0.0f || mpShiftControl->IsActive() ||
        !mpPhysicsControl->GetPhysicsData().mIsAccelerating.GetCurrent())
    {
        mfDamageThrottleAmount = 0.0f;
        return;
    }

    const CgsSound::Utils::DataPoint<bool> lWindow = mDamagedWindow.Update(afTimeStep);
    if (lWindow.GetCurrent())
    {
        const CgsSound::Utils::DataPoint<bool> lThrottle = mDamagedThrottle.Update(afTimeStep);
        mfDamageThrottleAmount = lThrottle.GetCurrent() ? mfDamageEngineAmount : 0.0f;
    }
    else
    {
        mfDamageThrottleAmount = 0.0f;
    }
}

void ClutchControl::Notify(const CgsSound::Io::MessageHeader* apkMessage)
{
    if (!apkMessage)
        return;
    if (apkMessage->GetEventId() == 20)
    {
        const CgsSound::Io::Message<f32>* lpDamage =
            static_cast<const CgsSound::Io::Message<f32>*>(apkMessage);
        mfDamageEngineAmount = lpDamage->mData;
        GenerateDamagedWindow();
    }
    else if (apkMessage->GetEventId() == 21)
    {
        mfDamageEngineAmount = 0.0f;
        GenerateDamagedWindow();
    }
}

AIClutchControl::AIClutchControl() : ClutchControl() {}
AIClutchControl::~AIClutchControl() {}

CgsSound::Logic::EffectControl* AIClutchControl::CreateObject(u32 /*luType*/)
{
    return new AIClutchControl();
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

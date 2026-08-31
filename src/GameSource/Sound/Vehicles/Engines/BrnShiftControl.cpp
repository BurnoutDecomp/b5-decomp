#include "GameSource/Sound/Vehicles/Engines/BrnShiftControl.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnEngineControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnHybridExhaustControl.h"

#include <algorithm>
#include <cstdio>

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

f32 EvaluateCurveY(const Matrix44& lrCurve, f32 lfX)
{
    const f32 lfX2 = lfX * lfX;
    return lrCurve.xAxis.y * (lfX2 * lfX) +
           lrCurve.yAxis.y * lfX2 + lrCurve.zAxis.y * lfX + lrCurve.wAxis.y;
}
}

ShiftControl::ShiftControl()
    : mpPhysicsControl(nullptr)
    , mpEngineControl(nullptr)
    , mpHybridControl(nullptr)
    , mbNeed_ShiftGearSnd(false)
    , mbNeed_DisengageSnd(false)
    , mbNeed_EngageSnd(false)
    , mShiftingPatternData(nullptr, nullptr)
    , meShiftState(E_SHFT_NONE)
    , meShiftStageChanged(E_SHFT_NONE)
    , miRaceCarIndex(0)
    , meShift_LFO(E_SHIFT_LFO_NONE)
    , mfVOL_LFO_AMP(0.0f)
    , mfVOL_LFO_FRQ(0.0f)
    , mfRPM_LFO_AMP(0.0f)
    , mfRPM_LFO_FRQ(0.0f)
    , mfRPMAtShift(0.0f)
    , mfLastUpShift(0.0f)
    , mpShiftingActivator(nullptr)
{
}

ShiftControl::~ShiftControl() {}

CgsSound::Logic::EffectControl* ShiftControl::CreateObject(u32 /*luType*/)
{
    return new ShiftControl();
}

s32 ShiftControl::GetController(s32 aiSlot)
{
    static const s32 kaiControllers[] = { 0, 4, 5 };
    return aiSlot >= 0 && aiSlot < 3 ? kaiControllers[aiSlot] : -1;
}

void ShiftControl::AttachController(CgsSound::Logic::EffectBase* apController)
{
    switch (apController->GetEffectID())
    {
    case 0: mpPhysicsControl = static_cast<PhysicsControl*>(apController); break;
    case 4: mpEngineControl = static_cast<EngineControl*>(apController); break;
    case 5: mpHybridControl = static_cast<HybridExhaustControl*>(apController); break;
    default: break;
    }
}

bool ShiftControl::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;
    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    if (!mpPhysicsControl)
        return false;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    CGS_ASSERT(lpModule != nullptr, "mpLogicModule");
    if (!lpModule)
        return false;

    void* lpPattern = lpModule->GetGlobalData().ShiftPatterns(
        static_cast<u32>(mpPhysicsControl->GetVehicleEngineAttributes().ShiftPatternType()));
    mShiftingPatternData.ChangeWithDefault(lpPattern);
    miRaceCarIndex = static_cast<s32>(mpPhysicsControl->GetAttachInfo().muVehicleIndex);
    meShiftState = E_SHFT_NONE;
    meShiftStageChanged = E_SHFT_NONE;
    return true;
}

void ShiftControl::SetupLoadData()
{
    if (!mpPhysicsControl)
        return;
    const char* lpcComponent = mpPhysicsControl->GetEngineComponentName(
        BrnSound::Vehicles::VehicleState::E_EXHAUST);
    if (!lpcComponent)
        return;
    const u32 luHash = static_cast<u32>(CgsResource::ID::HashString(
        reinterpret_cast<const u8*>(lpcComponent)));
    char lacBundle[64];
    std::snprintf(lacBundle, sizeof(lacBundle), "Engines\\%08x.bundle", luHash);
    LoadAsset(lacBundle, lpcComponent, BrnSound::Logic::ResourceRegistrar::E_ATTRIBSYS);
}

void ShiftControl::BeginUpShift(IShiftingActivator* lpShiftingActivator)
{
    meShift_LFO = E_SHIFT_LFO_NONE;
    mfVOL_LFO_AMP = mfVOL_LFO_FRQ = mfRPM_LFO_AMP = mfRPM_LFO_FRQ = 0.0f;
    meShiftStageChanged = E_SHFT_NONE;
    mfRPMAtShift = 0.0f;
    meShiftState = E_SHFT_NONE;
    mpShiftingActivator = lpShiftingActivator;
    if (!mpPhysicsControl || !lpShiftingActivator ||
        mpPhysicsControl->GetPhysicsData().mNormalizedRpm.GetCurrent() < 2000.0f)
        return;

    meShiftState = E_SHFT_UP_DISENGAGE;
    meShiftStageChanged = E_SHFT_UP_DISENGAGE;
    mfLastUpShift = mfRunningTime;
    mfRPMAtShift = lpShiftingActivator->GetStartRPM();

    f32 lfFallTime = mShiftingPatternData.UpDisengageFallTime();
    if (mpPhysicsControl->GetPhysicsData().IsBoosting.GetCurrent())
        lfFallTime *= 0.7f;
    mInterpShiftRPM.Initialize(mfRPMAtShift,
        mfRPMAtShift - mShiftingPatternData.UpDisengageFallRpm(),
        lfFallTime, CgsSound::Utils::Curve::E_LINEAR);
    mInterpShiftVol.Initialize(1.0f, 0.0f, 1.0f, CgsSound::Utils::Curve::E_LINEAR);
    mInterpShiftThrottle.Initialize(
        mpPhysicsControl->GetPhysicsData().mThrottle.GetCurrent(), 0.0f, 100.0f,
        CgsSound::Utils::Curve::E_LINEAR);
    mbNeed_DisengageSnd = true;
}

void ShiftControl::BeginDownShift(IShiftingActivator* lpShiftingActivator)
{
    if (!mpPhysicsControl || !lpShiftingActivator ||
        mpPhysicsControl->GetPhysicsData().mGear.GetCurrent() == 0)
        return;
    if (meShiftState >= E_SHFT_DOWN_DISENGAGE &&
        meShiftState <= E_SHFT_DOWN_ENGAGING_FALL)
        return;

    mpShiftingActivator = lpShiftingActivator;
    meShift_LFO = E_SHIFT_LFO_NONE;
    mfVOL_LFO_AMP = mfVOL_LFO_FRQ = mfRPM_LFO_AMP = mfRPM_LFO_FRQ = 0.0f;
    meShiftState = E_SHFT_DOWN_DISENGAGE;
    meShiftStageChanged = E_SHFT_DOWN_DISENGAGE;
    mfRPMAtShift = lpShiftingActivator->GetStartRPM();
    mfLastUpShift = mfRunningTime;

    const f32 lfFinish = ClampRpm(lpShiftingActivator->GetTargetRPM() -
                                  mShiftingPatternData.DownDisengageFallRpm());
    mInterpShiftRPM.Initialize(mfRPMAtShift, lfFinish,
        mShiftingPatternData.DownDisengageFallTime(), CgsSound::Utils::Curve::E_LINEAR);
    mInterpShiftVol.Initialize(1.0f, 0.0f, 1.0f, CgsSound::Utils::Curve::E_LINEAR);
    mInterpShiftThrottle.Initialize(
        mpPhysicsControl->GetPhysicsData().mThrottle.GetCurrent(), 0.0f, 50.0f,
        CgsSound::Utils::Curve::E_LINEAR);
}

void ShiftControl::UpdateThrottle(f32 afTimeStep)
{
    if (!IsActive())
        return;
    if (meShiftState == E_SHFT_UP_ENGAGING ||
        meShiftState == E_SHFT_DOWN_ENGAGING_REATTACH)
    {
        mInterpShiftThrottle.Update(afTimeStep,
            mpPhysicsControl->GetPhysicsData().mThrottle.GetCurrent());
    }
    else
    {
        mInterpShiftThrottle.Update(afTimeStep);
    }
}

void ShiftControl::UpdateRPM(f32 afTimeStep)
{
    if (!IsActive())
        return;

    if (meShiftState == E_SHFT_UP_ENGAGING && !mInterpShiftRPM.IsFinished())
    {
        const f32 lfProgress = mInterpShiftRPM.GetTotalDuration() > 0.0f
            ? mInterpShiftRPM.GetElapsedTime() / mInterpShiftRPM.GetTotalDuration() : 1.0f;
        const f32 lfCurve = EvaluateCurveY(mShiftingPatternData.UpDisengageFallCurve(),
            (std::max)(0.0f, (std::min)(1.0f, lfProgress)));
        mInterpShiftRPM.Update(afTimeStep, ClampRpm(
            mpShiftingActivator->GetTargetRPM() +
            mShiftingPatternData.UpEngageRpm() * lfCurve));
    }
    else if (meShiftState == E_SHFT_UP_LFO ||
             meShiftState == E_SHFT_DOWN_ENGAGING_REATTACH)
    {
        mInterpShiftRPM.Update(afTimeStep, mpShiftingActivator->GetTargetRPM());
    }
    else
    {
        mInterpShiftRPM.Update(afTimeStep);
    }
}

void ShiftControl::PostShiftFX_Init()
{
    const s32 liGear = mpPhysicsControl->GetPhysicsData().mGear.GetCurrent();
    mfRPM_LFO_AMP = mShiftingPatternData.RpmLfoAmplitude(liGear);
    mfRPM_LFO_FRQ = mShiftingPatternData.RpmLfoFrequency();
    mfVOL_LFO_AMP = mShiftingPatternData.VolLfoAmplitude(liGear);
    mfVOL_LFO_FRQ = mShiftingPatternData.VolLfoFrequency();
    meShift_LFO = E_SHIFT_LFO_ON;
    mInterpRPM_LFODecay.Initialize(mfRPM_LFO_AMP, 0.0f,
        mShiftingPatternData.RpmLfoDecayTime(), CgsSound::Utils::Curve::E_LINEAR);
    mInterpVol_LFODecay.Initialize(mfVOL_LFO_AMP, 0.0f,
        mShiftingPatternData.VolLfoDecayTime(), CgsSound::Utils::Curve::E_LINEAR);
}

void ShiftControl::PostShiftFX_Update(f32 afTimeStep)
{
    if (meShift_LFO == E_SHIFT_LFO_NONE)
        return;
    mInterpRPM_LFODecay.Update(afTimeStep);
    mInterpVol_LFODecay.Update(afTimeStep);
    if (mInterpRPM_LFODecay.IsFinished() && mInterpVol_LFODecay.IsFinished())
        PostShiftFX_End();
    else
    {
        mfRPM_LFO_AMP = mInterpRPM_LFODecay.GetValueFloat();
        mfVOL_LFO_AMP = mInterpVol_LFODecay.GetValueFloat();
    }
}

void ShiftControl::PostShiftFX_End()
{
    meShift_LFO = E_SHIFT_LFO_NONE;
    mfRPM_LFO_AMP = mfVOL_LFO_AMP = mfRPM_LFO_FRQ = mfVOL_LFO_FRQ = 0.0f;
}

void ShiftControl::EndShifting()
{
    mfRPMAtShift = 0.0f;
    mpShiftingActivator = nullptr;
    meShiftState = E_SHFT_NONE;
    meShiftStageChanged = E_SHFT_NONE;
    PostShiftFX_End();
}

void ShiftControl::UpdateGearShiftState(f32 afTimeStep)
{
    if (meShiftState == E_SHFT_NONE)
        return;
    UpdateRPM(afTimeStep);
    UpdateThrottle(afTimeStep);
    mInterpShiftVol.Update(afTimeStep);
    PostShiftFX_Update(afTimeStep);

    switch (meShiftState)
    {
    case E_SHFT_UP_DISENGAGE:
        if (mInterpShiftRPM.IsFinished())
        {
            meShiftState = meShiftStageChanged = E_SHFT_UP_ENGAGING;
            const f32 lfTarget = ClampRpm(mpShiftingActivator->GetTargetRPM());
            f32 lfTime = mShiftingPatternData.UpEngageTime();
            if (mpPhysicsControl->GetPhysicsData().IsBoosting.GetCurrent())
                lfTime *= 0.7f;
            mInterpShiftRPM.Initialize(lfTarget, lfTarget, lfTime,
                CgsSound::Utils::Curve::E_POWER);
            const f32 lfThrottle = mpPhysicsControl->GetPhysicsData().mThrottle.GetCurrent();
            mInterpShiftThrottle.Initialize(lfThrottle, lfThrottle, 10.0f,
                CgsSound::Utils::Curve::E_LINEAR);
            mInterpShiftVol.Initialize(1.0f + mShiftingPatternData.UpEngageAttackGain(),
                1.0f, mShiftingPatternData.UpEngageAttackTime(),
                CgsSound::Utils::Curve::E_LINEAR);
            PostShiftFX_Init();
        }
        break;
    case E_SHFT_UP_ENGAGING:
        if (mInterpShiftRPM.IsFinished())
            meShiftState = meShiftStageChanged = E_SHFT_UP_LFO;
        break;
    case E_SHFT_UP_LFO:
        if (meShift_LFO == E_SHIFT_LFO_NONE)
            EndShifting();
        break;
    case E_SHFT_DOWN_DISENGAGE:
        if (mInterpShiftRPM.IsFinished())
        {
            meShiftState = meShiftStageChanged = E_SHFT_DOWN_ENGAGING_RISE;
            f32 lfTime = mShiftingPatternData.DownEngageRiseTime();
            if (mpPhysicsControl->GetPhysicsData().mGear.GetCurrent() < 2)
                lfTime *= 0.7f;
            mInterpShiftRPM.Initialize(mInterpShiftRPM.GetValueFloat(),
                ClampRpm(mpShiftingActivator->GetTargetRPM() +
                         mShiftingPatternData.DownEngageRiseRpm()),
                lfTime, CgsSound::Utils::Curve::E_POWER);
            mInterpShiftThrottle.Initialize(0.0f, 1.0f, 215.0f,
                CgsSound::Utils::Curve::E_LINEAR);
        }
        break;
    case E_SHFT_DOWN_ENGAGING_RISE:
        if (mInterpShiftRPM.IsFinished())
        {
            meShiftState = meShiftStageChanged = E_SHFT_DOWN_ENGAGING_FALL;
            f32 lfTime = mShiftingPatternData.DownEngageFallTime();
            if (mpPhysicsControl->GetPhysicsData().mGear.GetCurrent() < 2)
                lfTime *= 0.7f;
            mInterpShiftRPM.Initialize(mInterpShiftRPM.GetValueFloat(),
                mInterpShiftRPM.GetValueFloat() - mShiftingPatternData.DownDisengageFallRpm(),
                lfTime, CgsSound::Utils::Curve::E_LINEAR);
            mInterpShiftThrottle.Initialize(mInterpShiftThrottle.GetValueFloat(), 0.0f,
                (std::max)(10.0f, mShiftingPatternData.DownDisengageFallTime() - 215.0f),
                CgsSound::Utils::Curve::E_LINEAR);
        }
        break;
    case E_SHFT_DOWN_ENGAGING_FALL:
        if (mInterpShiftRPM.IsFinished())
        {
            meShiftState = meShiftStageChanged = E_SHFT_DOWN_ENGAGING_REATTACH;
            const f32 lfTarget = mpShiftingActivator->GetTargetRPM();
            const f32 lfTime = (std::max)(10.0f,
                (std::min)(800.0f, std::fabs(lfTarget - mInterpShiftRPM.GetValueFloat()) * 0.3f));
            mInterpShiftRPM.Initialize(mInterpShiftRPM.GetValueFloat(), lfTarget, lfTime,
                CgsSound::Utils::Curve::E_POWER);
            mInterpShiftThrottle.Initialize(0.0f,
                mpPhysicsControl->GetPhysicsData().mThrottle.GetCurrent(), 60.0f,
                CgsSound::Utils::Curve::E_LINEAR);
        }
        break;
    case E_SHFT_DOWN_ENGAGING_REATTACH:
        if (mInterpShiftRPM.IsFinished())
            EndShifting();
        break;
    default:
        break;
    }
}

void ShiftControl::UpdateParams(f32 afTimeStep)
{
    if (!mpPhysicsControl)
        return;
    meShiftStageChanged = E_SHFT_NONE;
    mbNeed_ShiftGearSnd = mbNeed_DisengageSnd = mbNeed_EngageSnd = false;

    if (!IsActive())
    {
        const s32 liCurrentGear = mpPhysicsControl->GetPhysicsData().mGear.GetCurrent();
        const s32 liPreviousGear = mpPhysicsControl->GetPhysicsData().mGear.GetPrevious();
        if (liCurrentGear > liPreviousGear && mpEngineControl)
            BeginUpShift(mpEngineControl);
        else if (liCurrentGear < liPreviousGear && mpEngineControl)
            BeginDownShift(mpEngineControl);
    }

    UpdateGearShiftState(afTimeStep);
    SetMixerInputValue(0, meShiftState == E_SHFT_UP_DISENGAGE ? 0x7FFF : 0);
    SetMixerInputValue(1, meShiftStageChanged == E_SHFT_UP_ENGAGING ? 0x7FFF : 0);
    SetMixerInputValue(2, IsDownShifting() ? 0x7FFF : 0);
    SetMixerInputValue(3, meShiftStageChanged == E_SHFT_DOWN_ENGAGING_RISE ? 0x7FFF : 0);
    SetMixerInputValue(4, meShiftStageChanged == E_SHFT_DOWN_ENGAGING_FALL ? 0x7FFF : 0);
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

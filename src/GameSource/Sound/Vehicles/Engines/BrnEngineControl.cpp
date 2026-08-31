#include "GameSource/Sound/Vehicles/Engines/BrnEngineControl.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnShiftControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnClutchControl.h"
#include "GameSource/Sound/Vehicles/Brn3dCarPosition.h"
#include "GameSource/Sound/Vehicles/Wheels/BrnWheelControl.h"

#include <algorithm>
#include <cmath>

// =============================================================================
// BrnSound::Vehicles::Engines::EngineControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnEngineControl.h for the
// tri-base (BrnEffectControl + IShiftingActivator sub-object) shape rationale.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

EngineControl::EngineControl()
    : mpPhysicsControl(nullptr)
    , mpShiftControl(nullptr)
    , mpClutchControl(nullptr)
    , mpCar3DControl(nullptr)
    , mpWheelControl(nullptr)
    , mfAudioRpm(0.0f)
    , mfAudioThrottle(0.0f)
    , mfAudioEngineVolume(0.0f)
    , mAudioPitch()
    , mAudioDistortion()
    , mbClutchStateOn(false)
    , mfVOL_LFO(0.0f)
    , mfRPM_LFO(0.0f)
    , mfAngleRPM_LFO(0.0f)
    , mfAngleVOL_LFO(0.0f)
    , meRedLiningState(E_REDLINING_STATE_OFF)
    , mfRedlingRPMOffset(0.0f)
    , mfRedlingVolumeScale(0.0f)
    , mfRedliningTime(0.0f)
    , meDistortionState(E_DISTORTION_NONE)
    , mfRPMRamping(0.0f)
{
}

f32 EngineControl::GetStartRPM()
{
    // The console call enters through the IShiftingActivator sub-object at +0x38;
    // its +0x18 load is EngineControl::mfAudioRpm.mCurrentValue at full-object +0x50.
    return mfAudioRpm.GetCurrent();
}

// ---------------------------------------------------------------------------
// ~EngineControl  @ 0x826B2BA0  (the X360 `vector deleting destructor')
//
//   stw  off_820AF228, 0x38(r31)   ; IShiftingActivator sub-object vptr settle
//   stw  off_820AEA6C, 0(r31)      ; primary vptr (EffectControl path)
//   stw  off_820AEA38, 4(r31)      ; (transient) base-class IResourceRequester vptr
//   li   r6, 3 ; stw r6, 0x28(r31) ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  off_820AA820, 4(r31)      ; final IResourceRequester sub-object vptr
//   stb  0, 0x31(r31)              ; mbResourcesReady = false
//   stw  0, 0x24(r31)              ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { ... deallocate via off_82FFB954 (the MemBase allocator) }
//   return this
//
// Byte-identical to the committed BrnEffectControl vector-deleting-destructor @
// 0x826AEF68 (same off_820AEA6C / off_820AEA38 -> off_820AA820 progressive vptr
// settle and the same +0x24/+0x28/+0x31 member teardown) PLUS the one extra
// IShiftingActivator sub-object vptr store at +0x38 -- the same tri-base pattern
// already reconstructed for the sibling ClutchControl / WheelControl homes. All of
// the torn-down members (meAttachState/meDetachState/mbResourcesReady) are owned by
// the inherited BrnEffectControl base; EngineControl itself adds nothing to the
// teardown, so the leaf destructor body is empty (same treatment as WheelControl).
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free
// the object; that allocator is not homed here, so operator-delete dispatch is left
// to the host toolchain (the `delete' half of the X360 vector deleting destructor).
// The host compiler reproduces the IShiftingActivator teardown through the declared
// base rather than through an explicit vptr store.
// ---------------------------------------------------------------------------
EngineControl::~EngineControl()
{
}

s32 EngineControl::GetController(s32 aiSlot)
{
    static const s32 kaiControllers[] = { 0, 3, 2, 7, 1 };
    return (aiSlot >= 0 && aiSlot < static_cast<s32>(sizeof(kaiControllers) / sizeof(kaiControllers[0])))
        ? kaiControllers[aiSlot] : -1;
}

void EngineControl::AttachController(CgsSound::Logic::EffectBase* apController)
{
    switch (apController->GetEffectID())
    {
    case 0: mpPhysicsControl = static_cast<PhysicsControl*>(apController); return;
    case 1: mpWheelControl = static_cast<BrnSound::Vehicles::Wheels::WheelControl*>(apController); return;
    case 2: mpShiftControl = static_cast<ShiftControl*>(apController); return;
    case 3: mpClutchControl = static_cast<ClutchControl*>(apController); return;
    case 7: mpCar3DControl = static_cast<BrnSound::Vehicles::Car3DControl*>(apController); return;
    default:
        CGS_ASSERT(false, "false");
        return;
    }
}

bool EngineControl::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;

    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    if (!mpPhysicsControl)
        return false;

    SetMixerInputValue(3, 0x7FFF);
    // ARTIST initializes the pitch line to a live unity ramp.  The line is read
    // directly by the Ginsu/loop players as the authored boost-pitch multiplier.
    mAudioPitch.mfElapsedTime = 0.0f;
    mAudioPitch.mfLength = 0.01f;
    mAudioPitch.mfStart = 1.0f;
    mAudioPitch.mfFinish = 1.0f;
    mAudioPitch.meCurveTypes = CgsSound::Utils::Curve::E_LINEAR;
    mAudioPitch.mfCurrentValue = 1.0f;
    mAudioPitch.mbComplete = false;
    mbClutchStateOn = false;
    mfVOL_LFO = 0.0f;
    mfRPM_LFO = 0.0f;
    mfAngleRPM_LFO = 0.0f;
    mfAngleVOL_LFO = 0.0f;
    meRedLiningState = E_REDLINING_STATE_OFF;
    mfRedlingRPMOffset = 0.0f;
    mfRedlingVolumeScale = 1.0f;
    mfRedliningTime = 0.0f;
    meDistortionState = E_DISTORTION_NONE;
    mfRPMRamping = 0.0f;
    return true;
}

void EngineControl::UpdateParams(f32 afTimeStep)
{
    if (!mpPhysicsControl)
        return;

    UpdateEngineLFO(afTimeStep);
    UpdateRedLiningRPM(afTimeStep);
    UpdateRPM(afTimeStep);
    UpdateThrottle(afTimeStep);
    UpdateVolume(afTimeStep);
    UpdateEnginePitch(afTimeStep);
    UpdateDistortion(afTimeStep);

    SetMixerInputValue(1, mfAudioThrottle.GetCurrent() > 0.2f ? 0x7FFF : 0);

    const BrnSound::Vehicles::VehicleData* lpRaw = mpPhysicsControl->GetRawPhysicsData();
    bool lbReverse = false;
    if (lpRaw)
    {
        const bool lbReverseControl = GetStateId() == 1
            ? lpRaw->mi8Gear == 0
            : (lpRaw->mfBrake == 1.0f && lpRaw->mfGas == 0.0f);
        lbReverse = lbReverseControl && lpRaw->mfSpeedMPH < 0.0f;
    }
    SetMixerInputValue(2, lbReverse ? 0x7FFF : 0);
}

bool EngineControl::ShouldTurnOnClutch(f32 /*afTargetRpm*/) const
{
    if (!mpClutchControl || !mpShiftControl || !mpPhysicsControl)
        return false;
    if (mpClutchControl->IsActive())
        return false;
    return (!mpShiftControl->IsActive() ||
            mpShiftControl->GetShiftingState() == ShiftControl::E_SHFT_DOWN_ENGAGING_REATTACH) &&
           mfAudioRpm.GetCurrent() <= 2500.0f &&
           mpPhysicsControl->GetPhysicsData().mGear.GetCurrent() == 1;
}

f32 EngineControl::GetTargetRPM()
{
    const f32 lfPhysicsRpm = mpPhysicsControl
        ? mpPhysicsControl->GetPhysicsData().mNormalizedRpm.GetCurrent()
        : 0.0f;
    const f32 lfWheelRpm = mpWheelControl ? mpWheelControl->GetAudioRPM() : 0.0f;
    return lfPhysicsRpm + lfWheelRpm;
}

void EngineControl::UpdateRPM(f32 afTimeStep)
{
    f32 lfTargetRpm;
    if (mpWheelControl && mpWheelControl->IsActive())
        lfTargetRpm = mpWheelControl->GetModifiedRpm();
    else if (mpShiftControl && mpShiftControl->IsActive())
        lfTargetRpm = mpShiftControl->GetShiftingRPM();
    else if (mpClutchControl && mpClutchControl->IsActive())
        lfTargetRpm = mpClutchControl->GetClutchRPM() +
                      (mpWheelControl ? mpWheelControl->GetAudioRPM() : 0.0f);
    else if (meRedLiningState != E_REDLINING_STATE_OFF)
        lfTargetRpm = GetTargetRPM() + mfRedlingRPMOffset;
    else
        lfTargetRpm = GetTargetRPM();

    if (ShouldTurnOnClutch(lfTargetRpm))
    {
        mbClutchStateOn = true;
        const f32 lfCurrent = mfAudioRpm.GetCurrent();
        if (lfTargetRpm - lfCurrent > 999.0f)
            lfTargetRpm = lfCurrent + 999.0f;
        else if (lfCurrent - lfTargetRpm > 256.0f * afTimeStep)
            lfTargetRpm = lfCurrent - 256.0f * afTimeStep;
    }
    else
    {
        mbClutchStateOn = false;
    }

    lfTargetRpm = (std::max)(1000.0f, (std::min)(10000.0f,
        lfTargetRpm + mfRPM_LFO));
    mfAudioRpm.Update(lfTargetRpm);
}

void EngineControl::UpdateThrottle(f32 /*afTimeStep*/)
{
    f32 lfThrottle;
    if (mpWheelControl && mpWheelControl->IsActive())
        lfThrottle = mpWheelControl->GetModifiedThrottle();
    else if (mpShiftControl && mpShiftControl->IsActive())
        lfThrottle = mpShiftControl->GetShiftingThrottle();
    else if (mpClutchControl && mpClutchControl->IsActive())
        lfThrottle = mpClutchControl->GetClutchThrottle();
    else
        lfThrottle = mpPhysicsControl->GetPhysicsData().mThrottle.GetCurrent();

    if (meRedLiningState == E_REDLINING_STATE_LOW)
        lfThrottle = 0.0f;
    if (mpClutchControl)
        lfThrottle -= mpClutchControl->GetDamageThrottle();
    mfAudioThrottle.Update((std::max)(0.0f, lfThrottle));
}

void EngineControl::UpdateVolume(f32 /*afTimeStep*/)
{
    f32 lfVolume = 1.0f;
    if (mpWheelControl && mpWheelControl->IsActive())
        lfVolume = mpWheelControl->GetModifiedVolume();
    else if (mpShiftControl && mpShiftControl->IsActive())
        lfVolume = mpShiftControl->GetShiftingVolume();
    else if (mpClutchControl && mpClutchControl->IsActive())
        lfVolume = mpClutchControl->GetClutchVolume();
    mfAudioEngineVolume.Update(mfRedlingVolumeScale * lfVolume * (mfVOL_LFO + 1.0f));
}

void EngineControl::UpdateEnginePitch(f32 /*afTimeStep*/)
{
    SetMixerInputValue(0,
        mpPhysicsControl->GetPhysicsData().IsBoosting.GetCurrent() ? 0x7FFF : 0);
}

void EngineControl::UpdateDistortion(f32 afTimeStep)
{
    const bool lbFastOn = mpPhysicsControl->GetPhysicsData().IsBoosting.GetCurrent() ||
        (mpShiftControl && mpShiftControl->IsDownShifting());
    if (lbFastOn)
    {
        if (meDistortionState != E_DISTORTION_FAST_ON)
        {
            meDistortionState = E_DISTORTION_FAST_ON;
            mAudioDistortion.Initialize(mAudioDistortion.GetValueFloat(), 1.0f, 100.0f,
                CgsSound::Utils::Curve::E_LINEAR);
        }
    }
    else if (meRedLiningState == E_REDLINING_STATE_HIGH)
    {
        meDistortionState = E_DISTORTION_INSTANT_ON;
        mAudioDistortion.Reset(1.0f);
    }
    else if (meDistortionState != E_DISTORTION_NONE)
    {
        meDistortionState = E_DISTORTION_NONE;
        mAudioDistortion.Initialize(mAudioDistortion.GetValueFloat(), 0.0f, 300.0f,
            CgsSound::Utils::Curve::E_LINEAR);
    }
    mAudioDistortion.Update(afTimeStep);
}

void EngineControl::UpdateRedLiningRPM(f32 afTimeStep)
{
    const f32 KF_REDLINE_RPM = 11000.0f;
    mfRedlingRPMOffset = 0.0f;
    mfRedlingVolumeScale = 1.0f;

    if (meRedLiningState != E_REDLINING_STATE_OFF)
    {
        if ((meRedLiningState == E_REDLINING_STATE_LOW &&
             mfAudioRpm.GetCurrent() + 200.0f < KF_REDLINE_RPM) ||
            (meRedLiningState == E_REDLINING_STATE_HIGH &&
             mfAudioRpm.GetCurrent() < KF_REDLINE_RPM))
        {
            meRedLiningState = E_REDLINING_STATE_OFF;
            return;
        }
    }
    else
    {
        if ((mfAudioRpm.GetCurrent() < KF_REDLINE_RPM &&
             mfAudioRpm.GetPrevious() < KF_REDLINE_RPM) ||
            mpPhysicsControl->GetPhysicsData().mGear.GetCurrent() != 1 ||
            (mpShiftControl && mpShiftControl->IsActive()))
            return;
    }

    mfRedliningTime -= afTimeStep;
    if (meRedLiningState == E_REDLINING_STATE_OFF)
    {
        meRedLiningState = E_REDLINING_STATE_HIGH;
        mfRedliningTime = 0.05f;
    }
    if (meRedLiningState == E_REDLINING_STATE_HIGH)
    {
        if (mfRedliningTime >= 0.0f)
        {
            mfRedlingVolumeScale = 1.2f;
            return;
        }
        meRedLiningState = E_REDLINING_STATE_LOW;
        mfRedliningTime = 0.001f;
    }
    if (meRedLiningState == E_REDLINING_STATE_LOW)
    {
        if (mfRedliningTime >= 0.0f)
        {
            mfRedlingRPMOffset = -200.0f;
            mfRedlingVolumeScale = 0.9f;
        }
        else
        {
            meRedLiningState = E_REDLINING_STATE_HIGH;
            mfRedliningTime = 0.05f;
        }
    }
}

void EngineControl::UpdateEngineLFO(f32 afTimeStep)
{
    if (!mpShiftControl || !mpShiftControl->IsActive())
    {
        mfVOL_LFO = mfRPM_LFO = mfAngleRPM_LFO = mfAngleVOL_LFO = 0.0f;
        return;
    }

    const f32 lfRpmFrequency = (std::max)(0.001f,
        (std::min)(100000.0f, mpShiftControl->GetRPM_LFO_Frequncy()));
    const f32 lfVolFrequency = (std::max)(0.001f,
        (std::min)(100000.0f, mpShiftControl->GetVolLFO_Frequency()));
    const f32 KF_TWO_PI = 6.2831855f;
    mfAngleRPM_LFO += afTimeStep / (lfRpmFrequency * 0.001f) * KF_TWO_PI;
    mfAngleVOL_LFO += afTimeStep / (lfVolFrequency * 0.001f) * KF_TWO_PI;
    if (mfAngleRPM_LFO > KF_TWO_PI) mfAngleRPM_LFO -= KF_TWO_PI;
    if (mfAngleVOL_LFO > KF_TWO_PI) mfAngleVOL_LFO -= KF_TWO_PI;
    mfRPM_LFO = std::sin(mfAngleRPM_LFO) * mpShiftControl->GetRPM_LFO_Amplitude();
    mfVOL_LFO = std::sin(mfAngleVOL_LFO) * mpShiftControl->GetVolLFO_Amplitude();
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

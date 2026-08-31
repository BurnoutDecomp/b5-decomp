#include "GameSource/Sound/Vehicles/Engines/BrnEngineControl.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnShiftControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnClutchControl.h"
#include "GameSource/Sound/Vehicles/Brn3dCarPosition.h"
#include "GameSource/Sound/Vehicles/Wheels/BrnWheelControl.h"

#include <algorithm>

// =============================================================================
// BrnSound::Vehicles::Engines::EngineControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnEngineControl.h for the
// MINIMAL home note and the tri-base (BrnEffectControl + IShiftingActivator
// sub-object) shape rationale.
//
// This TU's recon'd function set is exactly two entries:
//   EngineControl::GetStartRPM                  @ 0x82698FC8
//   EngineControl::`vector deleting destructor'  @ 0x826B2BA0  (-> ~EngineControl anchor)
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

f32 EngineControl::GetStartRPM() const
{
    // The console call enters through the IShiftingActivator sub-object at +0x38;
    // its +0x18 load is EngineControl::mfAudioRpm.mCurrentValue at full-object +0x50.
    return mfAudioRpm.GetCurrent();
}

// ---------------------------------------------------------------------------
// ~EngineControl  @ 0x826B2BA0  (the X360 `vector deleting destructor')
//
//   stw  off_820AF228, 0x38(r31)   ; IShiftingActivator sub-object vptr settle (structural;
//                                    see header FLAG -- not a hand-declared base here)
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
// FLAG: the +0x38 IShiftingActivator sub-object vptr settle is not reproduced as a
// hand-declared base (see header FLAG) to avoid re-forking ShiftControl's own header
// home; it is documented here for asm-parity bookkeeping only.
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
    mAudioPitch.mfElapsedTime = 0.01f;
    mAudioPitch.mfLength = 1.0f;
    mAudioPitch.mfStart = 1.0f;
    mAudioPitch.mfFinish = 0.0f;
    mAudioPitch.meCurveTypes = CgsSound::Utils::Curve::E_POWER;
    mAudioPitch.mfCurrentValue = 0.0f;
    mAudioDistortion.mfCurrentValue = 1.0f;
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

    // ARTIST @ 0x826B2C58 dispatches these seven stages in this order.  The
    // normal driving path is kept member-for-member here; shift/clutch-specific
    // ramps remain owned by their respective controllers.
    UpdateEngineLFO(afTimeStep);
    UpdateRedLiningRPM(afTimeStep);
    UpdateRPM(afTimeStep);
    UpdateThrottle(afTimeStep);
    UpdateVolume(afTimeStep);
    UpdateEnginePitch(afTimeStep);
    UpdateDistortion(afTimeStep);

    SetMixerInputValue(1, mfAudioRpm.GetPrevious() > 0.2f ? 0x7FFF : 0);
}

void EngineControl::UpdateRPM(f32 /*afTimeStep*/)
{
    const f32 lfTarget = mpPhysicsControl->GetPhysicsData().mNormalizedRpm.GetCurrent();
    const f32 lfRpm = (std::max)(1000.0f,
        (std::min)(10000.0f, lfTarget + mfRPM_LFO + mfRedlingRPMOffset));
    mbClutchStateOn = false;
    mfAudioRpm.Update(lfRpm);
}

void EngineControl::UpdateThrottle(f32 /*afTimeStep*/)
{
    f32 lfThrottle = mpPhysicsControl->GetPhysicsData().mThrottle.GetCurrent();
    if (meRedLiningState == E_REDLINING_STATE_LOW)
        lfThrottle = 0.0f;
    mfAudioThrottle.Update((std::max)(0.0f, lfThrottle));
}

void EngineControl::UpdateVolume(f32 /*afTimeStep*/)
{
    mfAudioEngineVolume.Update(mfRedlingVolumeScale * (mfVOL_LFO + 1.0f));
}

void EngineControl::UpdateEnginePitch(f32 /*afTimeStep*/)
{
    // ARTIST @ 0x826992B8: mixer input zero is the authored boost-pitch
    // control.  This stage was absent entirely, so boost could not affect the
    // engine dynamic mix.
    SetMixerInputValue(0,
        mpPhysicsControl->GetPhysicsData().IsBoosting.GetCurrent() ? 0x7FFF : 0);
}

void EngineControl::UpdateDistortion(f32 /*afTimeStep*/)
{
}

void EngineControl::UpdateRedLiningRPM(f32 /*afTimeStep*/)
{
    mfRedlingRPMOffset = 0.0f;
    mfRedlingVolumeScale = 1.0f;
}

void EngineControl::UpdateEngineLFO(f32 /*afTimeStep*/)
{
    // With no active shift stage ARTIST clears both LFO outputs and phases.
    mfVOL_LFO = 0.0f;
    mfRPM_LFO = 0.0f;
    mfAngleRPM_LFO = 0.0f;
    mfAngleVOL_LFO = 0.0f;
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

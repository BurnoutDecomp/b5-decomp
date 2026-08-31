#include "GameSource/Sound/Vehicles/Environment/BrnSpeedStreamControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <algorithm>

// =============================================================================
// BrnSound::Vehicles::Environment::SpeedStreamControl — out-of-line deleting-
// destructor body.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnSpeedStreamControl.h for the
// inheritance rationale and the X360-32-bit-vs-host-64-bit offset note.
//
// ~SpeedStreamControl  @ 0x826BA0A0  (the X360 `vector deleting destructor')
//   stw  &off_820AEA6C, 0(this)        ; primary vptr settle
//   stw  &off_820AEA38, 4(this)        ; IResourceRequester sub-object vptr (intermediate)
//   stw  3,            0x28(this)      ; meDetachState = E_DETACH_STATE_FINISHED
//   stw  &off_820AA820, 4(this)        ; IResourceRequester sub-object vptr (final settle)
//   stb  0,            0x31(this)      ; mbResourcesReady = false
//   stw  0,            0x24(this)      ; meAttachState = E_ATTACH_STATE_NONE
//   if (a2 & 1) { deallocate via off_82FFB954 (the global sound allocator) }
//   return this
//
// The dual-vptr settle and the attach/detach/resources-ready member clears are the
// inherited BrnEffectObject teardown the compiler emits; this leaf destructor body
// adds nothing of its own.
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free
// the object; that allocator is not homed here, so operator-delete dispatch is left
// to the host toolchain (the `delete` half of the X360 deleting destructor) rather
// than reproducing the raw allocator vtable call.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

SpeedStreamControl::SpeedStreamControl()
    : BrnSound::Logic::BrnEffectControl()
    , mpPhysicsControl(nullptr)
    , mfSpeedHighTime(0.0f)
    , mfBoostHighTime(0.0f)
    , mSpeedStreamOn(false)
    , mBoostStreamOn(false)
{
}

s32 SpeedStreamControl::GetController(s32 aiSlot)
{
    return aiSlot == 0 ? 0 : -1;
}

void SpeedStreamControl::AttachController(CgsSound::Logic::EffectBase* apController)
{
    CGS_ASSERT(apController && apController->GetEffectID() == 0, "Unexpected control.");
    if (apController && apController->GetEffectID() == 0)
        mpPhysicsControl = static_cast<BrnSound::Vehicles::Engines::PhysicsControl*>(apController);
}

bool SpeedStreamControl::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;
    mfSpeedHighTime = 0.0f;
    mfBoostHighTime = 0.0f;
    mSpeedStreamOn.Flush(false);
    mBoostStreamOn.Flush(false);
    return true;
}

void SpeedStreamControl::UpdateParams(f32 afTimeStep)
{
    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    if (!mpPhysicsControl)
        return;

    const BrnSound::Vehicles::Engines::PhysicsControl::PhysicsData& lrPhysics =
        mpPhysicsControl->GetPhysicsData();
    const bool lbCrashing = lrPhysics.IsCrashing.GetCurrent();

    bool lbBoostStream = false;
    if (!lrPhysics.IsBoosting.GetCurrent() || lbCrashing)
        mfBoostHighTime = 0.0f;
    else
    {
        mfBoostHighTime += afTimeStep;
        lbBoostStream = mfBoostHighTime > 0.5f;
    }

    bool lbSpeedStream = false;
    if (lrPhysics.mSpeedMPH.GetCurrent() <= 75.0f || lbCrashing)
        mfSpeedHighTime = 0.0f;
    else
    {
        mfSpeedHighTime += afTimeStep;
        lbSpeedStream = mfSpeedHighTime > 0.5f;
    }

    mSpeedStreamOn.Update(lbSpeedStream);
    mBoostStreamOn.Update(lbBoostStream);
    UpdateDuckers();
}

void SpeedStreamControl::UpdateDuckers()
{
    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    if (!mpPhysicsControl)
        return;

    const BrnSound::Vehicles::Engines::PhysicsControl::PhysicsData& lrPhysics =
        mpPhysicsControl->GetPhysicsData();
    const bool lbCrashing = lrPhysics.IsCrashing.GetCurrent();
    const f32 lfSpeed = lrPhysics.mSpeedMPH.GetCurrent();

    f32 lfHighSpeed = 0.0f;
    if (!lbCrashing)
        lfHighSpeed = std::min(1.0f, std::max(0.0f, (lfSpeed - 85.0f) / 40.0f));
    SetMixerInputValue(0, static_cast<s32>(lfHighSpeed * 32767.0f));

    f32 lfLowSpeed = 1.0f;
    if (!lbCrashing)
    {
        lfLowSpeed = 0.0f;
        if (lfSpeed < 125.0f)
            lfLowSpeed = std::min(1.0f, std::max(0.0f, (125.0f - lfSpeed) / 40.0f));
    }
    SetMixerInputValue(1, static_cast<s32>(lfLowSpeed * 32767.0f));
    SetMixerInputValue(2, lbCrashing ? 0x7FFF : 0);
    SetMixerInputValue(3, mBoostStreamOn.GetCurrent() ? 0x7FFF : 0);
}

SpeedStreamControl::~SpeedStreamControl()
{
}

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound

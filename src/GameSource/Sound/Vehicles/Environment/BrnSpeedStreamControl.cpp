#include "GameSource/Sound/Vehicles/Environment/BrnSpeedStreamControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <algorithm>
#include <cmath>

// ARTIST compiled this file through the GameSource unity file; its asserts carry this path.
#define BRN_SPEEDSTREAMEFFECT_CPP_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Sound/Vehicles/Environment/BrnSpeedStreamEffect.cpp"

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

// ARTIST 0x82F2F7D8: {0x100F0, "SpeedStreamControl", BrnEffectControl, CreateObject}, registered by
// its startup thunk through ClassTypeInfo<EffectControl>::AddToClassTypeInfoArray.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl> SpeedStreamControl::sTypeInfo(
    0x100F0, "SpeedStreamControl", &BrnSound::Logic::BrnEffectControl::sTypeInfo, &SpeedStreamControl::CreateObject);

CgsSound::Logic::EffectControl* SpeedStreamControl::CreateObject(u32 luAllocator)
{
    if (luAllocator == 0)
        return new (sTypeInfo.typeName, false) SpeedStreamControl();
    else
        return new (sTypeInfo.typeName, true) SpeedStreamControl();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* SpeedStreamControl::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectControl>* SpeedStreamControl::GetTypeInfo() const
{
    return &sTypeInfo;
}

const char* SpeedStreamControl::GetTypeName() const
{
    return sTypeInfo.typeName;
}

SpeedStreamControl::SpeedStreamControl()
    : BrnSound::Logic::BrnEffectControl()
    , mpPhysicsControl(0)
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
    // ARTIST 0x82686358: no null test; a switch whose default asserts.
    switch (apController->GetEffectID())
    {
    case 0:
        mpPhysicsControl = static_cast<BrnSound::Vehicles::Engines::PhysicsControl*>(apController);
        break;
    default:
        CGS_FAIL_AT("Unexpected control.", BRN_SPEEDSTREAMEFFECT_CPP_FILE, 122);
        break;
    }
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
    CGS_ASSERT(mpPhysicsControl != 0, "mpPhysicsControl");
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
    // ARTIST826BA310..314: fnmsubs(width,0.75,threshold), rounded once.
    const f32 lfOnset = -(KF_SPEED_STREAM_CROSSFADE_WIDTH * 0.75f - KF_SPEED_STREAM_THRESHOLD_MPH);
    if (!(lrPhysics.mSpeedMPH.GetCurrent() > lfOnset) || lbCrashing)
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

namespace
{
f32 ClampMixerInput(f32 lfValue)
{
    lfValue = (-lfValue >= 0.0f) ? 0.0f : lfValue;
    return (1.0f - lfValue >= 0.0f) ? lfValue : 1.0f;
}
}

void SpeedStreamControl::UpdateDuckers()
{
    CGS_ASSERT(mpPhysicsControl != 0, "mpPhysicsControl");
    if (!mpPhysicsControl)
        return;

    const BrnSound::Vehicles::Engines::PhysicsControl::PhysicsData& lrPhysics =
        mpPhysicsControl->GetPhysicsData();
    const bool lbCrashing = lrPhysics.IsCrashing.GetCurrent();
    const f32 lfSpeed = lrPhysics.mSpeedMPH.GetCurrent();
    // Original fmuls/fadds/fsubs at8269BA78..98; preserve the separate half-width
    // rounding and fsel clamps, including the zero-width/unordered path.
    const f32 lfHalfWidth = KF_SPEED_STREAM_CROSSFADE_WIDTH * 0.5f;
    const f32 lfUpper = lfHalfWidth + KF_SPEED_STREAM_THRESHOLD_MPH;
    const f32 lfLower = KF_SPEED_STREAM_THRESHOLD_MPH - lfHalfWidth;


    f32 lfHighSpeed = 0.0f;
    if (!lbCrashing)
        lfHighSpeed = ClampMixerInput((lfSpeed - lfLower) / KF_SPEED_STREAM_CROSSFADE_WIDTH);
    SetMixerInputValue(0, static_cast<s32>(lfHighSpeed * 32767.0f));

    f32 lfLowSpeed = 1.0f;
    if (!lbCrashing)
    {
        lfLowSpeed = 0.0f;
        if (lfSpeed < lfUpper)
            lfLowSpeed = ClampMixerInput((lfUpper - lfSpeed) / KF_SPEED_STREAM_CROSSFADE_WIDTH);
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

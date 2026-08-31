#include "GameSource/Sound/Vehicles/Wheels/BrnSkidEffect.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"
#include "GameShared/GameClasses/Sound/Logic/CgsStateManager.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Vehicles/Wheels/BrnWheelControl.h"
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"

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
const u32 KAU_SKID_PARAMETERS[26] =
{
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_pitch")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_vol_overall")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_vol_break")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_vol_peel")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_vol_lat")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_stereo_sep")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_surface_type_l")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_break_l")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_peel_l")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_lat_l")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_squall_l")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_surface_type_r")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_break_r")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_peel_r")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_lat_r")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_squall_r")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_speed")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_yaw")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_drift")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_reverse")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_filter_fx_lopass")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_filter_fx_hipass")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_filter_fx_dry")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_filter_fx_wet")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_car_type")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_lat_azimuth")),
};

const u32 KU_SEND01 = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
const u32 KU_REVERB_SEND = static_cast<u32>(CgsSound::Playback::Name::MakeHash("ReverbSend"));

inline s32 MixerValue(f32 afValue)
{
    // ARTIST/DecFIGS use fctiwz after multiplying the AEMS-domain value by 32.
    return static_cast<s32>(afValue * 32.0f);
}
}

SkidEffect::SkidEffect()
    : BrnEffectObject()
    , mDataHandle()
    , mpWheelControl(nullptr)
    , mpPhysicsControl(nullptr)
    , mfOverallMax(0.0f)
    , mfSkidAzimuth(0.0f)
    , mbSkidsLatched(false)
    , mSkidsVoice()
    , mSkidFunctorPointer()
{
    mDataHandle.Clear();
    mSkidFunctorPointer.Construct(this, &SkidEffect::OnPostInit);
}

SkidEffect::~SkidEffect()
{
}

const char* SkidEffect::GetTypeName() const
{
    return "SkidEffect";
}

s32 SkidEffect::GetController(s32 aiSlot)
{
    if (aiSlot == 0)
        return 1;
    if (aiSlot == 1)
        return 0;
    return -1;
}

void SkidEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    switch (apController->GetEffectID())
    {
    case 0:
        mpPhysicsControl = static_cast<BrnSound::Vehicles::Engines::PhysicsControl*>(apController);
        break;
    case 1:
        mpWheelControl = static_cast<WheelControl*>(apController);
        break;
    default:
        break;
    }
}

bool SkidEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;

    CgsSound::Logic::Content* lpSkidsContent = nullptr;
    if (GetStateBase() && GetStateBase()->GetStateManager())
    {
        const CgsSound::Playback::Name lContentName("Skids.abi");
        lpSkidsContent = GetStateBase()->GetStateManager()->GetContent(lContentName);
    }
    CGS_ASSERT(lpSkidsContent != nullptr, "lpSkidsContent");

    CgsSound::Logic::VoiceWrapper::CreateParams lParams;
    lParams.mpLogicModule = GetLogicModule();
    lParams.mpOnPostInit = &mSkidFunctorPointer;
    lParams.mFactoryName = static_cast<u32>(CgsSound::Playback::AemsFactorySkName().GetValue());
    lParams.mVoiceSpecName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_Skids"));
    lParams.mpContent = lpSkidsContent;
    lParams.mSlotName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_Slot"));
    lParams.mSendName = KU_SEND01;
    lParams.mSubMixVoiceID = 1;
    lParams.mReverbSendName = KU_REVERB_SEND;
    lParams.mReverbSubMixVoiceID = 2;
    lParams.miSendIndex = 0;
    mSkidsVoice.Create(lParams);
    mSkidsVoice.Play(0);

    // ARTIST performs this immediately as well as through OnPostInit.
    mSkidsVoice.SetParameter(1, 32767.0f, &KAU_SKID_PARAMETERS[1]);
    mfOverallMax = 0.0f;
    mfSkidAzimuth = 16384.0f;
    return true;
}

void SkidEffect::UpdateParams(f32)
{
    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    CGS_ASSERT(mpWheelControl != nullptr, "mpWheelControl");
    if (!mpPhysicsControl || !mpWheelControl)
        return;

    const BrnSound::Vehicles::VehicleData* lpRaw = mpPhysicsControl->GetRawPhysicsData();
    CGS_ASSERT(lpRaw != nullptr, "mpVehiclePhysicsData");
    if (!lpRaw)
        return;

    const BrnSound::Vehicles::Engines::PhysicsControl::PhysicsData& lrPhysics =
        mpPhysicsControl->GetPhysicsData();
    const WheelControl::WheelData& lrWheels = mpWheelControl->GetWheelData();

    const f32 lfYaw = lrPhysics.mYaw.GetCurrent();
    const f32 lfDrift = lpRaw->mfAbsDriftScale * 1024.0f;
    const f32 lfSpeed = std::max(lpRaw->mfSpeedMPH - 256.0f, 0.0f);

    mSkidsVoice.SetParameter(17, lfYaw, &KAU_SKID_PARAMETERS[17]);
    mSkidsVoice.SetParameter(18, lfDrift, &KAU_SKID_PARAMETERS[18]);
    mSkidsVoice.SetParameter(19, lrWheels.mbReverse ? 1.0f : 0.0f,
                             &KAU_SKID_PARAMETERS[19]);
    mSkidsVoice.SetParameter(16, lfSpeed, &KAU_SKID_PARAMETERS[16]);
    mSkidsVoice.SetParameter(5, 16384.0f, &KAU_SKID_PARAMETERS[5]);

    SetMixerInputValue(0, MixerValue(lfYaw));
    SetMixerInputValue(1, MixerValue(lfDrift));
    SetMixerInputValue(2, MixerValue(lfSpeed));

    f32 lfBrakeMax = 0.0f;
    f32 lfLateralMax = 0.0f;
    f32 lfPeelMax = 0.0f;
    f32 lfOverallMax = 0.0f;
    for (s32 liSide = 0; liSide < WheelControl::E_MAX_SIDES; ++liSide)
    {
        const WheelControl::WheelSide& lrSide = lrWheels.maSide[liSide];
        const f32 lfBrake = lrSide.mfBrakeNormalized * 1024.0f;
        const f32 lfPeel = lrSide.mfPeelNormalized * 1024.0f;
        const f32 lfLateral = lrSide.mfLateralNormalized * 1024.0f;
        const f32 lfSurface = static_cast<f32>(lrSide.muRoadnoiseLoop);

        lfBrakeMax = std::max(lfBrakeMax, lfBrake);
        lfPeelMax = std::max(lfPeelMax, lfPeel);
        lfLateralMax = std::max(lfLateralMax, lfLateral);
        lfOverallMax = std::max(lfOverallMax, std::max(lfBrake, std::max(lfPeel, lfLateral)));

        const s32 liBase = (liSide == WheelControl::E_LEFT_HAND_SIDE) ? 6 : 11;
        mSkidsVoice.SetParameter(liBase + 1, lfBrake, &KAU_SKID_PARAMETERS[liBase + 1]);
        mSkidsVoice.SetParameter(liBase + 2, lfPeel, &KAU_SKID_PARAMETERS[liBase + 2]);
        mSkidsVoice.SetParameter(liBase + 3, lfLateral, &KAU_SKID_PARAMETERS[liBase + 3]);
        mSkidsVoice.SetParameter(liBase, lfSurface, &KAU_SKID_PARAMETERS[liBase]);
    }

    mbSkidsLatched.Update(std::fabs(lfLateralMax) > 1.1920929e-7f);
    if (mbSkidsLatched.GetCurrent() && !mbSkidsLatched.GetPrevious())
    {
        if (lpRaw->mfSteering > 0.01f)
            mfSkidAzimuth = 16384.0f;
        else if (lpRaw->mfSteering < -0.01f)
            mfSkidAzimuth = 49152.0f;
    }

    const f32 lfDelta = lfOverallMax - mfOverallMax;
    if (std::fabs(lfDelta) > 100.0f)
        lfOverallMax = mfOverallMax + ((lfDelta < 0.0f) ? -100.0f : 100.0f);
    mfOverallMax = lfOverallMax;

    SetMixerInputValue(3, MixerValue(lfPeelMax));
    SetMixerInputValue(4, MixerValue(lfBrakeMax));
    SetMixerInputValue(5, MixerValue(lfLateralMax));
    SetMixerInputValue(6, 0);
    SetMixerInputValue(7, MixerValue(mfOverallMax));
}

void SkidEffect::ProcessUpdate()
{
    const f32 lfLateralVolume = GetMixerOutputValue(2, Nicotine::DMixIO::DMX_VOL);
    const f32 lfBrakeVolume = GetMixerOutputValue(1, Nicotine::DMixIO::DMX_VOL);
    const f32 lfPeelVolume = GetMixerOutputValue(0, Nicotine::DMixIO::DMX_VOL);
    const f32 lfPitch = GetMixerOutputValue(3, Nicotine::DMixIO::DMX_PITCH);
    const f32 lfReverbSend =
        GetMixerOutputValue(7, Nicotine::DMixIO::DMX_VOL) / 32767.0f;

    mSkidsVoice.SetParameter(2, lfBrakeVolume, &KAU_SKID_PARAMETERS[2]);
    mSkidsVoice.SetParameter(4, lfLateralVolume, &KAU_SKID_PARAMETERS[4]);
    mSkidsVoice.SetParameter(3, lfPeelVolume, &KAU_SKID_PARAMETERS[3]);
    mSkidsVoice.SetParameter(0, lfPitch, &KAU_SKID_PARAMETERS[0]);
    mSkidsVoice.SetParameter(25, mfSkidAzimuth, &KAU_SKID_PARAMETERS[25]);
    mSkidsVoice.SetGain(0, 1.0f, &KU_SEND01);
    mSkidsVoice.SetGain(1, lfReverbSend, &KU_REVERB_SEND);
    mSkidsVoice.Update();
}

bool SkidEffect::Detach()
{
    mSkidsVoice.Release();
    return BrnSound::Logic::BrnEffectObject::Detach();
}

void SkidEffect::OnPostInit(CgsSound::Logic::VoiceWrapper& arVoice)
{
    arVoice.SetParameter(24, 0.0f, &KAU_SKID_PARAMETERS[24]);
}

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound

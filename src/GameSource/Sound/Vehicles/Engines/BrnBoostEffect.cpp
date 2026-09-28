#include "GameShared/GameClasses/Sound/Logic/CgsState.h"
#include "GameShared/GameClasses/Sound/Playback/CgsSoundPcmTrace.h"
#include "GameSource/Sound/Vehicles/Engines/BrnBoostEffect.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Vehicles/Environment/BrnSpeedStreamControl.h"
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

namespace
{
const u32 KU_AEMS_PARAMETERS[12] =
{
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_velocity")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_start_stage_2")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_control")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_stop_boost")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_boost_remaining")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_car_speed")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_volume")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_time_since_last_boostin")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_time_since_last_boostout")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_time_boosting")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_is_boost_blue")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_skid_intensity")),
};
const u32 KU_SEND01 = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
const u32 KU_REVERB_SEND = static_cast<u32>(CgsSound::Playback::Name::MakeHash("ReverbSend"));
}

BoostEffect::BoostEffect()
    : BrnSound::Logic::BrnEffectObject()
    , BrnSound::Logic::Streaming::IStreamUser()
    , mBoostVoice()
    , mBoostFunctionPointer()
    , mParams()
    , mfParam_AEMS_velocity(0.0f)
    , mfParam_AEMS_start_stage_2(0.0f)
    , mfParam_AEMS_boost_remaining(0.0f)
    , mfParam_AEMS_car_speed(0.0f)
    , mfParam_AEMS_volume(0.0f)
    , mfParam_AEMS_control(0.0f)
    , mfParam_AEMS_time_since_last_boostin(0.0f)
    , mfParam_AEMS_time_since_last_boostout(0.0f)
    , mfParam_AEMS_time_boosting(0.0f)
    , mfParam_AEMS_is_boost_blue(0.0f)
    , mfParam_AEMS_skid_intensity(0.0f)
    , mTimeOfLastBoostOut(0.0f)
    , mTimeOfLastBoostIn(0.0f)
    , mTimeInBoost(0.0f)
    , mpPhysicsControl(nullptr)
    , mpSpeedStreamControl(nullptr)
{
    mBoostFunctionPointer.Construct(this, &BoostEffect::OnPostInit);
}

BoostEffect::~BoostEffect() {}

const char* BoostEffect::GetTypeName() const { return "BoostEffect"; }

s32 BoostEffect::GetController(s32 aiSlot)
{
    if (aiSlot == 0) return 0;
    if (aiSlot == 1) return 15;
    return -1;
}

void BoostEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    switch (apController->GetEffectID())
    {
    case 0: mpPhysicsControl = static_cast<PhysicsControl*>(apController); return;
    case 15:
        mpSpeedStreamControl = static_cast<BrnSound::Vehicles::Environment::SpeedStreamControl*>(apController);
        return;
    default: CGS_ASSERT(false, "Cound't attach controller "); return;
    }
}

void BoostEffect::SetupLoadData()
{
    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    if (!mpPhysicsControl)
        return;

    const char* lpcBank = mpPhysicsControl->GetVehicleEngineAttributes().BoostBank();
    CGS_ASSERT(lpcBank != nullptr, "mpPhysicsControl->GetVehicleEngineAttributes().mBoostBank()");
    CGS_ASSERT(lpcBank && *lpcBank, "strcmp(mpPhysicsControl->GetVehicleEngineAttributes().mBoostBank(),\"\") != 0");
    if (!lpcBank || !*lpcBank)
        return;

    char lacBundle[64];
    std::snprintf(lacBundle, sizeof(lacBundle), "sound\\aems\\%s.bundle", lpcBank);
    static_cast<BrnSound::Logic::IResourceRequester*>(this)->LoadAsset(
        lacBundle, nullptr, BrnSound::Logic::ResourceRegistrar::E_DATA);
}

bool BoostEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;

    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    if (!mpPhysicsControl)
        return false;

    const char* lpcBank = mpPhysicsControl->GetVehicleEngineAttributes().BoostBank();
    char lacContent[64];
    std::snprintf(lacContent, sizeof(lacContent), "%s.abi", lpcBank);

    CgsSound::Logic::VoiceWrapper::CreateParams lParams;
    lParams.mpLogicModule = GetLogicModule();
    lParams.mpOnPostInit = &mBoostFunctionPointer;
    lParams.mFactoryName = static_cast<u32>(CgsSound::Playback::AemsFactorySkName().GetValue());
    lParams.mVoiceSpecName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_Class"));
    lParams.mContentSpecName = static_cast<u32>(CgsSound::Playback::Name::MakeHash(lacContent));
    lParams.mSlotName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_Slot"));
    lParams.mSendName = KU_SEND01;
    lParams.mSubMixVoiceID = 1;
    lParams.mReverbSendName = KU_REVERB_SEND;
    lParams.mReverbSubMixVoiceID = 2;
    lParams.miSendIndex = 0;
    mBoostVoice.Create(lParams);
    mBoostVoice.Play(0);

    mfParam_AEMS_velocity = 0.0f;
    mfParam_AEMS_start_stage_2 = 0.0f;
    mfParam_AEMS_boost_remaining = 0.0f;
    mfParam_AEMS_car_speed = 0.0f;
    mfParam_AEMS_volume = 0.0f;
    mfParam_AEMS_control = 2.0f;
    mfParam_AEMS_time_since_last_boostin = 0.0f;
    mfParam_AEMS_time_since_last_boostout = 0.0f;
    mfParam_AEMS_time_boosting = 0.0f;
    mfParam_AEMS_is_boost_blue = 0.0f;
    mfParam_AEMS_skid_intensity = 0.0f;
    mTimeOfLastBoostOut.Update(0.0f);
    mTimeOfLastBoostIn.Update(0.0f);
    mTimeInBoost.Update(0.0f);
    return true;
}

void BoostEffect::UpdateParams(f32 afTimeStep)
{
    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    if (!mpPhysicsControl)
        return;

    const PhysicsControl::PhysicsData& lrPhysics = mpPhysicsControl->GetPhysicsData();
    // ARTIST826B4260..4268: owning State::mfCurTime, not an effect-local clock.
    const f32 lfNow = mpState->mfCurTime;
    mfParam_AEMS_start_stage_2 = 0.0f;
    mfParam_AEMS_velocity = 1024.0f;
    mfParam_AEMS_volume = GetMixerOutputValue(0, Nicotine::DMixIO::DMX_VOL);
    mfParam_AEMS_is_boost_blue = lrPhysics.IsBlueBoost ? 1.0f : 0.0f;
    mfParam_AEMS_skid_intensity = lrPhysics.mDrifting.GetCurrent() * 1024.0f;
    mfParam_AEMS_boost_remaining = lrPhysics.mfBoostRemaining * 1024.0f;
    // 826B42F0..4310: fsel caps MPH at256, then scales by1/256 and1024.
    // The negative/unordered arm retains the original speed (no lower clamp).
    const f32 lfSpeed = lrPhysics.mSpeedMPH.GetCurrent();
    const f32 lfLimitedSpeed = lfSpeed - 256.0f >= 0.0f ? 256.0f : lfSpeed;
    mfParam_AEMS_car_speed = (lfLimitedSpeed * 0.00390625f) * 1024.0f;

    if (lrPhysics.IsBoosting.GetCurrent() && !lrPhysics.IsBoosting.GetPrevious())
    {
        mfParam_AEMS_control = 1.0f;
        mTimeOfLastBoostIn.Update(lfNow);
        mTimeInBoost.Update(0.0f);
    }
    else if (!lrPhysics.IsBoosting.GetCurrent() && lrPhysics.IsBoosting.GetPrevious())
    {
        mfParam_AEMS_control = 2.0f;
        mTimeOfLastBoostOut.Update(lfNow);
    }

    if (lrPhysics.IsBoosting.GetCurrent())
        mTimeInBoost.Update(mTimeInBoost.GetCurrent() + afTimeStep);

    // 826B43A8..4408: elapsed centiseconds clamped0..32767. The subtract
    // supplies fsel's predicate; it is not the returned timer value. fsel with
    // unordered predicate chooses the negative arm, hence a NaN ends at32767.
    const auto ClampTimer = [](f32 lfElapsed) {
        const f32 lfScaled = lfElapsed * 100.0f; // flt820049E0
        const f32 lfPositive = -lfScaled >= 0.0f ? 0.0f : lfScaled;
        return 32767.0f - lfPositive >= 0.0f ? lfPositive : 32767.0f; // flt820AD310
    };
    mfParam_AEMS_time_boosting = ClampTimer(mTimeInBoost.GetCurrent());
    mfParam_AEMS_time_since_last_boostin = ClampTimer(lfNow - mTimeOfLastBoostIn.GetPrevious());
    mfParam_AEMS_time_since_last_boostout = ClampTimer(lfNow - mTimeOfLastBoostOut.GetCurrent());

    UpdateBoostStream();
}

void BoostEffect::ProcessUpdate()
{
    UpdateAemsBoostParameters();
    mBoostVoice.Update();
    // FLAG PC-platform witness: boost edges and the authored control inputs.
    if (CgsSound::PcmTrace::File()) {
        static int lastStage = -1, lastBoost = -1;
        const int stage = mBoostVoice.GetUpdateStage();
        const int boost = mpPhysicsControl->GetPhysicsData().IsBoosting.GetCurrent();
        if (stage != lastStage || boost != lastBoost) {
            lastStage = stage; lastBoost = boost;
            CgsSound::PcmTrace::Log("boost-input stage=%d boosting=%d speed=%.9g time=%.9g sincein=%.9g sinceout=%.9g volume=%.9g control=%.9g\n",
                stage, boost, mfParam_AEMS_car_speed, mfParam_AEMS_time_boosting,
                mfParam_AEMS_time_since_last_boostin, mfParam_AEMS_time_since_last_boostout,
                mfParam_AEMS_volume, mfParam_AEMS_control);
        }
    }
}

void BoostEffect::UpdateAemsBoostParameters()
{
    mBoostVoice.SetParameter(0, mfParam_AEMS_velocity, &KU_AEMS_PARAMETERS[0]);
    mBoostVoice.SetParameter(1, mfParam_AEMS_start_stage_2, &KU_AEMS_PARAMETERS[1]);
    mBoostVoice.SetParameter(4, mfParam_AEMS_boost_remaining, &KU_AEMS_PARAMETERS[4]);
    mBoostVoice.SetParameter(5, mfParam_AEMS_car_speed, &KU_AEMS_PARAMETERS[5]);
    mBoostVoice.SetParameter(6, mfParam_AEMS_volume, &KU_AEMS_PARAMETERS[6]);
    mBoostVoice.SetParameter(2, mfParam_AEMS_control, &KU_AEMS_PARAMETERS[2]);
    mBoostVoice.SetParameter(7, mfParam_AEMS_time_since_last_boostin, &KU_AEMS_PARAMETERS[7]);
    mBoostVoice.SetParameter(8, mfParam_AEMS_time_since_last_boostout, &KU_AEMS_PARAMETERS[8]);
    mBoostVoice.SetParameter(9, mfParam_AEMS_time_boosting, &KU_AEMS_PARAMETERS[9]);
    mBoostVoice.SetParameter(10, mfParam_AEMS_is_boost_blue, &KU_AEMS_PARAMETERS[10]);
    mBoostVoice.SetParameter(11, mfParam_AEMS_skid_intensity, &KU_AEMS_PARAMETERS[11]);
    mBoostVoice.SetGain(1, GetMixerOutputValue(5, Nicotine::DMixIO::DMX_VOL) / 32767.0f,
                        &KU_REVERB_SEND);
    mBoostVoice.SetGain(0, 3.0f, &KU_SEND01);
}

void BoostEffect::OnPostInit(CgsSound::Logic::VoiceWrapper&)
{
    UpdateAemsBoostParameters();
}

const CgsSound::Logic::VoiceWrapper::CreateParams& BoostEffect::GetCreateParams() const
{
    return mParams;
}

void BoostEffect::UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                                    f32 afGain, f32)
{
    const u32 luPauseControl = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("PauseControl"));
    arVoice.SetParameter(1, 0.0f, &luPauseControl);
    const u32 luSend = mParams.mSendName;
    arVoice.SetGain(0,
        GetRWACMixerOutputValue(4, Nicotine::DMixIO::DMX_VOL) * afGain,
        &luSend);
}

void BoostEffect::UpdateBoostStream()
{
    CGS_ASSERT(mpSpeedStreamControl != nullptr, "mpSpeedStreamControl");
    if (!mpSpeedStreamControl)
        return;

    const CgsSound::Utils::DataPoint<bool>& lrBoost =
        mpSpeedStreamControl->GetBoostStreamStatus();
    if (!lrBoost.HasChanged())
        return;

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    BrnSound::Logic::Streaming::StreamingStateManager* lpStreamingManager =
        lpModule ? static_cast<BrnSound::Logic::Streaming::StreamingStateManager*>(
            lpModule->GetEnvironment().GetStateManager(6)) : nullptr;
    CGS_ASSERT(lpStreamingManager != nullptr, "lpStreamingStateMan");
    if (!lpStreamingManager)
        return;

    if (lrBoost.GetCurrent())
    {
        mParams.Clear();
        mParams.mpLogicModule = GetLogicModule();
        mParams.mFactoryName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("~GenericRwacFactory::SK_NAME~"));
        mParams.mVoiceSpecName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("MusicVoiceSpec"));
        mParams.mContentSpecName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("BoostUrban"));
        mParams.mSlotName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("~PlayerVoice::SK_PLAYER_SLOT_NAME~"));
        mParams.mSendName = KU_SEND01;
        mParams.mSubMixVoiceID = 1;
        mParams.miSendIndex = 0;
        lpStreamingManager->PostStreamRequest(
            BrnSound::Logic::Streaming::StreamRequest(this, 3, 0.1f));
    }
    else
    {
        lpStreamingManager->PostStreamRequest(
            BrnSound::Logic::Streaming::StreamStopRequest(this, 0.4f));
    }
}

bool BoostEffect::Detach()
{
    if (!BrnSound::Logic::BrnEffectObject::Detach())
        return false;
    mBoostVoice.Release();

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    BrnSound::Logic::Streaming::StreamingStateManager* lpStreamingManager =
        lpModule ? static_cast<BrnSound::Logic::Streaming::StreamingStateManager*>(
            lpModule->GetEnvironment().GetStateManager(6)) : nullptr;
    CGS_ASSERT(lpStreamingManager != nullptr, "lpStreamingStateMan");
    if (lpStreamingManager)
        lpStreamingManager->PostStreamRequest(
            BrnSound::Logic::Streaming::StreamStopRequest(this, 0.25f));
    return true;
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

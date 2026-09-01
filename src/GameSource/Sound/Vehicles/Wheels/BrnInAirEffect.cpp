#include "GameSource/Sound/Vehicles/Wheels/BrnInAirEffect.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Sound/Logic/CgsEnvironment.h"
#include "GameShared/GameClasses/Sound/Logic/CgsState.h"
#include "GameShared/GameClasses/Sound/Logic/CgsStateManager.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameSource/Director/Camera/Camera.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"
#include "GameSource/Sound/Collision/BrnCollisionStateManager.h"
#include "GameSource/Sound/Global/BrnGlobalStateManager.h"
#include "GameSource/Sound/Module/BrnRootSoundModuleIo.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Traffic/BrnTrafficState.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"
#include "GameSource/Sound/Vehicles/Wheels/BrnWheelControl.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.h"
#include "SDKs/EATech/include/Nicotine/DMixIO.hpp"

#include <algorithm>

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

namespace
{
const f32 KF_TIME_IN_AIR_FOR_JUMP = 0.15f;
const f32 KF_JUMP_CAMERA_LANDING_WINDOW = 0.5f;
const f32 KF_COMPRESSION_MIN_RATE = 0.05f;
const f32 KF_COMPRESSION_MAX_RATE = 0.15f;
const f32 KF_COMPRESSION_MAX_VOLUME = 2.0f;
const f32 KF_MIN_TIME_SINCE_RESET = 0.5f;

const u32 KU_SEND01 = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("Send01"));
const u32 KU_SPLICE_PITCH = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Pitch~"));
const u32 KU_SPLICE_AZIMUTH = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Azimuth~"));
const u32 KU_AEMS_VOLUME = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("AEMS_volume"));
const u32 KU_AEMS_PITCH = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("AEMS_pitch"));
const u32 KU_AEMS_AZIMUTH = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("AEMS_azimuth"));
const u32 KU_AEMS_TIME_IN_AIR = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("AEMS_timeinair"));
const u32 KU_AEMS_SPEED = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("AEMS_speed"));
const u32 KU_AEMS_HEIGHT = static_cast<u32>(
    CgsSound::Playback::Name::MakeHash("AEMS_height"));

u32 guSmallLandingRoundRobin = 0;
u32 guSuspensionRoundRobin = 0;
u32 guCrashLandingRoundRobin = 0;
u32 guJunkyardLandingRoundRobin = 0;
u32 guJumpCamLandingRoundRobin = 0;

CgsSound::Logic::VoiceWrapper::CreateParams MakeSplicerParams(
    CgsSound::Logic::Module* apModule,
    const CgsSound::Logic::Content* apContent)
{
    CgsSound::Logic::VoiceWrapper::CreateParams lParams;
    lParams.mpLogicModule = apModule;
    lParams.mFactoryName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("~SplicerFactory::SK_NAME~"));
    lParams.mVoiceSpecName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("SplicerVoiceSpec"));
    lParams.mpContent = apContent;
    lParams.mSlotName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("~SplicerPlayerVoice::Slot~"));
    lParams.mSendName = KU_SEND01;
    lParams.mSubMixVoiceID = 1;
    lParams.miSendIndex = 0;
    return lParams;
}

void SetSplicerVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                           f32 afGain, f32 afPitch, f32 afAzimuth)
{
    if (!arVoice.IsPlaying())
        return;

    arVoice.SetGain(0, afGain, &KU_SEND01);
    arVoice.SetParameter(0, afPitch, &KU_SPLICE_PITCH);
    arVoice.SetParameter(1, afAzimuth, &KU_SPLICE_AZIMUTH);
}
}

void InAirEffect::SuspensionStatus::Clear()
{
    mfSuspensionHeight = 0.0f;
    meSuspensionLatchedState = E_NONE;
    mSuspensionDelta.Flush(0.0f);
}

InAirEffect::InAirPhysicsData::InAirPhysicsData()
    : mIsOnGround(false)
    , mfTimeInAir(0.0f)
    , mfTimeSinceReset(0.0f)
    , mbIsCrashing(false)
{
    for (u32 luWheel = 0; luWheel < 4; ++luWheel)
    {
        maWheelOnGround[luWheel].Flush(false);
        mafSuspensionHeights[luWheel] = 0.0f;
    }
}

InAirEffect::InAirEffect()
    : BrnSound::Logic::BrnEffectObject()
    , mpWheelControl(nullptr)
    , mpPhysicsControl(nullptr)
    , mpEnclosureControl(nullptr)
    , mInAirVoice()
    , mLandingVoices()
    , mHardLandingVoice()
    , mJunkyardLandingSweetnerVoice()
    , mJumpCamLandingVoice()
    , mLaunchFunctionPointer()
    , mfHardLandingVoiceSecondGain(0.0f)
    , mfJumpCamLandingVoiceSecondGain(0.0f)
    , mfJunkyardLandingSweetnerVoiceSecondGain(0.0f)
    , mfSuspensionSensitivity(0.0f)
    , mfSuspensionThreshold(0.0f)
    , mBin()
    , muRoundRobin(0)
    , mpCollisionMgr(nullptr)
    , mfTimeSinceJumpCamera(0.0f)
    , mPhysicsData()
{
}

InAirEffect::~InAirEffect()
{
}

const char* InAirEffect::GetTypeName() const
{
    return "InAirEffect";
}

s32 InAirEffect::GetController(s32 aiSlot)
{
    if (aiSlot == 0)
        return 1;
    if (aiSlot == 1)
        return 0;
    return -1;
}

void InAirEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    if (!apController)
        return;

    if (apController->GetEffectID() == 0)
        mpPhysicsControl = static_cast<BrnSound::Vehicles::Engines::PhysicsControl*>(apController);
    else if (apController->GetEffectID() == 1)
        mpWheelControl = static_cast<WheelControl*>(apController);
}

void InAirEffect::SetupLoadData()
{
    LoadAsset("sound\\aems\\InAir.bundle", nullptr,
              BrnSound::Logic::ResourceRegistrar::E_DATA);
}

bool InAirEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;

    mfTimeSinceJumpCamera = 0.0f;
    if (GetInstanceId() == 1)
    {
        mfSuspensionSensitivity = 0.05f;
        mfSuspensionThreshold = 0.15f;
    }
    else
    {
        mfSuspensionSensitivity = 0.01f;
        mfSuspensionThreshold = 0.05f;
    }

    mLandingVoices.Prepare(GetLogicModule());

    if (GetInstanceId() == 1)
    {
        CgsSound::Logic::Content* lpContent = nullptr;
        if (GetStateBase() && GetStateBase()->GetStateManager())
        {
            lpContent = GetStateBase()->GetStateManager()->GetContent(
                CgsSound::Playback::Name("inair.abi"));
        }
        CGS_ASSERT(lpContent != nullptr, "lpContent");

        CgsSound::Logic::VoiceWrapper::CreateParams lParams;
        lParams.mpLogicModule = GetLogicModule();
        lParams.mFactoryName = static_cast<u32>(
            CgsSound::Playback::AemsFactorySkName().GetValue());
        lParams.mVoiceSpecName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("AEMS_PlayInAir"));
        lParams.mpContent = lpContent;
        lParams.mSlotName = static_cast<u32>(
            CgsSound::Playback::Name::MakeHash("AEMS_Slot"));
        lParams.mSendName = KU_SEND01;
        lParams.mSubMixVoiceID = 1;
        lParams.miSendIndex = 0;
        mInAirVoice.Create(lParams);
        mInAirVoice.Play(0);
        mInAirVoice.SetParameter(5, 0.0f, &KU_AEMS_HEIGHT);
    }

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    CGS_ASSERT(lpModule != nullptr, "lpLogicModule");
    if (lpModule)
    {
        mpCollisionMgr = static_cast<BrnSound::Logic::Collision::CollisionStateManager*>(
            lpModule->GetEnvironment().GetStateManager(5));
        mBin.ChangeWithDefault(lpModule->GetGlobalData().InAirCrashBin());
    }
    CGS_ASSERT(mpCollisionMgr != nullptr, "mpCollisionMgr");
    CGS_ASSERT(mBin.mNumCollisionsMedium() > 0,
               "mBin.mNumCollisionsMedium() > 0");

    for (u32 luWheel = 0; luWheel < 4; ++luWheel)
        mSuspensionStatus[luWheel].Clear();
    return true;
}

bool InAirEffect::Detach()
{
    mInAirVoice.Release();
    mLandingVoices.Release();
    mHardLandingVoice.Release();
    mJunkyardLandingSweetnerVoice.Release();
    mJumpCamLandingVoice.Release();
    mpCollisionMgr = nullptr;
    return BrnSound::Logic::BrnEffectObject::Detach();
}

void InAirEffect::ProcessUpdate()
{
    const f32 lfLandingGain =
        GetMixerOutputValue(0, Nicotine::DMixIO::DMX_VOL) / 32767.0f;
    const f32 lfPitch =
        GetMixerOutputValue(1, Nicotine::DMixIO::DMX_PITCH) / 4096.0f;
    const f32 lfAzimuth =
        GetMixerOutputValue(2, Nicotine::DMixIO::DMX_VOL) / 32767.0f;

    mLandingVoices.SetGain(0, lfLandingGain, 0, &KU_SEND01);
    mLandingVoices.SetParameter(0, lfPitch, 0, &KU_SPLICE_PITCH);
    mLandingVoices.SetParameter(1, lfAzimuth, 0, &KU_SPLICE_AZIMUTH);

    SetSplicerVoiceParams(
        mJumpCamLandingVoice,
        GetRWACMixerOutputValue(0, Nicotine::DMixIO::DMX_VOL) *
            mfJumpCamLandingVoiceSecondGain,
        lfPitch, GetRWACMixerOutputValue(2, Nicotine::DMixIO::DMX_VOL));
    SetSplicerVoiceParams(
        mHardLandingVoice,
        GetRWACMixerOutputValue(12, Nicotine::DMixIO::DMX_VOL) *
            mfHardLandingVoiceSecondGain,
        lfPitch, GetRWACMixerOutputValue(2, Nicotine::DMixIO::DMX_VOL));
    SetSplicerVoiceParams(
        mJunkyardLandingSweetnerVoice,
        GetRWACMixerOutputValue(12, Nicotine::DMixIO::DMX_VOL) *
            mfJunkyardLandingSweetnerVoiceSecondGain,
        lfPitch, GetRWACMixerOutputValue(2, Nicotine::DMixIO::DMX_VOL));

    if (mInAirVoice.IsPlaying())
    {
        mInAirVoice.SetParameter(
            0, GetMixerOutputValue(3, Nicotine::DMixIO::DMX_VOL), &KU_AEMS_VOLUME);
        mInAirVoice.SetParameter(
            1, GetMixerOutputValue(4, Nicotine::DMixIO::DMX_PITCH), &KU_AEMS_PITCH);
        mInAirVoice.SetParameter(
            2, GetMixerOutputValue(5, Nicotine::DMixIO::DMX_VOL), &KU_AEMS_AZIMUTH);
        mInAirVoice.SetParameter(
            3, mPhysicsData.mfTimeInAir.GetCurrent() * 1000.0f, &KU_AEMS_TIME_IN_AIR);
        if (mpPhysicsControl && mpPhysicsControl->GetRawPhysicsData())
        {
            mInAirVoice.SetParameter(
                4, mpPhysicsControl->GetRawPhysicsData()->mfSpeedMPH, &KU_AEMS_SPEED);
        }
    }
}

void InAirEffect::UpdateParams(f32 afTimeStep)
{
    SetMixerInputValue(0, 0);
    SetMixerInputValue(2, 0);
    SetMixerInputValue(1, 0);
    SetMixerInputValue(0, 0);

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    BrnSound::Module::Io::LogicInputBuffer* lpInput =
        lpModule ? lpModule->GetBrnInputStructure() : nullptr;
    CGS_ASSERT(lpInput != nullptr, "lpInput");
    if (GetInstanceId() == 1 && lpInput && lpInput->GetDirectorCamera() &&
        (lpInput->GetDirectorCamera()->mState_uFlags & 0x80) != 0)
    {
        mfTimeSinceJumpCamera = 0.0f;
    }
    else
    {
        mfTimeSinceJumpCamera += afTimeStep;
    }

    UpdatePhysicsData(afTimeStep);
    UpdateWheelLandings(afTimeStep);
    if (GetInstanceId() == 0 || GetInstanceId() == 1)
        UpdateSuspensionSqueeks(afTimeStep);

    mInAirVoice.Update();
    mJunkyardLandingSweetnerVoice.Update();
    mJumpCamLandingVoice.Update();
    mLandingVoices.Update();
    mHardLandingVoice.Update();
}

void InAirEffect::UpdatePhysicsData(f32 afTimeStep)
{
    CGS_ASSERT(mpWheelControl != nullptr, "mpWheelControl");
    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    if (!mpWheelControl || !mpPhysicsControl)
        return;

    mPhysicsData.mIsOnGround = mpWheelControl->IsOnGround();
    const BrnSound::Vehicles::VehicleData* lpRaw =
        mpPhysicsControl->GetRawPhysicsData();
    CGS_ASSERT(lpRaw != nullptr, "mpVehiclePhysicsData");
    if (!lpRaw)
        return;

    for (s32 liWheel = 0; liWheel < 4; ++liWheel)
    {
        mPhysicsData.maWheelOnGround[liWheel] =
            mpWheelControl->GetSingleWheelStatus(liWheel).mIsOnGround;
        mPhysicsData.mafSuspensionHeights[liWheel] =
            lpRaw->maWheels[liWheel].mfSuspensionHeight;
    }

    const f32 lfPreviousTime = mPhysicsData.mfTimeInAir.GetCurrent();
    mPhysicsData.mfTimeInAir.Update(
        mPhysicsData.mIsOnGround.GetCurrent() ? 0.0f : lfPreviousTime + afTimeStep);
    mPhysicsData.mfTimeSinceReset =
        mpPhysicsControl->GetPhysicsData().mfTimeSinceRespawn;
    mPhysicsData.mbIsCrashing = lpRaw->mbCrashing;
}

void InAirEffect::UpdateWheelLandings(f32)
{
    bool lbWheelJustLanded = false;
    for (u32 luWheel = 0; luWheel < 4; ++luWheel)
    {
        const CgsSound::Utils::DataPoint<bool>& lrWheel =
            mPhysicsData.maWheelOnGround[luWheel];
        if (lrWheel.GetCurrent() && !lrWheel.GetPrevious())
        {
            lbWheelJustLanded = true;
            break;
        }
    }

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());

    if (lbWheelJustLanded)
    {
        PlayLanding(mPhysicsData.mbIsCrashing
                        ? BrnSound::Logic::Collision::E_COLLISION_SPLICE_CRASH_LANDINGS
                        : BrnSound::Logic::Collision::E_COLLISION_SPLICE_SMALL_LANDINGS,
                    1.0f);
        SetMixerInputValue(0, 0x7FFF);

        if (GetInstanceId() == 1 && lpModule)
        {
            CgsSound::Io::Message<bool> lMessage(false);
            lMessage.Construct(14, 0, 0, 2,
                CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT);
            lpModule->PostMessage(lMessage);
        }
    }

    if (mPhysicsData.mIsOnGround.GetCurrent())
    {
        if (mPhysicsData.mfTimeInAir.GetPrevious() > KF_TIME_IN_AIR_FOR_JUMP)
        {
            PlayLanding(
                BrnSound::Logic::Collision::E_COLLISION_SPLICE_HARD_LANDINGS, 1.0f);
            SetMixerInputValue(1, 0x7FFF);

            if (GetInstanceId() == 0)
            {
                PlayLanding(
                    BrnSound::Logic::Collision::E_COLLISION_SPLICE_JUNKYARD_LANDING_SWEETNER,
                    1.0f);
                SetMixerInputValue(0, 0x7FFF);
            }

            if (GetInstanceId() != 0 &&
                mfTimeSinceJumpCamera < KF_JUMP_CAMERA_LANDING_WINDOW)
            {
                PlayJumpCamLanding();
            }
        }
    }
    else if (mPhysicsData.mfTimeInAir.GetCurrent() > KF_TIME_IN_AIR_FOR_JUMP)
    {
        SetMixerInputValue(2, 0x7FFF);
        if (GetInstanceId() == 1 && lpModule)
        {
            CgsSound::Io::Message<bool> lMessage(true);
            lMessage.Construct(14, 0, 0, 2,
                CgsSound::Io::MessageHeader::E_EFFECT_TYPE_OBJECT);
            lpModule->PostMessage(lMessage);
        }
    }
}

void InAirEffect::UpdateSuspensionSqueeks(f32)
{
    for (u32 luWheel = 0; luWheel < 4; ++luWheel)
    {
        SuspensionStatus& lrStatus = mSuspensionStatus[luWheel];
        const f32 lfPreviousHeight = lrStatus.mfSuspensionHeight;
        const f32 lfCurrentHeight = mPhysicsData.mafSuspensionHeights[luWheel];
        lrStatus.mfSuspensionHeight = lfCurrentHeight;
        lrStatus.mSuspensionDelta.Record(lfCurrentHeight - lfPreviousHeight);

        if (lfCurrentHeight <= mfSuspensionThreshold)
        {
            lrStatus.meSuspensionLatchedState = SuspensionStatus::E_NONE;
            continue;
        }

        const f32 lfAverage = lrStatus.mSuspensionDelta.GetAverage();
        if (lrStatus.meSuspensionLatchedState == SuspensionStatus::E_NONE &&
            lfAverage > mfSuspensionSensitivity)
        {
            const f32 lfClamped = std::max(
                KF_COMPRESSION_MIN_RATE,
                std::min(KF_COMPRESSION_MAX_RATE, lfAverage));
            const f32 lfFraction =
                (lfClamped - KF_COMPRESSION_MIN_RATE) /
                (KF_COMPRESSION_MAX_RATE - KF_COMPRESSION_MIN_RATE);
            PlayLanding(
                BrnSound::Logic::Collision::E_COLLISION_SPLICE_SUSPENSION,
                1.0f + lfFraction * (KF_COMPRESSION_MAX_VOLUME - 1.0f));
        }
        lrStatus.meSuspensionLatchedState = SuspensionStatus::E_COMPRESSED;
    }
}

BrnSound::Logic::BrnEffectObject::SampleTag InAirEffect::GetSampleLandingId(
    BrnSound::Logic::Collision::ECollisionSpliceTags aeTag)
{
    BrnSound::Logic::BrnEffectObject::SampleTag lTag;
    lTag.Construct();

    switch (aeTag)
    {
    case BrnSound::Logic::Collision::E_COLLISION_SPLICE_SUSPENSION:
        GetSampleTag(1, 1, guSuspensionRoundRobin++, lTag);
        break;

    case BrnSound::Logic::Collision::E_COLLISION_SPLICE_HARD_LANDINGS:
    {
        u16 lauSamples[32];
        BrnSound::Logic::Collision::CrashBinUtils<Attrib::Gen::crashbin> lUtils;
        const u32 luCount = lUtils.GetSampleIds(
            &mBin, &Attrib::Gen::crashbin::mNumCollisionsMedium,
            &Attrib::Gen::crashbin::mCollisionsMedium, lauSamples, 32);
        if (luCount)
        {
            lTag.miSampleIndex = static_cast<s16>(lauSamples[muRoundRobin % luCount]);
            ++muRoundRobin;
            lTag.mfVolume = 1.0f;
        }
        break;
    }

    case BrnSound::Logic::Collision::E_COLLISION_SPLICE_CRASH_LANDINGS:
        GetSampleTag(1, 3, guCrashLandingRoundRobin++, lTag);
        break;

    case BrnSound::Logic::Collision::E_COLLISION_SPLICE_JUNKYARD_LANDING_SWEETNER:
        GetSampleTag(1, 4, guJunkyardLandingRoundRobin++, lTag);
        break;

    case BrnSound::Logic::Collision::E_COLLISION_SPLICE_SMALL_LANDINGS:
    default:
        GetSampleTag(1, 0, guSmallLandingRoundRobin++, lTag);
        break;
    }
    return lTag;
}

void InAirEffect::PlayLanding(
    BrnSound::Logic::Collision::ECollisionSpliceTags aeTag,
    f32 afVolumeModifier)
{
    if (mPhysicsData.mfTimeSinceReset <= KF_MIN_TIME_SINCE_RESET || !mpCollisionMgr)
        return;

    const BrnSound::Logic::BrnEffectObject::SampleTag lTag =
        GetSampleLandingId(aeTag);
    if (lTag.miSampleIndex < 0)
        return;

    const CgsSound::Logic::VoiceWrapper::CreateParams lParams = MakeSplicerParams(
        GetLogicModule(),
        &mpCollisionMgr->GetSplicerBank(
            BrnSound::Logic::Collision::E_COLLISION_SPLICE_BANK_COLLISION));

    if (aeTag == BrnSound::Logic::Collision::E_COLLISION_SPLICE_HARD_LANDINGS)
    {
        if (mHardLandingVoice.IsAlive())
            mHardLandingVoice.Release();
        mHardLandingVoice.Create(lParams);
        mHardLandingVoice.Play(static_cast<u16>(lTag.miSampleIndex));
        mfHardLandingVoiceSecondGain = lTag.mfVolume * afVolumeModifier;
    }
    else if (aeTag ==
             BrnSound::Logic::Collision::E_COLLISION_SPLICE_JUNKYARD_LANDING_SWEETNER)
    {
        if (mJunkyardLandingSweetnerVoice.IsAlive())
            mJunkyardLandingSweetnerVoice.Release();
        mJunkyardLandingSweetnerVoice.Create(lParams);
        mJunkyardLandingSweetnerVoice.Play(static_cast<u16>(lTag.miSampleIndex));
        mfJunkyardLandingSweetnerVoiceSecondGain =
            lTag.mfVolume * afVolumeModifier;
    }
    else
    {
        CgsSound::Logic::PooledVoice* lpVoice = mLandingVoices.GetFreeVoice();
        lpVoice->mfSecondaryGain = 1.0f;
        lpVoice->mWrapper.Create(lParams);
        lpVoice->muAge = 0;
        lpVoice->mbInUse = 1;
        lpVoice->mWrapper.Play(static_cast<u16>(lTag.miSampleIndex));
        lpVoice->mfSecondaryGain = lTag.mfVolume * afVolumeModifier;
    }
}

void InAirEffect::PlayJumpCamLanding()
{
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    BrnSound::Logic::GlobalStateManager* lpGlobal = lpModule
        ? static_cast<BrnSound::Logic::GlobalStateManager*>(
              lpModule->GetEnvironment().GetStateManager(0))
        : nullptr;
    CGS_ASSERT(lpGlobal != nullptr, "lpGlobalStateMgr");
    if (!lpGlobal || mPhysicsData.mfTimeSinceReset <= KF_MIN_TIME_SINCE_RESET)
        return;

    if (mJumpCamLandingVoice.IsAlive())
        mJumpCamLandingVoice.Release();

    BrnSound::Logic::BrnEffectObject::SampleTag lTag;
    lTag.Construct();
    if (!GetSampleTag(4, 3, guJumpCamLandingRoundRobin++, lTag) ||
        lTag.miSampleIndex < 0)
        return;

    const CgsSound::Logic::VoiceWrapper::CreateParams lParams = MakeSplicerParams(
        GetLogicModule(), &lpGlobal->GetPresentationSpliceBank());
    mJumpCamLandingVoice.Create(lParams);
    mJumpCamLandingVoice.Play(static_cast<u16>(lTag.miSampleIndex));
    mfJumpCamLandingVoiceSecondGain = lTag.mfVolume;
}

TrafficInAir::TrafficInAir()
    : InAirEffect()
    , mpTrafficEntity(nullptr)
{
}

TrafficInAir::~TrafficInAir()
{
}

const char* TrafficInAir::GetTypeName() const
{
    return "TrafficInAir";
}

bool TrafficInAir::Attach()
{
    if (!InAirEffect::Attach())
        return false;

    BrnSound::Logic::Traffic::TrafficState* lpState =
        static_cast<BrnSound::Logic::Traffic::TrafficState*>(GetStateBase());
    CGS_ASSERT(lpState != nullptr, "lpState");
    mpTrafficEntity = lpState ? lpState->GetTrafficEntity() : nullptr;
    return true;
}

void TrafficInAir::Clear()
{
    mPhysicsData.mIsOnGround.Update(true);
    for (u32 luWheel = 0; luWheel < 4; ++luWheel)
        mPhysicsData.maWheelOnGround[luWheel].Update(true);
    mPhysicsData.mfTimeInAir.Flush(0.0f);
    mPhysicsData.mfTimeSinceReset = 0.0f;
    mPhysicsData.mbIsCrashing = false;
}

void TrafficInAir::UpdatePhysicsData(f32 afTimeStep)
{
    if (!mpTrafficEntity || !mpTrafficEntity->mbIsPhysical)
    {
        Clear();
        return;
    }

    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(GetLogicModule());
    BrnSound::Module::Io::LogicInputBuffer* lpInput =
        lpModule ? lpModule->GetBrnInputStructure() : nullptr;
    CGS_ASSERT(lpInput != nullptr, "lpInputBuffer");
    const BrnSound::Module::Io::RootInputBuffer::PhysicalTrafficStateQueue* lpQueue =
        lpInput ? lpInput->GetPhysicalTrafficStates() : nullptr;

    const BrnPhysics::Vehicle::PhysicalTrafficState* lpTraffic = nullptr;
    if (lpQueue)
    {
        for (s32 liIndex = 0; liIndex < lpQueue->GetLength(); ++liIndex)
        {
            const BrnPhysics::Vehicle::PhysicalTrafficState& lrState =
                lpQueue->GetEvent(liIndex);
            if (lrState.mEntityID.muValue == mpTrafficEntity->mEntityId.muValue)
            {
                lpTraffic = &lrState;
                break;
            }
        }
    }

    if (!lpTraffic)
    {
        Clear();
        return;
    }

    bool lbAnyWheelOnGround = false;
    for (u32 luWheel = 0; luWheel < 4; ++luWheel)
    {
        const bool lbOnGround =
            lpTraffic->maWheels[luWheel].mRoadContact.mbIsOnGround;
        mPhysicsData.maWheelOnGround[luWheel].Update(lbOnGround);
        mPhysicsData.mafSuspensionHeights[luWheel] =
            lpTraffic->maWheels[luWheel].mfSuspensionHeight;
        lbAnyWheelOnGround = lbAnyWheelOnGround || lbOnGround;
    }
    mPhysicsData.mIsOnGround.Update(lbAnyWheelOnGround);

    bool lbPlayerFatal = false;
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
        lpVehicles = lpInput->GetVehicleInterface();
    if (lpVehicles)
        lbPlayerFatal = lpVehicles->IsPlayerCarFatalyCrashing();
    mPhysicsData.mbIsCrashing =
        lpTraffic->mbIsFatallyCrashing && lbPlayerFatal;

    const f32 lfPreviousTime = mPhysicsData.mfTimeInAir.GetCurrent();
    mPhysicsData.mfTimeInAir.Update(
        lbAnyWheelOnGround ? 0.0f : lfPreviousTime + afTimeStep);
    mPhysicsData.mfTimeSinceReset = mpPhysicsControl
        ? mpPhysicsControl->GetPhysicsData().mfTimeSinceRespawn
        : 0.0f;
}

void TrafficInAir::ProcessUpdate()
{
    // The playback/recording snapshots use this same typed physics state.  The
    // serialiser remains owned by SoundLogicModule; this effect never reaches it
    // through the console's raw module offset on the 64-bit host.
    InAirEffect::ProcessUpdate();
}

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound

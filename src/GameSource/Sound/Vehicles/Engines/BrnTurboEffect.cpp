#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameSource/Sound/Vehicles/Engines/BrnTurboEffect.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameSource/Sound/Module/LogicModule/BrnMessageData.h"
#include "GameSource/Sound/Vehicles/Engines/BrnEngineControl.h"
#include "GameSource/Sound/Vehicles/Engines/BrnHybridExhaustControl.h"

#include "rw/math/fpu/scalar_operation.h"  // rw::math::fpu::Min / Max / Clamp
#include <cstdio>
#include <cstring>
#include "GameShared/GameClasses/Development/CgsStrStream.h"

// ARTIST compiled this file through the GameSource unity file; its asserts carry this path.
#define BRN_TURBOEFFECT_CPP_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Sound/Vehicles/Engines/BrnTurboEffect.cpp"

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

namespace
{
const f32 KF_TURBO_SPOOL_RATE = 30000.0f;
const f32 KF_TURBO_PEAK_HOLD = 1.0f;
const f32 KF_TURBO_DELTA_THRESHOLD = 0.2f;
const f32 KF_TURBO_HEAVY_BLOWOFF = 32000.0f;
const f32 KF_TURBO_LIGHT_BLOWOFF = 10000.0f;
const f32 KF_Q15_MAX = 32767.0f;

// PROVISIONAL NAME: no DWARF/PS3/SDK name found (per-effect AEMS parameter names).
const u32 KAU_TURBO_PARAMETERS[9] =
{
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_spoolFrequency")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_pitch")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_azimuth")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_spoolVolume")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_startSpool")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_blowOffHeavyVolume")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_startBlowOffHeavy")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_blowOffLightVolume")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_startBlowOffLight")),
};
// PROVISIONAL NAME: no DWARF/PS3/SDK name found; the turbo voice's own send name (ARTIST 0x830060A4).
const u32 KU_TURBO_SEND = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
}

TurboEffect::TurboEffect()
    : BrnSound::Logic::BrnEffectObject()
    , mTurboVoice()
    , mTurboState(E_TURBO_NONE)
    , mfTurboVolume(0.0f)
    , mfTurboSpool(0.0f)
    , mfTurboSpoolScale(0.0f)
    , mu8TurboBlowoff(0)
    , mfTimeAtPeak(0.0f)
    , mpEngineControl(0)
    , mpHybridControl(0)
{
}

TurboEffect::~TurboEffect() {}

// ARTIST 0x82F2F648: {0x100B0, "TurboEffect", BrnEffectObject, CreateObject}, registered by
// its startup thunk through ClassTypeInfo<EffectObject>::AddToClassTypeInfoArray.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> TurboEffect::sTypeInfo(
    0x100B0, "TurboEffect", &BrnSound::Logic::BrnEffectObject::sTypeInfo, &TurboEffect::CreateObject);

CgsSound::Logic::EffectObject* TurboEffect::CreateObject(u32 luAllocator)
{
    if (luAllocator == 0)
        return new (sTypeInfo.typeName, false) TurboEffect();
    else
        return new (sTypeInfo.typeName, true) TurboEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* TurboEffect::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* TurboEffect::GetTypeInfo() const
{
    return &sTypeInfo;
}

const char* TurboEffect::GetTypeName() const {
    return sTypeInfo.typeName;
}

s32 TurboEffect::GetController(s32 aiSlot)
{
    // ARTIST 0x82685330: a switch over the controller slot (unsigned range test).
    switch (aiSlot)
    {
    case 0:
        return 4;
    case 1:
        return 5;
    default:
        return -1;
    }
}

void TurboEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    // ARTIST 0x82685370: no null test; a switch whose default streams the controller id.
    switch (apController->GetEffectID())
    {
    case 4:
        mpEngineControl = static_cast<EngineControl*>(apController);
        break;
    case 5:
        mpHybridControl = static_cast<HybridExhaustControl*>(apController);
        break;
    default:
        CGS_FAIL_STREAM_AT("Cound't attach controller " << apController->GetId(),
                             BRN_TURBOEFFECT_CPP_FILE, 176);
        break;
    }
}

void TurboEffect::SetupLoadData()
{
    CGS_ASSERT(mpHybridControl != 0, "mpHybridControl");
    if (!mpHybridControl)
        return;

    const char* lpcAsset = mpHybridControl->GetVehicleEngineAttributes().TurboAssetName();
    // ARTIST compares the attribute string with unk_820046A7, the shared empty
    // string.  An engine with no turbo therefore skips the effect; "NONE" is not
    // a sentinel in this path.
    if (!lpcAsset || *lpcAsset == '\0')
    {
        mbHasLoadedData = false;
        SetAttachState(CgsSound::Logic::EffectBase::E_ATTACH_STATE_PREPARING);
        return;
    }

    char lacBundle[64];
    CgsCore::SPrintf(lacBundle, sizeof(lacBundle), "sound\\aems\\%s.bundle", lpcAsset);
    mbHasLoadedData = true;
    static_cast<BrnSound::Logic::IResourceRequester*>(this)->LoadAsset(
        lacBundle, 0, BrnSound::Logic::ResourceRegistrar::E_DATA);
}

bool TurboEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;
    if (!mbHasLoadedData)
        return true;

    CGS_ASSERT(mpHybridControl != 0, "mpHybridControl");
    if (!mpHybridControl)
        return false;

    mu8TurboBlowoff = 0;
    mfTurboSpool = 0.0f;
    mfTurboSpoolScale = 0.0f;
    mfTimeAtPeak = 0.0f;
    mTurboState.Flush(E_TURBO_NONE);
    mfTurboVolume = 0.0f;

    const char* lpcAsset = mpHybridControl->GetVehicleEngineAttributes().TurboAssetName();
    char lacContent[64];
    CgsCore::SPrintf(lacContent, sizeof(lacContent), "%s.abi", lpcAsset);

    CgsSound::Logic::VoiceWrapper::CreateParams lParams;
    lParams.mpLogicModule = GetLogicModule();
    lParams.mFactoryName = static_cast<u32>(CgsSound::Playback::AemsFactorySkName().GetValue());
    lParams.mVoiceSpecName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_TurboClass"));
    lParams.mContentSpecName = static_cast<u32>(CgsSound::Playback::Name::MakeHash(lacContent));
    lParams.mSlotName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_Slot"));
    lParams.mSendName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));  // ARTIST hashes it here (0x826F2BB8)
    lParams.mSubMixVoiceID = 1;
    lParams.miSendIndex = 0;
    mTurboVoice.Create(lParams);
    mTurboVoice.Play(0);
    return true;
}

void TurboEffect::UpdateParams(f32 afTimeStep)
{
    if (!mbHasLoadedData)
        return;

    SetMixerInputValue(0, 0);
    SetMixerInputValue(1, 0);
    SetMixerInputValue(2, 0);
    CGS_ASSERT(mpHybridControl != 0, "mpHybridControl");
    if (!mpHybridControl)
        return;

    const f32 lfDeltaRpm = mpHybridControl->GetDeltaRPM();
    switch (mTurboState.GetCurrent())
    {
    case E_TURBO_NONE:
        mfTurboSpool = 0.0f;
        if (lfDeltaRpm >= KF_TURBO_DELTA_THRESHOLD)
            mTurboState.Update(E_TURBO_SPOOLING);
        break;

    case E_TURBO_SPOOLING:
        SetMixerInputValue(2, 0x7FFF);
        mfTurboSpool = rw::math::fpu::Min(KF_Q15_MAX,
            mfTurboSpool + lfDeltaRpm * KF_TURBO_SPOOL_RATE * afTimeStep);
        if (mfTurboSpool > 32766.0f)
        {
            mfTimeAtPeak += afTimeStep;
            if (mfTimeAtPeak > KF_TURBO_PEAK_HOLD)
                SetMixerInputValue(1, 0x7FFF);
        }
        else
        {
            mfTimeAtPeak = 0.0f;
        }
        if (lfDeltaRpm < KF_TURBO_DELTA_THRESHOLD)
            mTurboState.Update(E_TURBO_PLAY_BLOWOFF);
        break;

    case E_TURBO_PLAY_BLOWOFF:
        SetMixerInputValue(0, 0x7FFF);
        if (mfTurboSpool >= KF_TURBO_LIGHT_BLOWOFF)
            mu8TurboBlowoff = mfTurboSpool >= KF_TURBO_HEAVY_BLOWOFF ? 2 : 1;
        else
            mu8TurboBlowoff = 0;
        mTurboState.Update(E_TURBO_NONE);
        break;
    }

    mTurboVoice.Update();
}

void TurboEffect::ProcessUpdate()
{
    if (!mbHasLoadedData)
        return;

    mTurboVoice.SetParameter(3, mfTurboVolume * GetMixerOutputValue(0, Nicotine::DMixIO::DMX_VOL), &KAU_TURBO_PARAMETERS[3]);
    mTurboVoice.SetParameter(8, GetMixerOutputValue(3, Nicotine::DMixIO::DMX_VOL), &KAU_TURBO_PARAMETERS[8]);
    mTurboVoice.SetParameter(7, mfTurboVolume * GetMixerOutputValue(3, Nicotine::DMixIO::DMX_VOL), &KAU_TURBO_PARAMETERS[7]);
    mTurboVoice.SetParameter(5, mfTurboVolume * GetMixerOutputValue(3, Nicotine::DMixIO::DMX_VOL), &KAU_TURBO_PARAMETERS[5]);
    mTurboVoice.SetParameter(2, GetMixerOutputValue(1, Nicotine::DMixIO::DMX_AZIM), &KAU_TURBO_PARAMETERS[2]);
    mTurboVoice.SetParameter(1, GetMixerOutputValue(2, Nicotine::DMixIO::DMX_PITCH), &KAU_TURBO_PARAMETERS[1]);
    mTurboVoice.SetParameter(0, mfTurboSpool, &KAU_TURBO_PARAMETERS[0]);
    mTurboVoice.SetGain(0, 1.0f, &KU_TURBO_SEND);
    mTurboVoice.SetParameter(4, 1.0f, &KAU_TURBO_PARAMETERS[4]);

    if (mu8TurboBlowoff == 1)
    {
        mTurboVoice.SetParameter(8, 1.0f, &KAU_TURBO_PARAMETERS[8]);
        mu8TurboBlowoff = 0;
    }
    else if (mu8TurboBlowoff == 2)
    {
        mTurboVoice.SetParameter(6, 1.0f, &KAU_TURBO_PARAMETERS[6]);
        mu8TurboBlowoff = 0;
    }
    else
    {
        mTurboVoice.SetParameter(6, 0.0f, &KAU_TURBO_PARAMETERS[6]);
        mTurboVoice.SetParameter(8, 0.0f, &KAU_TURBO_PARAMETERS[8]);
    }
}

bool TurboEffect::Detach()
{
    if (!BrnSound::Logic::BrnEffectObject::Detach())
        return false;
    mTurboVoice.Release();
    return true;
}

void TurboEffect::Notify(const CgsSound::Io::MessageHeader* apMessage)
{
    // ARTIST reads the message id directly (no null test of the message).
    if (apMessage->GetEventId() == E_SOUNDMESSAGE_FX_VOLUMES)
    {
        const CgsSound::Io::Message<FxVolumes>* lpVolumes =
            static_cast<const CgsSound::Io::Message<FxVolumes>*>(apMessage);
        mfTurboVolume = lpVolumes->mData.mfTurboVolume;
    }
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

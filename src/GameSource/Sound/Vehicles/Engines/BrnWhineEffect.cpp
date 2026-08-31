#include "GameSource/Sound/Vehicles/Engines/BrnWhineEffect.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameSource/Sound/Module/LogicModule/BrnMessageData.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"

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
const u32 KU_AEMS_PARAMETERS[5] =
{
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_spoolFrequency")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_pitch")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_azimuth")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_spoolVolume")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_startSpool")),
};
const u32 KU_SEND01 = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
}

WhineEffect::WhineEffect()
    : BrnSound::Logic::BrnEffectObject()
    , mWhineVoice()
    , mpPhysicsControl(nullptr)
    , mfWhineVolume(0.0f)
{
}

WhineEffect::~WhineEffect() {}

const char* WhineEffect::GetTypeName() const { return "WhineEffect"; }

s32 WhineEffect::GetController(s32 aiSlot)
{
    return aiSlot == 0 ? 0 : -1;
}

void WhineEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    if (apController->GetEffectID() != 0)
    {
        CGS_ASSERT(false, "Cound't attach controller ");
        return;
    }
    mpPhysicsControl = static_cast<PhysicsControl*>(apController);
}

void WhineEffect::SetupLoadData()
{
    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    if (!mpPhysicsControl)
        return;

    const char* lpcAsset = mpPhysicsControl->GetVehicleEngineAttributes().WhineAssetName();
    // ARTIST compares with unk_820046A7, the shared empty string.  Cars without
    // a gear-whine asset leave this field empty and do not create an AEMS voice.
    if (!lpcAsset || *lpcAsset == '\0')
    {
        mbHasLoadedData = false;
        SetAttachState(CgsSound::Logic::EffectBase::E_ATTACH_STATE_PREPARING);
        return;
    }

    char lacBundle[64];
    std::snprintf(lacBundle, sizeof(lacBundle), "sound\\aems\\%s.bundle", lpcAsset);
    mbHasLoadedData = true;
    static_cast<BrnSound::Logic::IResourceRequester*>(this)->LoadAsset(
        lacBundle, nullptr, BrnSound::Logic::ResourceRegistrar::E_DATA);
}

bool WhineEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;
    if (!mbHasLoadedData)
        return true;

    CGS_ASSERT(mpPhysicsControl != nullptr, "mpPhysicsControl");
    if (!mpPhysicsControl)
        return false;

    mfWhineVolume = 0.0f;
    const char* lpcAsset = mpPhysicsControl->GetVehicleEngineAttributes().WhineAssetName();
    char lacContent[64];
    std::snprintf(lacContent, sizeof(lacContent), "%s.abi", lpcAsset);

    CgsSound::Logic::VoiceWrapper::CreateParams lParams;
    lParams.mpLogicModule = GetLogicModule();
    lParams.mFactoryName = static_cast<u32>(CgsSound::Playback::AemsFactorySkName().GetValue());
    lParams.mVoiceSpecName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_GearWhineClass"));
    lParams.mContentSpecName = static_cast<u32>(CgsSound::Playback::Name::MakeHash(lacContent));
    lParams.mSlotName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_Slot"));
    lParams.mSendName = KU_SEND01;
    lParams.mSubMixVoiceID = 1;
    lParams.miSendIndex = 0;
    mWhineVoice.Create(lParams);
    mWhineVoice.Play(0);
    return true;
}

void WhineEffect::UpdateParams(f32)
{
    if (mbHasLoadedData)
        mWhineVoice.Update();
}

void WhineEffect::ProcessUpdate()
{
    if (!mbHasLoadedData || !mpPhysicsControl)
        return;

    mWhineVoice.SetParameter(3, mfWhineVolume * GetMixerOutputValue(0, 0), &KU_AEMS_PARAMETERS[3]);
    mWhineVoice.SetParameter(2, GetMixerOutputValue(1, 3), &KU_AEMS_PARAMETERS[2]);
    mWhineVoice.SetParameter(1, GetMixerOutputValue(2, 1), &KU_AEMS_PARAMETERS[1]);
    mWhineVoice.SetGain(0, 1.0f, &KU_SEND01);
    mWhineVoice.SetParameter(4, 1.0f, &KU_AEMS_PARAMETERS[4]);

    const f32 lfWhineFrequency = std::max(
        mpPhysicsControl->GetPhysicsData().mSpeedMPH.GetCurrent() / 180.0f - 1.0f,
        0.0f) * 32767.0f;
    mWhineVoice.SetParameter(0, lfWhineFrequency, &KU_AEMS_PARAMETERS[0]);
}

bool WhineEffect::Detach()
{
    if (!BrnSound::Logic::BrnEffectObject::Detach())
        return false;
    mWhineVoice.Release();
    return true;
}

void WhineEffect::Notify(const CgsSound::Io::MessageHeader* apMessage)
{
    if (apMessage && apMessage->GetEventId() == E_SOUNDMESSAGE_FX_VOLUMES)
    {
        const CgsSound::Io::Message<FxVolumes>* lpVolumes =
            static_cast<const CgsSound::Io::Message<FxVolumes>*>(apMessage);
        mfWhineVolume = lpVolumes->mData.mfWhineVolume;
    }
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameSource/Sound/Vehicles/Engines/BrnWhineEffect.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameSource/Sound/Module/LogicModule/BrnMessageData.h"
#include "GameSource/Sound/Vehicles/Engines/BrnPhysicsControl.h"

#include "rw/math/fpu/scalar_operation.h"  // rw::math::fpu::Min / Max / Clamp
#include <cstdio>
#include <cstring>
#include "GameShared/GameClasses/Development/CgsStrStream.h"

// ARTIST compiled this file through the GameSource unity file; its asserts carry this path.
#define BRN_WHINEEFFECT_CPP_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Sound/Vehicles/Engines/BrnWhineEffect.cpp"

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

namespace
{
// PROVISIONAL NAME: no DWARF/PS3/SDK name found (per-effect AEMS parameter names).
const u32 KAU_WHINE_PARAMETERS[5] =
{
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_spoolFrequency")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_pitch")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_azimuth")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_spoolVolume")),
    static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_startSpool")),
};
// PROVISIONAL NAME: no DWARF/PS3/SDK name found; the whine voice's own send name (ARTIST 0x8300835C).
const u32 KU_WHINE_SEND = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
}

WhineEffect::WhineEffect()
    : BrnSound::Logic::BrnEffectObject()
    , mWhineVoice()
    , mpPhysicsControl(0)
    , mfWhineVolume(0.0f)
{
}

WhineEffect::~WhineEffect() {}

// ARTIST 0x82F2F658: {0x100C0, "WhineEffect", BrnEffectObject, CreateObject}, registered by
// its startup thunk through ClassTypeInfo<EffectObject>::AddToClassTypeInfoArray.
CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> WhineEffect::sTypeInfo(
    0x100C0, "WhineEffect", &BrnSound::Logic::BrnEffectObject::sTypeInfo, &WhineEffect::CreateObject);

CgsSound::Logic::EffectObject* WhineEffect::CreateObject(u32 luAllocator)
{
    if (luAllocator == 0)
        return new (sTypeInfo.typeName, false) WhineEffect();
    else
        return new (sTypeInfo.typeName, true) WhineEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* WhineEffect::GetStaticTypeInfo()
{
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* WhineEffect::GetTypeInfo() const
{
    return &sTypeInfo;
}

const char* WhineEffect::GetTypeName() const {
    return sTypeInfo.typeName;
}

s32 WhineEffect::GetController(s32 aiSlot)
{
    return aiSlot == 0 ? 0 : -1;
}

void WhineEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    // ARTIST 0x82685460: a switch whose default streams the controller id (cpp:161).
    switch (apController->GetEffectID())
    {
    case 0:
        mpPhysicsControl = static_cast<PhysicsControl*>(apController);
        break;
    default:
        CGS_FAIL_STREAM_AT("Cound't attach controller " << apController->GetId(),
                             BRN_WHINEEFFECT_CPP_FILE, 161);
        break;
    }
}

void WhineEffect::SetupLoadData()
{
    CGS_ASSERT(mpPhysicsControl != 0, "mpPhysicsControl");
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
    CgsCore::SPrintf(lacBundle, sizeof(lacBundle), "sound\\aems\\%s.bundle", lpcAsset);
    mbHasLoadedData = true;
    static_cast<BrnSound::Logic::IResourceRequester*>(this)->LoadAsset(
        lacBundle, 0, BrnSound::Logic::ResourceRegistrar::E_DATA);
}

bool WhineEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;
    if (!mbHasLoadedData)
        return true;

    CGS_ASSERT(mpPhysicsControl != 0, "mpPhysicsControl");
    if (!mpPhysicsControl)
        return false;

    mfWhineVolume = 0.0f;
    const char* lpcAsset = mpPhysicsControl->GetVehicleEngineAttributes().WhineAssetName();
    char lacContent[64];
    CgsCore::SPrintf(lacContent, sizeof(lacContent), "%s.abi", lpcAsset);

    CgsSound::Logic::VoiceWrapper::CreateParams lParams;
    lParams.mpLogicModule = GetLogicModule();
    lParams.mFactoryName = static_cast<u32>(CgsSound::Playback::AemsFactorySkName().GetValue());
    lParams.mVoiceSpecName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_GearWhineClass"));
    lParams.mContentSpecName = static_cast<u32>(CgsSound::Playback::Name::MakeHash(lacContent));
    lParams.mSlotName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("AEMS_Slot"));
    lParams.mSendName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));  // ARTIST hashes it here (0x826F2D2C)
    lParams.mSubMixVoiceID = 1;
    lParams.miSendIndex = 0;
    mWhineVoice.Create(lParams);
    mWhineVoice.Play(0);
    return true;
}

// 0x826E4B00 (PS3 0x8A2078): the voice runs only while the effect is enabled (lbz +0x2C,
// EffectBase::mbEnabled through the DWARF IsEnabled(), CgsEffectBase.h:571).
void WhineEffect::UpdateParams(f32)
{
    if (IsEnabled())
        mWhineVoice.Update();
}

void WhineEffect::ProcessUpdate()
{
    if (!mbHasLoadedData || !mpPhysicsControl)
        return;

    mWhineVoice.SetParameter(3, mfWhineVolume * GetMixerOutputValue(0, 0), &KAU_WHINE_PARAMETERS[3]);
    mWhineVoice.SetParameter(2, GetMixerOutputValue(1, 3), &KAU_WHINE_PARAMETERS[2]);
    mWhineVoice.SetParameter(1, GetMixerOutputValue(2, 1), &KAU_WHINE_PARAMETERS[1]);
    mWhineVoice.SetGain(0, 1.0f, &KU_WHINE_SEND);
    mWhineVoice.SetParameter(4, 1.0f, &KAU_WHINE_PARAMETERS[4]);

    const f32 lfWhineFrequency = rw::math::fpu::Max(
        mpPhysicsControl->GetPhysicsData().mSpeedMPH.GetCurrent() / 180.0f - 1.0f,
        0.0f) * 32767.0f;
    mWhineVoice.SetParameter(0, lfWhineFrequency, &KAU_WHINE_PARAMETERS[0]);
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
    // ARTIST reads the message id directly (no null test of the message).
    if (apMessage->GetEventId() == E_SOUNDMESSAGE_FX_VOLUMES)
    {
        const CgsSound::Io::Message<FxVolumes>* lpVolumes =
            static_cast<const CgsSound::Io::Message<FxVolumes>*>(apMessage);
        mfWhineVolume = lpVolumes->mData.mfWhineVolume;
    }
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound

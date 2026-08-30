#include "GameSource/Sound/World/BrnEmitterEffect.h"

#include "GameSource/Sound/Module/LogicModule/BrnEmitter3dControl.h"
#include "GameSource/Sound/Module/LogicModule/BrnEmitterState.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "SharedClasses/Sound/World/BrnStaticSoundMap.h"
#include "GameSource/AttribSys/Generated/classes/worldemitter.h"
#include "GameSource/AttribSys/Generated/classes/worldemitterlist.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <algorithm>
#include <cmath>

namespace BrnSound
{
namespace Logic
{
namespace World
{

EmitterEffect::EmitterEffect()
    : BrnEffectObject()
    , mVoice()
    , mPos()
    , mp3dControl(0)
    , mi16PitchOutput(0)
{
}

EmitterEffect::~EmitterEffect()
{
}

const char* EmitterEffect::GetTypeName() const
{
    return "EmitterEffect";
}

CgsSound::Logic::EffectObject* EmitterEffect::CreateObject(u32)
{
    return new EmitterEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>*
EmitterEffect::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>
        sTypeInfo(0x70000, "EmitterEffect",
                  CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
                  &EmitterEffect::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>*
EmitterEffect::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const
    gpEmitterEffectReg =
        CgsSound::Logic::EffectObject::AddToClassTypeInfoArray(
            EmitterEffect::GetStaticTypeInfo());

s32 EmitterEffect::GetController(s32 aiIndex)
{
    return aiIndex == 0 ? 0 : -1;
}

void EmitterEffect::AttachController(CgsSound::Logic::EffectBase* apController)
{
    CGS_ASSERT(apController != 0, "lpController");
    if (!apController)
        return;
    CGS_ASSERT((apController->GetId() & 0x7F0) == 0, "Unexpected control.");
    if ((apController->GetId() & 0x7F0) == 0)
        mp3dControl = static_cast<Emitter3dControl*>(apController);
}

const BrnSound::World::StaticSoundEntity& EmitterEffect::GetSoundEntity() const
{
    const EmitterState* lpState = static_cast<const EmitterState*>(mpState);
    CGS_ASSERT(lpState != 0, "lpState");
    CGS_ASSERT(lpState && lpState->IsAttached(), "IsAttached()");
    return lpState->GetSoundEntity();
}

bool EmitterEffect::Attach()
{
    CgsSound::Logic::EffectBase::Attach();

    const BrnSound::World::StaticSoundEntity& lrEntity = GetSoundEntity();
    mPos = lrEntity.GetPos();
    CGS_ASSERT(mp3dControl != 0, "mp3dControl");
    if (mp3dControl)
        mp3dControl->AttachEmitterPosition(&mPos);

    BrnSound::Module::SoundLogicModule* lpLogicModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    CGS_ASSERT(lpLogicModule != 0, "lpLogicModule");
    if (!lpLogicModule)
        return true;

    Attrib::Gen::worldemitterlist lWorldEmitters(
        lpLogicModule->GetGlobalData().WorldEmitterList());
    const u32 luEmitter = lrEntity.GetType();
    CGS_ASSERT(luEmitter < lWorldEmitters.Num_mWorldEmitters(),
               "luEmitter < static_cast< uint32_t >( lWorldEmitters.mNumWorldEmitters() )");

    mi16PitchOutput = 1;
    if (luEmitter < lWorldEmitters.Num_mWorldEmitters())
    {
        Attrib::Gen::worldemitter lEmitter;
        lEmitter.ChangeWithDefault(lWorldEmitters.mWorldEmitters(luEmitter));
        CGS_ASSERT(!lEmitter.IsStream(),
                   "EmitterEffect : Emitter streams not yet supported.");
        if (!lEmitter.IsStream() && lEmitter.EmitterName())
        {
            if (lEmitter.AffectedByDoppler())
                mi16PitchOutput = 2;

            CgsSound::Logic::VoiceWrapper::CreateParams lParams;
            lParams.mpLogicModule = lpLogicModule;
            lParams.mFactoryName = static_cast<u32>(
                CgsSound::Playback::GenericRwacFactorySkName().GetValue());
            lParams.mVoiceSpecName = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("PositionalVoiceSpec"));
            lParams.mContentSpecName = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash(lEmitter.EmitterName()));
            lParams.mSlotName = static_cast<u32>(
                CgsSound::Playback::PlayerVoice::SK_PLAYER_SLOT_NAME.GetValue());
            lParams.mSendName = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("Send01"));
            lParams.mSubMixVoiceID = 1;
            lParams.mReverbSendName = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("ReverbSend"));
            lParams.mReverbSubMixVoiceID = 2;
            lParams.miSendIndex = 0;
            mVoice.Create(lParams);
            mVoice.Play(0);

            const u32 luRadius = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("SimplePanningRadius"));
            const u32 luCentre = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("SimplePanningCentreLevel"));
            const u32 luMain = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("SimplePanningMainLevel"));
            const u32 luLfe = static_cast<u32>(
                CgsSound::Playback::Name::MakeHash("SimplePanningLfeLevel"));
            mVoice.SetParameter(2, 0.85f, &luRadius);
            mVoice.SetParameter(3, 1.0f, &luCentre);
            mVoice.SetParameter(4, 1.0f, &luMain);
            mVoice.SetParameter(5, 0.0f, &luLfe);
        }
    }
    return true;
}

void EmitterEffect::ProcessUpdate()
{
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    CGS_ASSERT(lpModule != 0, "lpModule");
    if (!lpModule)
        return;

    const BrnSound::World::StaticSoundEntity& lrEntity = GetSoundEntity();
    const rw::math::vpu::Vector3 lListener =
        lpModule->GetFrameInformation().mPlayerTransform.Pos();
    const f32 lfDistance = rw::math::vpu::Length(lListener - lrEntity.GetPos());
    const f32 lfRadius = static_cast<f32>(lrEntity.GetRadius());
    const f32 lfDistanceGain = lfRadius > 0.0f
        ? 1.0f - std::max(0.0f, std::min(1.0f, lfDistance / lfRadius))
        : 0.0f;

    const f32 lfVolume = GetRWACMixerOutputValue(0, 0);
    const f32 lfReverb = GetRWACMixerOutputValue(4, 0);
    const f32 lfPitch = GetRWACMixerOutputValue(mi16PitchOutput, 1);
    const f32 lfAzimuth = GetRWACMixerOutputValue(3, 3);

    const u32 luAzimuth = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("SimplePanningAzimuth"));
    const u32 luPitch = static_cast<u32>(CgsSound::Playback::Name::MakeHash(
        "~GenericRwacPlayerVoice::SK_PLAYER_PARAMETER_PITCH~"));
    const u32 luSend = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Send01"));
    const u32 luReverb = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("ReverbSend"));
    mVoice.SetParameter(1, lfAzimuth, &luAzimuth);
    mVoice.SetParameter(0, lfPitch, &luPitch);
    mVoice.SetGain(0, lfDistanceGain * lfVolume, &luSend);
    mVoice.SetGain(1, lfReverb, &luReverb);
    mVoice.Update();
}

bool EmitterEffect::Detach()
{
    mVoice.Release();
    if (mp3dControl)
        mp3dControl->AttachEmitterPosition(0);
    return BrnEffectObject::Detach();
}

} // namespace World
} // namespace Logic
} // namespace BrnSound

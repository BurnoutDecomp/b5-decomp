#include "GameSource/Sound/Global/BrnSpeechEffect.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h"
#include "GameSource/AttribSys/Generated/classes/streammappings.h"
#include "GameSource/AttribSys/Generated/classes/languagestreamconfiguration.h"
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"

namespace BrnSound
{
namespace Logic
{

SpeechEffect::SpeechEffect()
    : BrnEffectObject(), Streaming::IStreamUser(), mpStreamingManager(0),
      mCreateParams(), mePlayState(E_STOPPED), mbSpeechStillPlaying(false),
      mbFirstTimeTipPlaying(false), muQueuedContentSpec(0),
      mbQueuedSpeechIsFirstTimeTip(false) {}

SpeechEffect::~SpeechEffect() {}

CgsSound::Logic::EffectObject* SpeechEffect::CreateObject(u32)
{
    return new SpeechEffect();
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* SpeechEffect::GetStaticTypeInfo()
{
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject> sTypeInfo(
        0x50, "SpeechEffect", CgsSound::Logic::EffectObject::GetStaticTypeInfo(),
        &SpeechEffect::CreateObject);
    return &sTypeInfo;
}

CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* SpeechEffect::GetTypeInfo() const
{
    return GetStaticTypeInfo();
}

const char* SpeechEffect::GetTypeName() const { return "SpeechEffect"; }

static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* const gpSpeechEffectReg =
    CgsSound::Logic::EffectObject::AddToClassTypeInfoArray(SpeechEffect::GetStaticTypeInfo());

bool SpeechEffect::Attach()
{
    if (!CgsSound::Logic::EffectBase::Attach())
        return false;
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    mpStreamingManager = static_cast<Streaming::StreamingStateManager*>(
        lpModule->GetEnvironment().GetStateManager(6));
    CGS_ASSERT(mpStreamingManager != 0, "lpStreamingStateManager");

    mCreateParams.Clear();
    mCreateParams.mpLogicModule = lpModule;
    mCreateParams.mFactoryName = static_cast<u32>(
        CgsSound::Playback::GenericRwacFactorySkName().GetValue());
    mCreateParams.mVoiceSpecName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("MusicVoiceSpec"));
    mCreateParams.mSlotName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Player"));
    mCreateParams.mSendName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Send01"));
    mCreateParams.mSubMixVoiceID = 1;
    mCreateParams.miSendIndex = 0;
    mePlayState = E_STOPPED;
    mbSpeechStillPlaying = false;
    muQueuedContentSpec = 0;
    return mpStreamingManager != 0;
}

bool SpeechEffect::Detach()
{
    if (mpStreamingManager && mePlayState == E_PLAYING)
        mpStreamingManager->PostStreamRequest(Streaming::StreamStopRequest(this, 0.25f));
    return BrnEffectObject::Detach();
}

const CgsSound::Logic::VoiceWrapper::CreateParams& SpeechEffect::GetCreateParams() const
{
    return mCreateParams;
}

void SpeechEffect::UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                                     f32 afGain, f32)
{
    const u32 luPauseControl = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("PauseControl"));
    arVoice.SetParameter(0, 0.0f, &luPauseControl);
    const u32 luSend = mCreateParams.mSendName;
    arVoice.SetGain(static_cast<u32>(mCreateParams.miSendIndex), afGain, &luSend);
    mbSpeechStillPlaying = true;
    mePlayState = E_PLAYING;
}

void SpeechEffect::StreamStopped()
{
    mbSpeechStillPlaying = false;
}

void SpeechEffect::PlayStream(u32 auContentSpec, bool abFirstTimeTip)
{
    if (!auContentSpec || !mpStreamingManager)
        return;
    if (mePlayState == E_PLAYING)
    {
        muQueuedContentSpec = auContentSpec;
        mbQueuedSpeechIsFirstTimeTip = abFirstTimeTip;
        return;
    }
    mCreateParams.mContentSpecName = auContentSpec;
    mbFirstTimeTipPlaying = abFirstTimeTip;
    mbSpeechStillPlaying = true;
    mePlayState = E_PLAYING;
    mpStreamingManager->PostStreamRequest(Streaming::StreamRequest(this, 6, 0.1f));
}

u32 SpeechEffect::GetSpeechMapping(u32 auMappingName) const
{
    const BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<const BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    Attrib::Gen::streammappings lMappings(lpModule->GetGlobalData().StreamMappings());
    Attrib::RefSpec lConfiguration;
    if (!lMappings.Find(auMappingName, lConfiguration))
        return 0;
    Attrib::Gen::languagestreamconfiguration lLanguage(lConfiguration);
    return lLanguage.ContentSpec(0);
}

bool SpeechEffect::PlaySpeechMapping(u32 auMappingName, bool abFirstTimeTip)
{
    const u32 luContentSpec = GetSpeechMapping(auMappingName);
    if (!luContentSpec)
        return false;
    PlayStream(luContentSpec, abFirstTimeTip);
    return true;
}

void SpeechEffect::PostSpeechFinished()
{
    BrnSound::Module::SoundLogicModule* lpModule =
        static_cast<BrnSound::Module::SoundLogicModule*>(mpLogicModule);
    u8 luPayload = 0;
    CgsModule::VariableEventQueue<256, 16>* lpGuiQueue =
        reinterpret_cast<CgsModule::VariableEventQueue<256, 16>*>(
            lpModule->GetPreUpdateOutput().maGuiOutEventQueueStorage);
    lpGuiQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&luPayload), 467, 1);
}

void SpeechEffect::UpdateParams(f32)
{
    if (mePlayState != E_PLAYING)
        return;
    if (!mbSpeechStillPlaying)
    {
        mePlayState = E_STOPPED;
        PostSpeechFinished();
        if (muQueuedContentSpec)
        {
            const u32 luQueued = muQueuedContentSpec;
            const bool lbFirstTime = mbQueuedSpeechIsFirstTimeTip;
            muQueuedContentSpec = 0;
            PlayStream(luQueued, lbFirstTime);
        }
    }
    mbSpeechStillPlaying = false;
}

void SpeechEffect::Notify(const CgsSound::Io::MessageHeader* apMessage)
{
    if (!apMessage || apMessage->GetEventId() != 36)
        return;
    const CgsSound::Io::Message<CgsSound::Playback::Name>* lpMessage =
        static_cast<const CgsSound::Io::Message<CgsSound::Playback::Name>*>(apMessage);
    PlaySpeechMapping(static_cast<u32>(lpMessage->mData.GetValue()), false);
}

const char* SpeechEffect::CompassDirectionToString(int aiDirection)
{
    static const char* const kapDirections[] = { "n", "nw", "w", "sw", "s", "se", "e", "ne" };
    if (aiDirection >= 0 && aiDirection < 8)
        return kapDirections[aiDirection];
    CGS_ASSERT(false, "Unknown compass direction");
    return "";
}

const char* SpeechEffect::GameModeToString(int, int aiMode)
{
    switch (aiMode)
    {
    case 0: return "r";
    case 3: return "rr";
    case 5: return "pc";
    case 7: return "sr";
    case 8: return "mm";
    default: return "";
    }
}

} // namespace Logic
} // namespace BrnSound

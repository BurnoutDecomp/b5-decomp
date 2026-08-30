#include "GameSource/Sound/Global/BrnMusicEffect.h"
#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cmath>

namespace BrnSound
{
namespace Logic
{

MusicStream::MusicStream()
    : mCreateParams(), mpStreamingManager(0), meState(E_STOPPED), mfFadeTime(0.0f),
      mfFadeDuration(0.25f), mfVolume(1.0f), mfHighPassFrequency(0.0f),
      mfLowPassFrequency(96000.0f), muPriority(6), muQueuedContentSpec(0),
      mbInternalPause(false), mbStreamPaused(false), mbSongQueued(false) {}

void MusicStream::Prepare(Module::SoundLogicModule* apModule,
                          Streaming::StreamingStateManager* apStreamingManager,
                          const char* apVoiceSpec)
{
    CGS_ASSERT(apModule != 0, "lpLogicModule");
    CGS_ASSERT(apStreamingManager != 0, "lpStreamingStateManager");
    mCreateParams.Clear();
    mCreateParams.mpLogicModule = apModule;
    mCreateParams.mFactoryName = static_cast<u32>(
        CgsSound::Playback::GenericRwacFactorySkName().GetValue());
    mCreateParams.mVoiceSpecName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash(apVoiceSpec));
    mCreateParams.mSlotName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("~PlayerVoice::SK_PLAYER_SLOT_NAME~"));
    mCreateParams.mSendName = static_cast<u32>(
        CgsSound::Playback::Name::MakeHash("Send01"));
    mCreateParams.mSubMixVoiceID = 1;
    mCreateParams.miSendIndex = 0;
    mpStreamingManager = apStreamingManager;
    meState = E_STOPPED;
    mfHighPassFrequency = 0.0f;
    mfLowPassFrequency = 96000.0f;
    mbInternalPause = false;
    mbStreamPaused = false;
    mbSongQueued = false;
}

void MusicStream::Queue(u32 auContentSpec, u32 auPriority)
{
    if (meState != E_STOPPED)
        Stop();
    mCreateParams.mContentSpecName = auContentSpec;
    muQueuedContentSpec = auContentSpec;
    muPriority = auPriority;
    if (!mbSongQueued)
        SetSongQueued(true);
}

void MusicStream::Stop(f32 afFadeOut)
{
    if (meState == E_STOPPED)
    {
        if (mbSongQueued)
            SetSongQueued(false);
        return;
    }
    mfFadeDuration = afFadeOut;
    mfFadeTime = 0.0f;
    meState = E_STOP_REQUESTED;
}

void MusicStream::Update(f32 afDeltaTime)
{
    if (meState == E_STOPPED && mbSongQueued)
    {
        mCreateParams.mContentSpecName = muQueuedContentSpec;
        mpStreamingManager->PostStreamRequest(
            Streaming::StreamRequest(this, muPriority, 0.1f));
        SetSongQueued(false);
        mbInternalPause = false;
        meState = E_PLAYING;
    }
    else if (meState == E_STOP_REQUESTED)
    {
        mfFadeTime = 0.0f;
        meState = E_FADING_OUT;
    }
    else if (meState == E_FADING_OUT)
    {
        mfFadeTime += afDeltaTime;
        if (mfFadeTime >= mfFadeDuration)
        {
            mpStreamingManager->PostStreamRequest(
                Streaming::StreamStopRequest(this, mfFadeDuration));
            meState = E_STOPPED;
        }
    }
}

void MusicStream::SetSongQueued(bool abSongQueued)
{
    CGS_ASSERT(abSongQueued != mbSongQueued, "lbSongQueued != mbSongQueued");
    mbSongQueued = abSongQueued;
}

const CgsSound::Logic::VoiceWrapper::CreateParams& MusicStream::GetCreateParams() const
{
    return mCreateParams;
}

void MusicStream::UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                                    f32 afGain, f32)
{
    const u32 luPauseControl = static_cast<u32>(CgsSound::Playback::Name::MakeHash("PauseControl"));
    const u32 luHighPassFrequency = static_cast<u32>(CgsSound::Playback::Name::MakeHash("HighPassFrequency"));
    const u32 luHighPassOrder = static_cast<u32>(CgsSound::Playback::Name::MakeHash("HighPassOrder"));
    const u32 luHighPassQ = static_cast<u32>(CgsSound::Playback::Name::MakeHash("HighPassQ"));
    const u32 luLowPassFrequency = static_cast<u32>(CgsSound::Playback::Name::MakeHash("LowPassFrequency"));
    const u32 luLowPassOrder = static_cast<u32>(CgsSound::Playback::Name::MakeHash("LowPassOrder"));
    const u32 luLowPassQ = static_cast<u32>(CgsSound::Playback::Name::MakeHash("LowPassQ"));

    arVoice.SetParameter(1, (mbInternalPause || mbStreamPaused) ? 1.0f : 0.0f,
                         &luPauseControl);
    arVoice.SetParameter(2, mfHighPassFrequency, &luHighPassFrequency);
    arVoice.SetParameter(3, 4.0f, &luHighPassOrder);
    arVoice.SetParameter(4, 0.95f, &luHighPassQ);

    if (mfLowPassFrequency < 300.0f)
        mfLowPassFrequency = 300.0f;
    arVoice.SetParameter(5, mfLowPassFrequency, &luLowPassFrequency);
    arVoice.SetParameter(6, 4.0f, &luLowPassOrder);
    arVoice.SetParameter(7, 0.99f, &luLowPassQ);

    f32 lfVolume = mfVolume;
    if ((meState == E_FADING_OUT || meState == E_PAUSING) &&
        std::fabs(mfFadeDuration) > 0.00000011920929f)
    {
        f32 lfFadeFraction = mfFadeTime / mfFadeDuration;
        if (lfFadeFraction < 0.0f)
            lfFadeFraction = 0.0f;
        else if (lfFadeFraction > 1.0f)
            lfFadeFraction = 1.0f;
        lfVolume *= CgsSound::Utils::Curve::GetOutput(
            1.0f - lfFadeFraction,
            CgsSound::Utils::Curve::E_ONE_MINUS_EQPWR);
    }

    const u32 luSend = mCreateParams.mSendName;
    arVoice.SetGain(static_cast<u32>(mCreateParams.miSendIndex),
                    afGain * lfVolume, &luSend);
}

void MusicStream::StreamStopped()
{
    meState = E_STOPPED;
}

} // namespace Logic
} // namespace BrnSound

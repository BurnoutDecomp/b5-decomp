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
    : mu32VoiceUpdateStage(0), mu32PrevVoiceUpdateStage(0), mCreateParams(),
      meState(E_STOPPED), mfFadeTime(0.0f), mfFadeDuration(0.0f),
      mfVolume(0.0f), mfHighPassFrequency(0.0f), mfLowPassFrequency(96000.0f),
      muPriority(0), mbInternalPause(false), mbStreamPaused(false),
      mbSongQueued(false), mu8QueuedOutputSlot(0), mu8OutputSlot(0) {}

void MusicStream::Construct()
{
    ResetVoiceStages();
}

bool MusicStream::Prepare(u32 auPriority)
{
    meState = E_STOPPED;
    mfFadeTime = 0.0f;
    mfFadeDuration = 0.0f;
    mfVolume = 0.0f;
    mfHighPassFrequency = 0.0f;
    mfLowPassFrequency = 96000.0f;
    muPriority = auPriority;
    mbStreamPaused = false;
    mbSongQueued = false;
    mu8QueuedOutputSlot = 0;
    mu8OutputSlot = 0;
    return true;
}

// ARTIST 0x8269CB88: create parameters are populated when a song is queued.
void MusicStream::Queue(CgsSound::Playback::Name aContentSpec,
                        CgsSound::Logic::Module* apModule, u8 auOutputSlot)
{
    CGS_ASSERT(apModule != 0, "lpLogicModule");
    mCreateParams.mpLogicModule = apModule;
    mCreateParams.mFactoryName = static_cast<u32>(CgsSound::Playback::GenericRwacFactorySkName().GetValue());
    mCreateParams.mVoiceSpecName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("MusicFiltVoiceSpec"));
    mCreateParams.mSlotName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("~PlayerVoice::SK_PLAYER_SLOT_NAME~"));
    mCreateParams.mContentSpecName = static_cast<u32>(aContentSpec.GetValue());
    mCreateParams.mSendName = static_cast<u32>(CgsSound::Playback::Name::MakeHash("Send01"));
    mCreateParams.miSendIndex = 0;
    mCreateParams.mSubMixVoiceID = 1;
    SetSongQueued(true);
    mu8QueuedOutputSlot = auOutputSlot;
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

void MusicStream::Update(Streaming::StreamingStateManager* apStreamingManager, f32 afDeltaTime)
{
    if (meState == E_STOPPED && mbSongQueued)
    {
        // ARTIST @ 0x826871F8 cancels this user's older requests before replacement.
        apStreamingManager->PostStreamRequest(
            Streaming::StreamStopRequest(this, 0.0f));
        apStreamingManager->PostStreamRequest(
            Streaming::StreamRequest(this, muPriority, 0.1f));
        mbInternalPause = false;
        SetSongQueued(false);
        mu8OutputSlot = mu8QueuedOutputSlot;
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
            apStreamingManager->PostStreamRequest(
                Streaming::StreamStopRequest(this, 0.0f));
            meState = E_STOPPED;
        }
    }
    // ⭐ MISSING ARM, RESTORED 2026-09-15. X360 MusicStream::Update @0x826871F8 case 4:
    //     v8 = mfFadeTime + dt; v9 = mfFadeDuration; mfFadeTime = v8;
    //     if (v8 >= v9) { *(this + 88) = 1;   // mbInternalPause
    //                     *(this + 60) = 1; } // meState = E_PLAYING
    // E_PAUSING is how the effect DUCKS a stream it wants to keep alive (every
    // MusicEffect::UpdateParams arm that starts a sting first pauses mEATraxStream with
    // a 0.1 s fade). Without this arm a paused stream sat in E_PAUSING forever: never
    // internally paused, never resumed, and -- because IsPlayingOrQueued() counts
    // E_PAUSING -- never re-queued either. The stream is left in E_PLAYING with
    // mbInternalPause set, which UpdateVoiceParams turns into the voice's PauseControl.
    else if (meState == E_PAUSING)
    {
        mfFadeTime += afDeltaTime;
        if (mfFadeTime >= mfFadeDuration)
        {
            mbInternalPause = true;
            meState = E_PLAYING;
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
    // X360 @0x826BB6F0 first two stores: `*(this+8) = *(this+4); *(this+4) = *(voice+0x48)`
    // -- the voice's update stage, latched for UpdateParams' 504 "audio ready" gate.
    mu32PrevVoiceUpdateStage = mu32VoiceUpdateStage;
    mu32VoiceUpdateStage = static_cast<u32>(arVoice.GetUpdateStage());

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
    arVoice.SetParameter(3, KF_MUSIC_HIGH_PASS_ORDER, &luHighPassOrder);
    arVoice.SetParameter(4, KF_MUSIC_HIGH_PASS_Q, &luHighPassQ);

    if (mfLowPassFrequency < 300.0f)
        mfLowPassFrequency = 300.0f;
    arVoice.SetParameter(5, mfLowPassFrequency, &luLowPassFrequency);
    arVoice.SetParameter(6, KF_MUSIC_LOW_PASS_ORDER, &luLowPassOrder);
    arVoice.SetParameter(7, KF_MUSIC_LOW_PASS_Q, &luLowPassQ);

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

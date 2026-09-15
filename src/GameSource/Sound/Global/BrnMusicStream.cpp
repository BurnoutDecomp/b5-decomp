#include "GameSource/Sound/Global/BrnMusicEffect.h"
#include "GameSource/Sound/Streaming/BrnStreamingStateManager.h"
#include "GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.h"
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"
#include "GameShared/GameClasses/Sound/CgsSoundUtils.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/CgsStreamDiag.h"  // [DIAG] NOT IN THE X360 BINARY
#include <cmath>

namespace BrnSound
{
namespace Logic
{

MusicStream::MusicStream()
    : mu32VoiceUpdateStage(0), mu32PrevVoiceUpdateStage(0),
      mCreateParams(), mpStreamingManager(0), meState(E_STOPPED), mfFadeTime(0.0f),
      mfFadeDuration(0.25f), mfVolume(1.0f), mfHighPassFrequency(0.0f),
      mfLowPassFrequency(96000.0f), muPriority(0), muQueuedContentSpec(0),
      mbInternalPause(false), mbStreamPaused(false), mbSongQueued(false),
      mu8QueuedOutputSlot(0), mu8OutputSlot(0),
      miDiagLastGainQ(-1) {}   // [DIAG] NOT IN THE X360 BINARY

void MusicStream::Prepare(Module::SoundLogicModule* apModule,
                          Streaming::StreamingStateManager* apStreamingManager,
                          const char* apVoiceSpec, u32 auPriority)
{
    muPriority = auPriority;   // X360 Attach @0x8269CC60: the per-stream request priority
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
    mu8QueuedOutputSlot = 0;
    mu8OutputSlot = 0;
}

void MusicStream::Queue(u32 auContentSpec, u8 auOutputSlot)
{
    if (meState != E_STOPPED)
        Stop();
    mCreateParams.mContentSpecName = auContentSpec;
    muQueuedContentSpec = auContentSpec;
    // (the request priority is NOT an argument here -- X360 Queue @0x8269CB88 leaves +0x54 alone)
    mu8QueuedOutputSlot = auOutputSlot;
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
            mpStreamingManager->PostStreamRequest(
                Streaming::StreamStopRequest(this, mfFadeDuration));
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

    // [DIAG] NOT IN THE X360 BINARY (BRN_STREAM_DIAG=1). THE decisive number for a
    // "queues but is inaudible" stream: the gain this stream actually writes to its
    // send, factored into the effect's streamsettings gain and mfVolume -- and
    // mfVolume is whatever MusicEffect::ProcessUpdate read out of the DYNAMIC MIXER
    // at this stream's own output slot.
    {
        // Edge key: the output slot, the ContentSpec, and -- the whole point --
        // whether the send gain is AUDIBLE or a hard zero. A stream whose mixer slot
        // reads 0 logs exactly one line and stops; a normally-jittering gain does not
        // storm the log.
        const f32 lfSend = afGain * lfVolume;
        const int liKey = static_cast<int>(mu8OutputSlot) * 4
                        + (lfSend > 0.0009765625f ? 1 : 0) * 2
                        + (meState == E_PLAYING ? 1 : 0);
        if (liKey != miDiagLastGainQ)
        {
            miDiagLastGainQ = liKey;
            CgsSound::Diag::StreamDiagPrintf(
                "[sndstream] gain slot=%u spec=0x%08X effectGain=%.4f vol=%.4f "
                "-> send=%.4f state=%d pause=%d\n",
                static_cast<u32>(mu8OutputSlot),
                static_cast<u32>(mCreateParams.mContentSpecName),
                afGain, lfVolume, afGain * lfVolume, static_cast<int>(meState),
                (mbInternalPause || mbStreamPaused) ? 1 : 0);
        }
    }
}

void MusicStream::StreamStopped()
{
    meState = E_STOPPED;
}

} // namespace Logic
} // namespace BrnSound

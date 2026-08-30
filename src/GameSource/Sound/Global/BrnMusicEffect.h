#ifndef BRN_SOUND_LOGIC_BRN_MUSIC_EFFECT_H
#define BRN_SOUND_LOGIC_BRN_MUSIC_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"
#include "GameSource/Sound/Streaming/BrnIStreamUser.h"

namespace BrnSound
{
namespace Module { struct SoundLogicModule; }
namespace Logic
{
namespace Streaming { class StreamingStateManager; }

class MusicStream : public Streaming::IStreamUser
{
public:
    enum EState
    {
        E_STOPPED = 0,
        E_PLAYING = 1,
        E_STOP_REQUESTED = 2,
        E_FADING_OUT = 3,
        E_PAUSING = 4
    };

    MusicStream();
    void Prepare(Module::SoundLogicModule* apModule,
                 Streaming::StreamingStateManager* apStreamingManager,
                 const char* apVoiceSpec);
    void Queue(u32 auContentSpec, u32 auPriority = 6);
    void Stop(f32 afFadeOut = 0.25f);
    void Update(f32 afDeltaTime);
    void SetSongQueued(bool abSongQueued);

    const CgsSound::Logic::VoiceWrapper::CreateParams& GetCreateParams() const override;
    void UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                           f32 afGain, f32 afElapsedTime) override;
    void StreamStopped() override;

private:
    CgsSound::Logic::VoiceWrapper::CreateParams mCreateParams;
    Streaming::StreamingStateManager* mpStreamingManager;
    EState meState;
    f32 mfFadeTime;
    f32 mfFadeDuration;
    f32 mfVolume;
    f32 mfHighPassFrequency;
    f32 mfLowPassFrequency;
    u32 muPriority;
    u32 muQueuedContentSpec;
    bool mbInternalPause;
    bool mbStreamPaused;
    bool mbSongQueued;
};

class MusicEffect : public BrnEffectObject
{
public:
    enum EJunkyardAmbience
    {
        E_JUNKYARD_AMBIENCE_NONE = 0,
        E_JUNKYARD_AMBIENCE_NEW_PROFILE = 1
    };

    struct EaTraxData
    {
        EaTraxData();
        bool Prepare(Module::SoundLogicModule* apLogicModule);
        u64 maEaTraxHandles[4];
        u8 maReserved0x20[16];
        s32 miTrackIndex0;
        s32 miTrackIndex1;
        s32 miTrackIndex2;
        Module::SoundLogicModule* mpLogicModule;
        s32 miState;
        u8 mbReserved0x44;
    };

    MusicEffect();
    virtual ~MusicEffect();
    CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const override;
    const char* GetTypeName() const override;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectObject* CreateObject(u32 auType);
    bool Attach() override;
    void UpdateParams(f32 afDeltaTime) override;
    void ProcessUpdate() override;
    void Notify(const CgsSound::Io::MessageHeader* apMessage) override;

private:
    MusicStream mEaTraxStream;
    MusicStream mEventStream;
    MusicStream mSpecialStream;
    MusicStream mMenuStream;
    EaTraxData mEaTraxData;
};

} // namespace Logic
} // namespace BrnSound

#endif

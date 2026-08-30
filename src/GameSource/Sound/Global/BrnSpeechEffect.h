#ifndef BRN_SOUND_LOGIC_BRN_SPEECH_EFFECT_H
#define BRN_SOUND_LOGIC_BRN_SPEECH_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"
#include "GameSource/Sound/Streaming/BrnIStreamUser.h"

namespace BrnSound
{
namespace Logic
{
namespace Streaming { class StreamingStateManager; }

class SpeechEffect : public BrnEffectObject,
                     public Streaming::IStreamUser
{
public:
    enum EPlayState
    {
        E_STOPPED = 0,
        E_PLAY_REQUESTED = 1,
        E_PLAYING = 2
    };

    SpeechEffect();
    virtual ~SpeechEffect();

    CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetTypeInfo() const override;
    const char* GetTypeName() const override;
    static CgsSound::Logic::ClassTypeInfo<CgsSound::Logic::EffectObject>* GetStaticTypeInfo();
    static CgsSound::Logic::EffectObject* CreateObject(u32 auType);

    bool Attach() override;
    bool Detach() override;
    void UpdateParams(f32 afDeltaTime) override;
    void Notify(const CgsSound::Io::MessageHeader* apMessage) override;

    const CgsSound::Logic::VoiceWrapper::CreateParams& GetCreateParams() const override;
    void UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                           f32 afGain, f32 afElapsedTime) override;
    void StreamStopped() override;

    bool PlaySpeechMapping(u32 auMappingName, bool abFirstTimeTip = false);
    bool PlayFirstTimeTip(s32 aiTrainingType);
    void PlayStream(u32 auContentSpec, bool abFirstTimeTip = false);
    const char* CompassDirectionToString(int aiDirection);
    const char* GameModeToString(int aiUnused, int aiMode);

private:
    u32 GetSpeechMapping(u32 auMappingName) const;
    void PostSpeechFinished();

    Streaming::StreamingStateManager* mpStreamingManager;
    CgsSound::Logic::VoiceWrapper::CreateParams mCreateParams;
    EPlayState mePlayState;
    bool mbSpeechStillPlaying;
    bool mbFirstTimeTipPlaying;
    u32 muQueuedContentSpec;
    bool mbQueuedSpeechIsFirstTimeTip;
};

} // namespace Logic
} // namespace BrnSound

#endif

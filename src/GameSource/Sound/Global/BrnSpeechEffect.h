#ifndef BRN_SOUND_LOGIC_BRN_SPEECH_EFFECT_H
#define BRN_SOUND_LOGIC_BRN_SPEECH_EFFECT_H

#include "types.hpp"
#include "GameSource/Sound/Module/LogicModule/BrnEffectObject.h"
#include "GameSource/Sound/Streaming/BrnIStreamUser.h"

namespace Attrib
{
    struct RefSpec;
    namespace Gen
    {
        class languagestreamcollection;
        class languagestreamconfiguration;
    }
}

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

    // DWARF BrnSpeechEffect.cpp:344 -- CgsLanguage::ELanguage -> AttribSys::Enums::eLanguage.
    // X360 @0x82687F80 (a bare switch; the console calls it on the message payload).
    static s32 GetLanguage(s32 aeCgsLanguage);

private:
    // X360 @0x826BCC68 (`PlaySpeech(const languagestreamconfiguration&, bool)`) and its
    // RefSpec-taking sibling: ContentSpecs[meLanguage] -> PlayStream.
    bool PlaySpeech(const Attrib::RefSpec& arRefSpec, bool abFirstTimeTip);
    bool PlaySpeech(const Attrib::Gen::languagestreamconfiguration& arStream, bool abFirstTimeTip);
    // X360 @0x826D3098: pick a random element of the collection's Items array.
    bool PlayRandomSpeechVariation(const Attrib::Gen::languagestreamcollection& arVariations,
                                   bool abFirstTimeTip);

    // X360 @0x8269E6C8 / @0x8269E838.
    bool IsOneOnOne() const;
    bool IsLocalPlayerRunner() const;

    // X360 @0x8269E918 -- DWARF `bool GetSpeechMapping(SoundLogicModule*, Name, RefSpec&)`.
    bool GetSpeechMapping(u32 auMappingName, Attrib::RefSpec& arConfiguration) const;
    void PostSpeechFinished();

    Streaming::StreamingStateManager* mpStreamingManager;
    CgsSound::Logic::VoiceWrapper::CreateParams mCreateParams;
    EPlayState mePlayState;
    bool mbSpeechStillPlaying;
    bool mbFirstTimeTipPlaying;
    // X360 this+128 -- AttribSys::Enums::eLanguage, zeroed by Attach @0x826F7D48 and
    // rewritten by Notify case 0x21 (sound message 33, set language).
    s32 meLanguage;
    // X360 this+132 / this+136 -- Attach seeds both from the logic module's Random
    // (`muSeed` @ module+79280) modulo the authored array length.
    u32 muNextRoadRageIntroIndex;
    u32 muNextStuntRunIntroIndex;
    u32 muQueuedContentSpec;
    bool mbQueuedSpeechIsFirstTimeTip;
};

} // namespace Logic
} // namespace BrnSound

#endif

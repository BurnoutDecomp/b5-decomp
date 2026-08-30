#ifndef CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_PLAYER_VOICE_H
#define CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_PLAYER_VOICE_H

#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacVoice.h"
#include "rw/audio/core/plugins/SndPlayer1.h"

namespace CgsSound
{
namespace Playback
{

class GenericRwacFactory;

class GenericRwacPlayerVoice : public PlayerVoice, public GenericRwacVoice
{
public:
    GenericRwacPlayerVoice(GenericRwacFactory& arFactory,
                           const VoiceSpec& akrSpec, u32 au32Ident);
    virtual ~GenericRwacPlayerVoice();

    virtual f32 GetCpuTicks();
    virtual EProfileVoiceType GetProfileVoiceType();
    virtual void DoUpdate(System* apSystem, f32 af32DeltaTime);
    virtual bool DoConnectSend(u32 au32Index, SubmixVoice* apSubmix);

    rw::audio::core::SndPlayer1::IsRequestDoneParams& GetDoneParams()
    {
        return mIsRequestDoneParams;
    }

private:
    // DecFIGS CgsGenericRwacPlayerVoice.h:121; ARTIST +0x84/+0x88.
    rw::audio::core::SndPlayer1::IsRequestDoneParams mIsRequestDoneParams;
};

} // namespace Playback
} // namespace CgsSound

#endif

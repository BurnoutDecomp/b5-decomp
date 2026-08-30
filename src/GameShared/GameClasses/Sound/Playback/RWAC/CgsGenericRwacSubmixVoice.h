#ifndef CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_SUBMIX_VOICE_H
#define CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_SUBMIX_VOICE_H

#include "GameShared/GameClasses/Sound/Playback/CgsSubmixVoice.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacVoice.h"

namespace CgsSound
{
namespace Playback
{

class GenericRwacFactory;

class GenericRwacSubmixVoice : public SubmixVoice, public GenericRwacVoice
{
public:
    GenericRwacSubmixVoice(GenericRwacFactory& arFactory,
                           const VoiceSpec& akrSpec, u32 au32Ident);
    virtual ~GenericRwacSubmixVoice();

    virtual f32 GetCpuTicks();
    virtual EProfileVoiceType GetProfileVoiceType();
    virtual void DoUpdate(System* apSystem, f32 af32DeltaTime);
    virtual bool DoConnectSend(u32 au32Index, SubmixVoice* apSubmix);
};

} // namespace Playback
} // namespace CgsSound

#endif

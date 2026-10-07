#ifndef CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_MASTER_VOICE_H
#define CGS_SOUND_PLAYBACK_RWAC_GENERIC_RWAC_MASTER_VOICE_H

#include "GameShared/GameClasses/Sound/Playback/CgsMasterVoice.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacVoice.h"

namespace CgsSound
{
namespace Playback
{

class GenericRwacFactory;

class GenericRwacMasterVoice : public MasterVoice, public GenericRwacVoice
{
public:
    GenericRwacMasterVoice(GenericRwacFactory& arFactory,
                           const VoiceSpec& akrSpec, u32 au32Ident);
    virtual ~GenericRwacMasterVoice();

    virtual f32 GetCpuTicks();
    virtual void DisplayVoiceCpu(f32* /*lpfX*/, f32* /*lpfY*/, f32 /*lfScale*/, bool /*lbDetail*/) {}
    virtual EProfileVoiceType GetProfileVoiceType();
    virtual void DoUpdate(System* apSystem, f32 af32DeltaTime);
    virtual bool DoConnectSend(u32 au32Index, SubmixVoice* apSubmix);
};

} // namespace Playback
} // namespace CgsSound

#endif

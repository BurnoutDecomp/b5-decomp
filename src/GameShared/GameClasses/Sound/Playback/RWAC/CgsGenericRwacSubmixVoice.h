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
    // DWARF CgsGenericRwacSubmixVoice.h:109, header-inline with the base Factory&: ARTIST
    // FactoryNew<GenericRwacSubmixVoice> 0x826DFDA0 calls Voice's constructor and inlines
    // the rest.
    GenericRwacSubmixVoice(Factory& arFactory,
                           const VoiceSpec& akrSpec, u32 au32Ident)
        : SubmixVoice(sizeof(GenericRwacSubmixVoice), arFactory, akrSpec, au32Ident),
          GenericRwacVoice()
    {
    }
    virtual ~GenericRwacSubmixVoice();

    virtual f32 GetCpuTicks();
    virtual void DisplayVoiceCpu(f32* /*lpfX*/, f32* /*lpfY*/, f32 /*lfScale*/, bool /*lbDetail*/) {}
    virtual EProfileVoiceType GetProfileVoiceType();
    virtual void DoUpdate(System* apSystem, f32 af32DeltaTime);
    virtual bool DoConnectSend(u32 au32Index, SubmixVoice* apSubmix);
};

} // namespace Playback
} // namespace CgsSound

#endif

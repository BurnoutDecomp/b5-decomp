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
    // DWARF CgsGenericRwacMasterVoice.h:121, header-inline with the base Factory&: ARTIST
    // FactoryNew<GenericRwacMasterVoice> 0x826DFE38 calls MasterVoice's constructor and
    // inlines the rest.
    GenericRwacMasterVoice(Factory& arFactory,
                           const VoiceSpec& akrSpec, u32 au32Ident)
        : MasterVoice(sizeof(GenericRwacMasterVoice), arFactory, akrSpec, au32Ident),
          GenericRwacVoice()
    {
    }
    // No user-declared destructor: DWARF lists ~GenericRwacMasterVoice without a declaration line and
    // places its body on the class's closing line (CgsGenericRwacMasterVoice.h:40), the compiler's implicit
    // one; ARTIST's scalar deleting destructor 0x826E9928 calls the base destructor without
    // restoring the vftable pointer(s).

    virtual f32 GetCpuTicks();
    virtual void DisplayVoiceCpu(f32* /*lpfX*/, f32* /*lpfY*/, f32 /*lfScale*/, bool /*lbDetail*/) {}
    virtual EProfileVoiceType GetProfileVoiceType();
    virtual void DoUpdate(System* apSystem, f32 af32DeltaTime);
    virtual bool DoConnectSend(u32 au32Index, SubmixVoice* apSubmix);
};

} // namespace Playback
} // namespace CgsSound

#endif

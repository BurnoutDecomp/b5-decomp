#ifndef CGS_SOUND_PLAYBACK_AEMS_CGSAEMSPLAYERVOICE_H
#define CGS_SOUND_PLAYBACK_AEMS_CGSAEMSPLAYERVOICE_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/CgsRegistry.h"
#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h"
#include "GameShared/GameClasses/Sound/Playback/CgsEnvironment.h"
#include "GameShared/GameClasses/Sound/Playback/CgsFactory.h"
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsDataStructures.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacVoice.h"
#include "SDKs/Csis/CsisClass.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"   // GetAemsFactory (h:119)

#include <cstddef>

namespace CgsSound
{
namespace Playback
{

class AemsFactory;
struct AemsRWSamplePlayer;

// ARTIST @0x826DA1E8. The primary Playback::PlayerVoice is followed by the
// non-polymorphic GenericRwacVoice half, matching the game’s multiple-inheritance
// graph. The integer parameter block is tail-allocated after this object.
struct AemsPlayerVoice : public PlayerVoice, public GenericRwacVoice
{
    static size_t GetClientAllocationSize(Factory& arFactory,
                                          const VoiceSpec& arVoiceSpec);
    void* operator new(size_t auSize, Factory& arFactory,
                       const VoiceSpec& arVoiceSpec);

    // DWARF CgsAemsPlayerVoice.h:283: the base Factory& (FactoryNew passes *this).
    AemsPlayerVoice(Factory& arFactory, const VoiceSpec& arVoiceSpec,
                    u32 au32Ident);
    virtual ~AemsPlayerVoice();

    virtual f32 GetCpuTicks();
    virtual void DisplayVoiceCpu(f32* /*lpfX*/, f32* /*lpfY*/, f32 /*lfScale*/, bool /*lbDetail*/) {}
    virtual EProfileVoiceType GetProfileVoiceType();
    virtual void DoUpdate(System* apSystem, f32 af32DeltaTime);
    virtual bool DoConnectSend(u32 au32Index, SubmixVoice* apSubmix);
    virtual bool DoRemove();

    bool Play(u32 au32Param);
    bool Update(f32 af32Dt);
    bool Stop();

    void AddSamplePlayer(AemsRWSamplePlayer* apPlayer);
    void RemoveSamplePlayer(AemsRWSamplePlayer* apPlayer);
    // DWARF CgsAemsPlayerVoice.h:110.
    rw::audio::core::PlugIn* GetInternalSubmix() const
    {
        return mpInternalSubmix;
    }

    // DWARF CgsAemsPlayerVoice.h:119: the owning factory, reached through a null-checked
    // pointer downcast of the voice's Factory (`addi -4`, Play 0x826DAECC, Stop 0x826DAF40).
    AemsFactory& GetAemsFactory()
    {
        return *static_cast<AemsFactory*>(const_cast<Factory*>(&GetFactory()));
    }

private:
    friend struct AemsRWSamplePlayer;

    // DWARF CgsAemsPlayerVoice.h:204: the parameter block allocated after the object
    // (GetClientAllocationSize; ARTIST indexes it as this + 0xA8 + 4 * index).
    uint32_t& GetAemsParameter(uint32_t lu32Index)
    {
        return reinterpret_cast<uint32_t*>(this + 1)[lu32Index];
    }

    // DWARF CgsAemsPlayerVoice.h:201..224 (X360 +0x84..+0xA4, sizeof 0xA8: AemsFactory::
    // DoCreateVoice allocates `li r3,0xA8`).
    uint32_t                 muAemsTicks;            // :201
    AemsRWSamplePlayer*      mpFirstPlayer;          // :217
    rw::audio::core::PlugIn* mpInternalSubmix;       // :218
    Csis::ClassHandle        mCsisHandle;            // :219
    Csis::Class*             mpCsisClass;            // :220
    uint32_t                 mu32AemsFirstUserParam; // :221
    uint32_t                 mu32AemsParamCount;     // :222
    bool                     mbRemoving;             // :224
};

} // namespace Playback
} // namespace CgsSound

#endif

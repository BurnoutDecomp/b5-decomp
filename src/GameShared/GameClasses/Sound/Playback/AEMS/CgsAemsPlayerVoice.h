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

#include <cstddef>

namespace CgsSound
{
namespace Playback
{

class AemsFactory;
struct AemsRWSamplePlayer;

Registry* GetAemsFactoryRegistry(Factory* lpAemsFactory);

// ARTIST @0x826DA1E8. The primary Playback::PlayerVoice is followed by the
// non-polymorphic GenericRwacVoice half, matching the game’s multiple-inheritance
// graph. The integer parameter block is tail-allocated after this object.
struct AemsPlayerVoice : public PlayerVoice, public GenericRwacVoice
{
    static size_t GetClientAllocationSize(Factory& arFactory,
                                          const VoiceSpec& arVoiceSpec);
    void* operator new(size_t auSize, Factory& arFactory,
                       const VoiceSpec& arVoiceSpec);

    AemsPlayerVoice(AemsFactory& arFactory, const VoiceSpec& arVoiceSpec,
                    u32 au32Ident);
    virtual ~AemsPlayerVoice();

    virtual f32 GetCpuTicks();
    virtual EProfileVoiceType GetProfileVoiceType();
    virtual void DoUpdate(System* apSystem, f32 af32DeltaTime);
    virtual bool DoConnectSend(u32 au32Index, SubmixVoice* apSubmix);
    virtual bool DoRemove();

    bool Play(u32 au32Param);
    bool Update(f32 af32Dt);
    bool Stop();

    void AddSamplePlayer(AemsRWSamplePlayer* apPlayer);
    void RemoveSamplePlayer(AemsRWSamplePlayer* apPlayer);
    rw::audio::core::PlugIn* GetInternalSubmix() const
    {
        return mpInternalSubmix;
    }
    bool IsCreated() const { return mbCreated; }

private:
    friend struct AemsRWSamplePlayer;

    f32 mfSamplePlayerCpuTicks;
    AemsRWSamplePlayer* mpFirstPlayer;
    rw::audio::core::PlugIn* mpInternalSubmix;
    Csis::ClassHandle mhClass;
    Csis::Class* mpRequest;
    u32 mu32UserParameterStart;
    u32 mu32AemsInputParameterCount;
    bool mbRemoving;
    bool mbCreated;
    s32* mpAemsParameters;
};

} // namespace Playback
} // namespace CgsSound

#endif

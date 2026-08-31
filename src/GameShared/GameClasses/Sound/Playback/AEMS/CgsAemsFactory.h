#ifndef CGS_SOUND_PLAYBACK_AEMS_CGSAEMSFACTORY_H
#define CGS_SOUND_PLAYBACK_AEMS_CGSAEMSFACTORY_H

#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsInterfaceImplementation.h"
#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsCsisCommandQueue.h"
#include "GameShared/GameClasses/Sound/Playback/CgsRegistry.h"
#include "GameShared/GameClasses/Sound/Playback/CgsHandle.h"

namespace CgsSound
{
namespace Playback
{

class GenericRwacFactory;

// ARTIST dword_83008664, initialised from
// "~AemsFactory::SK_NAME~" by sub_82C65788.
const Name& AemsFactorySkName();

// CgsAemsFactory.h:68 (DecFIGS). One registered AEMS patch monitor.
struct PatchMonitor
{
    const char* mpName;
    void*       mpClientFunc;
    void*       mpClientData;
    s32         miPerfmon;
};

const u32 KU_MAX_PATCH_MONITORS = 16;

struct AemsFactorySpec
{
    Factory* mpRwacFactory;
    u32 mu32EntityCount;
    u32 mu32DataSize;
    u32 mu32StringTableSize;
};

// ARTIST @0x826DAAD0 / 0x826DAC28. AEMS owns the CSIS command ring and
// delegates the ordinary audio graph to the retained GenericRwacFactory.
class AemsFactory : public AemsRWSampleFactory
{
public:
    static Handle<AemsFactory> Create(Environment& arEnvironment,
                                      const AemsFactorySpec& akrSpec);
    AemsFactory(Environment& arEnvironment, const AemsFactorySpec& akrSpec);

    Registry* GetRegistry() { return mpRegistry; }
    CsisCommandQueue& GetCommandQueue() { return mCommandQueue; }
    GenericRwacFactory& GetRwacFactory() const { return *mpRwacFactory; }

protected:
    virtual bool DoCreateVoice(const VoiceSpec& akrSpec,
                               Handle<Voice>& arHandleOut,
                               u32 au32Ident);
    virtual bool DoCreateContent(const ContentSpec& akrSpec,
                                 Handle<Content>& arHandleOut,
                                 u32 au32Ident);
    virtual void DoUpdate(f32 af32DeltaTime);

    static void CsisPrint(const char* lpcText);
    PatchMonitor* FindPatchMonitor(const char* lpcName);

private:
    u32 muPatchMonitorCount;
    Registry* mpRegistry;
    GenericRwacFactory* mpRwacFactory;
    CsisCommandQueue mCommandQueue;
    PatchMonitor maPatchMonitors[KU_MAX_PATCH_MONITORS];
};

} // namespace Playback
} // namespace CgsSound

#endif

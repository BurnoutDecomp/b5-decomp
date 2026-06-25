#include "GameSource/Sound/BrnResourceRegistrar.h"

// BrnSound::Logic::ResourceRegistrar -- see the header. Minimal-then-grow: the broker's method
// bodies are reconstructed on top of this canonical home. The queue-driven logic (draining the
// request queues, resolving handles, the removal-candidate GC) needs the embedded VariableEventQueue
// request interfaces + the requested/queued pools + the LinearHashTable map, which grow in next; for
// now these are safe no-ops so the SoundLogicModule lifecycle + the effect Detach path can call them.

namespace BrnSound
{
namespace Logic
{
    // 0x826B0470
    void ResourceRegistrar::Construct()
    {
        miReserved = 0;
        // [grow-in] Construct+Clear mResourceRequestInterface (VEQ<4096,16>) +
        // mAttribSysRequestInterface (VEQ<2048,16>); InternalInit the requested/queued list pools.
    }

    // 0x82702228
    void ResourceRegistrar::Update()
    {
        // [grow-in] clear the request queues, UpdateRequests -> UpdateQueued -> ClearUnreferancedFiles.
    }

    // 0x826E2220
    void ResourceRegistrar::RemoveRequests(IResourceRequester* /*lpRequester*/)
    {
        // [grow-in] walk the requested-resource list, removing the nodes owned by lpRequester.
    }
}
}

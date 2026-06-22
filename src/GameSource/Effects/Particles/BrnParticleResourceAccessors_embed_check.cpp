// Embed check: the BrnParticle facade home headers must each be includable on their
// own and compose with the facade TU's headers without ODR / completeness errors.
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"
#include "GameShared/GameClasses/Module/CgsModuleIOHelper.h"
#include "GameSource/Effects/Particles/ParticleModuleIO.h"

namespace
{
    // SafeResourceHandle<T> default-constructs (inherits ResourceHandle's two ptrs) and
    // its accessors are inline; instantiate against a local complete type to exercise the
    // grown CgsResourceHandle.h template path.
    struct DummyResource { int mDummy; };

    void embed_check()
    {
        CgsResource::SafeResourceHandle<DummyResource> lHandle;
        lHandle.mpResourceMemory = 0;
        lHandle.mpSourceEntry = 0;
        (void)lHandle;
    }
}

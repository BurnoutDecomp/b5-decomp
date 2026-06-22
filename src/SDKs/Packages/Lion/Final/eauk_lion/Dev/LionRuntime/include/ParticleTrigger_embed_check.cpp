// Embed check: include the ParticleTrigger home header and exercise the bodied
// cLionFX::TriggerUpdate shim (both the null and non-null paths are reachable by
// name). Compile-only; not shipped.
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleTrigger.h"

namespace
{
    void ParticleTriggerEmbedCheck()
    {
        cTime lTime = {};

        // Null path: returns the (null) trigger without touching it.
        cParticleTrigger* lpNull = cLionFX::TriggerUpdate(nullptr, 0u, lTime);
        (void)lpNull;

        // Non-null path: forwards (flags, time) to Update (declared in this home,
        // bodied in its own TU -- compile-only, so the call need not link).
        cParticleTrigger lTrigger = {};
        cParticleTrigger* lpOut = cLionFX::TriggerUpdate(&lTrigger, 1u, lTime);
        (void)lpOut;

        const bool lbRunning = lTrigger.IsRunning();
        (void)lbRunning;
    }
}

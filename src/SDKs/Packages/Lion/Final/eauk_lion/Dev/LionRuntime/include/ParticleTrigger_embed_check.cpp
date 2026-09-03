// Embed check: include the ParticleTrigger home header and exercise the trigger's own
// surface. Compile-only; not shipped.
//
// ⛔ IT USED TO EXERCISE `cLionFX::TriggerUpdate` FROM THIS HEADER, and that was the reason
// the ODR fork retired from ParticleTrigger.h survived so long: the check kept a namespace
// spelling of cLionFX alive and compiling in a TU that never saw the real struct. The shim
// is now on cLionFX itself (LionFX.cpp); this check covers the trigger.
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleTrigger.h"

namespace
{
    void ParticleTriggerEmbedCheck()
    {
        cTime lTime = {};

        cParticleTrigger lTrigger = {};
        lTrigger.Init();

        // Declared in this home, bodied in its own TU -- compile-only, so the call need
        // not link.
        lTrigger.Update(1u, lTime);

        const bool lbRunning = lTrigger.IsRunning();
        (void)lbRunning;
    }
}

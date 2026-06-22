// Tiny embed check: include the ParticleBehaviour home header alongside the Lion
// sibling homes it composes with (serialiser + tokeniser) in one TU, to confirm
// they compose without ODR / redefinition clashes. Compile-only; not shipped.
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBehaviour.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSerialiser.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionTokeniser.h"

namespace
{
    void ParticleBehaviourEmbedCheck()
    {
        cParticleBehaviour lBehaviour = {};
        lBehaviour.mFlags = cParticleBehaviour::E_BV_AXIS;
        lBehaviour.BuildColourSteps();
        lBehaviour.CompileBaseVariance();
        lBehaviour.Relocate();

        // Compiled base-variance pack is reachable by name.
        const u32 luPacks = lBehaviour.mBVCompiled.size;
        (void)luPacks;
    }
}

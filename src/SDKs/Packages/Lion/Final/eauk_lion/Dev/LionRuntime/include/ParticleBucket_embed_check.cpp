// Tiny embed check: include the ParticleBucket home header and exercise the one
// bodied function so the layout + signature compose. Compile-only; not shipped.
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBucket.h"

namespace
{
    void ParticleBucketEmbedCheck()
    {
        cParticleBucket lBucket = {};

        u32 luSlot = 0;
        sParticleNucleus* lpNucleus = 0;
        cVector* lpVector = 0;
        cMatrix* lpMatrix = 0;

        const bool lbOk = lBucket.AllocateParticle(luSlot, &lpNucleus, &lpVector, &lpMatrix);

        (void)lbOk;
        (void)luSlot;
        (void)lpNucleus;
        (void)lpVector;
        (void)lpMatrix;
    }
}

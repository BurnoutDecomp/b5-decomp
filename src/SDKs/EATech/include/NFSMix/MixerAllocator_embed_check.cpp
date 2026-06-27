#include "SDKs/EATech/include/NFSMix/MixerAllocator.hpp"

// Compile-gate the mixer allocator interface + its global.
static void MixerAllocator_embed_check()
{
    (void)sizeof(MixerAllocator);
    MixerAllocator* lp = g_pMixerAllocator;
    (void)lp;
}

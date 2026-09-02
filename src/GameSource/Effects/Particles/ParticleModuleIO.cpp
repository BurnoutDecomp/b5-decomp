// ============================================================================
// GameSource/Effects/Particles/ParticleModuleIO.cpp
//
// BrnParticle::ParticleIO::PrepareOutputBuffer's two out-of-line accessors
// (ParticleModuleIO.h:126 / :129). The header has carried them as declaration-only
// since it landed; ParticleModule::LoadFXBundle is the first consumer that needs the
// bodies, so this is the TU the header's own note said would come.
//
// SHAPE: a bare `return &mResourceRequestInterface`. NO LOCK ASSERT.
//
// ⚠ CORRECTED after the first run (2026-09-02). The first draft asserted the buffer was
// locked, by analogy with the sibling accessors on other IO buffers -- and that was an
// INVENTED ARM that fired 889 times in one boot. The console does not: EffectsModule::Prepare
// @0x8229E768 inlines this accessor as one instruction,
//     addi r4, r30, 4        ; &lpParticleOutput->mResourceRequestInterface
//     bl   EffectsIO::OutputBuffer::SetResourceRequestInterface
// with no call and no test, and it does it while only the OTHER buffer is write-locked. An
// assert the binary does not have is exactly the defect class this project keeps re-finding.
// The CreateIOBuffer<PrepareOutputBuffer> instantiation @0x8228E4F0 pins the member at +4.
// ============================================================================

#include "GameSource/Effects/Particles/ParticleModuleIO.h"

namespace BrnParticle
{
namespace ParticleIO
{
    PrepareOutputBuffer::ResourceRequestInterface* PrepareOutputBuffer::GetResourceRequestInterface()
    {
        return &mResourceRequestInterface;
    }

    const PrepareOutputBuffer::ResourceRequestInterface*
    PrepareOutputBuffer::GetResourceRequestInterface() const
    {
        return &mResourceRequestInterface;
    }
}
}

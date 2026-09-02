// ============================================================================
// GameSource/Effects/Particles/ParticleModuleIO.cpp
//
// BrnParticle::ParticleIO::PrepareOutputBuffer's two out-of-line accessors
// (ParticleModuleIO.h:126 / :129). The header has carried them as declaration-only
// since it landed; ParticleModule::LoadFXBundle is the first consumer that needs the
// bodies, so this is the TU the header's own note said would come.
//
// SHAPE, from the console's sibling accessors on every other IO buffer (e.g.
// EffectsIO::OutputBuffer::GetResourceRequestInterface, BrnRootSoundModuleIo's
// ::G @0x823B8A68 / ::GetReso @0x826951D0): assert the buffer is locked the right way,
// then return &mResourceRequestInterface (the console's `addi r3, this, 4`). The
// CreateIOBuffer<PrepareOutputBuffer> instantiation @0x8228E4F0 pins the member at +4.
// ============================================================================

#include "GameSource/Effects/Particles/ParticleModuleIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnParticle
{
namespace ParticleIO
{
    PrepareOutputBuffer::ResourceRequestInterface* PrepareOutputBuffer::GetResourceRequestInterface()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mResourceRequestInterface;
    }

    const PrepareOutputBuffer::ResourceRequestInterface*
    PrepareOutputBuffer::GetResourceRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mResourceRequestInterface;
    }
}
}

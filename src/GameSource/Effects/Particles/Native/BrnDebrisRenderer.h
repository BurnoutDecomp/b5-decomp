#pragma once

// ============================================================================
// GameSource/Effects/Particles/Native/BrnDebrisRenderer.h
//
// BrnParticle::Native::BrnDebrisRenderer -- the immediate-mode renderer that draws the
// debris arrays. It caches the textured-plus-lighting Im3d renderer it borrows and the
// blend state it builds at construction time.
//
// LAYOUT AUTHORITY: DecFIGS DWARF (BrnDebrisRenderer.h:264) + X360 asm (Construct
// @ 0x82281DA8):
//   this[0] @ +0x00 -- mpRenderer   : BrnGraphics::Im3dTexPlusLighting* (borrowed)
//   this[1] @ +0x04 -- mpBlendState : renderengine::BlendState* built by Construct
//
// Only Construct is reconstructed in this pass; the remaining methods (Destruct,
// BeginRender, RenderDebrisArray, EndRender) are declared for the record and land in
// later passes. GROW additively.
// ============================================================================

#include "types.hpp"

namespace rw { class IResourceAllocator; }
namespace renderengine { class BlendState; }
namespace BrnGraphics { class Im3dTexPlusLighting; }

namespace BrnParticle
{
namespace Native
{
    class BrnDebrisRenderer
    {
    public:
        // BrnParticle::Native::BrnDebrisRenderer::Construct @ 0x82281DA8. Cache the borrowed
        // Im3d renderer and build the additive, alpha-tested debris blend state through the
        // resource allocator. Caller: ParticleModule::Prepare.
        void Construct(rw::IResourceAllocator* lpAllocator, BrnGraphics::Im3dTexPlusLighting* lpRenderer);

    private:
        BrnGraphics::Im3dTexPlusLighting* mpRenderer;   // this[0] @ +0x00
        renderengine::BlendState*         mpBlendState; // this[1] @ +0x04
    };
}
}

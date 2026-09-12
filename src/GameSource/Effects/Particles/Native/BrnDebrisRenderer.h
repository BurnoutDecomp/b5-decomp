#pragma once

// ============================================================================
// GameSource/Effects/Particles/Native/BrnDebrisRenderer.h
//
// BrnParticle::Native::BrnDebrisRenderer -- the immediate-mode renderer that draws the
// debris arrays. It caches the textured-plus-lighting Im3d renderer it borrows and the
// blend state it builds at construction time.
//
// LAYOUT AUTHORITY: the original declarations plus the attested behaviour of Construct:
//   this[0] @ +0x00 -- mpRenderer   : BrnGraphics::Im3dTexPlusLighting* (borrowed)
//   this[1] @ +0x04 -- mpBlendState : renderengine::BlendState* built by Construct
//
// Construct, BeginRender and RenderDebrisArray are reconstructed; Destruct and EndRender
// land in later passes. GROW additively.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                          // Matrix44 / Vector3
#include "GameSource/Effects/Particles/Native/BrnDebrisArray.h"      // BrnDebrisArray / EDebrisArrayID

namespace rw { class IResourceAllocator; }
namespace renderengine { class BlendState; }
namespace BrnGraphics { struct Im3dTexPlusLighting; }

namespace BrnParticle
{
namespace Native
{
    class BrnDebrisRenderer
    {
    public:
        // BrnParticle::Native::BrnDebrisRenderer::Construct. Cache the borrowed
        // Im3d renderer and build the additive, alpha-tested debris blend state through the
        // resource allocator. Caller: ParticleModule::Prepare.
        void Construct(rw::IResourceAllocator* lpAllocator, BrnGraphics::Im3dTexPlusLighting* lpRenderer);

        // BrnParticle::Native::BrnDebrisRenderer::BeginRender. Open the debris pass: start the
        // borrowed Im3d renderer, bind the pass's blend / rasteriser / depth-stencil / sampler
        // states, then push the four per-FRAME shader constants -- the view-projection, the eye
        // position, the (negated) world light direction and the light colour. The ambient colour
        // is passed and NOT read by the console body; it is kept in the signature because the
        // call site sets it. Caller: ParticleModule::RenderFullResParticles.
        void BeginRender(Matrix44::InParam lViewProjection,
                         Vector3           lLightDirection,
                         Vector3           lLightColour,
                         Vector3           lAmbientColour,
                         Vector3           lEye);

        // BrnParticle::Native::BrnDebrisRenderer::RenderDebrisArray. Draw one debris array: push
        // the array's per-ARRAY shiny constants, bind its texture and mesh, then walk its live
        // buckets 32 particles at a time, building one per-instance world matrix per particle
        // (rotation about the particle's own axis by its own angle, scaled by size * fade-in,
        // translated to its position, with the diffuse colour carried in the matrix's fourth
        // column) and issuing one indexed draw per full batch.
        //
        // ARGUMENT ORDER IS THE CONSOLE'S and looks odd on purpose: the float is the FIRST
        // declared parameter, which is why the pointer parameters start one general-purpose
        // register late.
        void RenderDebrisArray(f32                   lfCurrentTime,
                               const BrnDebrisArray* lpArray,
                               EDebrisArrayID        leArrayId,
                               bool                  lbFullLifetime);

    private:
        BrnGraphics::Im3dTexPlusLighting* mpRenderer;   // this[0] @ +0x00
        renderengine::BlendState*         mpBlendState; // this[1] @ +0x04
    };
}
}

#pragma once

// ============================================================================
// GameSource/Effects/Particles/Native/BrnSimpleParticleRenderer.h
//
// BrnParticle::Native::BrnSimpleParticleRenderer -- the DEVICE half of the native simple particles:
// it replays the frame's SimpleParticleBatchArray (built by SimpleParticleVertexBufferBuilder on the
// particle job) through BrnGraphics::Im3dSmokeRenderer, one QUADLIST draw per batch, each with its
// type's texture and its blend mode's state.
//
// ⭐ 2026-09-24 (FX-CRASHVFX): THE DWARF SHAPE. The earlier header modelled "a small POD struct that
// caches three pre-built blend states" with an int first slot and an `int Construct(int, int)` that
// returned a pointer through `reinterpret_cast<int>` (it could not compile on the host and was never
// mounted). The DecFIGS DWARF (BrnSimpleParticleRenderer.h:391-422) is:
//   struct BrnSimpleParticleRenderer {
//     BrnGraphics::Im3dSmokeRenderer* mpRenderer;          // :418  +0x00
//     BlendState*                     mpStandardBlend;     // :420  +0x04
//     BlendState*                     mpAdditiveBlend;     // :421  +0x08
//     BlendState*                     mpSubtractiveBlend;  // :422  +0x0C
//     void Construct(HeapMalloc*, BrnGraphics::Im3dSmokeRenderer*);   // :397  X360 @0x82284518
//     void Destruct();                                                  // :400
//     void Dispatch(renderengine::VertexBuffer*, const SimpleParticleBatchArray&, uint32_t, uint32_t,
//                   CgsRenderTarget*, float32_t, float32_t, bool8_t);   // :413  X360 @0x8228CA18
//   }
// The DWARF's BlendState is the runtime object every applier consumes, which this tree spells
// renderengine::BlendMaterialState (see shadowingdevice.cpp's SetState banner for the same
// reconciliation).
// ============================================================================

#include "types.hpp"
#include "GameSource/Effects/Particles/Native/BrnSimpleParticleBatch.h"   // SimpleParticleBatchArray

namespace CgsMemory { class HeapMalloc; }
namespace renderengine { class VertexBuffer; struct BlendMaterialState; }
namespace BrnGraphics { struct Im3dSmokeRenderer; }
class CgsRenderTarget;

namespace BrnParticle
{
namespace Native
{
    struct BrnSimpleParticleRenderer
    {
        // @0x82284518: keep the renderer, and build the three blend states the batches pick from --
        // the immediate-mode library's Standard / Additive / Subtractive templates, each with its
        // ALPHA half forced to ONE / INVSRCALPHA and an alpha test of GREATER than 1.
        void Construct(CgsMemory::HeapMalloc* lpHeapMalloc, BrnGraphics::Im3dSmokeRenderer* lpRenderer);

        // @0x8228CA18: draw batches [luFirstBatch, luLastBatch) of lrBatches out of lpVertexBuffer.
        // lpDepthTarget / the two planes / lbZFade drive the soft-particle program (see the .cpp).
        void Dispatch(renderengine::VertexBuffer* lpVertexBuffer,
                      const SimpleParticleBatchArray& lrBatches,
                      u32 luFirstBatch,
                      u32 luLastBatch,
                      CgsRenderTarget* lpDepthTarget,
                      f32 lfNearPlane,
                      f32 lfFarPlane,
                      bool lbZFade);

        BrnGraphics::Im3dSmokeRenderer*     mpRenderer;          // :418
        renderengine::BlendMaterialState*   mpStandardBlend;     // :420
        renderengine::BlendMaterialState*   mpAdditiveBlend;     // :421
        renderengine::BlendMaterialState*   mpSubtractiveBlend;  // :422
    };
}
}

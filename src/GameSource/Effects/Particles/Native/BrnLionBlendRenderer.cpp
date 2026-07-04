// ============================================================================
// GameSource/Effects/Particles/Native/BrnLionBlendRenderer.cpp
//
// BrnGraphics::LionBlendRenderer -- the concrete immediate-mode blend renderer that
// LionParticleRender drives (an Im3dBlend specialisation of CgsGraphics::ImRenderer<V>).
// Only EndRendering is reconstructed here; the remaining draw bodies (BeginRendering /
// SetCameraData / SetState / RenderSprites|Quads|Tilts) live in this TU too but are out
// of scope for this wave -- GROW this file when they land, do NOT fork the header.
// ============================================================================

#include "GameSource/Effects/Particles/Native/BrnLionBlendRenderer.h"

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"  // CgsGraphics::ImRendererBase (mgpActiveRenderer)
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace renderengine { class BlendMaterialState; }

// The process-wide default blend-state template bound at batch end (X360 .data home is the
// LionParticleRender TU; extern for the compile gate). Same static that
// LionParticleRenderMaterial.cpp / BrnSimpleParticleRenderer.cpp read their base blend
// parameters from.
extern renderengine::BlendMaterialState* dword_83010F20;

namespace BrnGraphics
{

// ---------------------------------------------------------------------------
// BrnGraphics::LionBlendRenderer::EndRendering  @ 0x8227E610
//
// End the immediate-mode blend batch: bind the shared default blend template through this
// renderer's own SetState (the committed header abstracts the X360 `this + 4` ImRendererBase
// subobject behind LionBlendRenderer's delegating SetState -- sub_82276E48(mpRenderer+4,
// dword_83010F20)), then run the inlined CgsGraphics::ImRenderer<V>::EndRendering: assert this
// renderer is the active one and clear the active-renderer module static.
// Called by BrnParticle::LionParticleRender::EndRendering.
// ---------------------------------------------------------------------------
void LionBlendRenderer::EndRendering()
{
    // SetState(dword_83010F20): the BlendState overload against the immediate-mode renderer.
    // The committed LionBlendRenderer::SetState(const BlendState*) is the by-name front for
    // the X360 sub_82276E48(mpRenderer+4, state) call; the return value (the renderer, X360 r3)
    // is discarded.
    SetState(reinterpret_cast<const BlendState*>(dword_83010F20));

    // Inlined CgsGraphics::ImRenderer<V>::EndRendering (CgsImRenderer.h): the active-renderer is
    // the X360 `this + 4` ImRendererBase subobject (the committed header keeps LionBlendRenderer
    // standalone, so the subobject is reached as the base-pointer stand-in here).
    CgsGraphics::ImRendererBase* lpBase = reinterpret_cast<CgsGraphics::ImRendererBase*>(this);
    CGS_ASSERT(CgsGraphics::ImRendererBase::mgpActiveRenderer == lpBase,
               "mgpActiveRenderer == this");
    CgsGraphics::ImRendererBase::mgpActiveRenderer = nullptr;
}

}  // namespace BrnGraphics

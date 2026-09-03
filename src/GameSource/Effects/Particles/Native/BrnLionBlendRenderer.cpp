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
#include "GameSource/Effects/Particles/LionParticleRender.h"   // LionParticleRender::CreateInternalMaterial / cParticleMaterial (the Lion trap stubs below)

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

// =================================================================================================
// The seven remaining LionBlendRenderer methods -- TRAP STUBS, deliberately.
//
// Every one of them is on the LION particle RENDER path and nothing else:
// LionParticleRender's virtuals (Render / RenderGroupBegin / BeginRendering / ...) are their only
// callers, and those virtuals are reached only from cLionFX's dispatch. The Lion core is not
// landed on this build -- cLionFX::Init is announced, not called (ParticleModule_Lifecycle.cpp),
// StartLionEffect always returns KU_HANDLE_INVALID, and no LionEffect slot is ever claimed -- so
// none of these can execute. A trap body is the project's honest "not done yet" for exactly that
// case (STRATEGY.md, "the stub scaffold"): it declares the function unfinished and crashes LOUDLY
// if the Lion path ever does come alive, instead of quietly drawing nothing.
//
// ⚠ They exist at all because the LINK needs them: mLionRenderer is a by-value ParticleModule
// member, so LionParticleRender's vtable is emitted and every virtual it names must resolve.
// EndRendering @0x8227E610 above is the one with a real body.
// =================================================================================================
namespace BrnGraphics
{
    void LionBlendRenderer::BeginRendering(const rw::math::vpu::Matrix44& /*arViewProjection*/,
                                           float32_t /*afA*/, bool8_t /*abB*/, float32_t /*afC*/,
                                           float32_t /*afD*/, float32_t /*afE*/, float32_t /*afF*/,
                                           float32_t /*afG*/,
                                           renderengine::TextureState* /*apTextureState*/)
    {
        CGS_ASSERT(false, "BrnGraphics::LionBlendRenderer::BeginRendering -- NOT RECONSTRUCTED (Lion render path)");
    }

    void LionBlendRenderer::SetCameraData(const rw::math::vpu::Matrix44Affine& /*arBackMat*/,
                                          const rw::math::vpu::Matrix44Affine& /*arViewMat*/,
                                          const rw::math::vpu::Matrix44& /*arViewProjection*/)
    {
        CGS_ASSERT(false, "BrnGraphics::LionBlendRenderer::SetCameraData -- NOT RECONSTRUCTED (Lion render path)");
    }

    void LionBlendRenderer::SetState(const renderengine::DepthStencilState* /*apState*/)
    {
        CGS_ASSERT(false, "BrnGraphics::LionBlendRenderer::SetState(DepthStencilState) -- NOT RECONSTRUCTED (Lion render path)");
    }

    void LionBlendRenderer::SetState(const BlendState* /*apState*/)
    {
        CGS_ASSERT(false, "BrnGraphics::LionBlendRenderer::SetState(BlendState) -- NOT RECONSTRUCTED (Lion render path)");
    }

    void LionBlendRenderer::RenderSprites(EffectsVertexBufferIterator& /*arIterator*/,
                                          RenderedParticle* /*apParticle*/, const cMatrix* /*apMatrix*/,
                                          U32 /*auCount*/, const cParticleEmitter* /*apEmitter*/,
                                          const cTime& /*arTime*/)
    {
        CGS_ASSERT(false, "BrnGraphics::LionBlendRenderer::RenderSprites -- NOT RECONSTRUCTED (Lion render path)");
    }

    void LionBlendRenderer::RenderQuads(EffectsVertexBufferIterator& /*arIterator*/,
                                        RenderedParticle* /*apParticle*/, const cMatrix* /*apMatrix*/,
                                        U32 /*auCount*/, const cParticleEmitter* /*apEmitter*/,
                                        const cTime& /*arTime*/)
    {
        CGS_ASSERT(false, "BrnGraphics::LionBlendRenderer::RenderQuads -- NOT RECONSTRUCTED (Lion render path)");
    }

    void LionBlendRenderer::RenderTilts(EffectsVertexBufferIterator& /*arIterator*/,
                                        RenderedParticle* /*apParticle*/, const cMatrix* /*apMatrix*/,
                                        U32 /*auCount*/, const cParticleEmitter* /*apEmitter*/,
                                        const cTime& /*arTime*/)
    {
        CGS_ASSERT(false, "BrnGraphics::LionBlendRenderer::RenderTilts -- NOT RECONSTRUCTED (Lion render path)");
    }
}

// =================================================================================================
// ONE more Lion-render-path symbol the link needs and nothing can reach -- same trap policy, same
// reason, homed here beside the blend renderer rather than pulling another TU onto the build list.
//
//   * LionParticleRender::CreateInternalMaterial @ LionParticleRenderMaterial.cpp -- reached only
//     from LionParticleRender::TextureRegister / SetMaterial, i.e. cLionFX's own dispatch.
//
// cLionFX::Init is announced and never called on this build (ParticleModule_Lifecycle.cpp), so no
// emitter, material or effect instance exists to reach it.
//
// ⭐ THE OTHER TWO ARE GONE (2026-09-03, boost-exhaust wave). cParticleMaterial::
// SetTextureMapHandle / SetNormalMapHandle used to be trap stand-ins here as well, because their
// real TU -- ParticleMaterial.cpp, which has both real bodies (@0x82909DD8 / @0x82909DE0) --
// "would drag four further un-homed Lion SDK symbols (cLionSerialiser::StringStore,
// gpLionParticleRender, gLionParticleMaterialTokenTable and the unnamed rodata off_82000D08)".
// All four are now homed, ParticleMaterial.cpp is mounted, and the duplicate definitions here
// were an LNK2005 the moment it was -- which is how they were found. A stand-in that outlives
// its reason is a fork waiting to happen.
// =================================================================================================
namespace BrnParticle
{
    U32 LionParticleRender::CreateInternalMaterial(const cParticleMaterial* /*apMaterial*/)
    {
        CGS_ASSERT(false, "BrnParticle::LionParticleRender::CreateInternalMaterial -- NOT RECONSTRUCTED (Lion render path)");
        return 0;
    }
}

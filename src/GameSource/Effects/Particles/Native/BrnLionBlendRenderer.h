#pragma once

// ============================================================================
// GameSource/Effects/Particles/Native/BrnLionBlendRenderer.h
//
// BrnGraphics::LionBlendRenderer -- the concrete immediate-mode blend renderer that
// LionParticleRender drives. It wraps a CgsGraphics Im3dBlend immediate renderer plus the
// camera matrices and emits the actual particle geometry (sprites / quads / tilts).
//
// LAYOUT / DECLARATION AUTHORITY: the member set and method signatures are from the
// DecFIGS DWARF (BrnLionBlendRenderer.h:38). This header models ONLY the surface that
// BrnParticle::LionParticleRender calls into (BeginRendering / EndRendering /
// SetCameraData / SetState / RenderSprites|Quads|Tilts); the renderer's own bodies live
// in BrnLionBlendRenderer.cpp (its own TU). It is declared here so the LionParticleRender
// TU can call it by NAME instead of poking mpRenderer at raw offsets.
//
// The Im3dBlend / matrix members are reproduced for completeness but are accessed only by
// the renderer's own TU; LionParticleRender holds it solely by pointer.
// ============================================================================

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRender/ParticleRender.h"

namespace renderengine
{
    class DepthStencilState;
    class MaterialState;            // a.k.a. BlendState (renderstates.h)
    class TextureState;
}

// The renderer's draw API uses the engine math matrices by value/reference; opaque to the
// caller here (LionParticleRender forwards its own stored matrices into these).
class Matrix44Affine;
namespace rw { namespace math { namespace vpu { class Matrix44; class Matrix44Affine; } } }

namespace BrnGraphics
{
    // BrnGraphics::BlendState alias used by the DWARF SetState overload -- the
    // render-engine material/blend state block.
    typedef renderengine::MaterialState BlendState;

    class LionBlendRenderer
    {
    public:
        // BrnLionBlendRenderer.h:53 -- begin an immediate-mode batch with the packed
        // view-projection matrix and the fog / alpha-test parameters, sampling apTextureState.
        void BeginRendering(const rw::math::vpu::Matrix44& arViewProjection,
                            float32_t afA, bool8_t abB, float32_t afC, float32_t afD,
                            float32_t afE, float32_t afF, float32_t afG,
                            renderengine::TextureState* apTextureState);

        // BrnLionBlendRenderer.h:55
        void EndRendering();

        // BrnLionBlendRenderer.h:74 -- load the back / view / view-projection matrices used
        // to orient camera-facing geometry.
        void SetCameraData(const rw::math::vpu::Matrix44Affine& arBackMat,
                           const rw::math::vpu::Matrix44Affine& arViewMat,
                           const rw::math::vpu::Matrix44& arViewProjection);

        // BrnLionBlendRenderer.h:81 / :86 -- bind a depth-stencil or blend render state on
        // the immediate-mode renderer.
        void SetState(const renderengine::DepthStencilState* apState);
        void SetState(const BlendState* apState);

        // BrnLionBlendRenderer.h:99 / :109 / :119 -- emit the particle geometry for the
        // selected draw shape.
        void RenderSprites(EffectsVertexBufferIterator& arIterator, RenderedParticle* apParticle,
                           const cMatrix* apMatrix, U32 auCount,
                           const cParticleEmitter* apEmitter, const cTime& arTime);
        void RenderQuads(EffectsVertexBufferIterator& arIterator, RenderedParticle* apParticle,
                         const cMatrix* apMatrix, U32 auCount,
                         const cParticleEmitter* apEmitter, const cTime& arTime);
        void RenderTilts(EffectsVertexBufferIterator& arIterator, RenderedParticle* apParticle,
                         const cMatrix* apMatrix, U32 auCount,
                         const cParticleEmitter* apEmitter, const cTime& arTime);
    };
}

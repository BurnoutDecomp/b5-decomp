#pragma once

#include "types.hpp"

// CgsGraphics::ImRenderer - the immediate-mode render layer. ImRendererBase carries
// the (static) render-state library and the SetState/SetTexture API; ImRenderer<V> is
// the per-vertex-type batch renderer (Begin/EndRendering + Render). Recovered from the
// DecFIGS DWARF (CgsImRenderer.h). The render-state types and the platform device
// types are heavy out-of-scope objects, so they are forward-declared here and the
// in-scope renderers reach them only through these calls.
namespace renderengine
{
    class Texture;
    enum PrimitiveType : s32;   // platform primitive-topology enum (external API)
}

namespace CgsGraphics
{
    class BlendState;
    class DepthStencilState;
    class RasterizerState;
    class RenderTargetState;
    class SamplerState;
    class TextureState;
    class ProgramBuffer;

    struct ImRendererBase
    {
        void SetState(const BlendState* lpState);
        void SetState(const DepthStencilState* lpState);
        void SetState(const DepthStencilState* lpState, u32 luStencilRef);
        void SetState(const RasterizerState* lpState);
        void SetState(const RenderTargetState* lpState);
        void SetState(const SamplerState* lpState);
        void SetState(const SamplerState* lpState, u32 luSampler);
        void SetState(const TextureState* lpState);
        void SetState(const TextureState* lpState, u32 luStage);
        void SetState(const ProgramBuffer* lpVertexProgram, const ProgramBuffer* lpPixelProgram);

        void SetTexture(renderengine::Texture* lpTexture);
        void SetTexture(renderengine::Texture* lpTexture, u32 luStage);
    };

    template <typename V>
    struct ImRenderer : public ImRendererBase
    {
        void BeginRendering();
        void EndRendering();
        void Render(renderengine::PrimitiveType lePrimitiveType, const V* lpVertices, u32 luCount);
    };
}

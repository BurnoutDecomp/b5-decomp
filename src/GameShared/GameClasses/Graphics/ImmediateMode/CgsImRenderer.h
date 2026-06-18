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
    class TextureState;         // the font atlas' sampler+texture state (RenderStart/SetState path)
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
        // Bind a renderengine texture state (the bitmap font's atlas) for the text path's submission.
        void SetState(const renderengine::TextureState* lpTextureState);

        void SetTexture(renderengine::Texture* lpTexture);
        void SetTexture(renderengine::Texture* lpTexture, u32 luStage);
    };

    template <typename V>
    struct ImRenderer : public ImRendererBase
    {
        void BeginRendering();
        void EndRendering();
        void Render(renderengine::PrimitiveType lePrimitiveType, const V* lpVertices, u32 luCount);

        // The double-buffer "reserve / submit" API the X360 text path drives (folded onto the PC
        // immediate renderer): RenderStart reserves luVertexCount vertices and returns the write
        // pointer; the caller fills it; RenderEnd submits the run as one primitive. Bodies in
        // CgsIm2d.cpp (PC: a scratch vertex run drawn via DrawPrimitiveUP).
        V*   RenderStart(u32 luVertexCount);
        void RenderEnd(renderengine::PrimitiveType lePrimitiveType, const V* lpVertices, u32 luVertexCount);
    };
}

#pragma once

#include "types.hpp"
// renderengine::RasterizerState::CullMode / DepthStencilState::Function / Texture / ResourceDescriptor5
// are named in the ImRendererBase state-library builder's declarations, so the full render-state home
// is pulled in (it is already the immediate-mode layer's render-state dependency: CgsIm2d.cpp and the
// render-buffer template include it too).
#include "pc/gcm/renderengine/renderstates.h"

// CgsGraphics::ImRenderer - the immediate-mode render layer. ImRendererBase carries
// the (static) render-state library and the SetState/SetTexture API; ImRenderer<V> is
// the per-vertex-type batch renderer (Begin/EndRendering + Render). Recovered from the
// DecFIGS DWARF (CgsImRenderer.h). The render-state types and the platform device
// types are heavy out-of-scope objects, so they are forward-declared here and the
// in-scope renderers reach them only through these calls.
namespace renderengine
{
    enum PrimitiveType : s32;   // platform primitive-topology enum (external API)
}

namespace rw { struct IResourceAllocator; }

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

        // ---- the (static) render-state library + its builders (X360 CgsImRenderer.cpp) -----------
        // The library of shared render states the immediate-mode layer hands out (built once by
        // ConstructOnceOnly). Members recovered from the DecFIGS DWARF (CgsImRenderer.h:56).
        struct StateLibrary
        {
            const BlendState*        mpBlendState_Standard;
            const BlendState*        mpBlendState_Additive;
            const BlendState*        mpBlendState_Subtractive;
            const BlendState*        mpBlendState_SubtractiveColour;
            const BlendState*        mpBlendState_Premultiplied;
            const BlendState*        mpBlendState_NoAlphaTest;
            const BlendState*        mpBlendState_NoBlendNoAlphaTest;
            const RasterizerState*   mpRasterizerState_CullNone;
            const RasterizerState*   mpRasterizerState_CullCCW;
            const RasterizerState*   mpRasterizerState_CullCW;
            const DepthStencilState* mpDepthStencilState_ZBufferOn;
            const DepthStencilState* mpDepthStencilState_ZBufferOnWriteOff;
            const DepthStencilState* mpDepthStencilState_ZBufferGreaterEqWriteOff;
            const DepthStencilState* mpDepthStencilState_ZBufferOff;
            renderengine::Texture*   mpTexture_White;
            const TextureState*      mpTextureState_Untextured;
            const SamplerState*      mpSamplerState_Linear_MipNearest_ClampUV;
            const SamplerState*      mpSamplerState_Linear_MipNearest_TileUV;
            const SamplerState*      mpSamplerState_Nearest_MipNearest_ClampUV;
            const SamplerState*      mpSamplerState_Nearest_MipNearest_TileUV;
        };

    private:
        // The state-library builders (each sizes / allocates / initialises one render state). Only
        // the ones the X360 ARTIST attests for this TU are reconstructed; the rest stay declared.
        const BlendState*        ConstructNoAlphaTestBlendState(rw::IResourceAllocator* lpAllocator);
        const BlendState*        ConstructNoBlendNoAlphaTestBlendState(rw::IResourceAllocator* lpAllocator);
        // X360 @ 0x827ED118. Build a parameterised blend state using 'this' as the ResourceAllocator.
        // luMode = 5-bit source-blend enum (v8[0] bits 0..4); liBlendA / liBlendB feed the
        // blend-op and destination-factor via the per-channel param word.
        const BlendState*        ConstructBlendState(u8 luMode, int liBlendA, int liBlendB);
        const RasterizerState*   ConstructRasteriserState(
            rw::IResourceAllocator* lpAllocator, renderengine::RasterizerState::CullMode leCullMode);
        const DepthStencilState* ConstructDepthStencilState(
            rw::IResourceAllocator* lpAllocator, bool lbDepthTestEnable, bool lbDepthWriteEnable,
            renderengine::DepthStencilState::Function leFunction);
        const TextureState*      ConstructDefaultTextureState(
            rw::IResourceAllocator* lpAllocator, renderengine::Texture* lpTexture);

        // The shared low-level shadow-state setters (the single X360 bodies the typed SetState /
        // SetTexture overloads drive). Return the renderer (X360 r3 passthrough) for the chained
        // submission path; the high-level overloads ignore the result.
        void* SetStateLowLevel(const void* lpState);

    public:
        // ---- module statics (X360 .data home of this TU) ----------------------------------------
        static ImRendererBase*        mgpActiveRenderer;          // dword_83010F9C
        static const void*            mgpLastState;               // dword_83010964 (last state set)
        static renderengine::Texture* mgpLastTexture;             // dword_830109E8 (last texture set)
        static u32                    mgbTextureStateDirty;       // dword_83010968
        static bool                   mgbStateShadowingDisabled;  // byte_83010907
        static void*                  mgpDevice;                  // off_83271608 (the D3D device)
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

        // Push a "set vertex/pixel program" command into the open render-buffer batch
        // (X360 CgsGraphics::ImRenderBuffer<V>::SetProgram @ 0x824590B8 for the
        // <Basic2dColouredTexturedVertex> instance, called by BrnFlapt::FlaptRenderer::SetShader/
        // RenderMesh/RenderTextField). Must be inside a BeginRendering/EndRendering block; the byte
        // program id is appended as a command word. The X360 ImRenderBuffer carries this command API
        // at +4 of the polymorphic buffer; on the PC fold (Im2dRenderBuffer == Im2d : ImRenderer<V>)
        // it is an ordinary base-class method reached by name.
        void SetProgram(s8 lcProgramId);
    };
}

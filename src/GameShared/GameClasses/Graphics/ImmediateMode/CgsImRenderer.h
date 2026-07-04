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
    class VertexDescriptor;     // the per-stream vertex-format descriptor (renderengine/VertexDescriptor.h)
    class ProgramBuffer;        // a single compiled vertex/pixel program (renderengine states/programbuffer.h)
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

    protected:
        // Build the shared render-state library exactly once (DWARF CgsImRenderer.h:170; X360 home is
        // CgsImRenderer.cpp -- declaration-only here, the dep TU bodies it). The typed ImRenderer<V>::
        // Construct paths call this behind a module-static guard the first time any renderer is built.
        void ConstructOnceOnly(rw::IResourceAllocator* lpAllocator);

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

    // The maximum number of vertex/pixel program pairs an immediate-mode renderer can carry
    // (X360 CgsImRenderer.h:241 -- KI8_MAX_PROGRAMS). Sizes the two per-renderer program tables.
    const s8 KI8_MAX_PROGRAMS = 8;

    template <typename V>
    struct ImRenderer : public ImRendererBase
    {
        void BeginRendering();
        // X360-attested signed-char overload (DWARF CgsImRenderer.h attests BOTH `void BeginRendering();`
        // and `void BeginRendering(signed char);` on the ImRenderer<V> template). Bodied per vertex type
        // (the LionBlend path @ X360 0x8227C3A8 supplies the LionBlendVertex instantiation). Distinct from
        // the no-arg PC-fold BeginRendering().
        void BeginRendering(s8 li8Program);
        void EndRendering();
        void Render(renderengine::PrimitiveType lePrimitiveType, const V* lpVertices, u32 luCount);

        // ---- the X360 program-table API (CgsImRenderer.h DWARF; bodied per vertex type) ---------
        // The X360 immediate-mode renderer owns a vertex descriptor for its vertex format and a pair
        // of program tables (one vertex + one pixel program per "program slot"). Construct sizes the
        // descriptor + uploads the supplied program binaries; SetProgram binds a slot's pair on the
        // device; SetTransform writes the world transform into the slot's shader-state block. These
        // are the X360 bodies (Im3dZOnly path); the PC 2D fold above does not drive them.
        //
        // lapVertexProgramBinary / lapPixelProgramBinary are parallel arrays of li8NumberPrograms
        // program-binary pointers; lauVertexProgramSize / lauPixelProgramSize their byte sizes.
        void Construct(rw::IResourceAllocator* lpAllocator,
                       const void* const* lapVertexProgramBinary,
                       const u32* lauVertexProgramSize,
                       const void* const* lapPixelProgramBinary,
                       const u32* lauPixelProgramSize,
                       s8 li8NumberPrograms);

        // Add one vertex/pixel program pair to the next free slot; returns the slot index.
        s8 AddProgram(rw::IResourceAllocator* lpAllocator,
                      const void* lpVertexProgramBinary, u32 luVertexProgramSize,
                      const void* lpPixelProgramBinary, u32 luPixelProgramSize);

        // Bind program slot li8Program's vertex+pixel programs on the device (shadow-cached on the
        // vertex program). Returns true when the bind actually changed the device state.
        bool SetProgram(s8 li8Program);

        // Write the world transform (a 4x4 matrix at lpTransform) into the current program slot's
        // device shader-state block. Returns the renderengine shader-state result (X360 r3).
        void* SetTransform(const void* lpTransform);

    protected:
        // ---- X360 program-table state (DWARF CgsImRenderer.h:302-308) ---------------------------
        // The PC 2D fold (CgsIm2d.cpp) never touches these; they exist so the X360 Im3dZOnly bodies
        // can reach the descriptor / program tables / current-slot BY NAME. The X360 build also
        // carries two heavy renderengine::Device::DirectDraw state blocks (mDirectDrawParameters /
        // mDirectDrawBatchIterator) ahead of the descriptor; those are out-of-scope (blocked
        // class:renderengine::Device) and the PC reconstruction reaches every field by name, so the
        // absolute X360 byte offsets (descriptor @ +0x10, vertex table @ +0x14, pixel table @ +0x34,
        // current slot @ +0x54) are NOT modelled with padding here (they are not LLP64-invariant).
        renderengine::VertexDescriptor*  mpVertexDescriptor;
        renderengine::ProgramBuffer*     mapVertexProgramBuffer[KI8_MAX_PROGRAMS];
        renderengine::ProgramBuffer*     mapPixelProgramBuffer[KI8_MAX_PROGRAMS];
        s8                               mi8CurrentProgram;
        // X360-only Im3d state, reached BY NAME by the Im3dZOnly bodies (the PC 2D fold never touches
        // these). The per-program device shader-state blocks SetTransform indexes (X360
        // `this + 4*(mi8CurrentProgram+22)` == &maShaderStateBlocks[slot]) and the persistent world-
        // transform store SetTransform writes (X360 stvx128 into this+0x80). Absolute offsets are not
        // LLP64-invariant so they are reached by name, not pinned with padding.
        u32                              maShaderStateBlocks[KI8_MAX_PROGRAMS];
        u8                               mauTransform[64];

    public:

        // The double-buffer "reserve / submit" API the X360 text path drives (folded onto the PC
        // immediate renderer): RenderStart reserves luVertexCount vertices and returns the write
        // pointer; the caller fills it; RenderEnd submits the run as one primitive. Bodies in
        // CgsIm2d.cpp (PC: a scratch vertex run drawn via DrawPrimitiveUP).
        V*   RenderStart(u32 luVertexCount);
        void RenderEnd(renderengine::PrimitiveType lePrimitiveType, const V* lpVertices, u32 luVertexCount);
    };
}

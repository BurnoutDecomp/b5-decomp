#pragma once
#include "SDKs/RenderEngineClub/MAIN/components/src/states/depthstencilstate.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/rasterizerstate.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/vertexdescriptor.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"

#include "types.hpp"
namespace renderengine
{
    class Texture;
    class TextureState;
    enum PrimitiveType;   // platform primitive-topology enum (external API)
    class VertexDescriptor;     // the per-stream vertex-format descriptor (renderengine/VertexDescriptor.h)
    class ProgramBuffer;        // a single compiled vertex/pixel program (renderengine states/programbuffer.h)
}

namespace rw { struct IResourceAllocator; }
namespace BrnGame { struct LoadingScreenRenderer; }
class cParticleRender;

namespace CgsGraphics
{
    struct OcclusionCullManager;

    class BlendState;
    class DepthStencilState;
    class RasterizerState;
    class RenderTargetState;
    class SamplerState;
    class TextureState;
    class ProgramBuffer;

    struct ImRendererBase
    {
        friend struct BrnGame::LoadingScreenRenderer;
        friend class ::cParticleRender;

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
        void SetState(const renderengine::TextureState* lpTextureState);
        void SetState(const renderengine::TextureState* lpTextureState, u32 luStage);

        void SetTexture(renderengine::Texture* lpTexture);
        void SetTexture(renderengine::Texture* lpTexture, u32 luStage);

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

        // DecFIGS h88; ARTIST82854A24 reads the shared white-texture slot.
        static const StateLibrary& GetStateLibrary() { return mgStateLibrary; }

    protected:
        static StateLibrary mgStateLibrary;
        static bool mgbStateLibraryConstructed;

        static void ConstructOnceOnly(rw::IResourceAllocator* lpAllocator);

    private:
        static const BlendState* ConstructNoAlphaTestBlendState(rw::IResourceAllocator* lpAllocator);
        static const BlendState* ConstructNoBlendNoAlphaTestBlendState(rw::IResourceAllocator* lpAllocator);
        const BlendState*        ConstructBlendState(u8 luMode, int liBlendA, int liBlendB);
        static const RasterizerState* ConstructRasteriserState(
            rw::IResourceAllocator* lpAllocator, renderengine::RasterizerState::CullMode leCullMode);
        static const DepthStencilState* ConstructDepthStencilState(
            rw::IResourceAllocator* lpAllocator, bool lbDepthTestEnable, bool lbDepthWriteEnable,
            renderengine::DepthStencilState::Function leFunction);
        static const SamplerState* ConstructSamplerState(rw::IResourceAllocator* lpAllocator,
                                                         u32 luArg2, u32 luArg3,
                                                         u32 luArg4, u32 luArg5);
        static renderengine::Texture* ConstructWhiteTexture(rw::IResourceAllocator* lpAllocator);
        static const TextureState* ConstructDefaultTextureState(
            rw::IResourceAllocator* lpAllocator, renderengine::Texture* lpTexture);

        void* SetStateLowLevel(const void* lpState);

    public:
        static ImRendererBase*        mgpActiveRenderer;          // dword_83010F9C
    };

    const s8 KI8_MAX_PROGRAMS = 8;

    template <typename V>
    struct ImRenderer : public ImRendererBase
    {
        friend struct OcclusionCullManager;

        void BeginRendering();
        void BeginRendering(s8 li8Program);
        void EndRendering();
        void Render(renderengine::PrimitiveType lePrimitiveType, const V* lpVertices, u32 luCount);

        void Construct(rw::IResourceAllocator* lpAllocator,
                       const void* const* lapVertexProgramBinary,
                       const u32* lauVertexProgramSize,
                       const void* const* lapPixelProgramBinary,
                       const u32* lauPixelProgramSize,
                       s8 li8NumberPrograms);

        s8 AddProgram(rw::IResourceAllocator* lpAllocator,
                      const void* lpVertexProgramBinary, u32 luVertexProgramSize,
                      const void* lpPixelProgramBinary, u32 luPixelProgramSize);

        virtual bool SetProgram(s8 li8Program);

    protected:
        renderengine::Device::DirectDraw::Parameters mDirectDrawParameters;
        renderengine::Device::DirectDraw::BatchIterator mDirectDrawBatchIterator;
        renderengine::VertexDescriptor*  mpVertexDescriptor;
        renderengine::ProgramBuffer*     mapVertexProgramBuffer[KI8_MAX_PROGRAMS];
        renderengine::ProgramBuffer*     mapPixelProgramBuffer[KI8_MAX_PROGRAMS];
        s8                               mi8CurrentProgram;
    };
}

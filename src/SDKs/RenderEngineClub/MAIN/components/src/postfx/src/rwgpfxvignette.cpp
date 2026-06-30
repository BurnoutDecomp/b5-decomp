#include "types.hpp"

#include <cstring>   // memset

#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxvignette.h"   // Vignette
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxhelper.h"     // PfxHelper::CreateProgram
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"           // BlendState
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"        // ProgramBuffer
#include "pc/gcm/renderengine/renderstates.h"                                      // DepthStencilState / RasterizerState
#include "pc/gcm/renderengine/VertexDescriptor.h"                                  // VertexDescriptor
#include "rw/rwcore_structs.h"                                                     // rw::IResourceAllocator / Resource
#include "rw/math/vpu/types.h"                                                     // rw::math::vpu::Vector4

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::graphics::postfx::Vignette::SetState  @ 0x823FE8B0
//   rw::graphics::postfx::Vignette::Vignette  @ 0x82403DB8
//
// Vignette is the post-fx screen-border darkening effect. The constructor builds the four render
// states the full-screen quad draws through (opaque-with-blend BlendState, Position+UV
// VertexDescriptor, no-depth DepthStencilState, no-cull RasterizerState) via the standard render-engine
// create idiom (build a Parameters block -> GetResourceDescriptor sizes it -> the allocator's
// vtable+0x10 call carves the rw::Resource -> Initialize constructs in place; the same idiom
// rwgpfxhelper.cpp / rwgpfxtint.cpp use), compiles the vignette vertex + pixel programs through
// PfxHelper::CreateProgram and binds their shader-constant handles, then seeds the per-frame State.
// SetState recomputes the shader-constant control vector + colours from a State.
//
// Every literal below is the immediate the X360 asm loads. The blend parameters are the shared
// opaque-blend values rwgpfxhelper.cpp documents (0x07060706 factors, masks 15, op 7, flag 135, -1)
// with the blend-enable bit set (v36[0] |= ROL(1,10) == 0x400).

namespace
{
    // The two vignette shader microcode blobs embedded in the X360 image (unk_82044E88, 288 bytes /
    // unk_82044FA8, 416 bytes) and the SetState gradient-table base (unk_82040000). HONEST
    // PLACEHOLDERS: compiled Xenos shader / GPU bytecode -- platform data with no recoverable bytes from
    // the function-only exports; declared so the create calls can name them.
    extern const u8 gauVignetteVertexProgramMicrocode;   // X360 &unk_82044E88
    const u8 gauVignetteVertexProgramMicrocode = 0;
    extern const u8 gauVignettePixelProgramMicrocode;    // X360 &unk_82044FA8
    const u8 gauVignettePixelProgramMicrocode = 0;
    extern const u8 gauVignetteGradientTable;            // X360 &unk_82040000
    const u8 gauVignetteGradientTable = 0;

    const u32 KU_VIGNETTE_VERTEX_PROGRAM_SIZE = 288;     // X360 li 0x120
    const u32 KU_VIGNETTE_PIXEL_PROGRAM_SIZE  = 416;     // X360 li 0x1A0

    // The packed VertexDescriptor element words the constructor seeds (X360 immediates: v60 / v63).
    const u32 KU_VIGNETTE_VTX_ELEMENT0 = 2761657u;       // X360 0x2A23B9 (Position float3)
    const u32 KU_VIGNETTE_VTX_ELEMENT1 = 2892709u;       // X360 0x2C23A5 (UV float2)

    const f32 KF_VIGNETTE_SHARPNESS_SCALE = 500.0f;      // X360 flt_8200A034
}

namespace rw
{
namespace graphics
{
namespace postfx
{
    // The X360 SetState mapping helper (sub_82C09970): maps the [-1,1] sharpness through a 0.5-centred
    // blend and a gradient table (unk_82040000), returning the gradient parameter the control vector is
    // built from (in f1). It is an out-of-this-TU helper (no homed body), so it is declaration-only --
    // no body invented. The trailing 0.5 / 16.0 immediates are the X360 call's last two FP args.
    f32 ComputeVignetteGradient(Vignette* lpVignette, const Vignette::State* lpState, u32 luArg3, u32 luArg4,
                                u32 luArg5, const void* lpGradientTable, const void* lpScratch,
                                void* lpScratch2, u32 luArg9, u32 luArg10, f32 lfHalf, f32 lfSixteen);

    // Allocate a render-state resource through an allocator and copy the carved handle in (the X360
    // vtable+0x10 DoAllocate call + the 5-word copy loop; modelled as a single rw::Resource by value).
    static rw::Resource AllocateStateResource(rw::IResourceAllocator* lpAllocator,
                                              const rw::BaseResourceDescriptors<5>& lrDescriptor)
    {
        return lpAllocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lrDescriptor), nullptr);
    }

    // ============================================================================================
    // Vignette::SetState  @ 0x823FE8B0
    // ============================================================================================
    void Vignette::SetState(const State& lrState)
    {
        // Blend the (broadcast) inner colour by 0.5 about 0.5 (v0 = inner*0.5 + 0.5), then run the
        // gradient-mapping helper to get the sharpness gradient parameter.
        f32 lfBlended = lrState.mInnerColour.x * 0.5f + 0.5f;

        u32 luScratchA = 0;
        u32 luScratchB = 0;
        f32 lfGradient = ComputeVignetteGradient(this, &lrState, 0, 0, 0, &gauVignetteGradientTable,
                                                 &lfBlended, &luScratchA, luScratchB, 0, 0.5f, 16.0f);

        // The control vector: { 0, 0, gradient*500 + 1, -(gradient*500) }.
        const f32 lfScaled = lfGradient * KF_VIGNETTE_SHARPNESS_SCALE;
        m_state.mInnerColour.x = 0.0f;
        m_state.mInnerColour.y = 0.0f;
        m_state.mInnerColour.z = lfScaled + 1.0f;
        m_state.mInnerColour.w = -lfScaled;

        // The three colour/control vectors copy straight from the incoming State.
        m_state.mOuterColour = lrState.mOuterColour;
        m_state.mControlA    = lrState.mControlA;
        m_state.mControlB    = lrState.mControlB;

        // The sharpness float (state +0x04) is carried into the object's State sharpness slot.
        m_state.mfSharpness = lrState.mInnerColour.y;
    }

    // ============================================================================================
    // Vignette::Vignette  @ 0x82403DB8
    // ============================================================================================
    Vignette::Vignette(rw::IResourceAllocator** lppParameters)
    {
        // Zero the four render-state resource backing blocks.
        std::memset(&m_blendStateResource, 0, sizeof(m_blendStateResource));
        std::memset(&m_vertexDescriptorResource, 0, sizeof(m_vertexDescriptorResource));
        std::memset(&m_depthStencilStateResource, 0, sizeof(m_depthStencilStateResource));
        std::memset(&m_rasterizerStateResource, 0, sizeof(m_rasterizerStateResource));

        // The allocator the effect keeps (lppParameters[0]).
        mpAllocator = lppParameters[0];

        // --- the opaque-with-blend BlendState (the shared post-fx blend, with blend-enable set) -------
        renderengine::BlendStateParameters lBlendParams;
        std::memset(&lBlendParams, 0, sizeof(lBlendParams));
        lBlendParams.maBlendFactor[0] = 0x07060706u;   // 117835526
        lBlendParams.maBlendFactor[1] = 0x07060706u;
        lBlendParams.maBlendFactor[2] = 0x07060706u;
        lBlendParams.maBlendFactor[3] = 0x07060706u;
        lBlendParams.muState15        = 7;             // v36[4]
        lBlendParams.muState4         = 15;            // v36[5]
        lBlendParams.muState5         = 15;            // v36[6]
        lBlendParams.muState6         = 15;            // v36[7]
        lBlendParams.muState7         = 15;            // v36[8]
        lBlendParams.muState8         = 135;           // v36[9] = 0x87
        lBlendParams.muState17        = 0;             // v36[10]
        lBlendParams.muState9         = static_cast<u32>(-1); // v36[11]
        lBlendParams.mbHasCustomBlendFactors = 1;      // v37 = 1
        // v36[0] = ROL(1,10) | (v36[0] & 0xFFFF0000): set the low-half blend-enable bit (0x400). The
        // factor word's low half holds the blend mode/enable; the high half is the factor's high half.
        lBlendParams.maBlendFactor[0] = 0x400u | (lBlendParams.maBlendFactor[0] & 0xFFFF0000u);

        rw::BaseResourceDescriptors<5> lBlendDescriptor;
        renderengine::BlendState::GetResourceDescriptor(&lBlendDescriptor, &lBlendParams);
        m_blendStateResource = AllocateStateResource(mpAllocator, lBlendDescriptor);
        mpBlendState = renderengine::BlendState::Initialize(
            reinterpret_cast<renderengine::BlendMaterialState**>(&m_blendStateResource), &lBlendParams);

        // --- the Position+UV VertexDescriptor --------------------------------------------------------
        renderengine::VertexDescriptor::Parameters lVertexParams;
        // The two packed element words + the trailing element flags the X360 seeds (v60/v63/v64).
        std::memset(&lVertexParams, 0, sizeof(lVertexParams));
        *reinterpret_cast<u32*>(&lVertexParams.maElements[0].miOffset) = KU_VIGNETTE_VTX_ELEMENT0;
        *reinterpret_cast<u32*>(&lVertexParams.maElements[1].miOffset) = KU_VIGNETTE_VTX_ELEMENT1;
        lVertexParams.maElementFlags[0] = 6;   // v64 = 6

        rw::BaseResourceDescriptors<5> lVertexDescriptor;
        renderengine::VertexDescriptor::GetResourceDescriptor(&lVertexDescriptor, &lVertexParams);
        m_vertexDescriptorResource = AllocateStateResource(mpAllocator, lVertexDescriptor);
        mpVertexDescriptor = renderengine::VertexDescriptor::Initialize(
            reinterpret_cast<rw::Resource*>(&m_vertexDescriptorResource), &lVertexParams);

        // --- the no-depth DepthStencilState ----------------------------------------------------------
        renderengine::DepthStencilState::Parameters lDepthParams;
        std::memset(&lDepthParams, 0, sizeof(lDepthParams));
        lDepthParams.muFunction        = 3;             // v44[0] = 3
        lDepthParams.muState4          = 7;             // v44[4] = 7 (E_FUNCTION_ALWAYS)
        lDepthParams.muState8          = 7;             // v44[8] = 7
        lDepthParams.muStencilReadMask = static_cast<u32>(-1);  // v44[11] = -1
        lDepthParams.muStencilWriteMask= static_cast<u32>(-1);  // v44[12] = -1
        lDepthParams.muState14         = static_cast<u32>(-1);  // v44[14] = -1
        lDepthParams.muState15         = static_cast<u32>(-1);  // v44[15] = -1

        rw::BaseResourceDescriptors<5> lDepthDescriptor;
        renderengine::DepthStencilState::GetResourceDescriptor(
            reinterpret_cast<renderengine::ResourceDescriptor5*>(&lDepthDescriptor), &lDepthParams);
        m_depthStencilStateResource = AllocateStateResource(mpAllocator, lDepthDescriptor);
        mpDepthStencilState = renderengine::DepthStencilState::Initialize(
            reinterpret_cast<renderengine::DepthStencilState**>(&m_depthStencilStateResource), &lDepthParams);

        // --- the no-cull RasterizerState -------------------------------------------------------------
        renderengine::RasterizerState::Parameters lRasterParams;
        std::memset(&lRasterParams, 0, sizeof(lRasterParams));
        lRasterParams.muFillMode               = 0;     // v30[0] = 0
        lRasterParams.muCullMode               = 0;     // v30[1] = 0
        lRasterParams.muMultisampleEnable      = static_cast<u32>(-1);     // v30[4] = -1 (asm stw r26 @0x82404088)
        lRasterParams.muAntialiasedLineEnable  = static_cast<u32>(0xFFFF); // v30[5] = 0xFFFF
        lRasterParams.mu8DepthClipEnable       = 1;     // v32 = 1
        lRasterParams.mu8FrontCounterClockwise = 1;     // v33 = 1
        lRasterParams.mu8PaddingMode           = 1;     // v35 = 1

        rw::BaseResourceDescriptors<5> lRasterDescriptor;
        renderengine::RasterizerState::GetResourceDescriptor(
            reinterpret_cast<renderengine::ResourceDescriptor5*>(&lRasterDescriptor), &lRasterParams);
        m_rasterizerStateResource = AllocateStateResource(mpAllocator, lRasterDescriptor);
        mpRasterizerState = renderengine::RasterizerState::Initialize(
            reinterpret_cast<renderengine::RasterizerState**>(&m_rasterizerStateResource), &lRasterParams);

        // --- compile the vignette vertex + pixel programs and bind their shader-constant handles ------
        mpVertexProgram = PfxHelper::CreateProgram(0, &gauVignetteVertexProgramMicrocode,
                                                   KU_VIGNETTE_VERTEX_PROGRAM_SIZE, mpAllocator);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            mpVertexProgram, reinterpret_cast<const u8*>("uvOffset"), &mUvOffsetHandle);

        mpPixelProgram = PfxHelper::CreateProgram(1, &gauVignettePixelProgramMicrocode,
                                                  KU_VIGNETTE_PIXEL_PROGRAM_SIZE, mpAllocator);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            mpPixelProgram, reinterpret_cast<const u8*>("controlScalars"), &mControlScalarsHandle);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            mpPixelProgram, reinterpret_cast<const u8*>("innerColour"), &mInnerColourHandle);
        renderengine::ProgramBuffer::GetVariableHandleByName(
            mpPixelProgram, reinterpret_cast<const u8*>("outerColour"), &mOuterColourHandle);

        // Seed the per-frame State from the initial State (lppParameters[1..]).
        SetState(*reinterpret_cast<const State*>(lppParameters + 1));
    }
}
}
}

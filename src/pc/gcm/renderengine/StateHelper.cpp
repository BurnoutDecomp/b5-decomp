#include "renderstates.h"
#include "device.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"  // renderengine::BlendState
#include "rw/rwcore_structs.h"  // rw::ResourceAllocatorRegistry / rw::IResourceAllocator / rw::Resource

#include <cstring>  // std::memcpy

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   renderengine::StateHelper::Initialize                                  @ 0x82B64108
//   renderengine::StateHelper::ResizeDefaultScissorRectAndViewportParameters @ 0x82B63D30
//
// The render-engine's default render-state table. At boot (BrnRendererModule::Construct) the X360
// builds a fixed set of default state objects -- four blend states, one depth/stencil state, one
// rasterizer state -- by sizing each through the state class's GetResourceDescriptor, carving the
// backing store through the RenderWare default resource allocator, and calling the state class's
// Initialize. The resulting handles are stashed in file-scope default-state slots (the X360 globals
// dword_8327187C..dword_832718D4). Then, if the device is ready, it builds the default full-screen
// scissor rectangle + viewport parameter block from the current display resolution.

namespace renderengine
{
namespace
{
    // The default-state handle slots (X360 file-scope globals). Kept here so the table is referenced
    // by name. Each holds the handle returned by the corresponding state class's Initialize.
    BlendMaterialState* gpDefaultBlendState0 = nullptr;   // X360 dword_8327187C  (factor0 0x07060706)
    BlendMaterialState* gpDefaultBlendState1 = nullptr;   // X360 dword_83271880  (factor0 0x07060101)
    BlendMaterialState* gpDefaultBlendState2 = nullptr;   // X360 dword_83271888  (factor0 0x07060109)
    BlendMaterialState* gpDefaultBlendState3 = nullptr;   // X360 dword_83271884  (factor0 0x07060706)
    DepthStencilState*  gpDefaultDepthStencilState = nullptr; // X360 dword_832718D0
    RasterizerState*    gpDefaultRasterizerState = nullptr;   // X360 dword_832718D4

    // The default full-screen scissor rectangle (X360 dword_832718D8..dword_832718E4): {left, top,
    // right, bottom} = {0, 0, width, height}.
    s32 gDefaultScissorLeft   = 0;   // X360 dword_832718D8
    s32 gDefaultScissorTop    = 0;   // X360 dword_832718DC
    s32 gDefaultScissorRight  = 0;   // X360 dword_832718E0
    s32 gDefaultScissorBottom = 0;   // X360 dword_832718E4

    // The default full-screen viewport parameter block (X360 unk_82F87B64, a 7-float / 28-byte block
    // memcpy'd in by ResizeDefaultScissorRectAndViewportParameters):
    //   [0] x, [1] y, [2] width, [3] height, [4] minZ, [5] maxZ, [6] (reserved).
    f32 gaDefaultViewport[7] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    // -----------------------------------------------------------------------------------------------
    // Carve the backing store for one render-state resource through the RenderWare default allocator.
    // The X360 sizes the resource via the state class's GetResourceDescriptor (slot-0 {size, align}),
    // fetches rw::ResourceAllocatorRegistry::GetDefaultAllocator(), then calls the allocator's Alloc
    // (vtable slot +0x10). On PC the same carve goes through the allocator's DoAllocate with a
    // descriptor whose slot 0 is {size, align} and the remaining slots {0, 1}; the carved resource's
    // first base resource is the state-object storage. Returns that storage pointer.
    void* AllocateStateResource(const renderengine::ResourceDescriptor5::Entry& lrEntry0)
    {
        rw::IResourceAllocator* lpAllocator = rw::ResourceAllocatorRegistry::GetDefaultAllocator();

        rw::ResourceDescriptor lAllocDescriptor;
        lAllocDescriptor.m_baseResourceDescriptors[0].m_size      = lrEntry0.muSize;
        lAllocDescriptor.m_baseResourceDescriptors[0].m_alignment = lrEntry0.muAlignment;
        lAllocDescriptor.m_baseResourceDescriptors[1].m_size      = 0u;
        lAllocDescriptor.m_baseResourceDescriptors[1].m_alignment = 1u;
        lAllocDescriptor.m_baseResourceDescriptors[2].m_size      = 0u;
        lAllocDescriptor.m_baseResourceDescriptors[2].m_alignment = 1u;
        lAllocDescriptor.m_baseResourceDescriptors[3].m_size      = 0u;
        lAllocDescriptor.m_baseResourceDescriptors[3].m_alignment = 1u;

        rw::Resource lResource = lpAllocator->DoAllocate(lAllocDescriptor, nullptr);
        return lResource.m_baseResources[0];
    }

    // Fill the default-blend parameter block. Every default blend state shares the same body; only the
    // first blend-factor word and the custom-factor flag differ between them (the X360 edits the first
    // factor word with an insrwi into the high 16 bits and stores 0x07060706 in the other three).
    void BuildDefaultBlendParameters(BlendStateParameters* lpParams, u32 luBlendFactor0, u8 lbCustom)
    {
        lpParams->maBlendFactor[0] = luBlendFactor0;
        lpParams->maBlendFactor[1] = 0x07060706u;
        lpParams->maBlendFactor[2] = 0x07060706u;
        lpParams->maBlendFactor[3] = 0x07060706u;
        lpParams->muState15 = 7u;     // word4
        lpParams->muState4  = 15u;    // word5
        lpParams->muState5  = 15u;    // word6
        lpParams->muState6  = 15u;    // word7
        lpParams->muState7  = 15u;    // word8
        lpParams->muState8  = 135u;   // word9 (0x87)
        lpParams->muState17 = 0u;     // word10
        lpParams->muState9  = 0xFFFFFFFFu; // word11 (-1)
        lpParams->mbHasCustomBlendFactors = lbCustom;
        lpParams->mbState10 = 0;
        lpParams->mbState11 = 0;
        lpParams->mbState12 = 0;
        lpParams->mbState13 = 0;
        lpParams->mbState14 = 0;
        lpParams->mbState16 = 0;
    }

    BlendMaterialState* CreateDefaultBlendState(const BlendStateParameters* lpParams)
    {
        renderengine::ResourceDescriptor5 lDescriptor;
        BlendState::GetResourceDescriptor(&lDescriptor, lpParams);
        BlendMaterialState* lpState =
            static_cast<BlendMaterialState*>(AllocateStateResource(lDescriptor.maEntries[0]));
        BlendState::Initialize(&lpState, lpParams);
        return lpState;
    }
}

int StateHelper::Initialize()
{
    // ---- Four default blend states ----
    BlendStateParameters lBlendParams;

    BuildDefaultBlendParameters(&lBlendParams, 0x07060706u, /*custom*/ 0);
    gpDefaultBlendState0 = CreateDefaultBlendState(&lBlendParams);

    // asm 0x82B64230: li r10,0x101 + insrwi r11,r10,16,16 overwrites the LOW half of the base
    // factor word 0x07060706 with 0x0101 -> 0x07060101 (Hex-Rays v32[0]=117833985).
    BuildDefaultBlendParameters(&lBlendParams, 0x07060101u, /*custom*/ 1);
    gpDefaultBlendState1 = CreateDefaultBlendState(&lBlendParams);

    // asm 0x82B642B8: li r9,0x109 + insrwi -> 0x07060109 (Hex-Rays v24[0]=117833993).
    BuildDefaultBlendParameters(&lBlendParams, 0x07060109u, /*custom*/ 1);
    gpDefaultBlendState2 = CreateDefaultBlendState(&lBlendParams);

    BuildDefaultBlendParameters(&lBlendParams, 0x07060706u, /*custom*/ 1);
    gpDefaultBlendState3 = CreateDefaultBlendState(&lBlendParams);

    // ---- Default depth/stencil state ----
    DepthStencilState::Parameters lDepthParams = {};
    lDepthParams.muFunction        = 3u;                         // word0
    lDepthParams.muState4          = DepthStencilState::E_FUNCTION_ALWAYS; // word4 == 7
    lDepthParams.muState8          = DepthStencilState::E_FUNCTION_ALWAYS; // word8 == 7
    lDepthParams.muStencilReadMask = 0xFFFFFFFFu;                // word11 == -1
    lDepthParams.muStencilWriteMask= 0xFFFFFFFFu;                // word12 == -1
    lDepthParams.muState14         = 0xFFFFFFFFu;                // word14 == -1
    lDepthParams.muState15         = 0xFFFFFFFFu;                // word15 == -1
    lDepthParams.mbDepthTestEnable = 1;
    lDepthParams.mbDepthWriteEnable= 1;

    renderengine::ResourceDescriptor5 lDepthDescriptor;
    DepthStencilState::GetResourceDescriptor(&lDepthDescriptor, &lDepthParams);
    DepthStencilState* lpDepthState =
        static_cast<DepthStencilState*>(AllocateStateResource(lDepthDescriptor.maEntries[0]));
    gpDefaultDepthStencilState = DepthStencilState::Initialize(&lpDepthState, &lDepthParams);

    // ---- Default rasterizer state ----
    RasterizerState::Parameters lRasterParams = {};
    lRasterParams.muFillMode             = 0u;
    lRasterParams.muCullMode             = 2u;
    lRasterParams.muDepthBias            = 0u;          // X360 stores 0.0f bits
    lRasterParams.muSlopeScaledDepthBias = 0u;          // X360 stores 0.0f bits
    lRasterParams.muMultisampleEnable    = 0xFFFFFFFFu; // word4 == -1
    lRasterParams.muAntialiasedLineEnable= 0xFFFFu;     // word5 == 0xFFFF
    lRasterParams.mu8ScissorEnable         = 0;
    lRasterParams.mu8DepthClipEnable       = 1;
    lRasterParams.mu8FrontCounterClockwise = 1;
    lRasterParams.mu8ConservativeRaster    = 0;
    lRasterParams.mu8PaddingMode           = 1;

    renderengine::ResourceDescriptor5 lRasterDescriptor;
    RasterizerState::GetResourceDescriptor(&lRasterDescriptor, &lRasterParams);
    RasterizerState* lpRasterState =
        static_cast<RasterizerState*>(AllocateStateResource(lRasterDescriptor.maEntries[0]));
    gpDefaultRasterizerState = RasterizerState::Initialize(&lpRasterState, &lRasterParams);

    // The X360 gates the default scissor/viewport build on the device being ready (dword_83271600 == 2).
    // [PC: the device-ready condition is the live D3D device having been created.]
    if (renderengine::gDevice != nullptr)
    {
        ResizeDefaultScissorRectAndViewportParameters();
        return 1;
    }
    return 0;
}

int StateHelper::ResizeDefaultScissorRectAndViewportParameters()
{
    // The X360 unpacks the current display resolution from a packed word (dword_83271124):
    //   width  = (word >> 18) + 1
    //   height = ((word >> 3) & 0x7FFF) + 1
    // [PC: the resolution is held decoded in renderengine::gDisplayWidth / gDisplayHeight.]
    const s32 liWidth  = renderengine::gDisplayWidth;
    const s32 liHeight = renderengine::gDisplayHeight;

    // Build the default full-screen viewport parameter block (7 floats / 28 bytes):
    //   x=0, y=0, width, height, minZ=0, maxZ=1, reserved=0.
    f32 laViewport[7];
    laViewport[0] = 0.0f;
    laViewport[1] = 0.0f;
    laViewport[2] = static_cast<f32>(liWidth);
    laViewport[3] = static_cast<f32>(liHeight);
    laViewport[4] = 0.0f;
    laViewport[5] = 1.0f;
    laViewport[6] = 0.0f;
    std::memcpy(gaDefaultViewport, laViewport, sizeof(laViewport));

    // Default scissor rectangle: {left, top, right, bottom} = {0, 0, width, height}.
    gDefaultScissorLeft   = 0;
    gDefaultScissorTop    = 0;
    gDefaultScissorRight  = liWidth;
    gDefaultScissorBottom = liHeight;
    return 0;
}
}

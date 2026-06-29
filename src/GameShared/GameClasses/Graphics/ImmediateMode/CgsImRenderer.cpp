// CgsGraphics::ImRendererBase -- the immediate-mode render layer's state-library builder and the
// low-level SetState / SetTexture shadow-state API.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   CgsGraphics::ImRendererBase::ConstructNoAlphaTestBlendState        @ 0x827ED200
//   CgsGraphics::ImRendererBase::ConstructNoBlendNoAlphaTestBlendState @ 0x827ED2E8
//   CgsGraphics::ImRendererBase::ConstructRasteriserState             @ 0x827ED3B8
//   CgsGraphics::ImRendererBase::ConstructDepthStencilState           @ 0x827ED468
//   CgsGraphics::ImRendererBase::ConstructDefaultTextureState         @ 0x827ED608
//   CgsGraphics::ImRendererBase::SetState                             @ 0x82276D08
//   CgsGraphics::ImRendererBase::SetTexture                           @ 0x82276EE8
//
// Each Construct* helper builds a render-state parameter block on the stack, sizes the state with the
// matching renderengine::*State::GetResourceDescriptor, allocates the state object through the rw
// resource allocator's virtual Create, then renderengine::*State::Initialize turns the carved memory
// into the live state. The asm is authoritative for every stored constant (the param words); the
// renderengine state types + their factory calls are the dependency homes (renderstates.h /
// blendstate.h). SetState / SetTexture are the X360 low-level shadow-state setters that cache the last
// state / texture in the module statics and only re-issue the device call on a change.

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "pc/gcm/renderengine/renderstates.h"
#include "pc/gcm/renderengine/texture.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"
#include "rw/rwcore_structs.h"

#include <cstring>   // memset

// The X360 D3D texture-bind intrinsic the low-level SetTexture path issues (binds a texture to a
// sampler stage on the device). Platform API: declared here so the body links against the X360 /
// platform D3D layer; no project header homes it.
extern "C" unsigned int D3DDevice_SetTexture(void* lpDevice, int liSampler, void* lpTexture, unsigned int luFlags);

namespace CgsGraphics
{
namespace
{
    // The rw resource allocator's Create slot the X360 immediate-mode builder dispatches through
    // (vtable +0x10): given the sized descriptor it carves the five state-handle slots the matching
    // *State::Initialize then turns into the live state. The immediate-mode library only ever reaches
    // the allocator through this one virtual, so it is modelled by name (same shape as the rasterizer
    // state factory's allocator interface).
    class ResourceAllocator
    {
    public:
        virtual void* Create(
            void* lpStateHandlesOut,
            ResourceAllocator* lpAllocator,
            const void* lpDescriptor,
            int liFlags) = 0;
    };

    // The blend-state factor word the asm splats across all four channels (0x07060706); a single
    // packed RGBA blend-factor descriptor word the X360 GPU consumes.
    const u32 KU_BLEND_FACTOR_WORD = 0x07060706u;
}

// ---------------------------------------------------------------------------------------------------
// CgsGraphics::ImRendererBase::ConstructNoAlphaTestBlendState  @ 0x827ED200
// Build the "no alpha test" blend state: the standard premultiplied blend with alpha-test disabled
// (the asm differs from the no-blend-no-alpha-test build only by maState[10] = 30 and the leading
// custom-blend-factor flag = 1).
// ---------------------------------------------------------------------------------------------------
const BlendState* ImRendererBase::ConstructNoAlphaTestBlendState(rw::IResourceAllocator* lpAllocator)
{
    renderengine::BlendStateParameters lParameters;

    lParameters.maBlendFactor[0] = KU_BLEND_FACTOR_WORD;
    lParameters.maBlendFactor[1] = KU_BLEND_FACTOR_WORD;
    lParameters.maBlendFactor[2] = KU_BLEND_FACTOR_WORD;
    lParameters.maBlendFactor[3] = KU_BLEND_FACTOR_WORD;
    lParameters.muState15 = 7;          // v5[4]
    lParameters.muState4  = 15;         // v5[5]
    lParameters.muState5  = 15;         // v5[6]
    lParameters.muState6  = 15;         // v5[7]
    lParameters.muState7  = 15;         // v5[8]
    lParameters.muState8  = 135;        // v5[9]  (0x87)
    lParameters.muState17 = 30;         // v5[10] (0x1E)
    lParameters.muState9  = 0xFFFFFFFFu;// v5[11] (-1)
    lParameters.mbHasCustomBlendFactors = 1;  // v6
    lParameters.mbState10 = 0;
    lParameters.mbState11 = 0;
    lParameters.mbState12 = 0;
    lParameters.mbState13 = 0;
    lParameters.mbState14 = 0;
    lParameters.mbState16 = 0;

    renderengine::BlendMaterialState* lapStateHandles[5] = {};
    void* lpDescriptor[12] = {};
    renderengine::BlendState::GetResourceDescriptor(lpDescriptor, &lParameters);

    ResourceAllocator* lpAllocatorIf = reinterpret_cast<ResourceAllocator*>(lpAllocator);
    lpAllocatorIf->Create(lapStateHandles, lpAllocatorIf, lpDescriptor, 0);

    return reinterpret_cast<const BlendState*>(
        renderengine::BlendState::Initialize(lapStateHandles, &lParameters));
}

// ---------------------------------------------------------------------------------------------------
// CgsGraphics::ImRendererBase::ConstructNoBlendNoAlphaTestBlendState  @ 0x827ED2E8
// Build the "no blend, no alpha test" blend state (opaque write of all four channels).
// ---------------------------------------------------------------------------------------------------
const BlendState* ImRendererBase::ConstructNoBlendNoAlphaTestBlendState(rw::IResourceAllocator* lpAllocator)
{
    renderengine::BlendStateParameters lParameters;

    lParameters.maBlendFactor[0] = KU_BLEND_FACTOR_WORD;
    lParameters.maBlendFactor[1] = KU_BLEND_FACTOR_WORD;
    lParameters.maBlendFactor[2] = KU_BLEND_FACTOR_WORD;
    lParameters.maBlendFactor[3] = KU_BLEND_FACTOR_WORD;
    lParameters.muState15 = 7;          // v5[4]
    lParameters.muState4  = 15;         // v5[5]
    lParameters.muState5  = 15;         // v5[6]
    lParameters.muState6  = 15;         // v5[7]
    lParameters.muState7  = 15;         // v5[8]
    lParameters.muState8  = 135;        // v5[9]  (0x87)
    lParameters.muState17 = 0;          // v5[10]
    lParameters.muState9  = 0xFFFFFFFFu;// v5[11] (-1)
    lParameters.mbHasCustomBlendFactors = 0;  // v6
    lParameters.mbState10 = 0;
    lParameters.mbState11 = 0;
    lParameters.mbState12 = 0;
    lParameters.mbState13 = 0;
    lParameters.mbState14 = 0;
    lParameters.mbState16 = 0;

    renderengine::BlendMaterialState* lapStateHandles[5] = {};
    void* lpDescriptor[12] = {};
    renderengine::BlendState::GetResourceDescriptor(lpDescriptor, &lParameters);

    ResourceAllocator* lpAllocatorIf = reinterpret_cast<ResourceAllocator*>(lpAllocator);
    lpAllocatorIf->Create(lapStateHandles, lpAllocatorIf, lpDescriptor, 0);

    return reinterpret_cast<const BlendState*>(
        renderengine::BlendState::Initialize(lapStateHandles, &lParameters));
}

// ---------------------------------------------------------------------------------------------------
// CgsGraphics::ImRendererBase::ConstructRasteriserState  @ 0x827ED3B8
// Build a rasterizer state with the requested cull mode (the rest is the immediate-mode default:
// solid fill, no depth bias, scissor disabled, depth-clip on, front-counter-clockwise).
// ---------------------------------------------------------------------------------------------------
const RasterizerState* ImRendererBase::ConstructRasteriserState(
    rw::IResourceAllocator* lpAllocator, renderengine::RasterizerState::CullMode leCullMode)
{
    renderengine::RasterizerState::Parameters lParameters;

    lParameters.muFillMode               = 0;                 // v5[0]
    lParameters.muCullMode               = static_cast<u32>(leCullMode);  // v5[1] = a2
    lParameters.muDepthBias              = 0;                 // v5[2] = 0.0f
    lParameters.muSlopeScaledDepthBias   = 0;                 // v5[3] = 0.0f
    lParameters.muMultisampleEnable      = 0xFFFFFFFFu;       // v5[4] = -1
    lParameters.muAntialiasedLineEnable  = 0x0000FFFFu;       // v5[5] = 0xFFFF
    lParameters.mu8ScissorEnable         = 0;                 // v6
    lParameters.mu8DepthClipEnable       = 1;                 // v7
    lParameters.mu8FrontCounterClockwise = 1;                 // v8
    lParameters.mu8ConservativeRaster    = 0;                 // v9
    lParameters.mu8PaddingMode           = 1;                 // v10

    renderengine::ResourceDescriptor5 lDescriptor;
    renderengine::RasterizerState* lapStateHandles[8] = {};
    renderengine::RasterizerState::GetResourceDescriptor(&lDescriptor, &lParameters);

    ResourceAllocator* lpAllocatorIf = reinterpret_cast<ResourceAllocator*>(lpAllocator);
    lpAllocatorIf->Create(lapStateHandles, lpAllocatorIf, &lDescriptor, 0);

    return reinterpret_cast<const RasterizerState*>(
        renderengine::RasterizerState::Initialize(lapStateHandles, &lParameters));
}

// ---------------------------------------------------------------------------------------------------
// CgsGraphics::ImRendererBase::ConstructDepthStencilState  @ 0x827ED468
// Build a depth/stencil state from the requested depth-test / depth-write enables and comparison
// function. The stencil masks default to all-ones; the func words default to "always pass".
// ---------------------------------------------------------------------------------------------------
const DepthStencilState* ImRendererBase::ConstructDepthStencilState(
    rw::IResourceAllocator* lpAllocator, bool lbDepthTestEnable, bool lbDepthWriteEnable,
    renderengine::DepthStencilState::Function leFunction)
{
    renderengine::DepthStencilState::Parameters lParameters;

    lParameters.muFunction = static_cast<u32>(leFunction);  // v7[0] = a4
    std::memset(lParameters.maState1, 0, sizeof(lParameters.maState1));  // v7[1..3]
    lParameters.muState4  = renderengine::DepthStencilState::E_FUNCTION_ALWAYS;  // v7[4] = 7
    std::memset(lParameters.maState5, 0, sizeof(lParameters.maState5));  // v7[5..7]
    lParameters.muState8  = renderengine::DepthStencilState::E_FUNCTION_ALWAYS;  // v7[8] = 7
    lParameters.muState9  = 0;             // v7[9]
    lParameters.muState10 = 0;             // v7[10]
    lParameters.muStencilReadMask  = 0xFFFFFFFFu;  // v7[11] = -1
    lParameters.muStencilWriteMask = 0xFFFFFFFFu;  // v7[12] = -1
    lParameters.muState13 = 0;             // v7[13]
    lParameters.muState14 = 0xFFFFFFFFu;   // v7[14] = -1
    lParameters.muState15 = 0xFFFFFFFFu;   // v7[15] = -1
    lParameters.muState16 = 0;             // v7[16]
    lParameters.mbDepthTestEnable  = static_cast<u8>(lbDepthTestEnable);   // v8 = a2
    lParameters.mbDepthWriteEnable = static_cast<u8>(lbDepthWriteEnable);  // v9 = a3
    lParameters.mu8Flag2 = 0;              // v10
    lParameters.mu8Flag3 = 0;              // v11
    lParameters.mu8Flag4 = 0;              // v12
    lParameters.mu8Flag5 = 0;              // v13

    renderengine::ResourceDescriptor5 lDescriptor;
    renderengine::DepthStencilState* lapStateHandles[8] = {};
    renderengine::DepthStencilState::GetResourceDescriptor(&lDescriptor, &lParameters);

    ResourceAllocator* lpAllocatorIf = reinterpret_cast<ResourceAllocator*>(lpAllocator);
    lpAllocatorIf->Create(lapStateHandles, lpAllocatorIf, &lDescriptor, 0);

    return reinterpret_cast<const DepthStencilState*>(
        renderengine::DepthStencilState::Initialize(lapStateHandles, &lParameters));
}

// ---------------------------------------------------------------------------------------------------
// CgsGraphics::ImRendererBase::ConstructDefaultTextureState  @ 0x827ED608
// Build the default texture (sampler) state for the supplied texture: bilinear with max-anisotropy 13,
// clamp addressing on all axes, the trailing state flags (0,0,0,1,1) and the bound raster.
// ---------------------------------------------------------------------------------------------------
const TextureState* ImRendererBase::ConstructDefaultTextureState(
    rw::IResourceAllocator* lpAllocator, renderengine::Texture* lpTexture)
{
    renderengine::TextureState::Parameters lParameters;

    lParameters.muAddressU      = 2;   // v5[0]
    lParameters.muAddressV      = 2;   // v5[1]
    lParameters.muAddressW      = 2;   // v5[2]
    lParameters.muMagFilter     = 1;   // v5[3]
    lParameters.muMinFilter     = 1;   // v5[4]
    lParameters.muMipFilter     = 0;   // v5[5]  (memset 12 from v5[5] -> v5[5..7] = 0)
    lParameters.muField6        = 0;   // v5[6]
    lParameters.muField7        = 0;   // v5[7]
    lParameters.muMaxAnisotropy = 13;  // v5[8]  (0xD)
    lParameters.muField9        = 0;   // v5[9]
    lParameters.muField10       = 1;   // v5[10]
    lParameters.mfMipLodBias    = 0.0f;// v5[11] = 0.0
    lParameters.mfField12       = 0.0f;// v5[12] = 0.0
    lParameters.muField13       = 0;   // v5[13]  (memset 12 from v5[13] -> v5[13..15] = 0)
    lParameters.muField14       = 0;   // v5[14]
    lParameters.muField15       = 0;   // v5[15]
    lParameters.mu8Field40 = 0;        // v6
    lParameters.mu8Field41 = 0;        // v7
    lParameters.mu8Field42 = 0;        // v8
    lParameters.mu8Field43 = 1;        // v9
    lParameters.mu8Field44 = 1;        // v10
    lParameters.mpTexture  = lpTexture;// v11 = a2

    u32 lauDescriptor[12] = {};
    renderengine::TextureState::GetResourceDescriptor(lauDescriptor);

    rw::Resource* lpStateResource = nullptr;
    ResourceAllocator* lpAllocatorIf = reinterpret_cast<ResourceAllocator*>(lpAllocator);
    lpAllocatorIf->Create(&lpStateResource, lpAllocatorIf, lauDescriptor, 0);

    return reinterpret_cast<const TextureState*>(
        renderengine::TextureState::Initialize(lpStateResource, &lParameters));
}

// ---------------------------------------------------------------------------------------------------
// CgsGraphics::ImRendererBase::SetStateLowLevel  (X360 SetState @ 0x82276D08)
// Low-level render-state setter the typed SetState overloads drive: assert this is the active renderer,
// then (unless shadowing is disabled) re-issue the state to the device only if it differs from the last
// one set, caching it. Returns the renderer (X360 r3 passthrough).
// ---------------------------------------------------------------------------------------------------
void* ImRendererBase::SetStateLowLevel(const void* lpState)
{
    void* lpResult = this;

    CGS_ASSERT(mgpActiveRenderer == this, "mgpActiveRenderer == this");

    if (!mgbStateShadowingDisabled && mgpLastState != lpState)
    {
        const bool lbLastStateWasNull = (mgpLastState == nullptr);
        lpResult = shadow::Device::Xbox2SetStateLowLevelShadowed(
            const_cast<void*>(lpState), lbLastStateWasNull);
        mgpLastState = lpState;
    }

    return lpResult;
}

// ---------------------------------------------------------------------------------------------------
// CgsGraphics::ImRendererBase::SetTexture  @ 0x82276EE8
// Low-level texture setter: assert this is the active renderer, then bind the texture to sampler 0 and
// cache it, only re-issuing the device call when it changes (and clearing the dependent dirty flag).
// ---------------------------------------------------------------------------------------------------
void ImRendererBase::SetTexture(renderengine::Texture* lpTexture)
{
    CGS_ASSERT(mgpActiveRenderer == this, "mgpActiveRenderer == this");

    if (mgpLastTexture != lpTexture)
    {
        D3DDevice_SetTexture(mgpDevice, 0, lpTexture, 0x80000000u);  // bind to sampler 0
        mgpLastTexture = lpTexture;
        mgbTextureStateDirty = 0;
    }
}

// ---- module statics (X360 .data home of this TU) --------------------------------------------------
ImRendererBase*       ImRendererBase::mgpActiveRenderer        = nullptr;  // dword_83010F9C
const void*           ImRendererBase::mgpLastState             = nullptr;  // dword_83010964
renderengine::Texture* ImRendererBase::mgpLastTexture          = nullptr;  // dword_830109E8
u32                   ImRendererBase::mgbTextureStateDirty     = 0;        // dword_83010968
bool                  ImRendererBase::mgbStateShadowingDisabled = false;   // byte_83010907
void*                 ImRendererBase::mgpDevice                 = nullptr;  // off_83271608

}  // namespace CgsGraphics

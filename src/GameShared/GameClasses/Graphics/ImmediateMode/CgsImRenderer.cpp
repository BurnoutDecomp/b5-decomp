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
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"

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
        // NOT a vtable slot. Declaring this `virtual` put it at slot 0, which on the
        // rw::IResourceAllocator actually behind the reinterpret_cast is the VIRTUAL
        // DESTRUCTOR -- so the call allocated nothing and left the allocator's vptr
        // downgraded to the inert base for the rest of the run. Call the interface by
        // NAME instead; see CgsResourceAllocatorCreate.h.
        void* Create(
            void* lpStateHandlesOut,
            ResourceAllocator* /*lpAllocator*/,
            const void* lpDescriptor,
            int /*liFlags*/)
        {
            return CgsGraphics::ResourceAllocatorCreate(this, lpStateHandlesOut, lpDescriptor);
        }
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
// CgsGraphics::ImRendererBase::SetStateLowLevel   (X360 ImRendererBase::SetState @ 0x82276D08)
//
// WHAT 0x82276D08 ACTUALLY IS: the mgpActiveRenderer assert (`lwz r11, dword_83010F9C@l(r11)`
// @0x82276D24 / `cmplw cr6, r11, r3` @0x82276D28, assert string "mgpActiveRenderer == this" at
// CgsImRenderer.h:711 == line 0x2C7 @0x82276D38) followed by an INLINE EXPANSION of
// shadow::Device::SetState(const BlendState*) @0x82276A68. The expansion is exact -- gate on
// byte_83010907 (@0x82276D54 vs @0x82276A84), compare dword_83010964 (@0x82276D68 vs @0x82276A98),
// the same `addi r11, r11, 0` / `cntlzw` / `extrwi r4, r11, 1, 26` lbWasUnset decode (@0x82276D74..7C
// vs @0x82276AA4..AC), the same `bl shadow__Device__Xbox2SetStateLowLevelShadowed` (@0x82276D84 vs
// @0x82276AB0), the same write-back (@0x82276D88 vs @0x82276AB4).
//
// Its two siblings prove the shape rather than leaving it a single-case reading: 0x82276DA8 is the
// same assert (CgsImRenderer.h:732) over SetState(const DepthStencilState*) @0x82276AD0, and
// 0x82276E48 the same (CgsImRenderer.h:776) over SetState(const RasterizerState*) @0x82276B38.
// So this body is the assert plus one call, and the gate/compare/apply/cache belongs to the shadow
// device.
//
// THAT IS ALSO THE FIX FOR A SPLIT BRAIN: the block used to be duplicated here against
// ImRendererBase::mgpLastState, which is the SAME console word (dword_83010964) as the shadow block's
// m_pBlendState. Two host words for one console word means a blit's bind is invisible to the frame
// bracket's compare, so the bracket skips a bind the device actually needs. Delegating leaves one
// cache -- and one gate, since byte_83010907 moved to shadow::Device::mbBlendStateLocked with it.
//
// RETURN: the console leaves the applier's r3 in place, which Hex-Rays renders as a returned value.
// DWARF types shadow::Device::SetState `void` (source shadowingdevice.h:496), so that register is
// dead; this returns the renderer unconditionally, which is what every caller of the typed overloads
// uses. Nothing in the tree calls SetStateLowLevel today, so the change is observationally inert as
// well as correct.
// ---------------------------------------------------------------------------------------------------
void* ImRendererBase::SetStateLowLevel(const void* lpState)
{
    CGS_ASSERT(mgpActiveRenderer == this, "mgpActiveRenderer == this");

    shadow::Device::SetState(static_cast<const renderengine::BlendMaterialState*>(lpState));

    return this;
}

// ---------------------------------------------------------------------------------------------------
// CgsGraphics::ImRendererBase::SetTexture  @ 0x82276EE8
//
// The mgpActiveRenderer assert (`lwz r11, dword_83010F9C@l(r11)` @0x82276F04 / `cmplw cr6, r11, r3`
// @0x82276F08; assert string "mgpActiveRenderer == this" with line 0x384 @0x82276F18) followed by a
// SAMPLER-0 SPECIALISATION of shadow::Device::SetResource @0x82276C70. The console keeps both as
// separate bodies -- this one does not `bl` the other -- but they are the same operation on the same
// two words, and for sampler 0 they are behaviourally identical:
//
//   compare   0x82276EE8: `lwz r11, (dword_830109E8 - 0x83010950)(r31)`  @0x82276F38
//             0x82276C70: `lwzx r11, r31, r28` with r28 == base + (0x9E8-0x950), r31 == id*4
//                                                                        @0x82276CB8/BC
//   bind      0x82276EE8: D3DDevice_SetTexture(off_83271608, Sampler=0, pTexture, 0x80000000)
//                         -- `li r4, 0` @0x82276F54, `li r6, 0` + `oris r6, r6, 0x8000` @0x82276F48/50
//             0x82276C70: D3DDevice_SetTexture(off_83271608, Sampler=id, pTexture,
//                         (1ull<<63) >> (id+32))  -- `addi r11, r30, 0x20` @0x82276CC8,
//                         `extldi r10, r10, 64,63` @0x82276CD4, `srd r6, r10, r9` @0x82276CE8.
//                         For id == 0 that mask is exactly 0x80000000.
//   cache     0x82276EE8: `stw r30, (dword_830109E8 - ...)` @0x82276F64 and
//                         `stw r11(=0), (dword_83010968 - ...)` @0x82276F68
//             0x82276C70: `stwx r27, r31, r28` @0x82276CF8 and `stwx r11(=0), r31, r10` @0x82276CFC
//   device    both read off_83271608 (@0x82276F58 and @0x82276CE4) -- the same pointer.
//
// The only thing SetResource has that this does not is its own bounds assert
// ("luSamplerId < MaxTextureStates", @0x82276CA0), which cannot fire for the constant 0.
//
// WHY DELEGATE RATHER THAN KEEP THE SPECIALISATION: dword_830109E8 and dword_83010968 are
// StateBlockShadow::m_apTextures[0] and m_apTextureStates[0] (DWARF source shadowingdevice.h:668 /
// :666; the arrays run 0x9E8..0xA24 and 0x968..0x9A4). shadow::Device already models both arrays.
// Keeping ImRendererBase::mgpLastTexture / mgbTextureStateDirty gave those two console words a SECOND
// host home, so an immediate-mode texture bind was invisible to shadow::Device::SetResource's compare
// and vice versa: the shadow device would skip a rebind the device actually needed, and the
// immediate-mode path would re-issue one it did not. Same defect shape, same block, two lines below
// the mgpLastState the previous pass removed. One console word, one host home.
//
// This is the same un-inlining move the sibling SetStateLowLevel makes, in the opposite direction: the
// console open-codes one body per call site; the host keeps one body and one cache.
// ---------------------------------------------------------------------------------------------------
void ImRendererBase::SetTexture(renderengine::Texture* lpTexture)
{
    CGS_ASSERT(mgpActiveRenderer == this, "mgpActiveRenderer == this");

    // Sampler 0 -- `li r4, 0` @0x82276F54. SetResource re-derives the same 0x80000000 bind mask and
    // caches into the same two slots.
    shadow::Device::SetResource(static_cast<void*>(lpTexture), 0);
}

// ---- module statics (X360 .data home of this TU) --------------------------------------------------
ImRendererBase*       ImRendererBase::mgpActiveRenderer        = nullptr;  // dword_83010F9C
// mgpLastState / mgpLastTexture / mgbTextureStateDirty / mgbStateShadowingDisabled were second host
// homes for dword_83010964 / dword_830109E8 / dword_83010968 / byte_83010907, all of which
// shadow::Device already owns -- see CgsImRenderer.h for the address evidence. Deleted with their
// declarations.
void*                 ImRendererBase::mgpDevice                 = nullptr;  // off_83271608

}  // namespace CgsGraphics

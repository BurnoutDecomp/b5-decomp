#pragma once

#include "types.hpp"
#include "SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h"  // rw::graphics::postfx::RenderTarget / Target

// CgsRenderTarget - Burnout's facade over an EATech post-fx render target. It owns the serialised
// description of a multi-section colour/depth render target (the colour surfaces in maRenderTargets,
// the depth/stencil surface in mDepthTarget, dimensions + mip/MSAA settings) and, once Prepare()
// has built it, an rw::graphics::postfx::RenderTarget (mpRenderTarget) it forwards draw-time calls
// into. Begin/End wrap a draw pass; GetTexture / GetDepthTexture hand back the resolved textures;
// SetRenderTargetState binds the target's surfaces + viewport/scissor on the device.
//
// Shape from the DecFIGS DWARF (GameShared/GameClasses/Graphics/CgsRenderTarget.h). The X360 ARTIST
// ledger attests only the draw-side surface (Begin/End/GetTexture/GetDepthTexture/SetRenderTargetState
// + the build-side Construct/Destruct/Prepare virtuals); the many serialise-time setters the DWARF
// lists are PS3-only and left out per the X360-attestation gate. The data layout is kept in full so
// the object sizes correctly and members are reached by name.
//
// LAYOUT NOTE (offset authority = X360 asm): on the X360 image mpRenderTarget sits at +0x108 and the
// width/height scalars at +0x04/+0x08 (read directly by the attested bodies). This recon preserves
// the *semantics* with named members, not the byte offsets (x64 widens the pointers); the member
// ORDER follows the DWARF.

namespace renderengine
{
    class Texture;
    class RenderTargetState;
}

namespace rw { struct IResourceAllocator; }

// CgsRenderTarget.h:32 (DWARF) - reconstructed Burnout render-target facade.
class CgsRenderTarget
{
public:
    // CgsRenderTarget.h:34 (DWARF) - one colour or depth surface's serialised description. Pure data
    // here: the X360-attested CgsRenderTarget bodies never touch these fields (they go through the
    // built rw::graphics::postfx::RenderTarget), so only the layout is reconstructed. The setters/
    // getters the DWARF lists are PS3-serialise-side and absent from the X360 ledger.
    struct CgsRenderTargetSurface
    {
        u32                                 mu32FilterMode;     // renderengine::SamplerState::FilterMode
        bool                                mbInUse;
        bool                                mbUseSystemMemory;
        bool                                mbUseDevice;
        u32                                 mu32BufferFormat;
        u32                                 mu32TextureFormat;
        u32                                 mu32BaseEDRAM;
        s32                                 miPS3CompressionBase;
        s8                                  ms8TileIndex;
        s8                                  ms8ZCullIndex;
        s32                                 miZCullAddress;
        rw::graphics::postfx::Target*       mpTarget;           // a provided target, when not self-created
        u32                                 mu32TextureType;    // renderengine::Texture::Type
        const rw::graphics::postfx::Target* mpSharedTarget;     // shares memory with another target
        void*                               mpSharedAddress;
    };

    CgsRenderTarget();

    // Build-side virtuals (CgsResource::Type override surface). Construct creates the post-fx render
    // target from the serialised description; Prepare realises it; Destruct tears it down. Bodies live
    // alongside the draw-side ones in this TU.
    virtual void Construct(rw::IResourceAllocator* lpAllocator);
    virtual void Destruct();
    virtual bool Prepare();

    // --- the X360-attested draw-side surface -------------------------------------------------------
    // Begin/End a draw pass into the post-fx render target (forward to RenderTarget::Begin(0) /
    // End(true)).
    void Begin();
    void End();

    // The resolved colour texture for colour section luIndex, and the resolved depth/stencil texture.
    renderengine::Texture* GetTexture(u32 luIndex);
    renderengine::Texture* GetDepthTexture();

    // Bind this render target's surfaces (section 0's render-target state) on the device and set the
    // viewport + scissor to the target's full extent.
    void SetRenderTargetState(u32 luSection);

private:
    u32  mu32MaxWidth;
    u32  mu32MaxHeight;
    u32  mu32MaxNumMipMaps;
    s32  mn32MultiSampleFormat;
    u8   mu8NumSections;
    bool mbUseDepthStencilAsTexture;

    CgsRenderTargetSurface maRenderTargets[5];   // up to 5 colour sections
    CgsRenderTargetSurface mDepthTarget;          // the depth/stencil section

    // The built post-fx render target (DWARF type RenderTarget*; X360 +0x108). Null until Prepare().
    rw::graphics::postfx::RenderTarget* mpRenderTarget;
    rw::IResourceAllocator*             mpAllocator;
};

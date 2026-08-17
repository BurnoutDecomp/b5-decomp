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
// ledger attests the draw-side surface (Begin/End/GetTexture/GetDepthTexture/SetRenderTargetState)
// and all three build-side virtuals; the many serialise-time setters the DWARF lists are PS3-only and
// left out per the X360-attestation gate (the ones an attested caller inlines are added back as
// inline members). The data layout is kept in full so the object sizes correctly and members are
// reached by name.
//
// LAYOUT NOTE (offset authority = X360 asm): on the X360 image mpRenderTarget sits at +0x108 and the
// width/height scalars at +0x04/+0x08 (read directly by the attested bodies). This recon preserves
// the *semantics* with named members, not the byte offsets (x64 widens the pointers); the member
// ORDER follows the DWARF.
//
// COLOUR-SECTION COUNT (corrected 2026-08-12): the DWARF (Internal PS3) declares maRenderTargets[5]
// PLUS mDepthTarget, i.e. six 0x30-byte surface records. The X360 ARTIST build carries FOUR colour
// records plus the depth record - five in total - and the earlier six-record recon over-sized the
// object by 0x30 and put every member past the surface array (mpRenderTarget included) in the wrong
// place. Proof, all from the X360 asm:
//   sizeof(CgsRenderTarget) == 0x110      (operator new(0x110) in CreateShadowmapBuffer @0x823F6D98
//                                          and CreateBackBuffer @0x823F6F78)
//   mpRenderTarget          == +0x108     (lwz r3, 0x108(r3) in Begin @0x827E7748)
//   surfaces start          == +0x18, stride 0x30  (the ctor @0x823F47A0 seeds exactly five:
//                                          0x18 / 0x48 / 0x78 / 0xA8 / 0xD8)
//   the +0xD8 record is the DEPTH one     (CreateShadowmapBuffer writes the depth format 0x2D200196
//                                          to 0xE0/0xE4 and the in-use flag to 0xDC)
//   the "clear every colour section" loops in both Create*Buffer helpers run FOUR times.
// 0x18 + 5*0x30 == 0x108 == mpRenderTarget, and 0x108 + 2 pointers == 0x110. Six records cannot fit.

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
    // CgsRenderTarget.h:34 (DWARF) - one colour or depth surface's serialised description. Construct()
    // reads every field out of these records into the post-fx RenderTarget::Parameters block; the
    // serialise-side setters the DWARF lists are PS3-only and absent from the X360 ledger (the ones an
    // attested caller inlines live on CgsRenderTarget itself).
    struct CgsRenderTargetSurface
    {
        // CgsRenderTarget.h:57 (DWARF). The X360 ctor @0x823F47A0 inlines this five times (once per
        // surface record) - un-inlined back to the real per-surface constructor here.
        CgsRenderTargetSurface();

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

    // As SetRenderTargetState, but the viewport depth range is inverted (MinZ = 1, MaxZ = 0) for an
    // inverted-depth pass (used by BrnRendererModule::BeginRenderEnvironmentMapFace). The body always
    // binds section 0's render-target state; luSection mirrors the sibling's signature and is unused.
    void SetRenderTargetStateInvertDepth(u32 luSection);

    // --- serialise-side description setters --------------------------------------------------------
    // These seed the render target's description before Construct() builds the post-fx target. The
    // X360 inlines them (they appear as raw stores in callers such as BrnRendererMemory::CreateBackBuffer);
    // reconstructed here as inline members so callers reach the named fields rather than poking offsets.
    // The DWARF lists the full PS3 setter family; only the ones an X360-attested caller uses are added.
    void SetDimensions(u32 luWidth, u32 luHeight)            { mu32MaxWidth = luWidth; mu32MaxHeight = luHeight; }
    void SetNumMipMaps(u32 luNumMipMaps)                    { mu32MaxNumMipMaps = luNumMipMaps; }
    void SetMultisampleFormat(s32 lnFormat)                 { mn32MultiSampleFormat = lnFormat; }
    void SetNumSections(u8 lu8NumSections)                  { mu8NumSections = lu8NumSections; }
    void SetUseDepthStencilAsTexture(bool lbUse)            { mbUseDepthStencilAsTexture = lbUse; }

    // --- the colour-section description bits, one setter each (DWARF-attested names) ---------------
    //
    // ⚠️ THESE REPLACE AN EARLIER FUSED PAIR, and the split is not a style choice. The old
    // SetColourTargetInUse(u32) set filter mode + in-use + use-device together and
    // ClearColourTargetInUse(u32) cleared ONE section, which served the only two callers that existed
    // (CreateShadowmapBuffer, CreateBackBuffer) but cannot serve the rest of the pool: the eight other
    // Create*Buffer helpers set DIFFERENT SUBSETS of those bits, and the differences are load-bearing.
    // The DecFIGS DWARF names all four separately, and it also settles the shape of the clear:
    //     CgsRenderTarget.h:242  void ClearColourTargetInUse();                  <-- NO parameter
    //     CgsRenderTarget.h:245  void SetColourTargetInUse(uint32_t, bool);
    //     CgsRenderTarget.h:248  void SetColourTargetUseDevice(uint32_t, bool);
    //     CgsRenderTarget.h:272  void SetColourFilterMode(uint32_t, renderengine::SamplerState::FilterMode);
    // ClearColourTargetInUse taking no argument is why every pool helper shows a FOUR-ITERATION loop in
    // the X360 asm: the loop is inside this one method, inlined at each call site. Open-coding that
    // loop in each helper (as a first reconstruction pass did) is a failed inlining reversal.
    //
    // mbUseDevice is therefore NOT part of "in use". CreateBackBuffer is the only helper in the whole
    // pool that sets it, i.e. it means "this section IS the device's own surface", and leaving it clear
    // is exactly what makes Construct() derive colour mode CREATE (an off-screen surface) rather than
    // USE_DEVICE_FOR_WRITE. That is the bit the post-fx scene target depends on.
    void ClearColourTargetInUse()
    {
        for (u32 luSection = 0; luSection < KU_NUM_COLOUR_SECTIONS; ++luSection)
        {
            maRenderTargets[luSection].mbInUse     = false;
            maRenderTargets[luSection].mbUseDevice = false;
        }
    }
    void SetColourTargetInUse(u32 luSection, bool lbInUse)      { maRenderTargets[luSection].mbInUse = lbInUse; }
    void SetColourTargetUseDevice(u32 luSection, bool lbUse)    { maRenderTargets[luSection].mbUseDevice = lbUse; }

    // The value is a renderengine::SamplerState::FilterMode in the DWARF; that enum has no committed
    // home yet, so it is carried as its underlying u32 (the only value any attested caller passes is 1).
    void SetColourFilterMode(u32 luSection, u32 luFilterMode)   { maRenderTargets[luSection].mu32FilterMode = luFilterMode; }

    // DWARF CgsRenderTarget.h:305 -- SetTextureType(uint32_t, renderengine::Texture::Type). The type is
    // carried as its underlying u32 (renderengine::Texture::Type lives in pc/gcm/renderengine/texture.h;
    // including that here would drag the D3D9 texture layer into every consumer of this header).
    // BrnRendererMemory::CreateEnvmapBuffer @0x823F6C88 is the only attested caller: it passes 3
    // (E_TYPE_CUBE) to make the environment map's colour surface a cube map.
    void SetTextureType(u32 luSection, u32 luType)              { maRenderTargets[luSection].mu32TextureType = luType; }
    void SetColourTargetBufferFormat(u32 luSection, u32 luFormat)  { maRenderTargets[luSection].mu32BufferFormat = luFormat; }
    void SetColourTargetTextureFormat(u32 luSection, u32 luFormat) { maRenderTargets[luSection].mu32TextureFormat = luFormat; }
    void SetColourTargetBaseEDRAM(u32 luSection, u32 luBase)       { maRenderTargets[luSection].mu32BaseEDRAM = luBase; }

    // The depth/stencil section's own description (DWARF SetDepthTarget* family). Seeded by
    // BrnRendererMemory::CreateShadowmapBuffer, which builds a depth-ONLY target.
    void SetDepthTargetInUse(bool lbInUse)          { mDepthTarget.mbInUse = lbInUse; }
    void SetDepthTargetBufferFormat(u32 luFormat)   { mDepthTarget.mu32BufferFormat = luFormat; }
    void SetDepthTargetTextureFormat(u32 luFormat)  { mDepthTarget.mu32TextureFormat = luFormat; }
    void SetDepthTargetBaseEDRAM(u32 luBase)        { mDepthTarget.mu32BaseEDRAM = luBase; }
    void SetDepthTargetTileIndex(s8 ls8Index)       { mDepthTarget.ms8TileIndex = ls8Index; }

    // The built post-fx render target (null until Construct()/Prepare()). CreateBackBuffer reaches in
    // to share the down-sample buffer's depth-stencil section state.
    rw::graphics::postfx::RenderTarget* GetRenderTarget() const { return mpRenderTarget; }

    // The target's full extent + section count (X360 reads +0x04/+0x08/+the section byte directly).
    // Used by BrnGraphics::ShadowMapRenderManager::BeginRenderShadowMap to lay out the shadow viewport.
    u32 GetWidth()  const { return mu32MaxWidth; }
    u32 GetHeight() const { return mu32MaxHeight; }
    u32 GetNumSections() const { return mu8NumSections; }
    // DWARF CgsRenderTarget.h (dwarfdump file line 239) -- `int32_t GetMultisampleFormat()`, the read
    // sibling of the SetMultisampleFormat already above. INLINED on the console (no standalone symbol,
    // exactly like GetWidth / GetHeight / GetNumSections beside it), so this is a NAMED READ of a
    // member this class already owns, not new state. Its only caller is the env-map target's
    // "[envmap] target ..." bring-up log line, which reports the format the POOL REQUESTED -- the
    // sample count the D3D9 leaf actually got is the leaf's own to report.
    s32 GetMultisampleFormat() const { return mn32MultiSampleFormat; }

public:
    // The number of COLOUR surface records the X360 build carries (the depth/stencil record is
    // separate). See the COLOUR-SECTION COUNT note at the top of this file - the PS3 DWARF says 5,
    // ARTIST says 4, and every X360 body agrees with 4.
    static const u32 KU_NUM_COLOUR_SECTIONS = 4;

private:
    u32  mu32MaxWidth;                            // +0x04
    u32  mu32MaxHeight;                           // +0x08
    u32  mu32MaxNumMipMaps;                       // +0x0C
    s32  mn32MultiSampleFormat;                   // +0x10
    u8   mu8NumSections;                          // +0x14
    bool mbUseDepthStencilAsTexture;              // +0x15

    CgsRenderTargetSurface maRenderTargets[KU_NUM_COLOUR_SECTIONS];  // +0x18 (stride 0x30)
    CgsRenderTargetSurface mDepthTarget;                              // +0xD8

    // The built post-fx render target (DWARF type RenderTarget*; X360 +0x108). Null until Construct().
    rw::graphics::postfx::RenderTarget* mpRenderTarget;   // +0x108
    rw::IResourceAllocator*             mpAllocator;      // +0x10C

    // Pointer-invariant layout facts (never called; a MEMBER so it can reach the private members).
    static void _AssertLayout();
};

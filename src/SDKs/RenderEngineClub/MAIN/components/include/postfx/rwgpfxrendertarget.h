#pragma once

#include "types.hpp"
#include "rw/rwcore_structs.h"                  // rw::IResourceAllocator (the render-target's allocator)
#include "pc/gcm/renderengine/texture.h"        // renderengine::Texture / PixelBuffer (owned surfaces)
// renderengine::TextureState is held only by pointer here (mpColourTextureState / Target::mpTextureState);
// forward-declared below rather than pulling its (header-less) home in, so consumers that re-declare
// the vendor render-state types locally (e.g. GameSource/Graphics/BrnRendererMemory.cpp) don't ODR-clash.

// rw::graphics::postfx::RenderTarget - the EATech RenderEngineClub post-fx render-target object
// (SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h). A RenderTarget owns
// a set of colour Targets plus a depth/stencil Target and the per-section RenderTargetState the GPU
// binds; Begin/End wrap a draw pass into it, GetTexture / GetDepthStencilTexture hand back the
// resolved textures, and GetRenderTargetState returns the D3D surface-state to bind.
//
// Only the surface this build's CgsRenderTarget facade calls is declared here (the full class body
// + layout is reconstructed with its own TU). The member set / signatures come from the DecFIGS
// DWARF (rwgpfxrendertarget.h); the call shapes are confirmed against the X360 ARTIST asm that the
// CgsRenderTarget thin wrappers forward into:
//   RenderTarget::Begin(uint32_t)  @ guest rw__graphics__postfx__RenderTarget__Begin
//   RenderTarget::End(bool)        @ guest rw__graphics__postfx__RenderTarget__End
// This is a declaration-only home header (the compile gate needs the type's shape, not its body).

namespace renderengine
{
    class Texture;
    class RenderTargetState;   // the D3D surface-state object Device::SetState binds
    class TextureState;        // the colour sampler state (held by pointer only)
}

namespace rw
{
namespace graphics
{
namespace postfx
{
    // rwgpfxrendertarget.h:35 (DWARF) - one colour or depth surface owned by a RenderTarget: its GPU
    // PixelBuffer surface, the sampleable texture the surface resolves to, the sampler texture-state,
    // and (depth path) a hierarchical-Z companion texture. The X360 build (Target::CreateColor
    // @0x824034D0 / CreateDepth @0x82403688) fills these store-for-store; the offsets in the comments
    // are the X360 4-byte-pointer image (size 0x18). CgsRenderTarget also holds Target*/const Target*
    // by pointer (a provided / shared surface), so the full definition lives here.
    struct Target
    {
        // The serialised per-surface parameters the build copies (X360 memcpy 0x48 bytes out of the
        // RenderTarget Parameters at +0x1C colour / +0x13C depth). Only the words the create path
        // reads are named; the rest ride along verbatim so the layout matches.
        struct Parameters
        {
            u32 mpAllocator;     // +0x00  the rw allocator pointer (caller fills it from the RT)
            u32 muWidth;         // +0x04
            u32 muHeight;        // +0x08
            u32 muReserved0C;    // +0x0C
            u32 muFormat;        // +0x10
            u32 muReserved14;    // +0x14
            u32 muMipBase;       // +0x18
            u32 muReserved1C;    // +0x1C
            u32 muFilterWord;    // +0x20  (a2[8] -- the colour mag/min-filter source)
            u32 muEDRAMWord;     // +0x24  (a2[9] -- read by *(a2+36): the hierarchical-Z request)
            u32 muEDRAMBase;     // +0x28  (a2[10] -- the EDRAM base handed to Xbox2SetBaseEDRAM)
            u32 maReserved2C[6]; // +0x2C..+0x43
            u8  mu8Format;       // +0x44  (the surface format/sysmem byte stored into Target::mu8Format)
            u8  mau8Pad45[3];    // +0x45
        };

        u32                                       mpReserved0;     // +0x00
        renderengine::PixelBuffer::SurfaceHeader* mpPixelBuffer;   // +0x04  PixelBuffer::Initialize result
        renderengine::Texture*       mpTexture;       // +0x08  AllocateAndInitializeTexture result
        renderengine::TextureState*  mpTextureState;  // +0x0C  TextureState::Initialize result
        renderengine::Texture*       mpHiZTexture;    // +0x10  depth path: hierarchical-Z companion
        u8                           mu8Format;       // +0x14
        u8                           mau8Pad15[3];    // +0x15

        // X360 0x824034D0 -- build a colour surface (PixelBuffer + sampleable texture + sampler state).
        void CreateColor(const Parameters* lpParameters);
        // X360 0x82403688 -- build a depth/stencil surface (+ a hierarchical-Z companion when asked).
        void CreateDepth(const Parameters* lpParameters);
        // X360 0x823F9118 -- resolve the surface's tiled EDRAM PixelBuffer out to its texture.
        void Resolve();
    };

    // rwgpfxrendertarget.h:502 (DWARF). The fields are owned by this class's own TU; only the
    // accessors CgsRenderTarget uses are declared so callers reach the colour / depth textures and
    // the render-target state by name rather than by raw offset.
    class RenderTarget
    {
    public:
        // rwgpfxrendertarget.h:504 (DWARF) - per-target creation mode (flag set).
        enum RenderTargetMode
        {
            eRenderTarget_NONE                 = 0,
            eRenderTarget_CREATE               = 1,
            eRenderTarget_USE_DEVICE_FOR_WRITE = 2,
            eRenderTarget_USE_PROVIDED         = 4,
        };

        // Bind this render-target for a draw pass writing into the given slice/face, then resolve
        // (luDestSliceOrFace selects the cube face / array slice; the 2D path passes 0).
        void Begin(u32 luDestSliceOrFace);
        // End the pass; lbResolve resolves the colour surface back to its sampleable texture.
        void End(bool lbResolve);

        // The resolved colour texture for colour target luIndex.
        renderengine::Texture* GetTexture(u32 luIndex);
        // The resolved depth/stencil texture (the depth target sampled as a texture).
        renderengine::Texture* GetDepthStencilTexture();

        // The per-section D3D surface state to bind (Device::SetState). luSection selects the
        // section; the wrapper binds section 0.
        renderengine::RenderTargetState* GetRenderTargetState(u32 luSection);

        // The per-section render-target-state pointer array (X360: +0x1C + luSection*4, read by
        // RenderTarget::Begin). CreateBackBuffer patches section 0 to the engine default state and
        // copies section 4 (the resolved depth-stencil state) from the down-sample buffer so the back
        // buffer shares its depth surface. Declaration-only here (no asm in this TU); naming the
        // operation keeps the caller off raw offsets.
        renderengine::RenderTargetState* GetSectionRenderTargetState(u32 luSection);
        void SetSectionRenderTargetState(u32 luSection, renderengine::RenderTargetState* lpState);

        // --- the build-side surface (X360 ARTIST asm; reconstructed in rwgpfxrendertarget.cpp) -------
        // X360 0x82409B60 -- allocate the render-target object through the parameters' allocator (or the
        // registry default) and construct it; returns null when the allocation fails.
        static RenderTarget* Initialize(const Target::Parameters* lpParameters);

        // X360 0x824088B0 -- construct the render-target into `this` from the parameters (the X360
        // out-of-line constructor body; returns `this`).
        RenderTarget* Construct(const Target::Parameters* lpParameters);

        // X360 0x823F9338 -- resolve the colour and/or depth surfaces back to their textures.
        void Resolve(bool lbResolveDepthStencil, bool lbResolveColour);

        // X360 0x824037E8 -- build the per-section RenderTargetState and (when the colour surface wants
        // one) the colour-sampling TextureState.
        void CreateStates(const Target::Parameters* lpParameters);

        // --- layout (X360 4-byte-pointer image; size 0x98). Reached by name; offsets are docs. -------
        rw::IResourceAllocator*          mpAllocator;          // +0x00
        u32                              muWidth;              // +0x04
        u32                              muHeight;             // +0x08
        u32                              muReserved0C;         // +0x0C
        u32                              muColourMode;         // +0x10  RenderTargetMode
        u32                              muDepthStencilMode;   // +0x14  RenderTargetMode
        u8                               mu8HasColour;         // +0x18
        u8                               mau8Pad19[3];         // +0x19
        renderengine::RenderTargetState* mpSection0State;      // +0x1C  CreateStates' section-0 state
        Target                           maColourTargets[3];   // +0x20  (3 * 0x18)
        Target                           mDepthTarget;         // +0x80
        renderengine::RenderTargetState* mpProvidedState;      // +0x84  provided state (USE_PROVIDED)
        u32                              muProvidedTextureId;  // +0x88
        renderengine::TextureState*      mpColourTextureState; // +0x8C  colour-sampling state
        renderengine::RenderTargetState* mpProvidedColourState;// +0x90
        u8                               mu8UsesProvidedState; // +0x95
        u8                               mau8Pad96[2];         // +0x96
    };

    // The engine's default render-target state (X360 dword_83271614, installed by
    // renderengine::Device::Start). Used as section 0's state for targets that have none of their own.
    extern renderengine::RenderTargetState* gpDefaultRenderTargetState;
}
}
}

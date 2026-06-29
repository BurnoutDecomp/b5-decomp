#pragma once

#include "types.hpp"

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
}

namespace rw
{
namespace graphics
{
namespace postfx
{
    // rwgpfxrendertarget.h:35 (DWARF) - one colour or depth surface owned by a RenderTarget (the
    // pixel buffers + resolved texture + sampler state). Forward-declared here; the render-target
    // facade only holds Target* / const Target* (a provided / shared surface) by pointer.
    struct Target;

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
        // buffer shares its depth surface. Declaration-only here (the fields live in this class's own
        // TU); naming the operation keeps the caller off raw offsets.
        renderengine::RenderTargetState* GetSectionRenderTargetState(u32 luSection);
        void SetSectionRenderTargetState(u32 luSection, renderengine::RenderTargetState* lpState);
    };

    // The engine's default render-target state (X360 dword_83271614, installed by
    // renderengine::Device::Start). Used as section 0's state for targets that have none of their own.
    extern renderengine::RenderTargetState* gpDefaultRenderTargetState;
}
}
}

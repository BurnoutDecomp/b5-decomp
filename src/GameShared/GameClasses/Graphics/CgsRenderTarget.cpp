// CgsRenderTarget.cpp - Burnout's facade over an EATech post-fx render target.
//
// Only the X360 ARTIST-attested draw-side surface is reconstructed in this TU (the build-side
// Construct/Destruct/Prepare virtuals and the serialise-time setters are their own work):
//   Begin / End              forward a draw pass into the post-fx RenderTarget
//   GetTexture / GetDepthTexture  hand back the resolved colour / depth textures
//   SetRenderTargetState     bind the target's surface state + viewport + scissor on the device
//
// Behaviour + calling convention verified against the X360 asm (the compiler inlined the
// rw::graphics::postfx::RenderTarget accessors the C++ called; they are un-inlined back to named
// calls here, per the reconstruction rules).

#include "GameShared/GameClasses/Graphics/CgsRenderTarget.h"

#include <type_traits>                                      // std::is_polymorphic (uncalled _AssertLayout)

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT (BeginAssert/FireAssert/EndAssert)
#include "pc/gcm/renderengine/device.h"                     // renderengine::Device::SetState + gpD3DDevice

// X360 (Xenon) D3D9 viewport / scissor entry points. These are platform externals (XDK intrinsics);
// declared with their X360 ABI so the body compiles store-for-store against the same calls the image
// makes. On a non-X360 build they resolve to the platform shim layer.
extern "C" void D3DDevice_SetViewportF(void* lpDevice, const void* lpViewport);
extern "C" void D3DDevice_SetScissorRect(void* lpDevice, const void* lpRect);

// The X360 D3D device the image reads from off_83271608 (the engine's single device global; defined
// in the renderengine VertexBuffer/device TUs as renderengine::gpD3DDevice).
namespace renderengine { extern void* gpD3DDevice; }

namespace
{
    // The X360 float viewport descriptor D3DDevice_SetViewportF consumes (D3DVIEWPORT9 with float
    // X/Y/Width/Height; the X360 GPU takes the viewport in floats). Matches the SetRenderTargetState
    // store layout exactly.
    struct ViewportF
    {
        f32 mfX;       // +0x00
        f32 mfY;       // +0x04
        f32 mfWidth;   // +0x08
        f32 mfHeight;  // +0x0C
        f32 mfMinZ;    // +0x10
        f32 mfMaxZ;    // +0x14
        u32 mu32Pad;   // +0x18 (the trailing zero word the X360 asm writes)
    };

    // The integer scissor rectangle (RECT) D3DDevice_SetScissorRect consumes.
    struct ScissorRect
    {
        s32 miLeft;    // +0x00
        s32 miTop;     // +0x04
        s32 miRight;   // +0x08
        s32 miBottom;  // +0x0C
    };

    // The last render-target state installed on the device (X360 dword_83010A30): the wrapper skips a
    // redundant Device::SetState when the same state is rebound back-to-back.
    const renderengine::RenderTargetState* gpLastRenderTargetState = nullptr;

    // The default render-target state used when the target has no section-0 state of its own
    // (X360 dword_83271614, the engine's default-surface global; null until the engine sets it).
    const renderengine::RenderTargetState* gpDefaultRenderTargetState = nullptr;
}

// Construct an empty render-target description. The X360 ctor (0x823F47A0) installs the vtable and
// seeds every colour section's surface description to its "unused, no compression, no ZCull" default:
// filter mode 1, not-in-use, no system memory, texture type 1, and all of the EDRAM-tile / ZCull /
// compression-base fields cleared to -1 (the X360 "unassigned" sentinel). The shared-target pointers
// and the built post-fx target start null. Per-surface fields the asm does NOT write (use-device,
// buffer/texture format, base EDRAM) are left for the later serialise-side setters to fill, so they
// are not touched here either. mu8NumSections is seeded to 1.
//
// Stores reconstructed by name from the X360 asm (offsets there: maRenderTargets at +0x18, stride
// 0x30, mpRenderTarget at +0x108); this recon keeps the semantics, not the byte offsets.
CgsRenderTarget::CgsRenderTarget()
{
    mu8NumSections = 1;

    for (u32 luSection = 0; luSection < 5; ++luSection)
    {
        CgsRenderTargetSurface& lSurface = maRenderTargets[luSection];
        lSurface.mu32FilterMode       = 1;
        lSurface.mbInUse              = false;
        lSurface.mbUseSystemMemory    = false;
        lSurface.miPS3CompressionBase = -1;
        lSurface.ms8TileIndex         = -1;
        lSurface.ms8ZCullIndex        = -1;
        lSurface.miZCullAddress       = -1;
        lSurface.mpTarget             = nullptr;
        lSurface.mu32TextureType      = 1;
        lSurface.mpSharedTarget       = nullptr;
        lSurface.mpSharedAddress      = nullptr;
    }
    // The X360 ctor does not write mpRenderTarget (+0x108) or mpAllocator: the built post-fx target
    // and its allocator are installed later by Construct()/Prepare(), so they are not seeded here.
}

// Begin a draw pass into the post-fx render target (resolve face/slice 0).
void CgsRenderTarget::Begin()
{
    mpRenderTarget->Begin(0);
}

// End the draw pass, resolving the colour surface back to its sampleable texture.
void CgsRenderTarget::End()
{
    mpRenderTarget->End(true);
}

// The resolved colour texture for colour section luIndex.
renderengine::Texture* CgsRenderTarget::GetTexture(u32 luIndex)
{
    CGS_ASSERT(mpRenderTarget != nullptr, "mpRenderTarget");
    return mpRenderTarget->GetTexture(luIndex);
}

// The resolved depth/stencil texture.
renderengine::Texture* CgsRenderTarget::GetDepthTexture()
{
    CGS_ASSERT(mpRenderTarget != nullptr, "mpRenderTarget");
    return mpRenderTarget->GetDepthStencilTexture();
}

// Bind this render target's surface state on the device and set the viewport + scissor to its full
// extent. The X360 body reads section-0's render-target state regardless of luSection (the parameter
// selects the conceptual section but the bound state is always slot 0).
void CgsRenderTarget::SetRenderTargetState(u32 luSection)
{
    (void)luSection;

    const renderengine::RenderTargetState* lpState = mpRenderTarget->GetRenderTargetState(0);
    if (lpState == nullptr)
    {
        lpState = gpDefaultRenderTargetState;
    }

    // Skip the device call when the same surface state is already bound.
    if (gpLastRenderTargetState != lpState)
    {
        renderengine::Device::SetState(lpState);
        gpLastRenderTargetState = lpState;
    }

    // Viewport: the target's full extent, depth range [0, 1]. Width/height come from the stored
    // dimensions, converted u32 -> float (the X360 fcfid/frsp the asm emits).
    ViewportF lViewport;
    lViewport.mfX      = 0.0f;
    lViewport.mfY      = 0.0f;
    lViewport.mfWidth  = static_cast<f32>(mu32MaxWidth);
    lViewport.mfHeight = static_cast<f32>(mu32MaxHeight);
    lViewport.mfMinZ   = 0.0f;
    lViewport.mfMaxZ   = 1.0f;
    lViewport.mu32Pad  = 0;
    D3DDevice_SetViewportF(renderengine::gpD3DDevice, &lViewport);

    // Scissor: the same full extent.
    ScissorRect lScissor;
    lScissor.miLeft   = 0;
    lScissor.miTop    = 0;
    lScissor.miRight  = static_cast<s32>(mu32MaxWidth);
    lScissor.miBottom = static_cast<s32>(mu32MaxHeight);
    D3DDevice_SetScissorRect(renderengine::gpD3DDevice, &lScissor);
}

// As SetRenderTargetState, but the viewport depth range is INVERTED: MinZ = 1, MaxZ = 0. The X360 body
// (0x827E7668) is otherwise identical store-for-store to SetRenderTargetState - it binds section-0's
// render-target state (falling back to the engine default + skipping a redundant Device::SetState via
// the same dword_83010A30 / dword_83271614 globals) and sets the scissor to the full extent. Only the
// two depth-range floats are swapped (X360 flt_82001C98 = 1.0 into MinZ, flt_82001CC0 = 0.0 into X / Y
// / MaxZ). Called by BrnRendererModule::BeginRenderEnvironmentMapFace.
void CgsRenderTarget::SetRenderTargetStateInvertDepth(u32 luSection)
{
    (void)luSection;

    const renderengine::RenderTargetState* lpState = mpRenderTarget->GetRenderTargetState(0);
    if (lpState == nullptr)
    {
        lpState = gpDefaultRenderTargetState;
    }

    // Skip the device call when the same surface state is already bound.
    if (gpLastRenderTargetState != lpState)
    {
        renderengine::Device::SetState(lpState);
        gpLastRenderTargetState = lpState;
    }

    // Viewport: the target's full extent, inverted depth range [1, 0]. Width/height come from the
    // stored dimensions, converted u32 -> float (the X360 fcfid/frsp the asm emits).
    ViewportF lViewport;
    lViewport.mfX      = 0.0f;
    lViewport.mfY      = 0.0f;
    lViewport.mfWidth  = static_cast<f32>(mu32MaxWidth);
    lViewport.mfHeight = static_cast<f32>(mu32MaxHeight);
    lViewport.mfMinZ   = 1.0f;
    lViewport.mfMaxZ   = 0.0f;
    lViewport.mu32Pad  = 0;
    D3DDevice_SetViewportF(renderengine::gpD3DDevice, &lViewport);

    // Scissor: the same full extent.
    ScissorRect lScissor;
    lScissor.miLeft   = 0;
    lScissor.miTop    = 0;
    lScissor.miRight  = static_cast<s32>(mu32MaxWidth);
    lScissor.miBottom = static_cast<s32>(mu32MaxHeight);
    D3DDevice_SetScissorRect(renderengine::gpD3DDevice, &lScissor);
}

// Pointer-invariant layout facts (uncalled). The X360 byte offsets (surfaces at +0x18 stride 0x30,
// mpRenderTarget at +0x108) are not portable to the LLP64 host because the surface structs widen with
// their pointers; what IS invariant is that CgsRenderTarget is polymorphic - the ctor installs a vptr
// (off_820460B4) for the Construct/Destruct/Prepare virtuals - and that its surface description carries
// the EDRAM-tile / ZCull pointer-and-byte fields the ctor seeds.
static void _AssertLayout()
{
    static_assert(std::is_polymorphic<CgsRenderTarget>::value,
                  "CgsRenderTarget must be polymorphic - the X360 ctor installs a vptr at +0x00");
    // The tile / ZCull indices the ctor seeds to -1 are single signed-byte sentinels (pointer-invariant
    // width fact); the target slot it clears to null is a pointer.
    static_assert(sizeof(CgsRenderTarget::CgsRenderTargetSurface::ms8TileIndex) == 1,
                  "tile index is a single byte sentinel");
    static_assert(sizeof(CgsRenderTarget::CgsRenderTargetSurface::mpTarget) == sizeof(void*),
                  "surface target slot is a pointer");
}

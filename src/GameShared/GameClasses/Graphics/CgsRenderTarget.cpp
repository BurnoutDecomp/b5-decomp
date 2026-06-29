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

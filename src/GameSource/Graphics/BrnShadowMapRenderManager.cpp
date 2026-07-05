#include "GameSource/Graphics/BrnShadowMapRenderManager.h"

#include "GameSource/Graphics/BrnRendererMemory.h"                  // BrnRendererMemory::GetShadowMapBuffer
#include "GameShared/GameClasses/Graphics/CgsRenderTarget.h"        // CgsRenderTarget (Get*/SetRenderTargetState/GetRenderTarget)
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h" // RenderTarget::mDepthTarget.Resolve()
#include "pc/gcm/renderengine/Xbox2SurfaceShims.h"                  // renderengine::gpD3DDevice

// BrnGraphics::ShadowMapRenderManager -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//   BeginRenderShadowMap @ 0x823F7858
//   EndRenderShadowMap   @ 0x823FD708

namespace renderengine { class DepthStencilState; }

// ---- BrnShadowMapRenderManager.cpp file-local reconstruction preamble ----
// X360 D3D9 viewport/scissor intrinsics (platform externals; off_83271608 device == renderengine::gpD3DDevice).
extern "C" void D3DDevice_SetViewportF(void* lpDevice, const void* lpViewport);
extern "C" void D3DDevice_SetScissorRect(void* lpDevice, const void* lpRect);

namespace
{
    // The X360 float viewport descriptor D3DDevice_SetViewportF consumes (mirrors CgsRenderTarget.cpp).
    struct ViewportF   { f32 mfX, mfY, mfWidth, mfHeight, mfMinZ, mfMaxZ; u32 mu32Pad; };
    // The integer scissor rectangle D3DDevice_SetScissorRect consumes.
    struct ScissorRect { s32 miLeft, miTop, miRight, miBottom; };
    // The clear descriptor sub_82B61D78 consumes ({0x30, 1.0f, 0}).
    struct ClearDepthStencilParameters { u32 mu32Flags; f32 mfDepth; u32 mu32Stencil; };
}

// X360 dword_8301090C == CgsDepthStencilStateFactory::saDepthStencilStates[0] (Z-on / Z<= / Zwrite-on),
// owned by CgsDepthStencilStateFactory.cpp; declared-not-defined here (external linkage; link-time resolve).
extern renderengine::DepthStencilState* gpShadowDepthStencilState;
// X360 sub_82276AD0 == CgsGraphics::ImRendererBase::SetState(const DepthStencilState*): install the
// depth/stencil state on the device (the immediate-mode state cache is module-static). Declared-only.
void ImDeviceSetDepthStencilState(renderengine::DepthStencilState* lpState);
// X360 sub_82B61D78: clear the bound depth/stencil. Declared-only.
void DeviceClearDepthStencil(const ClearDepthStencilParameters* lpParameters);

namespace BrnGraphics { bool gbCombinedShadowMapViewport; }

// BrnGraphics::ShadowMapRenderManager::BeginRenderShadowMap  @ 0x823F7858
// Bind the shadow-map render target for one shadow face's draw pass, install the shared shadow-pass
// depth/stencil state, and (when lbClear) clear the depth/stencil. Two layouts, selected by the
// namespace-scope gbCombinedShadowMapViewport (X360 byte_82F2423F):
//   - COMBINED (set): all shadow faces share render-target slot 0 (GetShadowMapBuffer(0)).
//       * multi-section (GetNumSections() > 1): bind the whole target (SetRenderTargetState(liIndex)
//         selects the section) and view its full extent.
//       * single-section (GetNumSections() <= 1): pack the faces vertically into thirds; bind section 0
//         and view the liIndex'th (height/3) band.
//   - SEPARATE (clear): face liIndex owns its render target (GetShadowMapBuffer(liIndex)); bind section 0.
// After binding, the shared shadow-pass depth/stencil state (CgsDepthStencilStateFactory slot 0,
// Z-on / Z<= / Zwrite-on) is applied via ImRendererBase::SetState (sub_82276AD0), and when lbClear the
// depth/stencil is cleared to Z=1.0 / stencil 0 (flags 0x30).
void BrnGraphics::ShadowMapRenderManager::BeginRenderShadowMap(s32 liIndex, bool lbClear,
                                                              BrnRendererMemory* lpAllocatedRenderTargets)
{
    if (BrnGraphics::gbCombinedShadowMapViewport)
    {
        CgsRenderTarget* lpShadowBuffer = lpAllocatedRenderTargets->GetShadowMapBuffer(0);
        CGS_ASSERT(lpShadowBuffer != nullptr, "lpAllocatedRenderTargets->GetShadowMapBuffer(0)");
        CGS_ASSERT(lpShadowBuffer->GetRenderTarget() != nullptr,
                   "lpAllocatedRenderTargets->GetShadowMapBuffer(0)->GetRenderTarget()");

        if (lpShadowBuffer->GetNumSections() > 1)
        {
            // Multi-section combined target: bind the whole target (section == liIndex) and use the
            // full extent as the viewport/scissor.
            lpShadowBuffer->SetRenderTargetState(static_cast<u32>(liIndex));
            ImDeviceSetDepthStencilState(gpShadowDepthStencilState);

            const u32 luWidth  = lpShadowBuffer->GetWidth();
            const u32 luHeight = lpShadowBuffer->GetHeight();

            ViewportF lViewport;
            lViewport.mfX      = 0.0f;
            lViewport.mfY      = 0.0f;
            lViewport.mfWidth  = static_cast<f32>(luWidth);
            lViewport.mfHeight = static_cast<f32>(luHeight);
            lViewport.mfMinZ   = 0.0f;
            lViewport.mfMaxZ   = 1.0f;
            lViewport.mu32Pad  = 0;
            D3DDevice_SetViewportF(renderengine::gpD3DDevice, &lViewport);

            ScissorRect lScissor;
            lScissor.miLeft   = 0;
            lScissor.miTop    = 0;
            lScissor.miRight  = static_cast<s32>(luWidth);
            lScissor.miBottom = static_cast<s32>(luHeight);
            D3DDevice_SetScissorRect(renderengine::gpD3DDevice, &lScissor);
        }
        else
        {
            // Single-section combined target: pack the faces vertically into thirds and view the
            // liIndex'th band.
            const u32 luHeight     = lpShadowBuffer->GetHeight();
            const u32 luWidth      = lpShadowBuffer->GetWidth();
            const u32 luTileHeight = luHeight / 3u;
            const u32 luOffsetY    = luTileHeight * static_cast<u32>(liIndex);

            lpShadowBuffer->SetRenderTargetState(0);
            ImDeviceSetDepthStencilState(gpShadowDepthStencilState);

            ViewportF lViewport;
            lViewport.mfX      = 0.0f;
            lViewport.mfY      = static_cast<f32>(luOffsetY);
            lViewport.mfWidth  = static_cast<f32>(luWidth);
            lViewport.mfHeight = static_cast<f32>(luTileHeight);
            lViewport.mfMinZ   = 0.0f;
            lViewport.mfMaxZ   = 1.0f;
            lViewport.mu32Pad  = 0;
            D3DDevice_SetViewportF(renderengine::gpD3DDevice, &lViewport);

            ScissorRect lScissor;
            lScissor.miLeft   = 0;
            lScissor.miTop    = static_cast<s32>(luOffsetY);
            lScissor.miRight  = static_cast<s32>(luWidth);
            lScissor.miBottom = static_cast<s32>(luOffsetY + luTileHeight);
            D3DDevice_SetScissorRect(renderengine::gpD3DDevice, &lScissor);
        }
    }
    else
    {
        // Separate per-face targets: face liIndex owns its render target.
        CGS_ASSERT(lpAllocatedRenderTargets->GetShadowMapBuffer(liIndex) != nullptr,
                   "lpAllocatedRenderTargets->GetShadowMapBuffer(liIndex)");
        CGS_ASSERT(lpAllocatedRenderTargets->GetShadowMapBuffer(liIndex)->GetRenderTarget() != nullptr,
                   "lpAllocatedRenderTargets->GetShadowMapBuffer(liIndex)->GetRenderTarget()");

        lpAllocatedRenderTargets->GetShadowMapBuffer(liIndex)->SetRenderTargetState(0);
        ImDeviceSetDepthStencilState(gpShadowDepthStencilState);
    }

    if (lbClear)
    {
        // Clear the shadow depth/stencil (flags 0x30, Z = 1.0, stencil = 0).
        ClearDepthStencilParameters lClearZbuffer;
        lClearZbuffer.mu32Flags   = 0x30;
        lClearZbuffer.mfDepth     = 1.0f;
        lClearZbuffer.mu32Stencil = 0;
        DeviceClearDepthStencil(&lClearZbuffer);
    }
}

// BrnGraphics::ShadowMapRenderManager::EndRenderShadowMap  @ 0x823FD708
// Resolve the shadow-map depth surface out of tiled EDRAM into its sampleable texture at the end of a
// shadow pass. In combined mode all faces live in render-target slot 0, so the single combined buffer's
// depth target is resolved; in separate mode face liIndex's own render target is resolved. Both paths
// reduce to RenderTarget::mDepthTarget.Resolve().
void BrnGraphics::ShadowMapRenderManager::EndRenderShadowMap(s32 liIndex,
                                                            BrnRendererMemory* lpAllocatedRenderTargets)
{
    if (BrnGraphics::gbCombinedShadowMapViewport)
    {
        lpAllocatedRenderTargets->GetShadowMapBuffer(0)->GetRenderTarget()->mDepthTarget.Resolve();
        return;
    }

    lpAllocatedRenderTargets->GetShadowMapBuffer(liIndex)->GetRenderTarget()->mDepthTarget.Resolve();
}

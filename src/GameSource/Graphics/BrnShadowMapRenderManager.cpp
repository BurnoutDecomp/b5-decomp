#include "GameSource/Graphics/BrnShadowMapRenderManager.h"

#include "GameSource/Graphics/BrnRendererMemory.h"                  // BrnRendererMemory::GetShadowMapBuffer
#include "GameShared/GameClasses/Graphics/CgsRenderTarget.h"        // CgsRenderTarget (Get*/SetRenderTargetState/GetRenderTarget)
#include "GameShared/GameClasses/Graphics/CgsRasterizerStateFactory.h"
#include "GameShared/GameClasses/Graphics/CgsDepthStencilStateFactory.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h" // RenderTarget::mDepthTarget.Resolve()
                                                                    // ImDeviceSetDepthStencilState,
                                                                    // DeviceClearDepthStencil

// BrnGraphics::ShadowMapRenderManager -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//   BeginRenderShadowMap @ 0x823F7858
//   EndRenderShadowMap   @ 0x823FD708

// X360 byte_82F2423F. Read out of the ARTIST image by the shadow-map wave's data dump as 0x01,
// alongside the shadow target's geometry (1280x1920, three 640-row sections) -- so the
// multi-section COMBINED branch below is the live one, and the definition is seeded to match
// rather than left zero-initialised (which would have selected the never-taken separate-target
// branch and asked BrnRendererMemory for three shadow buffers instead of one).
namespace BrnGraphics { bool gbCombinedShadowMapViewport = true; }

// Inlined in ARTIST BrnRendererModule::Construct @8240BF30..BF54: manager+0 = 3,
// manager+4 = 2, manager+8 = true. DWARF names the argument luBaseRenderTargetIndex.
void BrnGraphics::ShadowMapRenderManager::Construct(u32 luBaseRenderTargetIndex)
{
    muWriteBufferIndex = luBaseRenderTargetIndex + 1;
    muReadBufferIndex = luBaseRenderTargetIndex;
    mbForceFrontFaceCull = true;
}

// ARTIST Render @8240C84C..C864 and 8240C8B0..C8C4, repeated for lists 1 and 4.
// Factory Construct @827EBF30 stores BACK at 83010A38 and FRONT at 83010A3C;
// these are initialized states, with zero depth/slope bias, not unresolved data.
void BrnGraphics::ShadowMapRenderManager::BeginFrontFaceCullRender()
{
    if (!mbForceFrontFaceCull)
    {
        shadow::Device::SetState(CgsRasterizerStateFactory::GetState(
            E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_FRONT));
        shadow::Device::LockRasteriserState();
    }
}

void BrnGraphics::ShadowMapRenderManager::EndFrontFaceCullRender()
{
    if (!mbForceFrontFaceCull)
    {
        shadow::Device::UnlockRasteriserState();
        shadow::Device::SetState(CgsRasterizerStateFactory::GetState(
            E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_BACK));
    }
}

// ARTIST Render @8240C8F0..C908 and 8240C924..C938, repeated for list 3.
// The flag selects which caster group receives the SAME fixed front-cull state.
void BrnGraphics::ShadowMapRenderManager::BeginBackFaceCullRender()
{
    if (mbForceFrontFaceCull)
    {
        shadow::Device::SetState(CgsRasterizerStateFactory::GetState(
            E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_FRONT));
        shadow::Device::LockRasteriserState();
    }
}

void BrnGraphics::ShadowMapRenderManager::EndBackFaceCullRender()
{
    if (mbForceFrontFaceCull)
    {
        shadow::Device::UnlockRasteriserState();
        shadow::Device::SetState(CgsRasterizerStateFactory::GetState(
            E_FACTORY_RASTERIZER_STATE_SCISSOR_CULL_MODE_BACK));
    }
}

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
        CGS_ASSERT(lpShadowBuffer != 0, "lpAllocatedRenderTargets->GetShadowMapBuffer(0)");
        CGS_ASSERT(lpShadowBuffer->GetRenderTarget() != 0,
                   "lpAllocatedRenderTargets->GetShadowMapBuffer(0)->GetRenderTarget()");

        if (lpShadowBuffer->GetNumSections() > 1)
        {
            // Multi-section combined target: bind the whole target (section == liIndex) and use the
            // full extent as the viewport/scissor.
            lpShadowBuffer->SetRenderTargetState(static_cast<u32>(liIndex));
            shadow::Device::SetState(CgsDepthStencilStateFactory::GetState(
                E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEON));

            const u32 luWidth  = lpShadowBuffer->GetWidth();
            const u32 luHeight = lpShadowBuffer->GetHeight();

            D3DVIEWPORTF9 lViewport;
            lViewport.X      = 0.0f;
            lViewport.Y      = 0.0f;
            lViewport.Width  = static_cast<f32>(luWidth);
            lViewport.Height = static_cast<f32>(luHeight);
            lViewport.MinZ   = 0.0f;
            lViewport.MaxZ   = 1.0f;
            lViewport.Flags  = 0;
            D3DDevice_SetViewportF(gpD3DDevice, &lViewport);

            RECT lScissor;
            lScissor.left   = 0;
            lScissor.top    = 0;
            lScissor.right  = static_cast<s32>(luWidth);
            lScissor.bottom = static_cast<s32>(luHeight);
            D3DDevice_SetScissorRect(gpD3DDevice, &lScissor);
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
            shadow::Device::SetState(CgsDepthStencilStateFactory::GetState(
                E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEON));

            D3DVIEWPORTF9 lViewport;
            lViewport.X      = 0.0f;
            lViewport.Y      = static_cast<f32>(luOffsetY);
            lViewport.Width  = static_cast<f32>(luWidth);
            lViewport.Height = static_cast<f32>(luTileHeight);
            lViewport.MinZ   = 0.0f;
            lViewport.MaxZ   = 1.0f;
            lViewport.Flags  = 0;
            D3DDevice_SetViewportF(gpD3DDevice, &lViewport);

            RECT lScissor;
            lScissor.left   = 0;
            lScissor.top    = static_cast<s32>(luOffsetY);
            lScissor.right  = static_cast<s32>(luWidth);
            lScissor.bottom = static_cast<s32>(luOffsetY + luTileHeight);
            D3DDevice_SetScissorRect(gpD3DDevice, &lScissor);
        }
    }
    else
    {
        // Separate per-face targets: face liIndex owns its render target.
        CGS_ASSERT(lpAllocatedRenderTargets->GetShadowMapBuffer(liIndex) != 0,
                   "lpAllocatedRenderTargets->GetShadowMapBuffer(liIndex)");
        CGS_ASSERT(lpAllocatedRenderTargets->GetShadowMapBuffer(liIndex)->GetRenderTarget() != 0,
                   "lpAllocatedRenderTargets->GetShadowMapBuffer(liIndex)->GetRenderTarget()");

        lpAllocatedRenderTargets->GetShadowMapBuffer(liIndex)->SetRenderTargetState(0);
        shadow::Device::SetState(CgsDepthStencilStateFactory::GetState(
                E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEON));
    }

    if (lbClear)
    {
        // Clear the shadow depth/stencil (Xenon D3DCLEAR flags 0x30 = ZBUFFER|STENCIL,
        // Z = 1.0, stencil = 0).
        renderengine::ClearDepthStencilParameters lClearZbuffer;
        lClearZbuffer.m_clearFlags   = 0x30;
        lClearZbuffer.m_depth     = 1.0f;
        lClearZbuffer.m_stencil = 0;
        renderengine::Device::Clear(lClearZbuffer);
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

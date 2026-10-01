#pragma once
#include <Windows.h>
#include <d3d9.h>
#include "pc/gcm/renderengine/ShaderConstantCachePCLeaf.h"
#include "pc/gcm/renderengine/SamplerStateCachePCLeaf.h"
#include "pc/gcm/renderengine/GeometryBindingsPCLeaf.h"

namespace renderengine
{
    // FLAG PC-platform leaf: assertions can interrupt an open offscreen pass.
    // Preserve its state and surfaces while the modal loop presents the visible
    // frame. D3DSBT_ALL covers pipeline state; RT/depth bindings are saved here.
    class PCAssertFrame
    {
    public:
        PCAssertFrame(IDirect3DDevice9* lpDevice, IDirect3DSurface9* lpBackBuffer)
            : mpDevice(lpDevice)
        {
            if (!mpDevice || !lpBackBuffer) return;
            D3DCAPS9 lCaps = {};
            D3DSURFACE_DESC lBack = {};
            if (FAILED(mpDevice->GetDeviceCaps(&lCaps)) || FAILED(lpBackBuffer->GetDesc(&lBack))
                || FAILED(mpDevice->CreateStateBlock(D3DSBT_ALL, &mpState))) return;
            muTargets = lCaps.NumSimultaneousRTs < 4 ? lCaps.NumSimultaneousRTs : 4;
            if (!muTargets || FAILED(mpDevice->GetRenderTarget(0, &mapTargets[0]))) return;
            for (unsigned i=1;i<muTargets;++i) mpDevice->GetRenderTarget(i, &mapTargets[i]);
            mpDevice->GetDepthStencilSurface(&mpDepth);
            mbSaved = true;
            mbSceneWasOpen = SUCCEEDED(mpDevice->EndScene());
            mpDevice->SetDepthStencilSurface(nullptr);
            for (unsigned i=1;i<muTargets;++i) mpDevice->SetRenderTarget(i, nullptr);
            if (FAILED(mpDevice->SetRenderTarget(0, lpBackBuffer))) return;
            const D3DVIEWPORT9 lViewport = {0,0,lBack.Width,lBack.Height,0.0f,1.0f};
            if (FAILED(mpDevice->SetViewport(&lViewport))) return;
            mpDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
            mbReady = true;
        }
        ~PCAssertFrame()
        {
            if (mbSaved)
            {
                // The modal loop normally ends its scene at every Present.
                // Close an interrupted modal draw before restoring its caller.
                mpDevice->EndScene();
                mpDevice->SetDepthStencilSurface(nullptr);
                for (unsigned i=1;i<muTargets;++i) mpDevice->SetRenderTarget(i, nullptr);
                mpDevice->SetRenderTarget(0, mapTargets[0]);
                for (unsigned i=1;i<muTargets;++i) mpDevice->SetRenderTarget(i, mapTargets[i]);
                mpDevice->SetDepthStencilSurface(mpDepth);
                mpState->Apply();
                gPCShaderConstantCache.Invalidate();
                gPCSamplerStateCache.Invalidate();
                GeometryBindingsPC::gCache.Invalidate();
                if (mbSceneWasOpen) mpDevice->BeginScene();
            }
            for (auto* lpTarget : mapTargets) if (lpTarget) lpTarget->Release();
            if (mpDepth) mpDepth->Release();
            if (mpState) mpState->Release();
        }
        bool IsReady() const { return mbReady; }
        PCAssertFrame(const PCAssertFrame&) = delete;
        PCAssertFrame& operator=(const PCAssertFrame&) = delete;
    private:
        IDirect3DDevice9* mpDevice;
        IDirect3DStateBlock9* mpState = nullptr;
        IDirect3DSurface9* mapTargets[4] = {};
        IDirect3DSurface9* mpDepth = nullptr;
        unsigned muTargets = 0;
        bool mbSaved = false, mbReady = false, mbSceneWasOpen = false;
    };
}

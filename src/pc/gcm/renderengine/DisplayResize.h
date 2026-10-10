#pragma once
#include "pc/gcm/renderengine/DepthRange.h"

#include <Windows.h>
#include <d3d9.h>
#include <utility>
#include "pc/gcm/renderengine/TextureUpload.h"

// FLAG PC-platform leaf: prepare replacement D3D9 surfaces without Reset(), so
// a window resize preserves the loaded world's textures, buffers and shaders.
// Nothing is published until every allocation in the resize has succeeded.
namespace renderengine
{
    struct PCSurfaceResize
    {
        IDirect3DSurface9* mpSurface = nullptr;
        IDirect3DTexture9* mpTexture = nullptr;

        PCSurfaceResize() = default;
        PCSurfaceResize(const PCSurfaceResize&) = delete;
        PCSurfaceResize& operator=(const PCSurfaceResize&) = delete;
        ~PCSurfaceResize()
        {
            if (mpSurface) mpSurface->Release();
            if (mpTexture) mpTexture->Release();
        }

        bool Prepare(IDirect3DDevice9* lpDevice, IDirect3DSurface9* lpOriginal,
                     UINT luWidth, UINT luHeight)
        {
            if (!lpOriginal) return true;
            D3DSURFACE_DESC lDesc = {};
            if (FAILED(lpOriginal->GetDesc(&lDesc))) return false;
            IDirect3DTexture9* lpContainer = nullptr;
            const bool lbTexture = SUCCEEDED(lpOriginal->GetContainer(
                __uuidof(IDirect3DTexture9), reinterpret_cast<void**>(&lpContainer)));
            if (lpContainer) lpContainer->Release();
            if (lbTexture)
            {
                return SUCCEEDED(lpDevice->CreateTexture(luWidth, luHeight, 1,
                    lDesc.Usage, lDesc.Format, D3DPOOL_DEFAULT, &mpTexture, nullptr))
                    && SUCCEEDED(mpTexture->GetSurfaceLevel(0, &mpSurface));
            }
            if ((lDesc.Usage & D3DUSAGE_DEPTHSTENCIL) != 0)
                return SUCCEEDED(lpDevice->CreateDepthStencilSurface(luWidth, luHeight,
                    lDesc.Format, lDesc.MultiSampleType, lDesc.MultiSampleQuality,
                    FALSE, &mpSurface, nullptr));
            return SUCCEEDED(lpDevice->CreateRenderTarget(luWidth, luHeight,
                lDesc.Format, lDesc.MultiSampleType, lDesc.MultiSampleQuality,
                FALSE, &mpSurface, nullptr));
        }
    };

    class PCFrameBuffer
    {
        IDirect3DSwapChain9* mpSwapChain = nullptr;
        IDirect3DSurface9* mpColour = nullptr;
        IDirect3DSurface9* mpDepth = nullptr;

    public:
        PCFrameBuffer() = default;
        PCFrameBuffer(const PCFrameBuffer&) = delete;
        PCFrameBuffer& operator=(const PCFrameBuffer&) = delete;
        ~PCFrameBuffer() { Release(); }
        void Swap(PCFrameBuffer& other)
        { std::swap(mpSwapChain,other.mpSwapChain);std::swap(mpColour,other.mpColour);std::swap(mpDepth,other.mpDepth); }

        void Release()
        {
            if (mpColour) mpColour->Release();
            if (mpDepth) mpDepth->Release();
            if (mpSwapChain) mpSwapChain->Release();
            mpColour = mpDepth = nullptr;
            mpSwapChain = nullptr;
        }

        bool Resize(IDirect3DDevice9* lpDevice, HWND lhWindow,
                    UINT luWidth, UINT luHeight, bool lbVSync)
        {
            if (!luWidth || !luHeight) return false;
            D3DPRESENT_PARAMETERS lParameters = {};
            lParameters.Windowed = TRUE;
            lParameters.SwapEffect = D3DSWAPEFFECT_COPY;
            lParameters.BackBufferFormat = D3DFMT_X8R8G8B8;
            lParameters.BackBufferWidth = luWidth;
            lParameters.BackBufferHeight = luHeight;
            lParameters.BackBufferCount = 1;
            lParameters.hDeviceWindow = lhWindow;
            lParameters.PresentationInterval = lbVSync
                ? D3DPRESENT_INTERVAL_DEFAULT : D3DPRESENT_INTERVAL_IMMEDIATE;
            PCFrameBuffer lPending;
            if (TextureUploadPC::IsExtended(lpDevice))
            {
                if (FAILED(lpDevice->CreateRenderTarget(luWidth,luHeight,D3DFMT_X8R8G8B8,
                    D3DMULTISAMPLE_NONE,0,FALSE,&lPending.mpColour,nullptr))) return false;
            }
            else if (FAILED(lpDevice->CreateAdditionalSwapChain(&lParameters, &lPending.mpSwapChain))
                || FAILED(lPending.mpSwapChain->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&lPending.mpColour)))
                return false;
            if (FAILED(lpDevice->CreateDepthStencilSurface(luWidth, luHeight, D3DFMT_D24S8,
                    D3DMULTISAMPLE_NONE, 0, FALSE, &lPending.mpDepth, nullptr)))
                return false;
            Swap(lPending);
            return true;
        }

        HRESULT GetBackBuffer(IDirect3DDevice9* lpDevice, IDirect3DSurface9** lppSurface) const
        {
            if (!mpColour)
                return lpDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, lppSurface);
            *lppSurface = mpColour;
            mpColour->AddRef();
            return S_OK;
        }

        void Bind(IDirect3DDevice9* lpDevice) const
        {
            lpDevice->SetDepthStencilSurface(nullptr);
            DepthRangePC::SetRenderTarget(lpDevice, 0, mpColour);
            lpDevice->SetDepthStencilSurface(mpDepth);
        }
    };
}

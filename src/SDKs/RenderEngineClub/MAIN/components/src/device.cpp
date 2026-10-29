#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/pixelbuffer.h"

D3DDevice* gpD3DDevice = 0;

namespace renderengine
{
    __declspec(align(16)) PixelBuffer gOriginalColorBuffer;
    __declspec(align(16)) PixelBuffer gOriginalDepthStencilBuffer;
    u32 guTilingEnabled = 0;

    // ARTIST 82B61540..82B616A8. Unknown modes deliberately preserve the
    // previous dimensions/refresh, and the bool's three padding bytes survive.
    void Device::Parameters::Initialize(u32 videoMode)
    {
        backBufferFormat = static_cast<PixelFormat>(D3DFMT_A8R8G8B8);
        frontBufferFormat = static_cast<PixelFormat>(D3DFMT_LE_X8R8G8B8);
        multiSampleType = D3DMULTISAMPLE_NONE;
        behaviorFlags = D3DCREATE_BUFFER_2_FRAMES;
        swapEffect = D3DSWAPEFFECT_DISCARD;
        depthStencilFormat = static_cast<PixelFormat>(D3DFMT_D24S8);
        depthStencilAutoCreate = true;
        numVsyncs = 1;
        flags = 0;
        frontBufferColorSpace = D3DCOLORSPACE_RGB;
        primaryRingBufferSize = 0;
        secondaryRingBufferSize = 0;
        ringBufferSegmentCount = 0;

        switch (videoMode)
        {
        case 0:
        case 2:
            backBufferWidth = frontBufferWidth = 640;
            backBufferHeight = frontBufferHeight = 480;
            refreshRate = 60;
            break;
        case 1:
            backBufferWidth = frontBufferWidth = 640;
            backBufferHeight = frontBufferHeight = 576;
            refreshRate = 50;
            break;
        case 3:
            backBufferWidth = frontBufferWidth = 720;
            backBufferHeight = frontBufferHeight = 480;
            refreshRate = 60;
            break;
        case 4:
            backBufferWidth = frontBufferWidth = 1280;
            backBufferHeight = frontBufferHeight = 720;
            refreshRate = 60;
            break;
        case 5:
            backBufferWidth = frontBufferWidth = 1920;
            backBufferHeight = frontBufferHeight = 1080;
            refreshRate = 60;
            break;
        case 6:
            {
                XVIDEO_MODE lVideoMode = {};
                CGS_STATIC_ASSERT((sizeof(XVIDEO_MODE) == 48), "ARTIST native video mode");
                XGetVideoMode(&lVideoMode);
                backBufferWidth = frontBufferWidth = lVideoMode.dwDisplayWidth;
                backBufferHeight = frontBufferHeight = lVideoMode.dwDisplayHeight;
                refreshRate = 60;
            }
            break;
        }

        videoScalerParameters.ScalerSourceRect.x1 = 0;
        videoScalerParameters.ScalerSourceRect.y1 = 0;
        videoScalerParameters.ScalerSourceRect.x2 = 0;
        videoScalerParameters.ScalerSourceRect.y2 = 0;
        videoScalerParameters.ScaledOutputWidth = 0;
        videoScalerParameters.ScaledOutputHeight = 0;
        videoScalerParameters.FilterProfile = 0;
    }

    RenderTargetState* Device::m_renderTargetState = 0;

    void Device::SetState(const RenderTargetState* lpState)
    {
        D3DDevice_SetSurfaces(gpD3DDevice, reinterpret_cast<const D3DSURFACES*>(lpState), 0);
    }

    void Device::BeginShaderStates(const ProgramVariableHandle& lrHandle,
                                  FloatShaderStateIterator& lrIterator)
    {
        D3DVECTOR4* lpConstants;
        if (lrHandle.mu8ShaderType)
            D3DDevice_BeginPixelShaderConstantF1(gpD3DDevice, lrHandle.mu8RegisterSet,
                &lpConstants, lrHandle.mu8RegisterCount);
        else
            D3DDevice_BeginVertexShaderConstantF1(gpD3DDevice, lrHandle.mu8RegisterSet,
                &lpConstants, lrHandle.mu8RegisterCount);
        lrIterator.m_dest = reinterpret_cast<rw::math::vpu::Vector4*>(lpConstants);
        lrIterator.m_handle = lrHandle;
    }

    // ARTIST 82B61F08..82B61F8C. GetTexture adds a reference; release it before
    // comparing the returned pointer and clearing each matching sampler.
    void Device::UnSetResource(D3DResource* lpResource)
    {
        D3DDevice* const lpDevice = gpD3DDevice;
        if (D3DResource_IsSet(lpResource, lpDevice))
        {
            for (DWORD luSampler = 0; luSampler < 26; ++luSampler)
            {
                D3DBaseTexture* const lpTexture = D3DDevice_GetTexture(lpDevice, luSampler);
                if (lpTexture)
                {
                    D3DResource_Release(lpTexture);
                    if (lpTexture == lpResource)
                    {
                        const UINT64 luPendingMask3 = (UINT64(1) << 63) >> (luSampler + 32);
                        D3DDevice_SetTexture(lpDevice, luSampler, 0, luPendingMask3);
                    }
                }
            }
        }
    }
}

#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/pixelbuffer.h"

D3DDevice* gpD3DDevice = 0;

namespace renderengine
{
    __declspec(align(16)) PixelBuffer gOriginalColorBuffer;
    __declspec(align(16)) PixelBuffer gOriginalDepthStencilBuffer;
    u32 guTilingEnabled = 0;

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

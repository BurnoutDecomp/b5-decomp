#pragma once

#include <d3d9.h>
#include <cstring>

// FLAG PC-platform leaf: D3D9 realization of the original per-unit sampler
// shadow (ARTIST shadow::Device::SetState, 0x822769E0 / 0x8227D158). World,
// post-effects and GUI share the device, so every native writer uses one shadow.
namespace renderengine
{
    class PCSamplerStateCache
    {
    public:
        struct Statistics
        {
            unsigned long long muRequests = 0;
            unsigned long long muUploads = 0;
            unsigned long long muSkipped = 0;
        } mStatistics;

        void Invalidate()
        {
            std::memset(mabValid, 0, sizeof(mabValid));
            mpDevice = nullptr;
        }

        template <class Device>
        HRESULT Set(Device* lpDevice, DWORD luSampler, D3DSAMPLERSTATETYPE leState, DWORD luValue)
        {
            ++mStatistics.muRequests;
            if (!lpDevice) return D3DERR_INVALIDCALL;
            if (mpDevice != lpDevice)
            {
                Invalidate();
                mpDevice = lpDevice;
            }
            // Sixteen pixel units, the displacement sampler, and four vertex
            // texture samplers. Keep their native IDs distinct in the shadow.
            const UINT luSlot = luSampler < 16u ? luSampler :
                (luSampler >= D3DDMAPSAMPLER && luSampler <= D3DVERTEXTEXTURESAMPLER3)
                    ? 16u + luSampler - D3DDMAPSAMPLER : 21u;
            const UINT luState = static_cast<UINT>(leState);
            const bool lbCanShadow = luSlot < 21u && luState >= D3DSAMP_ADDRESSU && luState <= D3DSAMP_DMAPOFFSET;
            if (lbCanShadow && mabValid[luSlot][luState] && mauValues[luSlot][luState] == luValue)
            {
                ++mStatistics.muSkipped;
                return S_OK;
            }
            ++mStatistics.muUploads;
            const HRESULT lResult = lpDevice->SetSamplerState(luSampler, leState, luValue);
            if (lbCanShadow)
            {
                mabValid[luSlot][luState] = SUCCEEDED(lResult);
                if (SUCCEEDED(lResult)) mauValues[luSlot][luState] = luValue;
            }
            return lResult;
        }

    private:
        const void* mpDevice = nullptr;
        DWORD mauValues[21][14] = {};
        bool mabValid[21][14] = {};
    };

    inline PCSamplerStateCache gPCSamplerStateCache;

    inline HRESULT PCSetSamplerState(IDirect3DDevice9* lpDevice, DWORD luSampler,
                                      D3DSAMPLERSTATETYPE leState, DWORD luValue)
    {
        return gPCSamplerStateCache.Set(lpDevice, luSampler, leState, luValue);
    }
}

#pragma once

#include <d3d9.h>
#include <cstring>

// FLAG PC-platform leaf: retain the original dispatcher's suppression of
// redundant constant uploads across the D3D9 world, immediate and GUI paths.
// ARTIST DrawRenderableMeshZOnly::Interpret (0x827F5AC8) compares each constant's
// latest push-buffer copy before writing Xenos registers. PC constant sources
// can be mutable, so shadow register VALUES rather than just source addresses.
namespace renderengine
{
    class PCShaderConstantCache
    {
    public:
        struct Statistics
        {
            unsigned long long muRequests = 0;
            unsigned long long muUploads = 0;
            unsigned long long muSkipped = 0;
            unsigned long long muUploadedRegisters = 0;
        } mStatistics;

        void Invalidate()
        {
            std::memset(mabVertexValid, 0, sizeof(mabVertexValid));
            std::memset(mabPixelValid, 0, sizeof(mabPixelValid));
            mpDevice = nullptr;
        }

        template <class Device>
        HRESULT Set(Device* lpDevice, bool lbPixel, UINT luFirst,
                    const float* lpfData, UINT luCount)
        {
            ++mStatistics.muRequests;
            if (!lpDevice)
                return D3DERR_INVALIDCALL;
            if (mpDevice != lpDevice)
            {
                Invalidate();
                mpDevice = lpDevice;
            }

            const UINT luCapacity = lbPixel ? 224u : 256u;
            float (*lpValues)[4] = lbPixel ? mafPixel : mafVertex;
            bool* lpValid = lbPixel ? mabPixelValid : mabVertexValid;
            const bool lbCanShadow = lpfData && luCount && luFirst < luCapacity &&
                                     luCount <= luCapacity - luFirst;
            if (lbCanShadow)
            {
                bool lbKnown = true;
                for (UINT lu = 0; lu < luCount; ++lu)
                {
                    if (!lpValid[luFirst + lu])
                    {
                        lbKnown = false;
                        break;
                    }
                }
                if (lbKnown && std::memcmp(lpValues[luFirst], lpfData, luCount * 4u * sizeof(float)) == 0)
                {
                    ++mStatistics.muSkipped;
                    return S_OK;
                }
            }

            ++mStatistics.muUploads;
            const HRESULT lResult = lbPixel
                ? lpDevice->SetPixelShaderConstantF(luFirst, lpfData, luCount)
                : lpDevice->SetVertexShaderConstantF(luFirst, lpfData, luCount);
            if (SUCCEEDED(lResult))
                mStatistics.muUploadedRegisters += luCount;
            if (lbCanShadow)
            {
                if (SUCCEEDED(lResult))
                    std::memcpy(lpValues[luFirst], lpfData, luCount * 4u * sizeof(float));
                // A failed upload must never establish a cached value.
                std::memset(lpValid + luFirst, SUCCEEDED(lResult) ? 1 : 0, luCount * sizeof(bool));
            }
            return lResult;
        }

    private:
        const void* mpDevice = nullptr;
        float mafVertex[256][4] = {};
        float mafPixel[224][4] = {};
        bool mabVertexValid[256] = {};
        bool mabPixelValid[224] = {};
    };

    // One shadow for the native device, shared by every constant writer. Adding
    // a raw D3D constant write or native state-block restore requires routing it
    // here or invalidating the shadow first. Shader changes retain constants.
    inline PCShaderConstantCache gPCShaderConstantCache;

    inline HRESULT PCSetVertexShaderConstantF(IDirect3DDevice9* lpDevice, UINT luFirst,
                                              const float* lpfData, UINT luCount)
    {
        return gPCShaderConstantCache.Set(lpDevice, false, luFirst, lpfData, luCount);
    }

    inline HRESULT PCSetPixelShaderConstantF(IDirect3DDevice9* lpDevice, UINT luFirst,
                                             const float* lpfData, UINT luCount)
    {
        return gPCShaderConstantCache.Set(lpDevice, true, luFirst, lpfData, luCount);
    }
}

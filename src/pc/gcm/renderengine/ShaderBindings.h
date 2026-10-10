#pragma once

#include <d3d9.h>
#include <cstdlib>

// FLAG PC-platform leaf: retain successful native shader/declaration bindings
// across the world, instancing, immediate and GUI paths. Console state shadows
// already suppress repeated state writes; native reassertion must share one
// shadow so another rendering path cannot leave a stale cached binding.
namespace renderengine
{
    class PCShaderBindingCache
    {
    public:
        struct Statistics
        {
            unsigned long long muRequests = 0, muNativeCalls = 0, muSkipped = 0;
            unsigned long long muVertexRequests = 0, muVertexSkipped = 0;
            unsigned long long muPixelRequests = 0, muPixelSkipped = 0;
            unsigned long long muDeclarationRequests = 0, muDeclarationSkipped = 0;
        } mStatistics;

        static bool Enabled()
        {
            static const bool sbEnabled = [] {
                const char* lpcValue = std::getenv("BRN_SHADER_BIND_CACHE");
                return !lpcValue || lpcValue[0] != '0';
            }();
            return sbEnabled;
        }

        void Invalidate()
        {
            mpDevice = nullptr;
            mVertex = {};
            mPixel = {};
            mDeclaration = {};
        }

        template<class DeviceType>
        HRESULT SetVertex(DeviceType* lpDevice, IDirect3DVertexShader9* lpShader)
        {
            return Set(lpDevice, lpShader, mVertex,
                [&] { return lpDevice->SetVertexShader(lpShader); },
                mStatistics.muVertexRequests, mStatistics.muVertexSkipped);
        }
        template<class DeviceType>
        HRESULT SetPixel(DeviceType* lpDevice, IDirect3DPixelShader9* lpShader)
        {
            return Set(lpDevice, lpShader, mPixel,
                [&] { return lpDevice->SetPixelShader(lpShader); },
                mStatistics.muPixelRequests, mStatistics.muPixelSkipped);
        }
        template<class DeviceType>
        HRESULT SetDeclaration(DeviceType* lpDevice, IDirect3DVertexDeclaration9* lpDeclaration)
        {
            return Set(lpDevice, lpDeclaration, mDeclaration,
                [&] { return lpDevice->SetVertexDeclaration(lpDeclaration); },
                mStatistics.muDeclarationRequests, mStatistics.muDeclarationSkipped);
        }
        template<class DeviceType>
        HRESULT SetFVF(DeviceType* lpDevice, DWORD luFvf)
        {
            if (!lpDevice) return D3DERR_INVALIDCALL;
            SelectDevice(lpDevice);
            // SetFVF installs a generated declaration even while a shader is
            // active. A later explicit declaration must be submitted again.
            mDeclaration = {};
            return lpDevice->SetFVF(luFvf);
        }

    private:
        template<class InterfaceType> struct Binding
        {
            InterfaceType* mpValue = nullptr;
            bool mbKnown = false;
        };
        // Borrowed identities: the device retains references to its bindings.
        // All native writers use these hooks; Apply/reset/creation invalidate.
        const void* mpDevice = nullptr;
        Binding<IDirect3DVertexShader9> mVertex;
        Binding<IDirect3DPixelShader9> mPixel;
        Binding<IDirect3DVertexDeclaration9> mDeclaration;

        void SelectDevice(const void* lpDevice)
        {
            if (mpDevice != lpDevice)
            {
                Invalidate();
                mpDevice = lpDevice;
            }
        }
        template<class DeviceType, class InterfaceType, class Setter>
        HRESULT Set(DeviceType* lpDevice, InterfaceType* lpValue, Binding<InterfaceType>& lrBinding,
                    const Setter& lrSetter, unsigned long long& lruRequests,
                    unsigned long long& lruSkipped)
        {
            ++mStatistics.muRequests;
            ++lruRequests;
            if (!lpDevice) return D3DERR_INVALIDCALL;
            SelectDevice(lpDevice);
            if (Enabled() && lrBinding.mbKnown && lrBinding.mpValue == lpValue)
            {
                ++mStatistics.muSkipped;
                ++lruSkipped;
                return S_OK;
            }
            ++mStatistics.muNativeCalls;
            const HRESULT lhResult = lrSetter();
            lrBinding.mbKnown = SUCCEEDED(lhResult);
            lrBinding.mpValue = lrBinding.mbKnown ? lpValue : nullptr;
            return lhResult;
        }
    };

    inline PCShaderBindingCache gPCShaderBindingCache;
    inline HRESULT PCSetVertexShader(IDirect3DDevice9* lpDevice, IDirect3DVertexShader9* lpShader)
    { return gPCShaderBindingCache.SetVertex(lpDevice, lpShader); }
    inline HRESULT PCSetPixelShader(IDirect3DDevice9* lpDevice, IDirect3DPixelShader9* lpShader)
    { return gPCShaderBindingCache.SetPixel(lpDevice, lpShader); }
    inline HRESULT PCSetVertexDeclaration(IDirect3DDevice9* lpDevice, IDirect3DVertexDeclaration9* lpDeclaration)
    { return gPCShaderBindingCache.SetDeclaration(lpDevice, lpDeclaration); }
    inline HRESULT PCSetFVF(IDirect3DDevice9* lpDevice, DWORD luFvf)
    { return gPCShaderBindingCache.SetFVF(lpDevice, luFvf); }
}

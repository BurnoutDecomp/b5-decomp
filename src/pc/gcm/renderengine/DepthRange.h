#pragma once
// FLAG PC-platform leaf: emulate Xenos inverted viewport depth on D3D9.
// The tested D3D9 backend clamps a requested [1,0] viewport to [1,1]. Use ordered depth bounds,
// reverse depth comparisons and mirror clear values instead. Engine state and
// shader clip positions stay unchanged; depth ordering is mathematically equivalent.
#include <d3d9.h>
#include <algorithm>
#include "pc/gcm/renderengine/reflections/RenderContext.h"
#include "pc/gcm/renderengine/shadows/RenderContext.h"

namespace renderengine { namespace DepthRangePC
{
    struct State
    {
        IDirect3DDevice9* mpDevice = nullptr;
        bool mbInverted = false;
        float mfMin = 0.0f, mfMax = 1.0f;
        D3DCMPFUNC meLogicalCompare = D3DCMP_LESSEQUAL;
    };
    inline State& GetState(IDirect3DDevice9* lpDevice)
    {
        static State sState;
        if (sState.mpDevice != lpDevice)
        {
            sState = State();
            sState.mpDevice = lpDevice;
            DWORD luCompare = D3DCMP_LESSEQUAL;
            if (lpDevice && SUCCEEDED(lpDevice->GetRenderState(D3DRS_ZFUNC, &luCompare)))
                sState.meLogicalCompare = static_cast<D3DCMPFUNC>(luCompare);
        }
        return sState;
    }
    inline D3DCMPFUNC NativeCompare(D3DCMPFUNC leCompare, bool lbInverted)
    {
        if (!lbInverted) return leCompare;
        switch (leCompare)
        {
        case D3DCMP_LESS: return D3DCMP_GREATER;
        case D3DCMP_LESSEQUAL: return D3DCMP_GREATEREQUAL;
        case D3DCMP_GREATER: return D3DCMP_LESS;
        case D3DCMP_GREATEREQUAL: return D3DCMP_LESSEQUAL;
        default: return leCompare;
        }
    }
    inline void SetDepthFunction(IDirect3DDevice9* lpDevice, D3DCMPFUNC leCompare)
    {
        State& lrState = GetState(lpDevice);
        lrState.meLogicalCompare = leCompare;
        lpDevice->SetRenderState(D3DRS_ZFUNC,
            CgsPC::Shadows::DepthFunction(CgsPC::Reflections::ResolveExtrasDepth(NativeCompare(leCompare, lrState.mbInverted))));
    }
    inline HRESULT SetRenderTarget(IDirect3DDevice9* lpDevice, DWORD luIndex,
                                    IDirect3DSurface9* lpSurface)
    {
        const HRESULT lhResult = lpDevice->SetRenderTarget(luIndex, lpSurface);
        if (SUCCEEDED(lhResult) && luIndex == 0u)
        {
            // D3D9 implicitly resets the viewport (including depth [0,1]) on
            // target 0 binds. BeginRenderAntiAliased relies on this handoff;
            // it does not issue an explicit viewport call after the cube pass.
            State& lrState = GetState(lpDevice);
            const bool lbWasInverted = lrState.mbInverted;
            lrState.mbInverted = false;
            lrState.mfMin = 0.0f;
            lrState.mfMax = 1.0f;
            if (lbWasInverted)
                lpDevice->SetRenderState(D3DRS_ZFUNC, lrState.meLogicalCompare);
        }
        return lhResult;
    }
    inline HRESULT SetViewport(IDirect3DDevice9* lpDevice, const D3DVIEWPORT9& lrViewport)
    {
        State& lrState = GetState(lpDevice);
        const bool lbInverted = lrViewport.MinZ > lrViewport.MaxZ;
        D3DVIEWPORT9 lNative = lrViewport;
        if (lbInverted) std::swap(lNative.MinZ, lNative.MaxZ);
        const HRESULT lhResult = lpDevice->SetViewport(&lNative);
        if (SUCCEEDED(lhResult))
        {
            const bool lbChanged = lrState.mbInverted != lbInverted;
            lrState.mbInverted = lbInverted;
            lrState.mfMin = lNative.MinZ;
            lrState.mfMax = lNative.MaxZ;
            // Reapply even if the engine's cached depth-state pointer is unchanged.
            if (lbChanged)
                lpDevice->SetRenderState(D3DRS_ZFUNC, NativeCompare(lrState.meLogicalCompare, lbInverted));
        }
        return lhResult;
    }
    inline float ClearDepth(IDirect3DDevice9* lpDevice, float lfDepth)
    {
        const State& lrState = GetState(lpDevice);
        return lrState.mbInverted ? lrState.mfMin + lrState.mfMax - lfDepth : lfDepth;
    }
} }

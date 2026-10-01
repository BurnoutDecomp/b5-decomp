#pragma once

#include <d3d9.h>
#include <climits>
#include <cstdlib>
#include "pc/gcm/renderengine/FrameProfilePCLeaf.h"

// FLAG PC-platform leaf: pooled native buffers can serve many console meshes.
// Avoid rebinding the same native stream/index buffer. The engine still emits
// every draw and its original index range. UP draws, state-block restoration
// and device initialization invalidate the corresponding cached bindings.
namespace renderengine::GeometryBindingsPC
{
    inline bool Enabled()
    {
        static const bool sbEnabled = [] {
            const char* lpcValue = std::getenv("BRN_GEOMETRY_BIND_CACHE");
            return !lpcValue || lpcValue[0] != '0';
        }();
        return sbEnabled;
    }

    struct VertexWindow
    {
        UINT muOffset;
        INT miBase;
    };

    inline VertexWindow Rebase(UINT luOffset, UINT luStride, INT liBase)
    {
        // World geometry uses stream zero; the only additional native stream
        // is per-instance data. Shifting complete vertices into the indexed
        // draw's base preserves offset + (base + index) * stride exactly.
        // Keep unusual/negative bases and unaligned layouts on the old path.
        if (!Enabled() || liBase < 0 || !luStride || (luStride & 3u) || (luOffset & 3u))
            return {luOffset, liBase};
        const UINT luVertices = luOffset / luStride;
        if (luVertices > static_cast<UINT>(INT_MAX - liBase))
            return {luOffset, liBase};
        return {luOffset % luStride, liBase + static_cast<INT>(luVertices)};
    }

    class Cache
    {
        const void* mpDevice = nullptr;
        IDirect3DVertexBuffer9* mpVertexBuffer = nullptr;
        IDirect3DIndexBuffer9* mpIndexBuffer = nullptr;
        UINT muOffset = 0, muStride = 0;
        bool mbVertexKnown = false, mbIndexKnown = false;

        void SelectDevice(const void* lpDevice)
        {
            if (mpDevice != lpDevice)
            {
                Invalidate();
                mpDevice = lpDevice;
            }
        }

    public:
        // Borrowed identities only: a known binding is retained by D3D itself.
        // Every operation which can unbind it must invalidate before that call.
        void Invalidate()
        {
            mpDevice = nullptr;
            mpVertexBuffer = nullptr;
            mpIndexBuffer = nullptr;
            mbVertexKnown = mbIndexKnown = false;
        }
        void InvalidateUP(const void* lpDevice, bool lbIndexed)
        {
            if (mpDevice != lpDevice) return;
            mbVertexKnown = false;
            if (lbIndexed) mbIndexKnown = false;
        }
        template<class DeviceType>
        HRESULT BindVertex(DeviceType* lpDevice, IDirect3DVertexBuffer9* lpBuffer,
                           UINT luOffset, UINT luStride)
        {
            if (!lpDevice) return D3DERR_INVALIDCALL;
            if (!Enabled())
            {
                FrameProfile::GeometryBinding(true, false);
                return lpDevice->SetStreamSource(0, lpBuffer, luOffset, luStride);
            }
            SelectDevice(lpDevice);
            const bool lbSkip = mbVertexKnown && mpVertexBuffer == lpBuffer
                && muOffset == luOffset && muStride == luStride;
            FrameProfile::GeometryBinding(true, lbSkip);
            if (lbSkip) return S_OK;
            const HRESULT lhResult = lpDevice->SetStreamSource(0, lpBuffer, luOffset, luStride);
            mbVertexKnown = SUCCEEDED(lhResult);
            if (mbVertexKnown)
            {
                mpVertexBuffer = lpBuffer;
                muOffset = luOffset;
                muStride = luStride;
            }
            return lhResult;
        }
        template<class DeviceType>
        HRESULT BindIndex(DeviceType* lpDevice, IDirect3DIndexBuffer9* lpBuffer)
        {
            if (!lpDevice) return D3DERR_INVALIDCALL;
            if (!Enabled())
            {
                FrameProfile::GeometryBinding(false, false);
                return lpDevice->SetIndices(lpBuffer);
            }
            SelectDevice(lpDevice);
            const bool lbSkip = mbIndexKnown && mpIndexBuffer == lpBuffer;
            FrameProfile::GeometryBinding(false, lbSkip);
            if (lbSkip) return S_OK;
            const HRESULT lhResult = lpDevice->SetIndices(lpBuffer);
            mbIndexKnown = SUCCEEDED(lhResult);
            if (mbIndexKnown) mpIndexBuffer = lpBuffer;
            return lhResult;
        }
    };

    inline Cache gCache;

    inline HRESULT DrawPrimitiveUP(IDirect3DDevice9* lpDevice, D3DPRIMITIVETYPE leType,
                                   UINT luCount, const void* lpVertices, UINT luStride)
    {
        gCache.InvalidateUP(lpDevice, false);
        return lpDevice->DrawPrimitiveUP(leType, luCount, lpVertices, luStride);
    }
    inline HRESULT DrawIndexedPrimitiveUP(IDirect3DDevice9* lpDevice, D3DPRIMITIVETYPE leType,
        UINT luMinVertex, UINT luNumVertices, UINT luCount, const void* lpIndices,
        D3DFORMAT leIndexFormat, const void* lpVertices, UINT luStride)
    {
        gCache.InvalidateUP(lpDevice, true);
        return lpDevice->DrawIndexedPrimitiveUP(leType, luMinVertex, luNumVertices, luCount,
            lpIndices, leIndexFormat, lpVertices, luStride);
    }
}

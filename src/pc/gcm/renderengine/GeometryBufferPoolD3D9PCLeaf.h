#pragma once

#include "pc/gcm/renderengine/GeometryBufferPoolPCLeaf.h"
#include "pc/gcm/renderengine/FrameProfilePCLeaf.h"
#include <d3d9.h>
#include <cstring>

// FLAG PC-platform leaf: D3D9 implementation of retained GPU page uploads.
namespace renderengine
{
    struct D3D9GeometryPoolBackend
    {
        using Context = IDirect3DDevice9*;
        using Buffer = IDirect3DResource9*;
        using Fence = IDirect3DQuery9*;
        bool Supported(Context lpDevice)
        {
            D3DCAPS9 lCaps = {};
            D3DDEVICE_CREATION_PARAMETERS lCreation = {};
            return lpDevice && SUCCEEDED(lpDevice->GetDeviceCaps(&lCaps))
                && SUCCEEDED(lpDevice->GetCreationParameters(&lCreation))
                && (lCaps.DevCaps2 & D3DDEVCAPS2_STREAMOFFSET)
                && !(lCreation.BehaviorFlags & D3DCREATE_SOFTWARE_VERTEXPROCESSING);
        }
        Buffer CreateBuffer(Context lpDevice, GeometryBufferKind leKind, unsigned luBytes)
        {
            const DWORD luUsage = D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY;
            if (leKind == GeometryBufferKind::Vertex)
            {
                IDirect3DVertexBuffer9* lpBuffer = nullptr;
                if (FAILED(lpDevice->CreateVertexBuffer(luBytes, luUsage, 0,
                                                       D3DPOOL_DEFAULT, &lpBuffer, nullptr))) return nullptr;
                FrameProfile::NativeBuffer();
                return lpBuffer;
            }
            IDirect3DIndexBuffer9* lpBuffer = nullptr;
            if (FAILED(lpDevice->CreateIndexBuffer(luBytes, luUsage,
                leKind == GeometryBufferKind::Index32 ? D3DFMT_INDEX32 : D3DFMT_INDEX16,
                D3DPOOL_DEFAULT, &lpBuffer, nullptr))) return nullptr;
            FrameProfile::NativeBuffer();
            return lpBuffer;
        }
        bool Upload(Buffer lpBuffer, GeometryBufferKind leKind, unsigned luOffset,
                    const void* lpData, unsigned luBytes, bool lbDiscard)
        {
            const DWORD luFlags = lbDiscard ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE;
            void* lpDestination = nullptr;
            HRESULT lResult;
            {
                FrameProfile::Scope lLockProfile(FrameProfile::GEOMETRY_LOCK);
                lResult = leKind == GeometryBufferKind::Vertex
                    ? static_cast<IDirect3DVertexBuffer9*>(lpBuffer)->Lock(luOffset, luBytes, &lpDestination, luFlags)
                    : static_cast<IDirect3DIndexBuffer9*>(lpBuffer)->Lock(luOffset, luBytes, &lpDestination, luFlags);
            }
            if (FAILED(lResult)) return false;
            if (lpDestination) std::memcpy(lpDestination, lpData, luBytes);
            {
                FrameProfile::Scope lUnlockProfile(FrameProfile::GEOMETRY_UNLOCK);
                lResult = leKind == GeometryBufferKind::Vertex
                    ? static_cast<IDirect3DVertexBuffer9*>(lpBuffer)->Unlock()
                    : static_cast<IDirect3DIndexBuffer9*>(lpBuffer)->Unlock();
            }
            return lpDestination && SUCCEEDED(lResult);
        }
        void DestroyBuffer(Buffer lpBuffer) { if (lpBuffer) lpBuffer->Release(); }
        Fence CreateFence(Context lpDevice)
        {
            IDirect3DQuery9* lpFence = nullptr;
            return SUCCEEDED(lpDevice->CreateQuery(D3DQUERYTYPE_EVENT, &lpFence)) ? lpFence : nullptr;
        }
        bool IssueFence(Fence lpFence) { return SUCCEEDED(lpFence->Issue(D3DISSUE_END)); }
        GeometryFenceStatus PollFence(Fence lpFence)
        {
            const HRESULT lResult = lpFence->GetData(nullptr, 0, 0); // no FLUSH, never wait
            return lResult == S_OK ? GeometryFenceStatus::Complete :
                lResult == S_FALSE ? GeometryFenceStatus::Pending : GeometryFenceStatus::Failed;
        }
        void DestroyFence(Fence lpFence) { if (lpFence) lpFence->Release(); }
    };
}

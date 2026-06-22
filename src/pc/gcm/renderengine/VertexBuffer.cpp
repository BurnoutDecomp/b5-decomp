#include "pc/gcm/renderengine/VertexBuffer.h"
#include "pc/gcm/renderengine/Xbox2VertexBufferShims.h"

#include <cstring>   // memset

// X360 reconstruction of renderengine::VertexBuffer. The bodies mirror the X360 image
// store-for-store: header dwords are written by name in the order the asm writes them, the
// XGSetVertexBufferHeader / D3D helper argument order is preserved, and the bit masks match
// the PPC rlwinm/clr operations exactly.

namespace renderengine
{
    // 0x82B62ED8 -- fill the GPU vertex-buffer header in place, then fix up its memory flags.
    VertexBufferHeader* VertexBuffer::Initialize(Wrapper* lpWrapper, const Parameters* lpParams,
                                                 int /*liArg3*/, int /*liArg4*/)
    {
        VertexBufferHeader* lpHeader = lpWrapper->mpHeader;   // v6 = *a1
        std::memset(lpHeader, 0, 0x28);                        // memset(*a1, 0, 40)

        const u32 luBaseFlags = lpHeader->muBaseAddress | 3u;  // v7 = v6[6] | 3
        lpHeader->muCommon = 1u;                               // *v6 = 1
        lpHeader->muFence = 0u;                                // v6[2] = 0
        lpHeader->muIdentifier = 0u;                           // v6[3] = 0
        lpHeader->muReferenceCount = 1u;                       // v6[1] = 1
        lpHeader->muUnused14 = 0xFFFF0000u;                    // v6[5] = -65536
        lpHeader->muBaseAddress = luBaseFlags;                 // v6[6] = v7
        lpHeader->muSize = lpParams->muLength;                 // v6[8] = a2[1]
        lpHeader->muFormat = lpParams->muFormat & 0xFFu;       // v6[9] = *a2  (clrlwi ...,24)

        XGSetVertexBufferHeader(static_cast<int>(lpParams->muLength), 0, 1,
                                static_cast<int>(lpWrapper->muBaseOffset), lpHeader);
        VertexBuffer::Xbox2CheckPhysicalMemoryFlags(reinterpret_cast<u32*>(lpHeader));
        return lpHeader;
    }

    // 0x8227D0A0 -- map the buffer for CPU writes, recording the lock state into lpLockInfoOut.
    int VertexBuffer::Lock(VertexBufferHeader* lpBuffer, int liFlags, LockInfo* lpLockInfoOut,
                           int liOffset, int liSize)
    {
        int liLockSize = liSize;                       // v5 = a5
        if (liSize == 0)
        {
            liLockSize = static_cast<int>(lpBuffer->muSize);   // v5 = *(a1 + 32)
        }
        VertexBuffer_Xbox2UnSet(lpBuffer);

        int liD3DFlags = 0;                            // v10 = 0
        if ((liFlags & 1) != 0)
        {
            if ((liFlags & 2) == 0)
            {
                if (XMemGetPageSize(reinterpret_cast<const void*>(static_cast<usize>(lpBuffer->muBaseAddress & 0xFFFFFFFCu))) <= 0x1000u)
                {
                    liD3DFlags = 16;                   // v10 = 16
                }
                // fall through to the lock
            }
            else if ((liFlags & 4) != 0)
            {
                liD3DFlags = 0x1000;                   // v10 = 4096
            }
        }
        else if ((liFlags & 2) != 0)
        {
            if ((liFlags & 4) != 0)
            {
                liD3DFlags = 0x1000;                   // v10 = 4096
            }
        }

        const int liMapped = D3DVertexBuffer_Lock(lpBuffer, 0, 0, liD3DFlags);  // v11
        lpLockInfoOut->muSize = static_cast<u32>(liLockSize);                   // a3[3] = v5
        lpLockInfoOut->muFlags = static_cast<u32>(liFlags);                     // a3[2] = a2
        lpLockInfoOut->mpBuffer = lpBuffer;                                     // a3[1] = a1
        lpLockInfoOut->mpBits = reinterpret_cast<void*>(static_cast<usize>(  // *a3 = v11 + a4
            static_cast<u32>(liMapped + liOffset)));
        return 1;
    }

    // 0x82B62F70 -- tear the GPU allocation down (block on the fence, invalidate the GPU cache).
    void VertexBuffer::Destruct(VertexBufferHeader* lpBuffer)
    {
        void* lpDevice = gpD3DDevice;                  // v2 = off_83271608
        if (!D3DResource_IsSet(lpBuffer, lpDevice))
        {
            if (lpBuffer->muFence != 0u)               // *(pThis + 2)
            {
                D3DResource_BlockUntilNotBusy(lpBuffer);
                lpBuffer->muFence = 0u;                // *(pThis + 2) = 0
            }
            D3DDevice_InvalidateGpuCache(lpDevice,
                reinterpret_cast<void*>(static_cast<usize>(lpBuffer->muBaseAddress & 0xFFFFFFFCu)),  // *(pThis+6) & ~3
                lpBuffer->muSize,                                                 // *(pThis+8)
                0u);
        }
    }

    // 0x82B62FF8 -- destruct then release the D3D reference; clear the header, keep the low flag bits.
    VertexBufferHeader* VertexBuffer::Release(VertexBufferHeader* lpBuffer)
    {
        VertexBufferHeader* lpResult = lpBuffer;       // pThis return value
        if (lpBuffer->muCommon != 0u)                  // *pThis
        {
            VertexBuffer::Destruct(lpBuffer);
            if ((lpBuffer->muCommon & 0x100000u) != 0u)  // *v1 & 0x100000
            {
                lpResult = reinterpret_cast<VertexBufferHeader*>(
                    static_cast<usize>(static_cast<u32>(D3DResource_Release(lpBuffer))));
            }
            const u32 luKeptFlags = lpBuffer->muBaseAddress & 3u;  // v2 = *(v1+6) & 3
            lpBuffer->muCommon = 0u;                                // *v1 = 0
            lpBuffer->muBaseAddress = luKeptFlags;                  // *(v1+6) = v2
        }
        return lpResult;
    }

    // 0x82B61130 -- read the serialised params back out (format @ +0x24 -> [0], size @ +0x20 -> [1]).
    VertexBufferHeader* VertexBuffer::GetParameters(VertexBufferHeader* lpBuffer, u32* lpParamsOut)
    {
        lpParamsOut[0] = lpBuffer->muFormat;   // *a2 = *(result + 36)
        lpParamsOut[1] = lpBuffer->muSize;     // a2[1] = *(result + 32)
        return lpBuffer;
    }

    // 0x82B63778 -- build the rw resource descriptor: four {size=0, align=1} entries seeded, then
    // slot0 = {4, 0x28} (the 0x28-byte header at align 4) and slot2 = {length, 4}.
    u64* VertexBuffer::GetResourceDescriptor(u64* lpDescriptorOut, int a2)
    {
        u32* lpDwords = reinterpret_cast<u32*>(lpDescriptorOut);  // v2 = result
        int liIndex = 4;                                          // v3 = 4
        do
        {
            --liIndex;
            lpDwords[0] = 0u;     // *v2 = 0
            lpDwords[1] = 1u;     // v2[1] = 1
            lpDwords += 2;        // v2 += 2
        }
        while (liIndex >= 0);

        // *result = 0x2800000004LL  -> low dword 4 (size), high dword 0x28 (align)
        lpDescriptorOut[0] = 0x0000002800000004ULL;
        // result[2] = (HIDWORD = *(a2 + 4)) : low dword 4 -> {4, *(a2+4)}
        const u32 luLength = *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(
            static_cast<usize>(static_cast<u32>(a2))) + 4);
        lpDescriptorOut[2] = (static_cast<u64>(luLength) << 32) | 4ULL;
        return lpDescriptorOut;
    }

    // 0x82B61148 -- set/clear the system-memory protection bit (0x200000) from the page protection.
    u32 VertexBuffer::Xbox2CheckPhysicalMemoryFlags(u32* lpHeaderDwords)
    {
        VertexBufferHeader* lpHeader = reinterpret_cast<VertexBufferHeader*>(lpHeaderDwords);
        u32 luResult = lpHeader->muBaseAddress & 0xFFFFFFFCu;   // a1[6] & ~3
        if (luResult != 0u)
        {
            luResult = XQueryMemoryProtect(luResult);
            const u32 luCommon = lpHeader->muCommon;            // v3 = *a1
            u32 luNew;
            if ((luResult & 0x600u) != 0u)
            {
                luNew = luCommon & 0xFFDFFFFFu;                 // v3 & ~0x200000
            }
            else
            {
                luNew = luCommon | 0x00200000u;                // v3 | 0x200000
            }
            lpHeader->muCommon = luNew;                         // *a1 = v4
        }
        return luResult;
    }
}

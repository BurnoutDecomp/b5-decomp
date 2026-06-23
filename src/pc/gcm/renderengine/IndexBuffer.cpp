#include "pc/gcm/renderengine/IndexBuffer.h"
#include "pc/gcm/renderengine/Xbox2VertexBufferShims.h"   // XMemGetPageSize / XQueryMemoryProtect

#include <cstring>   // memset
#include <cstddef>   // offsetof

// X360 reconstruction of renderengine::IndexBuffer. The bodies mirror the X360 image store-for-
// store: the header dwords are written by name in the order the asm writes them, the index-format
// tag (0x20000000 16-bit / 0xC0000000 32-bit) | 2 reproduces the lis/ori immediates, the rounded-up
// size math matches the PPC addi/clrrwi, and the bit masks match the rlwinm/clr operations exactly.

namespace renderengine
{
    static_assert(offsetof(IndexBufferHeader, muCommon)        == 0x00, "IndexBufferHeader layout");
    static_assert(offsetof(IndexBufferHeader, muReferenceCount)== 0x04, "IndexBufferHeader layout");
    static_assert(offsetof(IndexBufferHeader, muField14)       == 0x14, "IndexBufferHeader layout");
    static_assert(offsetof(IndexBufferHeader, muBaseAddress)   == 0x18, "IndexBufferHeader layout");
    static_assert(offsetof(IndexBufferHeader, muSizeRounded)   == 0x1C, "IndexBufferHeader layout");
    static_assert(offsetof(IndexBufferHeader, muIndexCount)    == 0x20, "IndexBufferHeader layout");
    static_assert(sizeof(IndexBufferHeader) == 0x24, "IndexBufferHeader is 36 bytes");

    // 0x82B60730 -- read the serialised index params back out.
    IndexBufferHeader* IndexBuffer::GetParameters(IndexBufferHeader* lpBuffer, IndexBufferParamsOut* lpOut)
    {
        lpOut->muField00 = 0u;                                          // *a2 = 0
        // clrrwi/subfic/subfe/rlwinm(0,27,27)/+0x10 == (*result high bit set ? 32 : 16).
        lpOut->muBits  = (static_cast<s32>(lpBuffer->muCommon) >= 0) ? 16u : 32u;  // a2[1]
        lpOut->muCount = lpBuffer->muIndexCount;                        // a2[2] = result[8] (+0x20)
        return lpBuffer;
    }

    // 0x82B62100 -- fill the GPU index-buffer header in place, then fix up its memory flags.
    IndexBufferHeader* IndexBuffer::Initialize(IndexBufferWrapper* lpWrapper, const IndexBufferParameters* lpParams)
    {
        IndexBufferHeader* lpHeader = lpWrapper->mpHeader;     // v4 = *a1
        std::memset(lpHeader, 0, 0x24);                         // memset(*a1, 0, 36)

        lpHeader->muReferenceCount = 1u;                       // v4[1] = 1
        lpHeader->muCommon = 2u;                               // *v4 = 2  (overwritten below)
        lpHeader->muField14 = 0xFFFF0000u;                     // v4[5] = -65536
        lpHeader->muBaseAddress = lpWrapper->muBaseAddress;    // v4[6] = a1[2]

        u32 luBytesPerIndex = 2u;                              // v5 = 2
        if (lpParams->muFormat != 16u)                         // *(a2 + 4) != 16
        {
            luBytesPerIndex = 4u;                              // v5 = 4
        }
        lpHeader->muSizeRounded = (luBytesPerIndex * lpParams->muCount + 15u) & 0xFFFFFFF0u;  // v4[7]
        lpHeader->muIndexCount = lpParams->muCount;            // v4[8] = *(a2 + 8)

        u32 luFormatTag = 0xC0000000u;                         // v6 = -1073741824 (lis -0x4000)
        if (lpParams->muFormat != 32u)                         // *(a2 + 4) != 32
        {
            luFormatTag = 0x20000000u;                         // v6 = 0x20000000
        }
        lpHeader->muCommon = luFormatTag | 2u;                 // *v4 = v6 | 2

        IndexBuffer::Xbox2CheckPhysicalMemoryFlags(reinterpret_cast<u32*>(lpHeader));
        return lpHeader;
    }

    // 0x82B60760 -- map the buffer for CPU writes, recording the lock state.
    int IndexBuffer::Lock(IndexBufferHeader* lpBuffer, char liFlags, IndexBufferLockInfo* lpLockInfoOut)
    {
        const u32 luAddress = lpBuffer->muBaseAddress;         // v5 = a1[6] (+0x18)
        if (luAddress == 0u)
        {
            return 0;                                          // LABEL_11 -> result = 0
        }

        int liD3DFlags = 0;                                    // v6 = 0
        if ((liFlags & 1) != 0)
        {
            if ((liFlags & 2) == 0)
            {
                if (XMemGetPageSize(reinterpret_cast<const void*>(static_cast<usize>(luAddress))) <= 0x1000u)
                {
                    liD3DFlags = 16;                           // v6 = 16
                }
                // fall through to the lock (LABEL_9)
            }
            else if ((liFlags & 8) != 0)
            {
                liD3DFlags = 0x1000;                           // v6 = 4096
            }
        }
        else if ((liFlags & 2) != 0)
        {
            if ((liFlags & 8) != 0)
            {
                liD3DFlags = 0x1000;                           // v6 = 4096
            }
        }

        const int liMapped = D3DIndexBuffer_Lock(lpBuffer, 0, 0, liD3DFlags);  // v7
        lpLockInfoOut->mpBits = reinterpret_cast<void*>(static_cast<usize>(static_cast<u32>(liMapped)));  // *a3 = v7
        if (liMapped == 0)
        {
            return 0;                                          // LABEL_11
        }

        // a3[1] = *a1 >= 0 ? 1 : 6 (andi. 5 / +1 against the sign-extended high bit).
        lpLockInfoOut->muStride = (static_cast<s32>(lpBuffer->muCommon) >= 0) ? 1u : 6u;
        return 1;
    }

    // 0x82B60818 -- set/clear the system-memory protection bit (0x200000) on muCommon.
    u32 IndexBuffer::Xbox2CheckPhysicalMemoryFlags(u32* lpHeaderDwords)
    {
        IndexBufferHeader* lpHeader = reinterpret_cast<IndexBufferHeader*>(lpHeaderDwords);
        u32 luResult = lpHeader->muBaseAddress;                // result = a1[6] (+0x18, lwz 0x18)
        if (luResult != 0u)
        {
            luResult = XQueryMemoryProtect(luResult);          // result = XQueryMemoryProtect()
            const u32 luCommon = lpHeader->muCommon;           // v3 = *a1
            u32 luNew;
            if ((luResult & 0x600u) != 0u)                     // rlwinm. mask 0x600
            {
                luNew = luCommon & 0xFFDFFFFFu;                // v4 = v3 & ~0x200000 (rlwinm 0,11,9)
            }
            else
            {
                luNew = luCommon | 0x00200000u;                // v4 = v3 | 0x200000 (oris 0x20)
            }
            lpHeader->muCommon = luNew;                        // *a1 = v4
        }
        return luResult;
    }
}

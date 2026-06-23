#include "pc/gcm/renderengine/VertexBufferHelper.h"
#include "pc/gcm/renderengine/RenderEngine.h"   // renderengine::VertexFormatGetStride

#include <cstddef>   // offsetof

// X360 reconstruction of renderengine::VertexBufferHelper. The bodies mirror the X360 image:
// CalculateStride accumulates VertexFormatGetStride over the format array exactly as the loop does,
// Lock multiplies the per-vertex stride (+0x50) by the vertex offset/count and forwards to
// VertexBuffer::Lock with the same argument order, normalising a nonzero byte-result to 1, and
// Unlock is the D3DVertexBuffer_Unlock tail call.

namespace renderengine
{
    static_assert(offsetof(VertexBufferFormat, muVertexStride) == 0x50, "VertexBufferFormat stride @ +0x50");

    // 0x82B63E08 -- sum the per-element strides for an array of vertex-format codes.
    int VertexBufferHelper::CalculateStride(const u32* lpFormatCodes, int liCount)
    {
        int liTotal = 0;                                   // v2 = 0
        if (liCount != 0)
        {
            int liLeft = liCount;                          // v4 = a2
            const u32* lpCode = lpFormatCodes;             // a1
            do
            {
                const int liStride = VertexFormatGetStride(static_cast<int>(*lpCode));  // *a1
                --liLeft;                                  // --v4
                ++lpCode;                                  // ++a1
                liTotal += liStride;                       // v2 += Stride
            }
            while (liLeft != 0);
        }
        return liTotal;
    }

    // 0x82B63FA8 -- lock a vertex range, converting vertex offset/count to bytes via +0x50.
    int VertexBufferHelper::Lock(VertexBufferHeader* lpBuffer, VertexBuffer::LockInfo* lpLockInfoOut,
                                 const VertexBufferFormat* lpFormat, int liFlags,
                                 int liFirstVertex, int liVertexCount)
    {
        const u32 luStride = lpFormat->muVertexStride;     // v?? = *(a3 + 80)
        const int liByteOffset = static_cast<int>(luStride) * liFirstVertex;  // stride * a5
        const int liByteSize   = static_cast<int>(luStride) * liVertexCount;  // stride * a6

        const int liResult = VertexBuffer::Lock(lpBuffer, liFlags, lpLockInfoOut, liByteOffset, liByteSize);
        if ((liResult & 0xFF) != 0)                        // clrlwi. r11, r3, 24
        {
            return 1;
        }
        return liResult;                                   // 0 on failure
    }

    // 0x82B63FF0 -- unlock the buffer (tail call).
    void VertexBufferHelper::Unlock(VertexBufferHeader* lpBuffer)
    {
        D3DVertexBuffer_Unlock(lpBuffer);
    }
}

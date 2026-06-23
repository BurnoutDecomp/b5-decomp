#pragma once

#include "types.hpp"
#include "pc/gcm/renderengine/VertexBuffer.h"   // VertexBuffer / VertexBufferHeader / LockInfo

// renderengine::VertexBufferHelper -- thin convenience helpers layered over renderengine::VertexBuffer
// and the vertex-format machinery. CalculateStride sums per-element strides for a vertex-format array,
// Lock maps a vertex range (converting vertex offset/count to byte offset/size via the format's
// per-vertex stride and forwarding to VertexBuffer::Lock), and Unlock tail-calls the D3D unlock.
// Member accesses are by name; the per-vertex stride is read from the format object at +0x50.
//
//   CalculateStride   0x82B63E08
//   Lock              0x82B63FA8
//   Unlock            0x82B63FF0

namespace renderengine
{
    // The vertex-format object Lock indexes for the per-vertex byte stride (read at +0x50). Only the
    // dword the helper touches is named; the rest of the format object is preserved opaquely.
    struct VertexBufferFormat
    {
        u8  mauOpaque[0x50];   // +0x00 .. +0x4F  (format/declaration state owned elsewhere)
        u32 muVertexStride;    // +0x50  bytes per vertex
    };

    class VertexBufferHelper
    {
    public:
        // 0x82B63E08 -- sum VertexFormatGetStride over an array of luCount format codes. Returns the
        // total stride (0 when luCount == 0).
        static int CalculateStride(const u32* lpFormatCodes, int liCount);

        // 0x82B63FA8 -- lock a vertex range. Converts the vertex offset / count (liFirstVertex /
        // liVertexCount) to byte offset / size using the format's per-vertex stride (+0x50) and
        // forwards to VertexBuffer::Lock. Returns 1 on success (the byte-result is normalised to 1),
        // 0 otherwise.
        static int Lock(VertexBufferHeader* lpBuffer, VertexBuffer::LockInfo* lpLockInfoOut,
                        const VertexBufferFormat* lpFormat, int liFlags,
                        int liFirstVertex, int liVertexCount);

        // 0x82B63FF0 -- unlock the buffer (tail-call to the D3D vertex-buffer unlock).
        static void Unlock(VertexBufferHeader* lpBuffer);
    };

    // Xenon D3D vertex-buffer unlock. Platform external -- only the signature the call site needs is
    // declared. Not reconstructed here.
    void D3DVertexBuffer_Unlock(VertexBufferHeader* lpThis);
}

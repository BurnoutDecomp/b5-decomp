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
        // VertexBufferHelper::Parameters -- the descriptor a caller fills in to request a vertex
        // buffer: a vertex count plus an inline array of vertex-format element codes terminated by
        // -1 (0xFFFFFFFF). CalculateBufferSize walks the array to count the elements, asks
        // CalculateStride for the per-vertex byte stride, and caches both the element count and the
        // stride alongside the total byte size. The field offsets are reproduced from the X360 image
        // (CalculateBufferSize @0x823F8BF0):
        //   +0x04  muBufferSizeBytes  (= vertex count * stride)
        //   +0x08  muVertexCount      (caller-supplied)
        //   +0x0C  mauFormatCodes[N]  (inline, -1 terminated)  -- element [0] at +0x0C
        //   +0x4C  muFormatCount      (count of codes before the -1 terminator)
        //   +0x50  muVertexStride     (= CalculateStride(mauFormatCodes, muFormatCount))
        // The inline format-code array spans +0x0C..+0x48 (16 dwords): the loop indexes a1[v4+3]
        // with v4 reaching at most the index of the terminator, so the array holds up to 16 codes
        // (the last slot at +0x48, with the +0x4C count word immediately after).
        struct Parameters
        {
            u32 muOpaque00;          // +0x00 (unread by CalculateBufferSize)
            u32 muBufferSizeBytes;   // +0x04 computed: vertex count * per-vertex stride
            u32 muVertexCount;       // +0x08 caller-supplied vertex count
            u32 mauFormatCodes[16];  // +0x0C inline format-code array, 0xFFFFFFFF terminated
            u32 muFormatCount;       // +0x4C computed: number of codes before the terminator
            u32 muVertexStride;      // +0x50 computed: per-vertex byte stride

            // 0x823F8BF0 -- count the format codes up to the -1 terminator, compute the per-vertex
            // stride via VertexBufferHelper::CalculateStride, cache the stride (+0x50) and the total
            // byte size (+0x04 = vertex count * stride), and return the stride.
            int CalculateBufferSize();
        };

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

#pragma once

#include "types.hpp"

// renderengine::VertexBuffer -- the platform vertex-buffer object. On X360 it wraps the
// Direct3D "vertex buffer header" (a GPUVERTEXBUFFER, derived from the D3DResource header):
// Initialize fills the header in place via XGSetVertexBufferHeader, Lock maps it for a CPU
// write, and Destruct/Release tear the GPU-resident allocation down. The reconstruction
// preserves the X360 object layout and store order so the resource-descriptor / fix-up
// pipeline marshals the same bytes; member accesses are by name.
//
//   Initialize                  0x82B62ED8
//   Lock                        0x8227D0A0
//   Destruct                    0x82B62F70
//   Release                     0x82B62FF8
//   GetParameters               0x82B61130
//   GetResourceDescriptor       0x82B63778
//   Xbox2CheckPhysicalMemoryFlags 0x82B61148

// The Direct3D vertex-buffer header (D3DVertexBuffer == GPUVERTEXBUFFER, derived from the
// D3DResource header). Only the fields the renderengine paths touch are named; the rest of
// the 40-byte GPU header is preserved opaquely. Offsets are the X360 D3D9 header offsets.
struct D3DResource;

namespace renderengine
{
    // The on-GPU vertex-buffer header, 40 (0x28) bytes, written by XGSetVertexBufferHeader.
    // Named after the dwords the renderengine code reads/writes; the GPU sets the others.
    struct VertexBufferHeader
    {
        u32 muCommon;        // +0x00  D3DResource Common (resource type + flags; bit20 0x100000 = owns D3D ref)
        u32 muReferenceCount;// +0x04
        u32 muFence;         // +0x08  GPU busy fence (BlockUntilNotBusy clears it)
        u32 muIdentifier;    // +0x0C
        u32 muBaseFlush;     // +0x10
        u32 muUnused14;      // +0x14  (Initialize seeds 0xFFFF0000)
        u32 muBaseAddress;   // +0x18  GPU base address | low-2-bit memory flags
        u32 muUnused1C;      // +0x1C
        u32 muSize;          // +0x20  buffer size in bytes
        u32 muFormat;        // +0x24  low byte = endian/format flag
    };

    class VertexBuffer
    {
    public:
        // The outer wrapper Initialize is handed: +0 -> the GPU header, +8 -> the GPU base
        // address/offset XGSetVertexBufferHeader is seeded with. (X360: a1[0]=header, a1[2]=offset.)
        struct Wrapper
        {
            VertexBufferHeader* mpHeader;   // +0x00
            u32                 muPad04;     // +0x04
            u32                 muBaseOffset;// +0x08  (a1[2])
        };

        // The parameters block Initialize reads: a2[0] low byte -> format, a2[1] -> length.
        struct Parameters
        {
            u32 muFormat;   // +0x00  (only low byte used)
            u32 muLength;   // +0x04
        };

        // The Lock output descriptor (a3[0..3]).
        struct LockInfo
        {
            void*               mpBits;    // +0x00  mapped pointer + caller offset
            VertexBufferHeader* mpBuffer;  // +0x04  the locked buffer header
            u32                 muFlags;   // +0x08  the requested lock flags
            u32                 muSize;    // +0x0C  the locked size
        };

        static VertexBufferHeader* Initialize(Wrapper* lpWrapper, const Parameters* lpParams,
                                              int liArg3, int liArg4);
        static int  Lock(VertexBufferHeader* lpBuffer, int liFlags, LockInfo* lpLockInfoOut,
                         int liOffset, int liSize);
        static void Destruct(VertexBufferHeader* lpBuffer);
        static VertexBufferHeader* Release(VertexBufferHeader* lpBuffer);

        // GetParameters reads the serialised raster params back out (format @+0x24, size @+0x20).
        static VertexBufferHeader* GetParameters(VertexBufferHeader* lpBuffer, u32* lpParamsOut);

        // GetResourceDescriptor builds the rw resource descriptor (five {size, align} entries) for
        // a vertex buffer: slot0 = the 0x28-byte header, slot2 = the buffer bytes (length from a2).
        static u64* GetResourceDescriptor(u64* lpDescriptorOut, int a2);

        // Set the system/graphics memory-protection flag (bit 0x200000) from the GPU page's
        // protection class; returns the queried protection.
        static u32  Xbox2CheckPhysicalMemoryFlags(u32* lpHeaderDwords);
    };
}

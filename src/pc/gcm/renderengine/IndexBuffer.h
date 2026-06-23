#pragma once

#include "types.hpp"

// renderengine::IndexBuffer -- the platform index-buffer object. On X360 it wraps the Direct3D
// "index buffer header" (a GPUINDEXBUFFER derived from the D3DResource header). Initialize fills the
// header in place from a parameters block (16- or 32-bit indices), Lock maps it for a CPU write, and
// Xbox2CheckPhysicalMemoryFlags fixes its memory-protection bit. The reconstruction preserves the
// X360 object layout and store order so the resource-descriptor / fix-up pipeline marshals the same
// bytes; member accesses are by name.
//
//   GetParameters                 0x82B60730
//   Initialize                    0x82B62100
//   Lock                          0x82B60760
//   Xbox2CheckPhysicalMemoryFlags 0x82B60818

namespace renderengine
{
    // The on-GPU index-buffer header, 36 (0x24) bytes (memset 0x24 by Initialize). Named after the
    // dwords the renderengine code reads/writes; the GPU sets the rest. The +0x18 base-address dword
    // carries the GPU address with its low memory flags, exactly like VertexBufferHeader.
    struct IndexBufferHeader
    {
        u32 muCommon;        // +0x00  D3DResource Common: index-format tag (0x20000000 16-bit /
                             //         0xC0000000 32-bit) | 2; high bit selects the index width.
        u32 muReferenceCount;// +0x04  (=1)
        u32 muField08;       // +0x08
        u32 muField0C;       // +0x0C
        u32 muField10;       // +0x10
        u32 muField14;       // +0x14  (Initialize seeds 0xFFFF0000)
        u32 muBaseAddress;   // +0x18  GPU base address | low memory flags (a1[2])
        u32 muSizeRounded;   // +0x1C  ((bytesPerIndex * count + 15) & ~15)
        u32 muIndexCount;    // +0x20  index count
    };

    // The parameters block Initialize reads: format width @ +0x04 (16 or 32), index count @ +0x08.
    struct IndexBufferParameters
    {
        u32 muField00;   // +0x00
        u32 muFormat;    // +0x04  index width in bits (16 or 32)
        u32 muCount;     // +0x08  index count
    };

    // The wrapper Initialize is handed: +0x00 -> the GPU header, +0x08 -> the GPU base address (a1[2]).
    struct IndexBufferWrapper
    {
        IndexBufferHeader* mpHeader;     // +0x00
        u32                muPad04;      // +0x04
        u32                muBaseAddress;// +0x08  (a1[2])
    };

    // GetParameters output (a2[0..2]): [0] = 0, [1] = bits-per-index (16/32), [2] = index count.
    struct IndexBufferParamsOut
    {
        u32 muField00;   // +0x00  (=0)
        u32 muBits;      // +0x04  16 (when muCommon >= 0) or 32
        u32 muCount;     // +0x08  index count (header +0x20)
    };

    // The Lock output descriptor (a3[0..1]).
    struct IndexBufferLockInfo
    {
        void* mpBits;    // +0x00  mapped pointer
        u32   muStride;  // +0x04  1 (16-bit) or 6 (32-bit), derived from the index width
    };

    class IndexBuffer
    {
    public:
        // 0x82B60730 -- read the serialised index params out (width @+0x04, count @+0x08).
        static IndexBufferHeader* GetParameters(IndexBufferHeader* lpBuffer, IndexBufferParamsOut* lpOut);

        // 0x82B62100 -- fill the GPU index-buffer header in place from the parameters block, then fix
        // up its memory-protection flags.
        static IndexBufferHeader* Initialize(IndexBufferWrapper* lpWrapper, const IndexBufferParameters* lpParams);

        // 0x82B60760 -- map the buffer for CPU writes, recording the lock state into lpLockInfoOut.
        // Returns 1 on success, 0 if the buffer has no GPU address or the D3D lock failed.
        static int Lock(IndexBufferHeader* lpBuffer, char liFlags, IndexBufferLockInfo* lpLockInfoOut);

        // 0x82B60818 -- set/clear the system-memory protection bit (0x200000) on muCommon from the
        // GPU page's protection class; returns the queried protection (0 if no base address).
        static u32 Xbox2CheckPhysicalMemoryFlags(u32* lpHeaderDwords);
    };

    // Xenon D3D index-buffer lock: map the buffer for CPU access, returning the mapped pointer.
    // Platform external -- only the signature the call site needs is declared.
    int D3DIndexBuffer_Lock(IndexBufferHeader* lpThis, int liOffsetToLock, int liSizeToLock, int liFlags);
}

#include "SDKs/D3D/CRingAllocList.h"
#include "SDKs/D3D/CDevice.h"

#include <cstddef> // offsetof (uncalled _CRingAllocListAssertLayout)
#include <windows.h> // MemoryBarrier (host mapping of the X360 eieio ordering op)

// ===========================================================================
// D3D::CRingAllocList -- reconstructed from BURNOUT_X360_ARTIST.XEX. See
// CRingAllocList.h for the layout and the chunk-chain design.
//
// Source-of-truth: X360 asm only (no DWARF, no reference source). Every method
// below is faithful pointer/link bookkeeping with cache-coherency barriers; no
// control flow was added.
// ===========================================================================

namespace D3D
{

// D3D::FlushCachedMemory(startPhys, endPhys, flags) -- flush the CPU cache over a
// GPU-visible physical-address range. Sibling D3D free function (its body is its
// own TU); declared here as an extern so the bodied methods compile and link
// against it, mirroring the declaration in CCommandBuffer.h.
extern s32 FlushCachedMemory(u32 uStartPhys, u32 uEndPhys, s32 iFlags);

// FLAG PC-platform leaf: X360 CPU-virtual -> GPU-visible physical-mirror address
// translation. The GPU reads the write-combined physical alias of a CPU buffer;
// this is the fixed X360 formula the asm inlines (srwi/addi/rlwinm/subf) and is
// reproduced verbatim. Meaningless off the X360 -- fixed hardware behaviour.
// (Same platform helper as CCommandBuffer.cpp; kept file-local by design.)
static inline u32 ToGpuPhysical(u32 uVirtual)
{
    return (((uVirtual >> 20) + 512) & 0x1000) + (uVirtual & 0x1FFFFFFFu) - 0x40000000u;
}

// --- @ 0x829468A8 ----------------------------------------------------------
// Bind the owning device, reset every chain cursor, then serve the first chunk
// via MakeSpace and hand back the write cursor it returns.
u32 CRingAllocList::Initialize(CDevice* pDevice)
{
    mpDevice      = pDevice;
    muCurrentNode = 0;
    muWritePtr    = 0;
    muWriteEnd    = 0;
    muHeadChunk   = 0;
    return MakeSpace();
}

// --- @ 0x82946638 ----------------------------------------------------------
// Close off the current chunk and chain in a fresh one. If a node is already
// live, patch its forward link to the current write cursor. Pull a new
// 0x1080-dword, 0x80-aligned chunk from the device ring: on success write its
// self-terminating link header, splice it in (patch the previous write slot, or
// set the head when this is the first chunk) and cache-flush the whole chunk;
// on failure fall back to the device out-of-memory chunk and drop the head.
// Either way, republish the cursors from the new base and return base + 4.
u32 CRingAllocList::MakeSpace()
{
    if (muCurrentNode != 0)
        *reinterpret_cast<u32*>(muCurrentNode) = ToGpuPhysical(muWritePtr);

    u32 uChunk = static_cast<u32>(mpDevice->BeginRingAlloc(0x1080, 0x80));
    u32 uBase  = uChunk;

    if (uChunk != 0)
    {
        // Seed the new chunk's link header to its own body start.
        *reinterpret_cast<u32*>(uChunk) = ToGpuPhysical(uChunk + 4);

        if (muWritePtr != 0)
            *reinterpret_cast<u32*>(muWritePtr) = ToGpuPhysical(uChunk);
        else
            muHeadChunk = uChunk;

        u32 uPhys = ToGpuPhysical(uChunk);
        FlushCachedMemory(uPhys, uPhys + 0x1080, 0);
    }
    else
    {
        uBase       = mpDevice->muOutOfMemoryBase;
        muHeadChunk = 0;
    }

    muCurrentNode = uBase;
    muWritePtr    = uBase + 4;
    muWriteEnd    = uBase + 0x107C;
    return uBase + 4;
}

// --- @ 0x82945A08 ----------------------------------------------------------
// Splice a caller-supplied chunk node (from an indirect buffer) into the chain.
// Write the node's forward link to the resume region, remember the previous
// write cursor, then: record the node as current, advance the write cursor to
// uNextWrite - 4. eieio-ordered, patch the previous write slot to point at the
// new node, and cache-flush both the node's link word and the patched slot.
s32 CRingAllocList::AddChunkNode(u32 uNode, u32 uNextWrite)
{
    u32 uPrevWrite = muWritePtr;

    u32 uNodePhys  = ToGpuPhysical(uNode);
    u32 uWritePhys = ToGpuPhysical(uNextWrite);

    *reinterpret_cast<u32*>(uNode) = uWritePhys;

    muCurrentNode = uNode;
    muWritePtr    = uNextWrite - 4;

    u32 uPrevPhys = ToGpuPhysical(uPrevWrite);

    ::MemoryBarrier(); // eieio: order the node-link store ahead of the flush
    FlushCachedMemory(uNodePhys, uWritePhys, 0);

    *reinterpret_cast<u32*>(uPrevWrite) = uNodePhys;

    ::MemoryBarrier(); // eieio: order the write-slot patch ahead of the flush
    return FlushCachedMemory(uPrevPhys, uPrevPhys + 16, 0);
}

// --- @ 0x829479D8 ----------------------------------------------------------
// Return the GPU-physical-mirror address of the head chunk, or 0 when empty.
u32 CRingAllocList::GetFirstChunk()
{
    if (muHeadChunk != 0)
        return ToGpuPhysical(muHeadChunk);
    return 0;
}

// --- @ 0x82945998 ----------------------------------------------------------
// Seal the chain. When a node is live, patch its forward link to the final write
// cursor and stamp the write slot with the 0xC0000000 GPU end marker. Always
// clear the write end limit.
void CRingAllocList::Finalize()
{
    if (muCurrentNode != 0)
    {
        *reinterpret_cast<u32*>(muCurrentNode) = ToGpuPhysical(muWritePtr);
        *reinterpret_cast<u32*>(muWritePtr)    = 0xC0000000u;
    }
    muWriteEnd = 0;
}

// Pointer-invariant layout facts (uncalled). The four leading chunk/write words
// are 32-bit GPU addresses and sit before the first host pointer, so their
// offsets are pre-widening and pinned absolutely; mpDevice is the first host
// pointer and its own offset (0x10 == 4 * u32) is invariant too.
void _CRingAllocListAssertLayout()
{
    static_assert(offsetof(CRingAllocList, muHeadChunk)   == 0x00, "head chunk @ word[0]");
    static_assert(offsetof(CRingAllocList, muCurrentNode) == 0x04, "current node @ word[1]");
    static_assert(offsetof(CRingAllocList, muWritePtr)    == 0x08, "write cursor @ word[2]");
    static_assert(offsetof(CRingAllocList, muWriteEnd)    == 0x0C, "write end @ word[3]");
    static_assert(offsetof(CRingAllocList, mpDevice)      == 0x10, "device back-pointer @ word[4]");
}

} // namespace D3D

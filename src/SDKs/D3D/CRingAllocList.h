#pragma once

// ===========================================================================
// D3D::CRingAllocList -- the X360 GPU command-stream "chunk chain" allocator
// inside BURNOUT_X360_ARTIST.XEX. This header is the canonical OWNING home for
// the CRingAllocList TYPE and its 5 methods (homed in CRingAllocList.cpp).
//
// A CRingAllocList threads a singly-linked chain of GPU command-buffer chunks
// served on demand by D3D::CDevice::BeginRingAlloc. Each chunk's FIRST dword is
// a "link" word holding the GPU-physical-mirror address of the next fetch
// target; the CPU patches those link words (and cache-flushes them with eieio /
// D3D::FlushCachedMemory) so the GPU command processor walks from chunk to
// chunk uninterrupted. The list tracks the head chunk (where the GPU starts),
// the node whose link word is patched next, and the current write cursor / end.
//
// LAYOUT -- there is NO DWARF and NO reference source for this TU (same footing
// as D3D::CDevice / D3D::CCommandBuffer / D3D::CBlocker). The member shape below
// is reconstructed purely from the byte offsets the bodied methods load/store in
// the X360 asm; only the words those methods touch are named. The first four
// words are 32-bit GPU addresses (pointer-invariant, pinned with offsetof); the
// device back-pointer is the first host pointer.
//
// The stored chunk/write addresses are 32-bit X360 GPU addresses treated as
// integers (the >>20 / &0x1FFFFFFF / -0x40000000 CPU-virtual -> GPU-physical
// mirror math is 32-bit), so they are held as u32 and dereferenced through a
// cast when a link word is written into GPU command-buffer memory -- external
// platform data, not a C++ object.
//
// `D3D` is the X360 graphics-SDK boundary, so its identifiers (CRingAllocList
// and the method names) are preserved verbatim per the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/D3D/CDevice.h"

namespace D3D
{

class CRingAllocList
{
public:
    // -------- FAITHFULLY BODIED (recovered from X360 asm) --------

    // @ 0x829468A8 -- bind the owning device, clear the chain, and serve the
    // first chunk via MakeSpace. Returns the initial write cursor.
    u32 Initialize(CDevice* pDevice);

    // @ 0x82946638 -- close off the current chunk (patch its link word), pull a
    // fresh 0x1080-dword chunk from the device ring, chain it in and cache-flush
    // it; on allocation failure fall back to the device out-of-memory chunk.
    // Returns the new write cursor (chunk base + 4).
    u32 MakeSpace();

    // @ 0x82945A08 -- splice a caller-supplied chunk node into the chain: write
    // the node's forward link, patch the previous write slot to point at it,
    // advance the write cursor, and cache-flush both edited link words.
    s32 AddChunkNode(u32 uNode, u32 uNextWrite);

    // @ 0x829479D8 -- return the GPU-physical-mirror address of the head chunk
    // (where the GPU begins fetching), or 0 when the chain is empty.
    u32 GetFirstChunk();

    // @ 0x82945998 -- seal the chain: patch the current node's link to the write
    // cursor, terminate the write slot with the 0xC0000000 end marker, and clear
    // the write end.
    void Finalize();

private:
    u32      muHeadChunk;   // +0x00  word[0]  first chunk (GPU begins fetching here)
    u32      muCurrentNode; // +0x04  word[1]  node whose forward link word is patched next
    u32      muWritePtr;    // +0x08  word[2]  current command write cursor
    u32      muWriteEnd;    // +0x0C  word[3]  end limit of the current chunk
    CDevice* mpDevice;      // +0x10  word[4]  owning device (ring source / OOM fallback)

    friend void _CRingAllocListAssertLayout();
};

} // namespace D3D

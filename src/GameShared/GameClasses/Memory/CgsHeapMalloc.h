#pragma once

#include "types.hpp"

// ============================================================================
// GameShared/GameClasses/Memory/CgsHeapMalloc.h
//
// CgsMemory::HeapMalloc -- the general-purpose heap allocator used across the
// engine (the sibling of the bump-pointer CgsMemory::LinearMalloc homed in
// CgsLinearMalloc.h). It owns a buffer (mpBuffer / mnBufferSize) and a
// GeneralAllocator free-list over it; Malloc/Free/ReAlloc service allocations
// from that buffer.
//
// Layout/method set are from the DecFIGS DWARF (CgsHeapMalloc.h:53):
//   private:  void* mpBuffer;  int32_t mnBufferSize;  GeneralAllocator mAllocator;
// The X360 ICE memory manager (ICE::ICEMemory : public HeapMalloc) embeds a
// SECOND HeapMalloc at offset +0x520, which fixes sizeof(HeapMalloc) == 0x520:
//   ICEMemory::Construct  -> HeapMalloc::Construct(this+0x520, block, size)
//   ICEMemory::GetMemory  -> HeapMalloc::Malloc(this+0x520, size, 4)
//   ICETake::FreeEditBuffer -> HeapMalloc::Free(this+0x520, block)
// so the GeneralAllocator body is modelled as a padding buffer sized to make the
// whole type 0x520 bytes (its internal free-list layout is not yet recovered).
//
// All methods are NON-STATIC members (verified against the X360 asm: the call
// sites pass the heap object as `this` in r3 -- e.g. ICEMemory::GetMemory does
// `Malloc(this+0x520, size, 4)` and FreeEditBuffer does `Free(this+0x520, block)`).
// ============================================================================

namespace CgsMemory
{
    // Tag is `class` to match the committed forward declaration in
    // BrnScoringSystem.h (`namespace CgsMemory { class HeapMalloc; }`) and
    // CgsHardwareInit.h -- avoids an elaborated-type tag mismatch (MSVC C4099).
    class HeapMalloc
    {
    public:
        static const s32 KI_DEFAULT_ALIGNMENT = 4;   // CgsHeapMalloc.h:37

        // Construct the heap over a caller-supplied [lpBuffer, lpBuffer+lnBufferSize)
        // block (initialises the GeneralAllocator free-list over it). X360 0x82866680.
        void Construct(void* lpBuffer, s32 lnBufferSize);
        void Destruct();
        bool Prepare();

        // Allocate lnSize bytes, lnAlignment-aligned, from this heap (null on
        // exhaustion). X360 0x82866780.
        void* Malloc(s32 lnSize, s32 lnAlignment);
        // Free a block previously returned by this heap's Malloc. X360 member call
        // `Free(this, block)` (the heap is the `this` in r3). 0x82866xxx.
        void  Free(void* lpBlock);
        void* ReAlloc(void* lpBlock, s32 lnSize);
        void* CAlloc(s32 lnCount, s32 lnSize);

        // Dump the live allocation list (diagnostics, used on the out-of-memory
        // path). X360 0x82866848.
        void  PrintAllocations();
        u32   GetLargestFreeBlock(bool lbContiguous);

    private:
        void* mpBuffer;        // CgsHeapMalloc.h:117  +0x0
        s32   mnBufferSize;    // CgsHeapMalloc.h:118  +0x4
        // CgsHeapMalloc.h:123  +0x8 : HeapMalloc::GeneralAllocator mAllocator. Its
        // internal free-list layout is not yet recovered; modelled as a padding
        // buffer so sizeof(HeapMalloc) == 0x520 (required by ICEMemory's embedded
        // +0x520 heap). GROW into named members when the GeneralAllocator TU lands.
        u8    mAllocator[0x520 - 0x8];   // GeneralAllocator (opaque free-list body)
    };
}

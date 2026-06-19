#ifndef SDKS_PACKAGES_ICE_ICEMEMORY_HPP
#define SDKS_PACKAGES_ICE_ICEMEMORY_HPP

#include "types.hpp"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"   // CgsMemory::HeapMalloc (base + embedded edit heap)

// ============================================================================
// SDKs/Packages/ICE/ICEMemory.hpp
//
// The real ICE memory manager (was a keystone minimal stub -- a fake
// ICEMemoryManager with a lone mEditHeap -- seeded during the ICEData keystone;
// this replaces it with the attested layout).
//
// ICE::ICEMemory IS-A CgsMemory::HeapMalloc (DWARF: struct ICEMemory : public
// HeapMalloc) AND embeds a SECOND HeapMalloc at +0x520, proven by the X360 asm:
//   * Construct(buffer, size): HeapMalloc::Construct(this, buffer, size) builds
//     the BASE heap (offset 0) over the caller block, then allocates a 1.75 MB
//     "ICE Debug Memory" block and HeapMalloc::Construct(this+0x520, block, 1.75M)
//     builds the embedded edit heap (mEditHeap).
//   * GetMemory(size): HeapMalloc::Malloc(this+0x520, size, 4) -- allocates from
//     mEditHeap, the +0x520 heap (NOT the base heap at offset 0).
//   * ICETake::FreeEditBuffer frees the edit buffer with
//     HeapMalloc::Free(this+0x520, block) -- the SAME mEditHeap.
// So the global dword_82FB62C0 is an ICE::ICEMemory* (named ICE::spICEMemory in
// the DWARF), and +0x520 is mEditHeap -- the embedded edit heap that GetMemory
// allocates from and FreeEditBuffer frees to. sizeof(HeapMalloc) == 0x520 places
// mEditHeap exactly at +0x520.
// ============================================================================

namespace ICE
{

// The ICE memory manager. The base CgsMemory::HeapMalloc (offset 0) is the ICE
// general heap; mEditHeap (offset +0x520) is the dedicated edit-buffer heap that
// ICETake's edit path allocates from / frees to.
struct ICEMemory : public CgsMemory::HeapMalloc
{
    // X360 +0x520: the edit-buffer heap (ICETakeData home). GetMemory allocates
    // from it; ICETake::FreeEditBuffer frees to it.
    CgsMemory::HeapMalloc mEditHeap;

    // Build the base heap over [lpBuffer, lpBuffer+lnBufferSize), then build the
    // embedded edit heap over a freshly allocated debug-memory block. X360 0x82533468.
    void  Construct(void* lpBuffer, s32 lnBufferSize);
    // Allocate luSize bytes from the embedded edit heap (mEditHeap). X360 0x8252CC80.
    void* GetMemory(u32 luSize);
    // Free a block previously returned by GetMemory back to the edit heap.
    void  FreeMemory(void* lpMemory);
};

// The ICE memory-manager singleton (X360 dword_82FB62C0). DWARF: ICE::spICEMemory.
// Defined out-of-line by the ICE memory TU; the per-TU cl /c gate does not link.
extern ICEMemory* spICEMemory;

} // namespace ICE

#endif // SDKS_PACKAGES_ICE_ICEMEMORY_HPP

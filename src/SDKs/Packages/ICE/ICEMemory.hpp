#ifndef SDKS_PACKAGES_ICE_ICEMEMORY_HPP
#define SDKS_PACKAGES_ICE_ICEMEMORY_HPP

#include "types.hpp"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"   // CgsMemory::HeapMalloc (edit-buffer heap)

// ============================================================================
// SDKs/Packages/ICE/ICEMemory.hpp
//
// The minimal ICE memory-management surface the ICETake edit-buffer code touches:
// the global ICE memory-manager singleton (X360: dword_82FB62C0), its embedded
// edit-buffer heap (a CgsMemory::HeapMalloc at +0x520), and the ICEMemory::GetMemory
// allocator entry point. The real definitions are owned by the ICE memory TU; the
// per-TU `cl /c` gate does not link, so DECLARATION-ONLY suffices.
//
// MINIMAL SLICE -- models exactly what ICETake::NewEditBuffer / FreeEditBuffer use:
//   * struct ICEMemoryManager { CgsMemory::HeapMalloc mEditHeap; }  (only the member
//     the edit path dereferences; the real manager has many more, added when homed)
//   * extern ICEMemoryManager* gpICEMemoryManager;                  (the singleton)
//   * ICEMemory::GetMemory(ICEMemoryManager*, u32) -> void*         (allocator entry)
//
// FLAG: ICEMemoryManager here is a placeholder for the not-yet-reconstructed ICE
// memory manager; mEditHeap is placed first only so the edit path compiles -- its
// real +0x520 offset / surrounding members land with the ICE memory TU. GROW this
// header in place when that TU is reconstructed; do NOT fork a parallel manager.
// ============================================================================

namespace ICE
{

// Placeholder for the ICE memory-manager singleton (X360 dword_82FB62C0). Only the
// edit-buffer heap member is modelled (the sole member the ICETake edit path uses).
struct ICEMemoryManager
{
    CgsMemory::HeapMalloc mEditHeap;   // X360 +0x520: edit ICETakeData home heap
};

// The global ICE memory-manager singleton. Defined out-of-line by the ICE memory TU.
extern ICEMemoryManager* gpICEMemoryManager;

namespace ICEMemory
{
    // Allocate luSize bytes from the ICE memory manager. DECLARATION-ONLY (defined in
    // the ICE memory TU; cl /c does not link).
    void* GetMemory(ICEMemoryManager* lpManager, u32 luSize);
}

} // namespace ICE

#endif // SDKS_PACKAGES_ICE_ICEMEMORY_HPP

#pragma once

#include "types.hpp"

// ============================================================================
// GameShared/GameClasses/Memory/CgsHeapMalloc.h
//
// CgsMemory::HeapMalloc -- the general-purpose heap allocator used across the
// engine (the sibling of the bump-pointer CgsMemory::LinearMalloc already homed
// in CgsLinearMalloc.h). The ICE memory manager embeds one of these as its
// edit-buffer heap; ICETake::FreeEditBuffer frees the edit ICETakeData back to it
// via the static CgsMemory::HeapMalloc::Free(heap, block) entry point
// (X360: CgsMemory::HeapMalloc::Free(dword_82FB62C0 + 0x520, lpTakeData)).
//
// MINIMAL SLICE -- declaration-only, modelling exactly the one method the ICE
// edit-buffer path calls (the static Free) plus the type identity needed so the
// ICE memory manager can hold one by value. When the real CgsMemory::HeapMalloc
// TU lands, GROW this header in place (add Create/Malloc/Realloc/etc. and the
// real field layout) -- do NOT fork a parallel HeapMalloc type.
// ============================================================================

namespace CgsMemory
{
    // Tag is `class` to match the committed forward declaration in
    // BrnScoringSystem.h (`namespace CgsMemory { class HeapMalloc; }`) -- avoids an
    // elaborated-type tag mismatch (MSVC C4099) if both are ever co-included.
    class HeapMalloc
    {
    public:
        // Free a block previously allocated from lpHeap. The X360 call site is the
        // static form `HeapMalloc::Free(&heap, block)` (heap passed explicitly),
        // so this is modelled as a static method taking the heap by pointer.
        static void Free(HeapMalloc* lpHeap, void* lpBlock);
    };
}

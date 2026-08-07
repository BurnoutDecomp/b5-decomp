#pragma once

// EATech Apt GC value-allocator bookkeeping header (_AptValueGC_MemItem).
// Reconstructed from the Feb-2007 leak; accessors confirmed vs X360 pseudocode
// (IsAllocated @0x82AD4E60, SetIsAllocated @0x82AD4E90, GetSize @0x82AD4EE8)
// and re-pinned against the x64 XB1 build (rung 1 for Apt):
//
// x64 SIZE ENCODING (GetSize `and rax,0FFFFFFFFFFFFFFFEh`): the size is stored
// IN PLACE in the full 64-bit word with the allocated flag in BIT 0 (sizes are
// 8-aligned, so bit 0 is free) -- word = size | flag, GetSize = word & ~1.
// The earlier `{bIsAllocated:1; nSize:31}` bitfield encoded (size<<1)|flag,
// which returned HALF the real size on every read -- the live-corruption class
// this audit hunts. The two layouts (size word at +0 vs +sizeof(void*)) follow
// the runtime mode selector exactly as before.

#include <cstddef>   // offsetof (_AssertLayout)
#include <cstdint>

typedef struct _AptValueGC_MemItem
{
    union {
        struct {
            struct _AptValueGC_MemItem * pNextItem;
            uintptr_t nSizeWord;   // size | allocated-flag (bit 0)
            struct _AptValueGC_MemItem * pPrevItem;
        }Type1;
        struct {
            uintptr_t nSizeWord;   // size | allocated-flag (bit 0)
            struct _AptValueGC_MemItem * pNextItem;
            struct _AptValueGC_MemItem * pPrevItem;
        }Type2;
    };

    bool IsAllocated(uint32_t nSizeOffset)
    {
        if(nSizeOffset == sizeof(void*))
            return (Type1.nSizeWord & 1u) != 0;
        else if(nSizeOffset == 0)
            return (Type2.nSizeWord & 1u) != 0;
        else
            return false;
    }

    void SetIsAllocated(uint32_t nSizeOffset, bool newValue)
    {
        if(nSizeOffset == sizeof(void*))
            Type1.nSizeWord = (Type1.nSizeWord & ~(uintptr_t)1) | (newValue ? 1u : 0u);
        else if(nSizeOffset == 0)
            Type2.nSizeWord = (Type2.nSizeWord & ~(uintptr_t)1) | (newValue ? 1u : 0u);
    }

    // x64 GetSize: the size word with the flag bit masked OFF -- NO shift
    // (`and rax,0FFFFFFFFFFFFFFFEh`).
    uintptr_t GetSize(uint32_t nSizeOffset)
    {
        if(nSizeOffset == sizeof(void*))
            return Type1.nSizeWord & ~(uintptr_t)1;
        else if(nSizeOffset == 0)
            return Type2.nSizeWord & ~(uintptr_t)1;
        else
            return 0;
    }

    // Layout pinned against the x64 XB1 export (never called).
    static void _AssertLayout()
    {
        static_assert(sizeof(_AptValueGC_MemItem) == 0x18, "x64: free-header is 3 qwords; GC alloc carves at header+24");
    }
}AptValueGC_MemItem;

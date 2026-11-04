#pragma once

// X360 Apt GC pool item. ARTIST 82AD4E60/82AD4E90/82AD4EE8
// place the allocation flag in bit 31 and the byte size in bits 0..30.

#include <cstddef>   // offsetof (_AssertLayout)
#include "types.hpp"

typedef struct _AptValueGC_MemItem
{
    union {
        struct {
            struct _AptValueGC_MemItem * pNextItem;
            uintptr_t nSizeWord;   // size | allocated-flag (bit 31)
            struct _AptValueGC_MemItem * pPrevItem;
        }Type1;
        struct {
            uintptr_t nSizeWord;   // size | allocated-flag (bit 31)
            struct _AptValueGC_MemItem * pNextItem;
            struct _AptValueGC_MemItem * pPrevItem;
        }Type2;
    };

    bool IsAllocated(uint32_t nSizeOffset)
    {
        if(nSizeOffset == sizeof(void*))
            return (Type1.nSizeWord & 0x80000000u) != 0;
        else if(nSizeOffset == 0)
            return (Type2.nSizeWord & 0x80000000u) != 0;
        else
            return false;
    }

    void SetIsAllocated(uint32_t nSizeOffset, bool newValue)
    {
        if(nSizeOffset == sizeof(void*))
            Type1.nSizeWord = (Type1.nSizeWord & 0x7fffffffu) | (newValue ? 0x80000000u : 0u);
        else if(nSizeOffset == 0)
            Type2.nSizeWord = (Type2.nSizeWord & 0x7fffffffu) | (newValue ? 0x80000000u : 0u);
    }

    // ARTIST GetSize clears the allocation bit without shifting.
    uintptr_t GetSize(uint32_t nSizeOffset)
    {
        if(nSizeOffset == sizeof(void*))
            return Type1.nSizeWord & 0x7fffffffu;
        else if(nSizeOffset == 0)
            return Type2.nSizeWord & 0x7fffffffu;
        else
            return 0;
    }

    // Native pointer and item layout.
    static void _AssertLayout()
    {
        CGS_STATIC_ASSERT(sizeof(void*) == 4, "X360 Apt GC requires 32-bit pointers");
        CGS_STATIC_ASSERT(offsetof(_AptValueGC_MemItem, Type1.nSizeWord) == 4, "X360 size word offset");
    }
}AptValueGC_MemItem;

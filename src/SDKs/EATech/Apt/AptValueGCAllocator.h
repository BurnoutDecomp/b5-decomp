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
            uintptr_t bIsAllocated : 1;
            uintptr_t nSize : 31;
            struct _AptValueGC_MemItem * pPrevItem;
        }Type1;
        struct {
            uintptr_t bIsAllocated : 1;
            uintptr_t nSize : 31;
            struct _AptValueGC_MemItem * pNextItem;
            struct _AptValueGC_MemItem * pPrevItem;
        }Type2;
    };

    bool IsAllocated(uint32_t nSizeOffset)
    {
        if(nSizeOffset == sizeof(void*))
            return Type1.bIsAllocated != 0;
        else if(nSizeOffset == 0)
            return Type2.bIsAllocated != 0;
        else
            return false;
    }

    void SetIsAllocated(uint32_t nSizeOffset, bool newValue);

    // ARTIST GetSize clears the allocation bit without shifting.
    uintptr_t GetSize(uint32_t nSizeOffset)
    {
        if(nSizeOffset == sizeof(void*))
            return Type1.nSize;
        else if(nSizeOffset == 0)
            return Type2.nSize;
        else
            return 0;
    }

    // Native pointer and item layout.
    static void _AssertLayout()
    {
        CGS_STATIC_ASSERT(sizeof(void*) == 4, "X360 Apt GC requires 32-bit pointers");
        CGS_STATIC_ASSERT(sizeof(_AptValueGC_MemItem) == 12, "ARTIST pool item layout");
        CGS_STATIC_ASSERT(offsetof(_AptValueGC_MemItem, Type1.pPrevItem) == 8, "ARTIST Type1 previous item");
        CGS_STATIC_ASSERT(offsetof(_AptValueGC_MemItem, Type2.pNextItem) == 4, "ARTIST Type2 next item");
    }
}AptValueGC_MemItem;

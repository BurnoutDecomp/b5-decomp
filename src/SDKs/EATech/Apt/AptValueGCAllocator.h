#pragma once

// EATech Apt GC value-allocator bookkeeping header (_AptValueGC_MemItem).
// Reconstructed from the Feb-2007 leak; accessors confirmed vs X360 pseudocode
// (IsAllocated @0x82AD4E60, SetIsAllocated @0x82AD4E90, GetSize @0x82AD4EE8).
// High bit of the size word = allocated flag, low 31 bits = byte count.

#include <cstdint>

typedef struct _AptValueGC_MemItem
{
    union {
        struct {
            struct _AptValueGC_MemItem * pNextItem;
            uintptr_t bIsAllocated:1;
            uintptr_t nSize:31;
            struct _AptValueGC_MemItem * pPrevItem;
        }Type1;
        struct {
            uintptr_t bIsAllocated:1;
            uintptr_t nSize:31;
            struct _AptValueGC_MemItem * pNextItem;
            struct _AptValueGC_MemItem * pPrevItem;
        }Type2;
    };

    bool IsAllocated(uint32_t nSizeOffset)
    {
        if(nSizeOffset == sizeof(void*))
            return Type1.bIsAllocated ? true : false;
        else if(nSizeOffset == 0)
            return Type2.bIsAllocated ? true : false;
        else
            return false;
    }

    void SetIsAllocated(uint32_t nSizeOffset, bool newValue)
    {
        if(nSizeOffset == sizeof(void*))
            Type1.bIsAllocated = (uint32_t)(newValue ? 1 : 0);
        else if(nSizeOffset == 0)
            Type2.bIsAllocated = (uint32_t)(newValue ? 1 : 0);
    }

    uintptr_t GetSize(uint32_t nSizeOffset)
    {
        if(nSizeOffset == sizeof(void*))
            return Type1.nSize;
        else if(nSizeOffset == 0)
            return Type2.nSize;
        else
            return 0;
    }
}AptValueGC_MemItem;

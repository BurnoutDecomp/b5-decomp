#include "SDKs/EATech/Apt/AptValueGCAllocator.h"

#include <cstddef>

CGS_STATIC_ASSERT((sizeof(((AptValueGC_MemItem*)0)->Type1) >= sizeof(void*)),
              "AptValueGC_MemItem Type1 must hold a pointer plus the size word");
CGS_STATIC_ASSERT((offsetof(AptValueGC_MemItem, Type1) == 0),
              "AptValueGC_MemItem union members must overlay at offset 0");
CGS_STATIC_ASSERT((offsetof(AptValueGC_MemItem, Type2) == 0),
              "AptValueGC_MemItem union members must overlay at offset 0");


// ARTIST 0x82AD4E90 changes the allocation bit in the selected size word.
void AptValueGC_MemItem::SetIsAllocated(uint32_t nSizeOffset, bool newValue)
{
    if (nSizeOffset == sizeof(void*))
        Type1.bIsAllocated = newValue ? 1u : 0u;
    else if (nSizeOffset == 0)
        Type2.bIsAllocated = newValue ? 1u : 0u;
}

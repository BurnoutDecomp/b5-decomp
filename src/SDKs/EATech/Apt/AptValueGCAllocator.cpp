#include "SDKs/EATech/Apt/AptValueGCAllocator.h"

#include <cstddef>

static_assert(sizeof(((AptValueGC_MemItem*)0)->Type1) >= sizeof(void*),
              "AptValueGC_MemItem Type1 must hold a pointer plus the size word");
static_assert(offsetof(AptValueGC_MemItem, Type1) == 0,
              "AptValueGC_MemItem union members must overlay at offset 0");
static_assert(offsetof(AptValueGC_MemItem, Type2) == 0,
              "AptValueGC_MemItem union members must overlay at offset 0");

#include "SharedClasses/Massive/MassiveLookupTable.h"

#include <cstdint>   // uintptr_t

// BrnMassive::MassiveLookupTable relocation pair. The console FixUp/FixDown also call an
// empty per-item routine on each item's mSceneID (the id needs no relocation); it has no
// effect and is not reproduced.

namespace BrnMassive
{
    void MassiveLookupTable::FixUp(void* lpBase)
    {
        _AssertLayout();
        mpItems = reinterpret_cast<MassiveLookupTableItem*>(
            reinterpret_cast<uintptr_t>(lpBase) + reinterpret_cast<uintptr_t>(mpItems));
    }

    void MassiveLookupTable::FixDown(void* lpBase)
    {
        mpItems = reinterpret_cast<MassiveLookupTableItem*>(
            reinterpret_cast<uintptr_t>(mpItems) - reinterpret_cast<uintptr_t>(lpBase));
    }
}

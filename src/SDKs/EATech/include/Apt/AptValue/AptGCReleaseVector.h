#pragma once

// ===========================================================================
// EATech Apt -- AptGCReleaseVector: the Apt GC deferred-release vector
// (X360 global gValuesToRelease @ off_8324E51C).
//
// A LIFO stack of AptValue* whose release was deferred (queued instead of freed
// inline). AptGC::CleanAll flushes it three times during teardown.
//
// DISTINCT TYPE from the operand-stack AptValueVector: its live count sits at
// +0x04, whereas the operand vector's mnTop is at +0x00 -- proven by the X360
// bodies ReleaseValues@0x82ADCF60 (count @ +4) vs PopAndPush@0x82ADBAB8 /
// SafePop@0x82ADBB58 (mnTop @ +0). LAYOUT:
//   [+0x00] mnCapacity   INFERRED -- the grow-capacity; NOT read by ReleaseValues,
//                         so its exact role awaits the vector's Add/grow method
//                         (a different TU). Documented placeholder, never poked.
//   [+0x04] mnTop        live count (the stack top)
//   [+0x08] mppItems     AptValue** slot array
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"

struct AptGCReleaseVector
{
    int32_t    mnCapacity;   // [+0x00] inferred; unread by ReleaseValues
    int32_t    mnTop;        // [+0x04] live count
    AptValue** mppItems;     // [+0x08] slot array

    // ReleaseValues @0x82ADCF60 -- pop every deferred value off the stack: a value
    // still referenced elsewhere just gets its deferred flag cleared; an
    // unreferenced one is ForceDelete()'d (PreDestroy + DestroyGCPointers + delete).
    void ReleaseValues()
    {
        while (mnTop)
        {
            AptValue* pValue = mppItems[--mnTop];
            if (pValue->getRefCount() != 0)
                pValue->ClearReleaseAtEnd();
            else
                pValue->ForceDelete();
        }
    }
};

// The single Apt GC deferred-release vector instance (X360 off_8324E51C). Defined
// by the Apt GC startup data; declared here for the helpers that query it.
extern AptGCReleaseVector gValuesToRelease;

// AptIsDeferredVectorFull @0x82ADD238 -- whether the deferred-release vector is at
// capacity (mnTop >= mnCapacity). This read confirms the layout (mnTop@+4 vs
// mnCapacity@+0). Used by CgsAptCommunicator::UpdateComponentReserved.
inline bool AptIsDeferredVectorFull()
{
    return gValuesToRelease.mnTop >= gValuesToRelease.mnCapacity;
}

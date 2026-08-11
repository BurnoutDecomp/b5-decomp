#pragma once

// ===========================================================================
// EATech Apt -- the Apt GC deferred-release vector (X360 console symbol
// gValuesToRelease @ off_8324E51C).
//
// A LIFO stack of AptValue* whose release was deferred (queued instead of freed
// inline). AptGC::CleanAll flushes it three times during teardown.
//
// UNIFIED 2026-08-07 (the one-console-slot/three-reconstruction-homes fix): the
// console slot is a POINTER to a pool-allocated AptValueVector -- X360
// AptCommonInitialize @0x82AE91F0 does `Allocate(off_8324D808, 12);
// AptValueVector::AptValueVector(mem, cfg[10]); off_8324E51C = mem` with the
// FAMILY-(B) ctor @0x82AE32F0 {capacity@+0 (mnTop member), live count@+4
// (mnCapacity member), items@+8}. ReleaseValues @0x82ADCF60 is that family's
// drain. The earlier reconstruction carried THREE homes for this slot (a static
// distinct-type instance, AptInit's file-local common pointer, and a null
// AptValue.cpp view); all now read the ONE global below.
//
// Family-(B) member map reminder (AptValueVector.h): capacity == mnTop (+0x00),
// live count == mnCapacity (+0x04), slots == mppItems (+0x08). Push =
// `if (mnCapacity < mnTop) mppItems[mnCapacity++] = v` -- the zombie-vector idiom.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptValue/AptValueVector.h"

// The single deferred-release vector pointer (X360 off_8324E51C == the console
// symbol gValuesToRelease). Null until AptCommonInitialize allocates + constructs
// it; nulled again by AptCommonShutdown. Defined in AptGlobals.cpp.
extern AptValueVector* gpValuesToRelease;

// AptIsDeferredVectorFull @0x82ADD238 -- whether the deferred-release vector is at
// capacity (family-(B): live count (mnCapacity) >= capacity (mnTop)). A null
// vector (pre-init) reports full, so pushers defer nothing -- the console never
// reaches this pre-init. Used by CgsAptCommunicator::UpdateComponentReserved.
inline bool AptIsDeferredVectorFull()
{
    return gpValuesToRelease == nullptr ||
           gpValuesToRelease->mnCapacity >= gpValuesToRelease->mnTop;
}

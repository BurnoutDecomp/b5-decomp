#pragma once

// ===========================================================================
// EATech Apt -- AptDefine.h (the slice the value leaves need).
//
// The two global Apt value pool managers + the non-GC size-saving array alloc
// helpers, from the LegacySrc reference. The concrete
// non-GC value types (AptBoolean/AptFloat/AptInteger/AptString) and the AptStd
// types route operator new / new[] through these.
//
// Per CXX_NAMING_CONVENTIONS the EA SDK identifiers are kept verbatim.
// ===========================================================================

#include <cstddef>   // size_t

class DOGMA_PoolManager;       // SDKs/EATech/Apt/DogmaAllocator.h
class AptValueGC_PoolManager;  // SDKs/EATech/Apt/AptValueGCPoolManager.h

// The value pools. Wired by AptAllocatorInitialize
// @0x82ADD118 (AptInit.cpp, WireAllocatorGlobals); null until that bring-up runs.
extern DOGMA_PoolManager*      gpNonGCPoolManager;
extern AptValueGC_PoolManager* gpGCPoolManager;

// The operator new[] path for non-GC types: allocate
// from the non-GC pool with the block size stashed in a header so delete[] can
// recover it.
void* AptNonGCAllocSaveSize(size_t nSize);
void  AptNonGCFreeSavedSize(void* p);

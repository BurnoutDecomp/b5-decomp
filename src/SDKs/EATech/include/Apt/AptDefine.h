#pragma once

// ===========================================================================
// EATech Apt -- AptDefine.h (the slice the value leaves need).
//
// The two global Apt value pool managers + the non-GC size-saving array alloc
// helpers, from the Feb-2007 leak (AptDefine.h:480-481, 539-540). The concrete
// non-GC value types (AptBoolean/AptFloat/AptInteger/AptString) and the AptStd
// types route operator new / new[] through these.
//
// Per CXX_NAMING_CONVENTIONS the EA SDK identifiers are kept verbatim.
// ===========================================================================

#include <cstddef>   // size_t

class DOGMA_PoolManager;       // SDKs/EATech/Apt/DogmaAllocator.h
class AptValueGC_PoolManager;  // SDKs/EATech/Apt/AptValueGCPoolManager.h

// leak AptDefine.h:480-481 -- the value pools. Wired by the Apt runtime startup
// (AptInit); null until then (FLAG: AptInit is not yet reconstructed).
extern DOGMA_PoolManager*      gpNonGCPoolManager;
extern AptValueGC_PoolManager* gpGCPoolManager;

// leak AptDefine.h:539-540 -- the operator new[] path for non-GC types: allocate
// from the non-GC pool with the block size stashed in a header so delete[] can
// recover it.
void* AptNonGCAllocSaveSize(size_t nSize);
void  AptNonGCFreeSavedSize(void* p);

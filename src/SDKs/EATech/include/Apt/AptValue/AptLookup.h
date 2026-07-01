#pragma once

// ===========================================================================
// EATech Apt -- AptLookup: the ActionScript "lookup" value (vtbl index
// AptVFT_Lookup = 8).
//
// AptLookup is the value the Apt VM hands back for a "lookup" reference -- one of
// a fixed pool of pre-allocated, slot-indexed value objects. The runtime builds a
// flat array of them once (AptLookup::Initialize, from AptValueInitialize), gives
// each its array position as mnIndex, and pins each one's reference count to
// MAX_REFCOUNT so the pool entries are never individually freed. Shutdown tears
// the whole array down.
//
// NO Feb-2007 source and NO DecFIGS DWARF exist for this class (X360-only, like
// the sibling singletons AptExtern / AptNone). SHAPE + BODIES are therefore
// reconstructed STRICTLY from the X360 ARTIST.XEX:
//     AptLookup::AptLookup                    @ 0x82AE5F90  (ctor; vtbl index 8)
//     AptLookup::Initialize                   @ 0x82AE7E88  (static; build the pool)
//     AptLookup::Shutdown                     @ 0x82AD7838  (static; tear it down)
//     AptLookup::`vector deleting destructor' @ 0x82AE7F90  (compiler thunk -- DROPPED)
//
// LAYOUT (12 bytes = 3 dwords): the AptValueNoGC base (vtable pointer + the 32-bit
// packed bitfield, 8 bytes) followed by a single dword member:
//     +0x00  vtable
//     +0x04  AptValue bitfield (mnValueData)
//     +0x08  mnIndex   -- this entry's slot index in the pool (0..N-1)
// The ctor zeroes mnIndex (stw 0,8(this)); Initialize assigns each entry its
// position (sLookupTable[i].mnIndex = i, the second loop in @0x82AE7E88); the
// scalar/vector deleting destructor strides 12 bytes per element and frees the
// 12*N+8-byte block, all of which pin sizeof(AptLookup) == 12.
//
// BASE: AptValueNoGC. The X360 ledger attests NO RegisterReferences /
// IsGarbageCollected body for AptLookup (the TU is exactly these four functions),
// and the pool array is NOT allocated through the GC pool (gpGCPoolManager) -- it
// comes from the plain shared Apt DOGMA pool (off_8324D808). A direct AptValueGC
// leaf would be forced to define RegisterReferences (it stays pure-virtual in
// AptValueGC) and so could not be instantiated as an array; the concrete
// AptValueNoGC base (IsGarbageCollected() == false, RegisterReferences() == no-op)
// is what makes the pool instantiable -- the same determination as AptExtern /
// AptNone. (The base ctor sub_82AE3000 == AptValue::AptValue(eType) does the GC
// bitfield init for every value type, GC or not, so it does not imply GC
// membership.)
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception);
// the reconstructed members/locals follow the mpX / mnX / siX / spX prefixes.
// ===========================================================================

#include <cstddef>   // size_t

#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValueNoGC base
#include "SDKs/EATech/Apt/DogmaAllocator.h"              // DOGMA_PoolManager (the shared Apt pool)

// ---------------------------------------------------------------------------
// FLAG (un-homed global): off_8324D808 is the shared Apt fixed-size DOGMA pool
// that backs the AptLookup pool array (the X360 Initialize allocates the array
// from it and the deleting destructor frees back to it). Declared as the same
// extern the sibling Apt pool users use (AptExtern.h / AptSharedPtr.h /
// AptLinkerThingy.h); the pool instance is constructed by the Apt runtime startup
// TU. Guarded for null until then.
// ---------------------------------------------------------------------------
extern DOGMA_PoolManager* gpAptSharedPtrPool;   // off_8324D808

// ---------------------------------------------------------------------------
// FLAG (un-homed Apt config global): the number of AptLookup pool entries to
// pre-allocate. The X360 reads it as dword_82F733EC (a read-only config slot in
// the 0x82F7xxxx data segment, written by the Apt startup config -- the sibling
// AptRegister pool reads the adjacent dword_82F733E8 the same way). Declared
// extern so Initialize compiles; its definition belongs to the Apt config TU.
// ---------------------------------------------------------------------------
extern int gAptLookupPoolSize;   // dword_82F733EC

class AptLookup : public AptValueNoGC
{
public:
    // The pool array's operator new[] / delete[] route through the shared Apt
    // DOGMA pool (off_8324D808) with the block size stashed in a leading dword so
    // delete[] -- which only receives the pointer -- can recover the size
    // DOGMA::Deallocate needs. This is the size-saving pool-array path the X360
    // emits inline in Initialize (Allocate(off_8324D808, size+4); *p = size) and in
    // the deleting destructor (Deallocate(off_8324D808, p, *p+4)). Guarded for null
    // until the Apt runtime wires the pool.
    static void* operator new[](size_t size)
    {
        if (gpAptSharedPtrPool == 0)
            return 0;
        size_t* lpBlock = static_cast<size_t*>(gpAptSharedPtrPool->Allocate(size + sizeof(size_t)));
        if (lpBlock == 0)
            return 0;
        *lpBlock = size;
        return lpBlock + 1;
    }
    static void operator delete[](void* p)
    {
        if (p == 0 || gpAptSharedPtrPool == 0)
            return;
        size_t* lpHeader = static_cast<size_t*>(p) - 1;
        gpAptSharedPtrPool->Deallocate(lpHeader, *lpHeader + sizeof(size_t));
    }

    // This entry's slot index in the pool.
    int GetIndex() const { return mnIndex; }

    // @0x82AE7E88 -- build the fixed AptLookup pool once: allocate gAptLookupPoolSize
    // entries, default-construct each, then give each its array position as mnIndex.
    // Idempotent (no-op once the pool exists).
    static void Initialize();

    // @0x82AD7838 -- tear the pool down (destruct + free the array) and clear it.
    static void Shutdown();

    // Pool-table access for the stream parser (_parseStream resolves a serialized
    // Lookup constant record to &pool[idx] when the table exists and idx < count):
    // the class-owned read of the fixed table the console addresses directly.
    static AptLookup* GetPoolEntry(int nIndex) { return &spLookupTable[nIndex]; }
    static bool       PoolHolds(int nIndex)    { return spLookupTable != 0 && nIndex < siNumLookups; }

protected:
    // @0x82AE5F90 -- base AptValueNoGC(AptVFT_Lookup), clear mnIndex, mark the value
    // defined, and pin its reference count to MAX_REFCOUNT (pool entries are never
    // freed individually). Body in AptLookup.cpp.
    AptLookup();

    // Trivial: AptLookup owns no GC pointers, so teardown is just the implicit
    // base-vtable revert the X360 deleting destructor performs per element
    // (store off_82145594 -- the AptValueNoGC base vtable -- into each slot).
    virtual ~AptLookup() {}

private:
    int mnIndex;   // +0x08 -- slot index in the pool

    // The pool: the contiguous array of entries + its live count. Written once by
    // Initialize, cleared by Shutdown (X360 off_8324E4A8 / dword_8324E4AC).
    static AptLookup* spLookupTable;   // off_8324E4A8
    static int        siNumLookups;    // dword_8324E4AC
};

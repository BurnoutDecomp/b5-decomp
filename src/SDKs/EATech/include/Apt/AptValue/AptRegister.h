#pragma once

// ===========================================================================
// EATech Apt (ActionScript / Flash player) -- AptRegister: the AS interpreter
// register value slot.
//
// SHAPE from the Feb-2007 leak value-type family (the AptValueNoGC leaves --
// AptBoolean/AptFloat/AptInteger/AptNone) + the X360 ARTIST.XEX bodies this TU
// owns:
//     AptRegister::AptRegister                  @ 0x82AE5F38
//     AptRegister::Initialize                   @ 0x82AE8050
//     AptRegister::Shutdown                     @ 0x82AD78A8
//     AptRegister::`vector deleting destructor' @ 0x82AE8158   (compiler thunk;
//                                                               not hand-written)
//
// AptRegister is one of the polymorphic AptValue value types (vtable object-type
// index AptVFT_Register == 4 -- pinned by the X360 ctor's base call
// `AptValueNoGC(4)`). The X360 ledger attests ONLY the four functions above for
// this class -- no RegisterReferences / IsGarbageCollected / GetValue / SetValue
// body -- so this is an honest minimal owning header. A direct AptValueGC leaf
// would be forced to define RegisterReferences (it stays pure-virtual in
// AptValueGC); its absence, plus the ctor's `sub_82AE3000(this, 4)` (the shared
// AptValueNoGC base ctor -- see AptNone.cpp) proves AptRegister derives from
// AptValueNoGC, which satisfies both GC virtuals inline.
//
// REGISTER FILE: the registers live in one heap array (the static spRegisters /
// snRegisterCount, X360 off_8324E4B0 / dword_8324E4B4) brought up once by
// Initialize and torn down by Shutdown. Each slot carries its own ordinal index
// (mnIndex): the ctor zeroes it and Initialize stamps each slot with its position
// in the file. Unlike the other non-GC value leaves (which pool through
// gpNonGCPoolManager), AptRegister's storage comes from the shared Apt fixed-size
// DOGMA pool gpAptSharedPtrPool (X360 off_8324D808 -- the same pool AptSharedPtr /
// AptActionQueue / the pseudo-display-list nodes use); the X360 Allocate/
// Deallocate calls in all three owned bodies load it from off_8324D808.
//
// The `vector deleting destructor' (@0x82AE8158) is a pure MSVC compiler thunk
// (per-element vtable reset for the trivial ~AptRegister, then route the backing
// block to the pool). It is regenerated from `virtual ~AptRegister()` + the
// size-saving pooled operator delete[] below, so -- like AptNone's -- it is NOT
// hand-written here.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API
// exception); project-owned additions (mnIndex, spRegisters, snRegisterCount,
// gnAptRegisterCount) follow the convention.
// ===========================================================================

#include <cstddef>   // size_t

#include "types.hpp"                                     // s32
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValueNoGC + AptVFT_Register
#include "SDKs/EATech/include/Apt/AptSharedPtr.h"        // gpAptSharedPtrPool (off_8324D808)
#include "SDKs/EATech/Apt/DogmaAllocator.h"              // DOGMA_PoolManager::Allocate/Deallocate

// ---------------------------------------------------------------------------
// FLAG (un-homed global): the configured AS register count (X360 dword_82F733E8,
// loaded by Initialize from the Apt runtime config block at unk_82F733B8). The
// value is published by the not-yet-reconstructed Apt runtime startup; declared
// here as an extern so this TU links. 0 until then -> Initialize brings up an
// empty register file.
// ---------------------------------------------------------------------------
extern s32 gnAptRegisterCount;   // X360 dword_82F733E8

class AptRegister : public AptValueNoGC
{
public:
    // ---- pooled allocation (X360 off_8324D808 == gpAptSharedPtrPool) ------
    // Scalar new/delete take/return a single 12-byte slot from the shared Apt
    // pool. The array forms use the size-saving idiom (stash the requested byte
    // size in a leading word so delete[] can recover it) -- combined with MSVC's
    // own array-length cookie this lays the heap array out exactly as the X360
    // Initialize / `vector deleting destructor' expect: [savedSize][count][slot0..].
    static void* operator new(size_t size)             { return gpAptSharedPtrPool->Allocate(size); }
    static void  operator delete(void* p, size_t size) { gpAptSharedPtrPool->Deallocate(p, size); }

    static void* operator new[](size_t size)
    {
        // Stash the requested size in a leading word, hand back the region past
        // it (X360: Allocate(off_8324D808, size + 4); *p = size; return p + 1).
        size_t* lpBlock = static_cast<size_t*>(gpAptSharedPtrPool->Allocate(size + sizeof(size_t)));
        *lpBlock = size;
        return lpBlock + 1;
    }

    static void operator delete[](void* p)
    {
        // Recover the leading saved-size word and free the whole block (X360:
        // Deallocate(off_8324D808, p - 1, *(p - 1) + 4)).
        size_t* lpBlock = static_cast<size_t*>(p) - 1;
        gpAptSharedPtrPool->Deallocate(lpBlock, *lpBlock + sizeof(size_t));
    }

    // ---- register file lifecycle -----------------------------------------
    // @0x82AE8050 / @0x82AD78A8. Bring up / tear down the global register array.
    // Idempotent: Initialize no-ops if the file already exists, Shutdown if it
    // does not.
    static void Initialize();
    static void Shutdown();

    s32 GetIndex() const { return mnIndex; }

    // Register-file access for the stream parser (_parseStream resolves a serialized
    // Register constant record to &file[idx] when the file exists and idx < count):
    // the class-owned read of the fixed file the console addresses directly.
    static AptRegister* GetFileEntry(s32 nIndex) { return &spRegisters[nIndex]; }
    static bool         FileHolds(s32 nIndex)    { return spRegisters != 0 && nIndex < snRegisterCount; }

private:
    // [0x08] this slot's ordinal position in the register file. The ctor zeroes
    // it; Initialize stamps each slot with its index. (vtable @0x00,
    // AptValue bitfield @0x04 are the AptValue base; sizeof(AptRegister) == 12.)
    s32 mnIndex;

    // The register file: one heap array of AptRegister + its element count.
    // X360 off_8324E4B0 / dword_8324E4B4.
    static AptRegister* spRegisters;
    static s32          snRegisterCount;

    // @0x82AE5F38 -- base AptValueNoGC(AptVFT_Register), clear the index, mark the
    // slot defined and pin its reference count (the register file is never
    // individually ref-freed). Body in AptRegister.cpp.
    AptRegister();

    // Trivial: the X360 `vector deleting destructor' only resets each element's
    // vtable (this empty body) before the pooled operator delete[] reclaims the
    // block -- so the deleting-destructor thunk is regenerated, not hand-written.
    virtual ~AptRegister() {}
};

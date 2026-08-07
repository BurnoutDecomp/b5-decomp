#pragma once

// ===========================================================================
// EATech Apt -- AptValueGC_PoolManager.
//
// The DOGMA pool allocator specialized for garbage-collected AptValue objects:
// it adds AptValueGC_MemItem allocate/free bookkeeping (the high-bit "is
// allocated" flag) and a live-object walk (GetFirst/GetNext) used by AptGC.
//
// SHAPE: derives from DOGMA_PoolManager (the IDA name + the ctor forwarding
// confirm the is-a relationship). No leak/DWARF body exists; everything is
// reconstructed store-for-store from the X360 ARTIST.XEX pseudocode:
//     AptValueGC_PoolManager::AptValueGC_PoolManager     @ 0x82ADBEF8
//     AptValueGC_PoolManager::StaticInitialize           @ 0x82ADB7E0
//     AptValueGC_PoolManager::DeallocateAptValueGC       @ 0x82AE57D8
//     AptValueGC_PoolManager::GetFirstAptValue           @ 0x82AE0DF8
//     AptValueGC_PoolManager::GetNextAptValue            @ 0x82AE0BE0
//     AptValueGC_PoolManager::`scalar deleting destructor'@ 0x82AE3858
//
// CONFIGURATION STATICS (X360 .data, set by StaticInitialize, read by the
// ctor / allocate path). These are this allocator's tuning, computed once from
// the per-VFT object-size table:
//   gAptValueGCSizeOffset   (byte_8324D804) -- AptValueGC_MemItem size-word
//                                              offset; 4 on X360 (== sizeof void*).
//   gAptValueGCMinItemSize  (byte_8324D805) -- min AptValue object size.
//   gAptValueGCStoreSizeFlag(byte_8324D806) -- bStoreFreeBlockSize seed (0).
//   gAptValueGCMaxItemSize  (dword_8324E2A4)-- max AptValue object size.
// The per-VFT object-size table (byte_82144A18[AptVFT_NumVFTs]) lives in
// .rdata; defined (zeroed) in AptGlobals.cpp -- see the note below.
//
// This is vendor/SDK code reconstructed beside its DOGMA base. Per
// CXX_NAMING_CONVENTIONS.md the EA SDK identifiers are kept verbatim.
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/Apt/DogmaAllocator.h"        // DOGMA_PoolManager
#include "SDKs/EATech/Apt/AptValueGCAllocator.h"    // AptValueGC_MemItem
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"  // AptVFT_NumVFTs

// ---------------------------------------------------------------------------
// Allocator tuning statics (X360 .data globals). Defined in
// AptValueGCPoolManager.cpp; populated by StaticInitialize().
// ---------------------------------------------------------------------------
extern uint8_t  gAptValueGCSizeOffset;    // byte_8324D804
extern uint8_t  gAptValueGCMinItemSize;   // byte_8324D805
extern uint8_t  gAptValueGCStoreSizeFlag; // byte_8324D806
extern uint32_t gAptValueGCMaxItemSize;   // dword_8324E2A4

// ---------------------------------------------------------------------------
// The per-virtual-function-table object-size table the X360 reaches as
// byte_82144A18[idx]. It maps an AptVirtualFunctionTable_Indices to the byte
// size of that concrete AptValue subclass. StaticInitialize scans entries
// [1, AptVFT_NumVFTs) for the min/max object size; the GC walk uses it to step
// item-to-item. Defined (zeroed) in AptGlobals.cpp -- the .rdata contents are
// class-set-generated and unrecovered there, so AptAllocatorInitialize
// (AptInit.cpp) overrides the table-derived min/max statics with the
// x64-correct 4/256 before the GC pool ctor reads them.
// ---------------------------------------------------------------------------
extern uint8_t byte_82144A18[AptVFT_NumVFTs];

class AptValueGC_PoolManager : public DOGMA_PoolManager
{
public:
    // ctor @ 0x82ADBEF8 -- forwards to DOGMA_PoolManager with the tuning
    // statics; mainPoolSize / overflowPoolSize are passed by the caller.
    AptValueGC_PoolManager(size_t mainPoolSizeBytes, size_t overflowPoolSizeBytes);

    // StaticInitialize @ 0x82ADB7E0 -- compute the tuning statics from the
    // per-VFT object-size table. Run once at Apt allocator init.
    static void StaticInitialize();

    // AllocateAptValueGC -- DOGMA allocate, then mark the AptValueGC_MemItem
    // allocated flag (the alloc-side mirror of DeallocateAptValueGC). The X360
    // never emits this out-of-line: it is folded inline into every GC value
    // type's `operator new` (DOGMA_PoolManager::Allocate(gpGCPoolManager, size)
    // then AptValueGC_MemItem::SetIsAllocated(p, gAptValueGCSizeOffset, 1) --
    // e.g. AptArray::operator new @0x82AE6088, AptGlobalExtensionObject::
    // operator new @0x82AE6588). Restored here as the named GC-allocator
    // operation so those `operator new`s read as a single call (de-inlining).
    void* AllocateAptValueGC(size_t nAllocatedSize);

    // DeallocateAptValueGC @ 0x82AE57D8 -- DOGMA free, then on success clear
    // the AptValueGC_MemItem allocated flag.
    bool DeallocateAptValueGC(void* pItem, size_t nAllocatedSize);

    // GetFirstAptValue @ 0x82AE0DF8 -- first live (allocated) AptValue in the
    // pools, or null.
    AptValue* GetFirstAptValue();

    // GetNextAptValue @ 0x82AE0BE0 -- next live AptValue after pCurrent (steps
    // by the current item's size), or the first outside-allocation, or null.
    AptValue* GetNextAptValue(AptValue* pCurrent);
};

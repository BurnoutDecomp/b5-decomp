#pragma once

// ===========================================================================
// EATech Apt (ActionScript / Flash player) -- AptSharedPtr<AptFile>.
//
// Reconstructed from the Feb-2007 leak SHAPE (SDKs/EATech/include/Apt/Apt.h
// DWARF: `struct AptSharedPtr<AptFile> { AptFile* pData; ... }`) plus the X360
// ARTIST.XEX pseudocode for the three method BODIES this TU owns:
//     AptSharedPtr<AptFile>::Dispose(AptFile*)                  @ 0x8284D158
//     AptSharedPtr<AptFile>::`vector deleting destructor'       @ 0x82AEC678
//     AptSharedPtr<AptFile>::operator=(const AptSharedPtr&)     @ 0x82AFD548
//
// `AptFile>` in the IDA naming is the mangled `AptSharedPtr<AptFile>` (the
// trailing '>' is the template-argument close). The DWARF confirms this is the
// AptSharedPtr instance, not AptFile itself.
//
// This is vendor/SDK code reconstructed in its canonical leak home
// (SDKs/EATech/include/Apt/). Per CXX_NAMING_CONVENTIONS.md the EA SDK
// identifiers (AptSharedPtr, AptFile, AptFilePtr, Dispose, Reset, pData) are an
// external/middleware API and kept verbatim.
//
// REFERENCE-COUNT MODEL (from the asm):
//   The pointee (the AptFile shared control object) begins with a 32-bit
//   reference count at offset 0. operator= mutates it with the X360
//   interrupt-masked load-linked / store-conditional idiom
//   (mfmsr/mtmsree/lwarx/stwcx.): increment the incoming object's count, then
//   decrement the outgoing object's count and delete it when the count reaches
//   zero. Dispose() routes the same decrement/free through two out-of-line
//   shared primitives (see the debug-hook note below).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/Apt/DogmaAllocator.h"   // DOGMA_PoolManager

struct AptFile;

// ---------------------------------------------------------------------------
// The three out-of-line reference-count primitives the Apt code calls on the
// shared AptFile. They are emitted once and shared across every use, so they are
// homed in AptSharedPtr.cpp (next to Dispose/operator=). PS3 EXTERNAL addresses:
//
//   AptSharedPtrIncRef(p)  @0x7E9210 -> atomically increment p's reference count
//                                       and return the NEW count.
//   AptSharedPtrDecRef(p)  @0x7E9240 -> atomically decrement p's reference count
//                                       and return the NEW count.
//   AptSharedPtrDelete(p)  @0x80CC64 -> ~AptFile(p) then free the block back to
//                                       the non-GC pool (count has reached zero).
// ---------------------------------------------------------------------------
int AptSharedPtrIncRef(AptFile* pData);
int AptSharedPtrDecRef(AptFile* pData);
int AptSharedPtrDelete(AptFile* pData);

// ---------------------------------------------------------------------------
// Dispose @0x8284D158 brackets its work with two argument-passing `STUB` calls
// (entry and exit). In the shipped X360 ARTIST build these are a debug/
// instrumentation thunk (allocation-tracking counter pair) whose body is not in
// the export set and which carries no engine behaviour; the inline no-op
// preserves the store/branch ORDER around it without fabricating a body.
// FLAG PC-platform leaf: X360 debug-instrumentation thunk (no PC counterpart; body absent from the export set).
inline void AptSharedPtrDisposeDebugHook(const void* /*a1*/, int /*a2*/, const char* /*a3*/) {}

// ---------------------------------------------------------------------------
// off_8324D808 -- the DOGMA_PoolManager* that backs the AptSharedPtr<AptFile>
// heap-array allocations (the `vector deleting destructor` frees its backing
// block through it). Defined in AptGlobals.cpp; constructed + wired by AptInit's
// AptAllocatorInitialize @0x82ADD118.
// ---------------------------------------------------------------------------
extern DOGMA_PoolManager* gpAptSharedPtrPool;   // off_8324D808

template <typename T>
struct AptSharedPtr;

template <>
struct AptSharedPtr<AptFile>
{
    AptFile* pData;

    // X360 @0x8284D158. Static: the asm takes the AptFile object pointer
    // directly in r3 (no AptSharedPtr `this`).
    static void Dispose(AptFile* pData);

    // X360 @0x82AFD548. Reference-counted copy assignment.
    AptSharedPtr<AptFile>& operator=(const AptSharedPtr<AptFile>& rhs);

    // X360 @0x82AEC678. MSVC array deleting destructor over a heap array of
    // AptSharedPtr<AptFile> (each element is a single pointer). `flags` is the
    // hidden MSVC second parameter: bit1 = array form, bit0 = also free the
    // backing block.
    void* VectorDeletingDestructor(char flags);
};

typedef AptSharedPtr<AptFile> AptFilePtr;

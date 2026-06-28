// ===========================================================================
// EATech Apt -- AptNativeFunction out-of-line bodies.
//
// Reconstructed from the X360 ARTIST.XEX (the authoritative spine):
//     AptNativeFunction::AptNativeFunction   @ 0x82AF0178
//     AptNativeFunction::operator new        @ 0x82AE60D8
//     AptNativeFunction::operator delete     @ 0x82AF01E8
// The `vector deleting destructor' @ 0x82AF0240 is a compiler thunk (restore
// vtable -> ~AptValueWithHash -> conditional operator delete) and is dropped, not
// hand-written; the implicit ~AptNativeFunction in the header covers it.
//
// See AptNativeFunction.h for the layout / base-class derivation.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptNativeFunction.h"

// ---------------------------------------------------------------------------
// ctor @ 0x82AF0178
//
// X360 store sequence: AptValueWithHash::AptValueWithHash(this, 9, 8); store the
// callback at +0x20; clear the +0x1C flags word; install the vtable; then
// SetAllowDelayedDeletion(false) (the `li r4, 0` argument).
// ---------------------------------------------------------------------------
AptNativeFunction::AptNativeFunction(AptExtFunctionPtr pFunction)
    : AptValueWithHash(AptVFT_NativeFunction, KI_HASH_CAPACITY)
    , mClassFlags(0)
    , mpFunction(pFunction)
{
    // X360 @0x82AF01CC: AptValue::SetAllowDelayedDeletion(this, 0).
    SetAllowDelayedDeletion(false);
}

// ---------------------------------------------------------------------------
// operator new @ 0x82AE60D8
//
// Allocate from the garbage-collected value pool (off_8324D834 == gpGCPoolManager)
// and mark the AptValueGC_MemItem header allocated (byte_8324D804 ==
// gAptValueGCSizeOffset selects the size-word offset). No null guard in the asm:
// this is only ever reached after the Apt runtime has wired the GC pool.
//
// The cast-to-AptValueGC_MemItem is allocator bookkeeping on the raw pool block
// (the high-bit "allocated" flag the GC walk reads), not a member poke into a live
// C++ object -- the same external-allocator pattern as
// AptValueGC_PoolManager::DeallocateAptValueGC.
// ---------------------------------------------------------------------------
void* AptNativeFunction::operator new(size_t size)
{
    void* lpMem = gpGCPoolManager->Allocate(size);
    reinterpret_cast<AptValueGC_MemItem*>(lpMem)->SetIsAllocated(gAptValueGCSizeOffset, true);
    return lpMem;
}

// ---------------------------------------------------------------------------
// operator delete @ 0x82AF01E8
//
// Return the block to the GC pool; on a successful free, clear the
// AptValueGC_MemItem allocated flag. (The X360 `clrlwi. r11, r3, 24` tests the
// bool result in r3 -- Deallocate returns true on success.)
// ---------------------------------------------------------------------------
void AptNativeFunction::operator delete(void* p, size_t size)
{
    if (gpGCPoolManager->Deallocate(p, size))
        reinterpret_cast<AptValueGC_MemItem*>(p)->SetIsAllocated(gAptValueGCSizeOffset, false);
}

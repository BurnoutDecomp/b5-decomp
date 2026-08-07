// ===========================================================================
// EATech Apt -- AptGlobalExtensionObject bodies.   Reconstructed from the X360
// ARTIST.XEX pseudocode/asm:
//     ctor          @ 0x82AF0688
//     operator new  @ 0x82AE6588
//     operator delete @ 0x82AF06E8
//     Set           @ 0x82AF5488
//     UnSet         @ 0x82AECD20
//     `vector deleting destructor' @ 0x82AF0750  (compiler thunk -- dropped;
//                                                 ~AptGlobalExtensionObject below
//                                                 is the real destructor body)
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptGlobalExtensionObject.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptDefine.h"             // gpGCPoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"          // AllocateAptValueGC / DeallocateAptValueGC

// ---------------------------------------------------------------------------
// operator new @0x82AE6588 -- GC-pool allocate + mark allocated.
//   v1 = DOGMA_PoolManager::Allocate(off_8324D834, size);          // off_8324D834 = gpGCPoolManager
//   AptValueGC_MemItem::SetIsAllocated(v1, byte_8324D804, 1);      // byte_8324D804 = gAptValueGCSizeOffset
//   return v1;
// The Allocate + SetIsAllocated(.,1) pair is the GC allocator's alloc operation
// (AllocateAptValueGC), folded inline by the X360. Guarded for null before
// AptAllocatorInitialize @0x82ADD118 (AptInit.cpp) wires gpGCPoolManager.
// ---------------------------------------------------------------------------
void* AptGlobalExtensionObject::operator new(size_t size)
{
    return (gpGCPoolManager != 0) ? gpGCPoolManager->AllocateAptValueGC(size) : 0;
}

// ---------------------------------------------------------------------------
// operator delete @0x82AF06E8 -- GC-pool free + clear allocated (on success).
//   if (DOGMA_PoolManager::Deallocate(off_8324D834, p, size))      // off_8324D834 = gpGCPoolManager
//       AptValueGC_MemItem::SetIsAllocated(p, byte_8324D804, 0);
// That Deallocate + gated SetIsAllocated(.,0) is exactly DeallocateAptValueGC.
// ---------------------------------------------------------------------------
void AptGlobalExtensionObject::operator delete(void* p, size_t size)
{
    if (gpGCPoolManager != 0)
        gpGCPoolManager->DeallocateAptValueGC(p, size);
}

// ---------------------------------------------------------------------------
// ctor @0x82AF0688
//   AptValueWithHash::AptValueWithHash(this, 30 /*AptVFT_GlobalExtension*/, 8);
//   this->mClassFlags = 0;                       // stb 0, 0x1C
//   this->mClassFlags &= 0xFF3FFFFF;             // rlwinm 0,10,7 -> clears bits 22,23 (no-op on 0)
//   *this = off_82145AD4;                         // AptGlobalExtensionObject vtable
//   AptValue::setGCRoot(this, 1);                 // brought up as a GC root
// The class-flags word is already zeroed by the AptObject base ctor; the inlined
// clear-of-bits-22,23 is therefore a no-op here, so the body is the base
// construction plus the GC-root mark.
// ---------------------------------------------------------------------------
AptGlobalExtensionObject::AptGlobalExtensionObject()
    : AptObject(AptVFT_GlobalExtension, 8)
{
    setGCRoot(1);
}

// ---------------------------------------------------------------------------
// destructor -- the `vector deleting destructor' thunk @0x82AF0750 sets this
// object's vtable then calls AptObject::~AptObject (and, with the delete flag,
// operator delete(this, 32)); i.e. AptGlobalExtensionObject adds nothing to the
// base teardown. The body is therefore empty; the base destructor releases the
// property hash.
// ---------------------------------------------------------------------------
AptGlobalExtensionObject::~AptGlobalExtensionObject()
{
}

// ---------------------------------------------------------------------------
// Set @0x82AF5488 -- `addi r3,r3,8 ; b AptNativeHash::Set`: tail-forward to the
// inherited property hash (mHash sits at +8 in AptValueWithHash), passing the
// key (r4) and value (r5) through unchanged.
// ---------------------------------------------------------------------------
void AptGlobalExtensionObject::Set(const EAStringC& key, AptValue* pValue)
{
    mHash.Set(key, pValue);
}

// ---------------------------------------------------------------------------
// UnSet @0x82AECD20 -- `addi r3,r3,8 ; b AptNativeHash::Unset`: tail-forward to
// the inherited property hash, passing the key (r4) through.
// ---------------------------------------------------------------------------
void AptGlobalExtensionObject::UnSet(const EAStringC& key)
{
    mHash.Unset(key);
}

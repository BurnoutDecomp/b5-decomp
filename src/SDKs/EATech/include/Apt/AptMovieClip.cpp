// ===========================================================================
// EATech Apt -- AptMovieClip bodies.   Reconstructed from the X360 ARTIST.XEX
// pseudocode/asm:
//     ctor            @ 0x82AF3128
//     operator new    @ 0x82AE91A0
//     ~AptMovieClip   @ 0x82AF31D8
//     `vector deleting destructor' @ 0x82AF31E8  (compiler thunk -- dropped;
//                                                 ~AptMovieClip below is the real
//                                                 destructor body)
//
// The scriptable MovieClip AS object: an AptObject distinguished only by its
// value type index AptVFT_MovieClip (22). It is a garbage-collected value, so it
// allocates from the GC value pool and participates in the GC mark/teardown via
// the inherited AptObject / AptValueWithHash virtuals.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptMovieClip.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AptValue (GC value base)
#include "SDKs/EATech/include/Apt/AptDefine.h"            // gpGCPoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"         // AllocateAptValueGC / DeallocateAptValueGC

// ---------------------------------------------------------------------------
// operator new @0x82AE91A0 -- GC-pool allocate + mark allocated.
//   v1 = DOGMA_PoolManager::Allocate(off_8324D834, size);          // off_8324D834 = gpGCPoolManager
//   AptValueGC_MemItem::SetIsAllocated(v1, byte_8324D804, 1);      // byte_8324D804 = gAptValueGCSizeOffset
//   return v1;
// The Allocate + SetIsAllocated(.,1) pair is the GC allocator's alloc operation
// (AllocateAptValueGC), folded inline by the X360 (gpGCPoolManager's Allocate is
// the inherited DOGMA_PoolManager::Allocate). Guarded for null until the Apt
// runtime startup (AptInit) wires the pool (FLAG: gpGCPoolManager is null until
// then). Same shape as the AptArray / AptPrototype / AptGlobalExtensionObject GC
// siblings.
// ---------------------------------------------------------------------------
void* AptMovieClip::operator new(size_t size)
{
    return (gpGCPoolManager != 0) ? gpGCPoolManager->AllocateAptValueGC(size) : 0;
}

// ---------------------------------------------------------------------------
// operator delete -- GC-pool free + clear allocated (on success).
//   if (DOGMA_PoolManager::Deallocate(off_8324D834, p, size))      // off_8324D834 = gpGCPoolManager
//       AptValueGC_MemItem::SetIsAllocated(p, byte_8324D804, 0);
// That Deallocate + gated SetIsAllocated(.,0) is exactly DeallocateAptValueGC.
// The X360 reaches this via the dropped `vector deleting destructor' thunk's
// `operator delete(this, 32)` call; the mirror of operator new above.
// ---------------------------------------------------------------------------
void AptMovieClip::operator delete(void* p, size_t size)
{
    if (gpGCPoolManager != 0)
        gpGCPoolManager->DeallocateAptValueGC(p, size);
}

// ---------------------------------------------------------------------------
// ctor @0x82AF3128
//   AptValueWithHash::AptValueWithHash(this, 22 /*AptVFT_MovieClip*/, 8);
//   this->mClassFlags = 0;                       // stb 0, 0x1C
//   this->mClassFlags &= 0xFF3FFFFF;             // rlwinm 0,10,7 -> clears bits 22,23 (no-op on 0)
//   *this = off_82145F44;                         // AptMovieClip vtable
// The X360 inlines the trivial AptObject base ctor down to the AptValueWithHash
// call plus the class-flags init; the class-flags word is already zeroed by that
// base ctor (mClassFlags(0)), so the inlined read-mask-write of bits 22,23 is a
// no-op. The body is therefore pure base construction -- unlike
// AptGlobalExtensionObject the MovieClip ctor brings up no GC-root mark, so there
// is nothing left to do here. (The vtable store is the compiler entering the
// most-derived ctor body and is not hand-written.)
// ---------------------------------------------------------------------------
AptMovieClip::AptMovieClip()
    : AptObject(AptVFT_MovieClip, 8)
{
}

// ---------------------------------------------------------------------------
// ~AptMovieClip @0x82AF31D8
//   *this = off_82145F44;                         // reset the AptMovieClip vtable
//   return AptObject::~AptObject(this);           // chain to the base teardown
// Empty: the X360 destructor resets the vtable then tail-chains to
// AptObject::~AptObject (which releases the embedded property hash). AptMovieClip
// owns no destructor work of its own, so the body is the codegen of an empty user
// destructor. The `vector deleting destructor' thunk @0x82AF31E8 is
// compiler-generated and intentionally not hand-written.
// ---------------------------------------------------------------------------
AptMovieClip::~AptMovieClip()
{
}

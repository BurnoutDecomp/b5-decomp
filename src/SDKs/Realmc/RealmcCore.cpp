#include "SDKs/Realmc/RealmcCore.h"

#include <intrin.h>  // _Interlocked* (MSVC) -- portable stand-in for the X360
                     // lwarx/stwcx. reservation idiom.

// ===========================================================================
// RealmcCore core primitives -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODIES both come from the X360 asm. See
// RealmcCore.h for the layout and the flagged platform/vendor externs.
// ===========================================================================

namespace RealmcCore
{

// The global allocator backend pointer (X360 off_832BE204). Defined here as a
// null-initialised pointer; the platform Realmc heap layer installs the real
// backend object at boot. (Owning definition for the `extern` in the header.)
IRealmcAllocatorBackend* g_pRealmcAllocator = nullptr;

// ---------------------------------------------------------------------------
// allocator::allocate @ 0x82C44BC8
//
//   lis  r11, off_832BE204@ha ; lwz r3, off_832BE204@l(r11)  -> r3 = backend
//   mr   r6, r5                                              -> r6 = nExtra
//   addi r5, r11, aRealmccoreAllo                            -> r5 = tag string
//   lwz  r10, 0(r3) ; lwz r11, 8(r10) ; mtctr r11 ; bctr     -> vtable slot +8
//
// Tail-call: backend->[+8](backend, r4=nSize, r5=tag, r6=nExtra).
// ---------------------------------------------------------------------------
void* allocator::allocate(std::size_t nSize, int nExtra)
{
    return g_pRealmcAllocator->Allocate(nSize, "RealmcCore::allocator", nExtra);
}

// ---------------------------------------------------------------------------
// allocator::deallocate @ 0x82C44BF0
//
//   lis r11, off_832BE204@ha ; lwz r3, off_832BE204@l(r11)   -> r3 = backend
//   lwz r11, 0(r3) ; lwz r11, 0xC(r11) ; mtctr r11 ; bctr    -> vtable slot +12
//
// Tail-call: backend->[+12](backend). The pseudocode passes only the backend;
// the sized-free signature (ptr, size) is reached with the X360-untouched
// argument registers, so the X360 caller leaves r4/r5 holding the block+size.
// Modelled faithfully as the no-argument forwarder the pseudocode shows.
// ---------------------------------------------------------------------------
void allocator::deallocate()
{
    g_pRealmcAllocator->Free(nullptr, 0);
}

// ---------------------------------------------------------------------------
// Message::Message @ 0x82C456D8
//
//   stw off_821BA2CC, 0(r3)              -> install base vtable
//   addi r7, r3, 4                       -> &muLock
//   <mfmsr/mtmsree/lwarx/stwcx./mtmsree> -> atomically store 0 to muLock,
//                                           retry while stwcx. fails (bne)
//   stw off_821BA2E8, 0(r3)              -> install final vtable
//
// The two vtable stores are MSVC's base-then-final ctor sequence (Message has a
// vtable-bearing base); the compiler reproduces both stores from the class
// definition. The interrupt-masked lwarx/stwcx. is the X360 reservation-init
// idiom; modelled portably as an atomic store of 0 to muLock.
// ---------------------------------------------------------------------------
Message::Message()
{
    _InterlockedExchange(reinterpret_cast<volatile long*>(&muLock), 0);
}

// ---------------------------------------------------------------------------
// Message::~Message
//
// Backs the X360 `vector deleting destructor' @ 0x82C45718, whose body is:
//   *a1 = off_821BA2CC                          -> restore base vtable
//   if (a2 & 1) backend->[+12](backend, a1, 8)  -> free 8 bytes (sizeof Message)
//   return a1
//
// MSVC synthesises the vector/scalar deleting destructor wrapper from this
// non-virtual-looking dtor + the class's operator delete path; the `8` is
// sizeof(Message) (vtable ptr + muLock). Nothing to do in the dtor body itself.
// ---------------------------------------------------------------------------
Message::~Message()
{
}

// ---------------------------------------------------------------------------
// Message::Apply @ 0x82C44C08
//
//   mr r11, r4 ; mr r4, r3 ; mr r3, r11   -> swap: r3 = pTarget, r4 = pThis
//   lwz r10, 0(r11) ; lwz r11, 0x54(r10)  -> pTarget vtable slot +0x54 (84)
//   mtctr r11 ; bctr                       -> tail-call (pTarget, pThis)
//
// i.e. pTarget->ApplyMessage(pThis). Note IDA's signature lists (a1=pThis,
// a2=pTarget); the asm swaps them so the *target* is `this` for the dispatch.
// ---------------------------------------------------------------------------
int Message::Apply(Message* pThis, IRealmcMessageTarget* pTarget)
{
    return pTarget->ApplyMessage(pThis);
}

} // namespace RealmcCore

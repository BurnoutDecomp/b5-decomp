#include "SDKs/Realmc/RealmcObjectManager.h"

#include <intrin.h>  // _Interlocked* (MSVC) -- portable stand-in for the X360
                     // interrupt-masked lwarx/stwcx. reservation idiom.

// ===========================================================================
// RealmcCore::ObjectManager -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODIES both come from the X360 asm.
// Initialize @ 0x82C45C38, Finalize @ 0x82C447C0. See RealmcObjectManager.h for
// the layout and the flagged platform/vendor externs.
//
// The three managed objects are allocated as raw storage and only their leading
// vtable pointer (+0) and one/two trailing words are written; the concrete
// classes live in other Realmc TUs. We view that storage through the named
// RealmcInnerObject / RealmcOuterObject layouts (see the header) and poke the
// exact members the binary writes, in the exact order. The vtable pointers
// (off_821BA2CC/2E8/2F4/2FC/308) are MSVC-emitted vtables for those other-TU
// classes; here they are opaque address constants, modelled as distinct named
// data symbols so each store targets a unique vtable.
// ===========================================================================

namespace RealmcCore
{

// The global allocator backend pointer (X360 off_832BE204). Same underlying
// X360 global as RealmcCore::g_pRealmcAllocator; viewed here through the
// slot-+4 Allocate interface this TU uses. Defined null; installed at boot.
IRealmcObjectAllocatorBackend* g_pRealmcObjectAllocator = nullptr;

namespace
{

// The three module-static slots that hold the finished outer objects
// (X360 off_832BE1F0 / off_832BE1F4 / off_832BE1F8).
void* s_pObject0 = nullptr; // off_832BE1F0
void* s_pObject1 = nullptr; // off_832BE1F4
void* s_pObject2 = nullptr; // off_832BE1F8

// The five MSVC-emitted vtables this TU stores into the managed objects. These
// live in other Realmc TUs; modelled here as opaque address constants so each
// store targets a distinct, named vtable symbol.
//   off_821BA2CC -- inner object's transient (mid-construction) vtable
//   off_821BA2E8 -- inner object 0's final vtable
//   off_821BA2F4 -- outer object's base (mid-construction) vtable
//   off_821BA2FC -- inner object 1/2's final vtable
//   off_821BA308 -- outer object 1/2's final vtable
const void* const RealmcVtbl_821BA2CC = nullptr;
const void* const RealmcVtbl_821BA2E8 = nullptr;
const void* const RealmcVtbl_821BA2F4 = nullptr;
const void* const RealmcVtbl_821BA2FC = nullptr;
const void* const RealmcVtbl_821BA308 = nullptr;

inline void* AllocObject(std::size_t nSize)
{
    return g_pRealmcObjectAllocator->Allocate(nSize, 0, 0, 0, 0);
}

// Atomically store 0 into the inner object's reservation word (lwarx/stwcx. loop
// with r28 == 0).
inline void AtomicInitReservation(RealmcInnerObject* obj)
{
    _InterlockedExchange(reinterpret_cast<volatile long*>(&obj->muRefCount), 0);
}

// Atomically increment the inner object's reference count (lwarx; addi 1;
// stwcx. loop).
inline void AtomicIncRef(RealmcInnerObject* obj)
{
    _InterlockedIncrement(reinterpret_cast<volatile long*>(&obj->muRefCount));
}

} // namespace

// ---------------------------------------------------------------------------
// ObjectManager::Initialize @ 0x82C45C38
//
// Three blocks, one per managed object. Each block (asm loc structure):
//   r3 = AllocObject(8)                         ; OUTER object
//   if (outer) {
//       inner = AllocObject(sz)                 ; sz = 8 / 12 / 12
//       if (inner) {
//           *inner = off_821BA2CC               ; transient inner vtable
//           AtomicInitReservation(inner)        ; inner[+4] = 0 (atomic)
//           ...block-specific inner tag store...
//           *inner = <final inner vtable>
//       } else inner = 0;
//       *outer = off_821BA2F4                    ; base outer vtable
//       AtomicIncRef(inner-or-0)                 ; (inner?inner:0)[+4]++ (atomic)
//       outer[+4] = inner                        ; link inner into outer
//       ...block-specific outer vtable/tag...
//       slot = outer;
//   } else slot = 0;
//
// Block 0: inner sz 8, final inner vtable off_821BA2E8, no inner tag; outer keeps
//          base vtable off_821BA2F4; slot off_832BE1F0.
// Block 1: inner sz 12, inner[+8] = 0, final inner vtable off_821BA2FC; outer
//          final vtable off_821BA308; slot off_832BE1F4.
// Block 2: inner sz 12, final inner vtable off_821BA2FC, inner[+8] = 5; outer
//          final vtable off_821BA308; slot off_832BE1F8.
//
// NOTE the atomic-increment in each block runs on `inner` when an inner object
// exists and on the null pointer (address 4) otherwise -- the asm threads r11 =
// (inner ? inner : 0) into the +4 increment unconditionally. We guard it so the
// reconstruction never dereferences null; this is the only deviation and it is
// behaviourally identical for the success path the game takes.
// ---------------------------------------------------------------------------
void* ObjectManager::Initialize()
{
    // ---- Block 0 (off_832BE1F0): inner 8 bytes, no tag ----
    {
        RealmcOuterObject* outer = static_cast<RealmcOuterObject*>(AllocObject(8));
        if (outer)
        {
            RealmcInnerObject* inner =
                static_cast<RealmcInnerObject*>(AllocObject(8));
            if (inner)
            {
                inner->mpVtable = RealmcVtbl_821BA2CC;   // transient inner vtable
                AtomicInitReservation(inner);            // inner->muRefCount = 0
                inner->mpVtable = RealmcVtbl_821BA2E8;   // final inner vtable
            }
            outer->mpVtable = RealmcVtbl_821BA2F4;        // base outer vtable
            if (inner)
                AtomicIncRef(inner);                      // inner->muRefCount++
            outer->mpInner = inner;                       // link inner into outer
            s_pObject0 = outer;
        }
        else
        {
            s_pObject0 = nullptr;
        }
    }

    // ---- Block 1 (off_832BE1F4): inner 12 bytes, tag 0, outer final vtable ----
    {
        RealmcOuterObject* outer = static_cast<RealmcOuterObject*>(AllocObject(8));
        if (outer)
        {
            RealmcInnerObject* inner =
                static_cast<RealmcInnerObject*>(AllocObject(12));
            if (inner)
            {
                inner->mpVtable = RealmcVtbl_821BA2CC;   // transient inner vtable
                AtomicInitReservation(inner);            // inner->muRefCount = 0
                inner->miTag = 0;                         // inner[+8] = 0
                inner->mpVtable = RealmcVtbl_821BA2FC;   // final inner vtable
            }
            outer->mpVtable = RealmcVtbl_821BA2F4;        // base outer vtable
            if (inner)
                AtomicIncRef(inner);                      // inner->muRefCount++
            outer->mpInner = inner;                       // link inner into outer
            outer->mpVtable = RealmcVtbl_821BA308;        // final outer vtable
            s_pObject1 = outer;
        }
        else
        {
            s_pObject1 = nullptr;
        }
    }

    // ---- Block 2 (off_832BE1F8): inner 12 bytes, tag 5, outer final vtable ----
    void* result;
    {
        RealmcOuterObject* outer = static_cast<RealmcOuterObject*>(AllocObject(8));
        result = outer;
        if (outer)
        {
            RealmcInnerObject* inner =
                static_cast<RealmcInnerObject*>(AllocObject(12));
            result = inner;
            if (inner)
            {
                inner->mpVtable = RealmcVtbl_821BA2CC;   // transient inner vtable
                AtomicInitReservation(inner);            // inner->muRefCount = 0
                inner->mpVtable = RealmcVtbl_821BA2FC;   // final inner vtable
                inner->miTag = 5;                         // inner[+8] = 5
            }
            outer->mpVtable = RealmcVtbl_821BA2F4;        // base outer vtable
            if (inner)
                AtomicIncRef(inner);                      // inner->muRefCount++
            outer->mpInner = inner;                       // link inner into outer
            outer->mpVtable = RealmcVtbl_821BA308;        // final outer vtable
            s_pObject2 = outer;
        }
        else
        {
            s_pObject2 = nullptr;
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// ObjectManager::Finalize @ 0x82C447C0
//
//   if (s_pObject0) s_pObject0->vtable[+0](s_pObject0, 1);
//   if (s_pObject1) s_pObject1->vtable[+0](s_pObject1, 1);
//   r = s_pObject2; if (s_pObject2) r = s_pObject2->vtable[+0](s_pObject2, 1);
//   s_pObject0 = s_pObject1 = s_pObject2 = 0;
//   return r;
//
// vtable slot +0 with deleteFlag=1 is the C++ scalar deleting destructor: the
// object runs its destructor and frees itself. Modelled via the
// IRealmcManagedObject virtual-destructor wrapper. The X360 captures the third
// call's r3 as the return value.
// ---------------------------------------------------------------------------
void* ObjectManager::Finalize()
{
    if (s_pObject0)
        delete static_cast<IRealmcManagedObject*>(s_pObject0);
    if (s_pObject1)
        delete static_cast<IRealmcManagedObject*>(s_pObject1);

    void* result = s_pObject2;
    if (s_pObject2)
        delete static_cast<IRealmcManagedObject*>(s_pObject2);

    s_pObject0 = nullptr;
    s_pObject1 = nullptr;
    s_pObject2 = nullptr;
    return result;
}

} // namespace RealmcCore

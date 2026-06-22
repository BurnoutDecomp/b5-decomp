#pragma once

// ===========================================================================
// RealmcCore::ObjectManager -- the Realmc memory-card "object manager" that
// constructs and tears down the three persistent Realmc backend objects used by
// the X360 memory-card interface (the RealmcIface / RealmcCore family in
// BURNOUT_X360_ARTIST.XEX). This header is the canonical OWNING home for:
//
//     RealmcCore::ObjectManager::Initialize  @ 0x82C45C38
//     RealmcCore::ObjectManager::Finalize    @ 0x82C447C0
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcCore, ObjectManager, Initialize,
// Finalize) are preserved verbatim per the naming convention. Sibling to
// RealmcCore.h.
//
// WHAT Initialize() DOES (from asm): three near-identical blocks, one per
// managed object, each of which
//   1. allocates an OUTER object (8 bytes) via the global Realmc allocator,
//   2. if that succeeds, allocates an INNER object and atomically zero-inits a
//      32-bit reservation word in it (the X360 lwarx/stwcx. interrupt-masked
//      init idiom), installs the inner object's vtable, and (for blocks 2 and 3)
//      stamps a small tag word,
//   3. installs the outer object's vtable, links the inner object into the
//      outer object, atomically increments a 32-bit reference count in the
//      inner object, and
//   4. stores the finished outer object into one of three module-static slots
//      (X360 off_832BE1F0 / off_832BE1F4 / off_832BE1F8).
// On allocation failure of an outer object the corresponding slot is nulled.
//
// Finalize() releases each of the three objects by calling its vtable slot +0
// (the C++ "scalar deleting destructor", invoked with deleteFlag=1 so the object
// frees itself) and nulls all three slots.
//
// PLATFORM/VENDOR EXTERNS (flagged):
//   * g_pRealmcObjectAllocator (X360 off_832BE204) -- a global pointer to the
//     Realmc allocator backend object. Reached here through vtable slot +4
//     (Allocate(size,a,b,c,d)). NOTE this is the SAME X360 global as RealmcCore's
//     g_pRealmcAllocator, but ObjectManager dispatches Allocate through vtable
//     slot +4 whereas RealmcCore::allocator dispatches through slot +8; the two
//     TUs view the backend through differently-shaped local interface decls.
//     Declared here with the slot order this TU actually uses, to keep the
//     reconstruction faithful to its own call sites. Installed by the platform
//     Realmc heap layer (another TU).
//   * The managed-object vtables (X360 off_821BA2CC / off_821BA2E8 /
//     off_821BA2F4 / off_821BA2FC / off_821BA308) are the C++ vtables MSVC emits
//     for the inner/outer object classes; the concrete classes live in other
//     Realmc TUs. This TU only allocates raw storage and pokes the leading
//     vtable pointer + a couple of words, so the objects are modelled here as
//     opaque byte blobs whose first word is a vtable pointer.
// ===========================================================================

#include <cstddef>
#include <cstdint>

namespace RealmcCore
{

// ---------------------------------------------------------------------------
// IRealmcObjectAllocatorBackend -- the allocator object reached through the
// global g_pRealmcObjectAllocator pointer (X360 off_832BE204). ObjectManager
// dispatches allocation through vtable slot +4 with five argument words
// (size, 0, 0, 0, 0); only the size is meaningful at these call sites.
// ---------------------------------------------------------------------------
class IRealmcObjectAllocatorBackend
{
public:
    virtual ~IRealmcObjectAllocatorBackend() {}          // vtable slot +0
    virtual void* Allocate(std::size_t nSize,
                           int a, int b, int c, int d) = 0; // vtable slot +4
};

// The global Realmc allocator backend (X360 off_832BE204). Installed by the
// platform Realmc heap layer (another TU); declared here for compile/link.
extern IRealmcObjectAllocatorBackend* g_pRealmcObjectAllocator;

// ---------------------------------------------------------------------------
// IRealmcManagedObject -- the shape ObjectManager treats every allocated outer
// and inner object as: a leading vtable pointer whose slot +0 is the scalar
// deleting destructor (called in Finalize with deleteFlag=1). The padding words
// below it pin the size the manager allocates / writes into.
//
// Finalize() does:  obj->vtable[+0](obj, /*deleteFlag*/ 1);
// modelled as the virtual destructor wrapper (slot +0).
// ---------------------------------------------------------------------------
class IRealmcManagedObject
{
public:
    virtual ~IRealmcManagedObject() {}                   // vtable slot +0
};

// ---------------------------------------------------------------------------
// RealmcInnerObject -- the inner object ObjectManager allocates and reference-
// counts. The asm writes (in order): a transient vtable at +0, atomically zeroes
// a reservation word at +4, optionally a tag at +8, the final vtable at +0,
// then (after linking) atomically increments the +4 word as a refcount.
//   +0  mpVtable  -- vtable pointer (transient then final)
//   +4  muRefCount-- reservation word, atomically zeroed then incremented
//   +8  miTag     -- 0 (block 1) / 5 (block 2); absent on block 0's 8-byte inner
struct RealmcInnerObject
{
    const void*   mpVtable;    // +0
    long          muRefCount;  // +4 (X360) -- atomic reservation/refcount word
    int           miTag;       // +8 (X360) -- present on the 12-byte inners
};

// RealmcOuterObject -- the outer object published into a static slot. The asm
// writes a base vtable at +0, links the inner object pointer at +4, then (blocks
// 1/2) overwrites +0 with the final vtable.
//   +0  mpVtable  -- vtable pointer (base then, blocks 1/2, final)
//   +4  mpInner   -- pointer to the linked inner object
//
// On X360 each pointer is 4 bytes; on a 64-bit host they widen, but every store
// goes through a named member, so the reconstruction is store-for-store and
// warning-clean.
struct RealmcOuterObject
{
    const void*        mpVtable; // +0
    RealmcInnerObject* mpInner;  // +4
};

// ---------------------------------------------------------------------------
// RealmcCore::ObjectManager -- a stateless façade; the three managed objects it
// owns live in the module-static slots (X360 off_832BE1F0/F4/F8), defined in
// RealmcObjectManager.cpp.
// ---------------------------------------------------------------------------
class ObjectManager
{
public:
    // @ 0x82C45C38 -- construct the three managed objects and publish them into
    // the three static slots. Returns the last-allocated object (X360 r3 == the
    // third block's result), matching the binary's _DWORD* return.
    static void* Initialize();

    // @ 0x82C447C0 -- release the three managed objects (vtable slot +0 with
    // deleteFlag=1) and null the three static slots. Returns the third slot's
    // pre-release value, matching the binary's void* return.
    static void* Finalize();
};

} // namespace RealmcCore

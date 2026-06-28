#pragma once

// ===========================================================================
// EATech Apt -- AptNativeHash: the ActionScript property table (string -> value).
//
// An open-addressing hash of EAStringC keys to ref-counted AptValue* values, with
// dedicated fast-slots for the magic __proto__ / prototype keys. Used pervasively:
// an AS object's members, the VM globals, and AptMovie's frame-label map.
//
// SHAPE + BODIES from the PS3 EXTERNAL ELF (cross-checked vs X360 ARTIST). STRUCT
// (console 20 bytes / 5 dwords; reconstructed with named members):
//   [0] mnCapacity  power-of-two table size (the ctor rounds up)
//   [1] mpTable     the bucket array (AptHashItem[mnCapacity]); null until the
//                   first insert (FirstAllocation). IsEmpty() == (mpTable==null).
//   [2] mp__Proto__ fast-slot for the __proto__ key (case-folded-FNV hash 27581)
//   [3] mpPrototype fast-slot for the prototype key (hash 1689)
//   [4] mnField4    zeroed by the ctor; not touched by the core set/get paths.
//
// A bucket key has THREE states: unused (raw null m_pData -- FirstAllocation
// memsets 0), tombstone (the empty string, left by a delete), occupied (a real
// ref-counted key). The probe is a bounded +/-8-bucket linear scan around the
// home bucket; if the key is not found and there is no reusable slot in that
// window, the table doubles (Expand) and the insert retries.
//
// AptValue refs: SetAt/OverwriteAt/the proto setters AddRef the new value and
// Release the old via the AptValue vtable (AddRef=vtbl[0], Release=vtbl[1]).
//
// SCOPE: the insert/lookup/lifecycle core. The DELETION path (Unset/UnsetAt/
// ClearData) + event-handlers + iteration + the AS object-method helpers are
// deferred. With deletion deferred, tombstones never arise at runtime, but the
// tombstone handling is reconstructed faithfully anyway.
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include <cstdint>

#include "SDKs/EATech/include/Apt/AptString/EAString.h"   // EAStringC keys (friend access)

struct AptValue;

// One bucket: a ref-counted key + its value. Console 8 bytes {key@+0, value@+4}.
struct AptHashItem
{
    EAStringC mKey;
    AptValue* mpValue;
};

// ---------------------------------------------------------------------------
// FLAG (homed by the Apt string-pool TU, not yet built): the two magic property
// keys the table routes to the fast-slots. StringPool::saConstant is the
// __proto__ key (hash 27581 -> mp__Proto__); gAptKeyPrototype is the prototype
// key (the binary's dword_1059C880, hash 1689 -> mpPrototype). Declared extern so
// Set/Lookup's special-case compiles; the hash literals are their case-folded FNV.
// ---------------------------------------------------------------------------
namespace StringPool { extern const EAStringC saConstant; }
extern const EAStringC gAptKeyPrototype;

struct AptNativeHash
{
    int32_t      mnCapacity;
    AptHashItem* mpTable;
    AptValue*    mp__Proto__;
    AptValue*    mpPrototype;
    int32_t      mnField4;

    explicit AptNativeHash(int32_t nCapacity);   // @0x7E3FD8
    ~AptNativeHash();                             // @0x7F8638 -> DestroyGCPointers

    // Store/replace pValue under key (routing __proto__/prototype to the slots).
    void Set(const EAStringC& key, AptValue* pValue);     // @0x800484
    // The value for key, or null. @0x7F8DF8
    AptValue* Lookup(const EAStringC& key) const;
    // @0x7DF28C -- true when no table has been allocated yet.
    bool IsEmpty() const;

    AptValue* Get__Proto__() const;                       // @0x7DF27C
    AptValue* GetPrototype() const;                       // @0x7DF284
    void      Set__Proto__(AptValue* pValue);             // @0x7DF8AC
    void      SetPrototype(AptValue* pValue);             // @0x7DF94C

private:
    void         FirstAllocation();                       // @0x7EFB34
    void         Expand();                                // @0x7F7EB8
    AptHashItem* HashFindKey(const EAStringC& key) const; // @0x7E8188
    void         HashSet(const EAStringC& key, AptValue* pValue); // @0x7F7FA4
    void         SetAt(int32_t nIndex, AptValue* pValue);        // @0x7E40EC
    void         OverwriteAt(int32_t nIndex, AptValue* pValue);  // @0x7E4238
    AptValue*    GetAt(int32_t nIndex) const;             // @0x7DF2D0
    void         DestroyGCPointers();                     // @0x7F7D88
    void         UnsetPrototype();                        // @0x7E402C
    void         Unset__Proto__();                        // @0x7E408C
};

#pragma once

// ===========================================================================
// EATech Apt -- AptNativeHash: the ActionScript property table (string -> value).
//
// An open-addressing hash of EAStringC keys to ref-counted AptValue* values, with
// dedicated fast-slots for the magic __proto__ / prototype keys. Used pervasively:
// an AS object's members, the VM globals, and AptMovie's frame-label map.
//
// SHAPE + BODIES from the X360 ARTIST.XEX (the authoritative spine; this is the
// build the FIGS branch was merged into), cross-checked against the PS3 EXTERNAL
// ELF for the shared core. STRUCT (console 20 bytes / 5 dwords; reconstructed with
// named members; Deallocate(this, 20) in the scalar-deleting dtor pins sizeof):
//   [0] mnCapacity         power-of-two table size (the ctor rounds up)        +0x0
//   [1] mpTable            the bucket array (AptHashItem[mnCapacity]); null     +0x4
//                          until the first insert (FirstAllocation).
//                          IsEmpty() == (mpTable==null).
//   [2] mp__Proto__        fast-slot for the __proto__ key (case-folded hash    +0x8
//                          27581 / 0x6BBD).
//   [3] mpPrototype        fast-slot for the prototype key (hash 1689 / 0x699). +0xC
//   [4] mnEventHandlerMask packed AS event-handler bitmask -- read/written by   +0x10
//                          HasEventHandler/SetEventHandler/RemoveEventHandler and
//                          maintained by UpdateObjectMethods. (The PS3-ELF recon
//                          left this field unnamed; the X360 spine uses it as the
//                          event mask -- @0x82AD5200/0x82AD51E0/0x82AD51F0.)
//
// A bucket key has THREE states: unused (raw null m_pData -- FirstAllocation
// memsets 0), tombstone (the empty-string sentinel s_EmptyInternalData, X360
// unk_82F72FF8, left by a delete), occupied (a real ref-counted key). The probe is
// a bounded +/-8-bucket linear scan around the home bucket; if the key is not
// found and there is no reusable slot in that window, the table doubles (Expand)
// and the insert retries.
//
// AptValue refs: SetAt/OverwriteAt/the proto setters AddRef the new value and
// Release the old via the AptValue vtable (AddRef=vtbl[0], Release=vtbl[1]).
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
// key (the binary's dword_8324E580/0x8324E698 key bodies, hash 1689 ->
// mpPrototype). Declared extern so Set/Lookup's special-case compiles; the hash
// literals are their case-folded FNV.
// ---------------------------------------------------------------------------
// StringPool is the AS string recycler/pool -- a CLASS (AptString.h declares
// `friend class StringPool`), not a namespace. Only its saConstant key (the
// __proto__ key) is needed here; the full class + the static's definition are the
// string-pool TU's follow-on.
class StringPool { public: static const EAStringC saConstant; };
extern const EAStringC gAptKeyPrototype;

struct AptNativeHash
{
    int32_t      mnCapacity;          // +0x0
    AptHashItem* mpTable;             // +0x4
    AptValue*    mp__Proto__;         // +0x8
    AptValue*    mpPrototype;         // +0xC
    int32_t      mnEventHandlerMask;  // +0x10

    explicit AptNativeHash(int32_t nCapacity);   // @0x82AD9EA0
    ~AptNativeHash();                             // X360: dtor folds into DestroyGCPointers

    // Store/replace pValue under key (routing __proto__/prototype to the slots).
    void Set(const EAStringC& key, AptValue* pValue);     // @0x82AF4B08
    // Remove key (releasing its value/key, or clearing the proto slot). @0x82AEB528
    void Unset(const EAStringC& key);
    // The value for key, or null. (Shared core; PS3 @0x7F8DF8.)
    AptValue* Lookup(const EAStringC& key) const;
    // X360 @0x82AD51D0 -- true when no table has been allocated yet.
    bool IsEmpty() const;

    // @0x82ADA3C0 -- the GC mark walk: register every held AptValue reference (the
    // proto slots + each bucket value) with the collector, owned by pOwner.
    void RegisterReferences(const AptValue* pOwner);

    AptValue* Get__Proto__() const;                       // shared core
    AptValue* GetPrototype() const;                       // @0x82BBD940
    void      Set__Proto__(AptValue* pValue);             // @0x82AD9F50
    void      SetPrototype(AptValue* pValue);             // @0x82ADA010

    // ---- bucket iteration (used by enumerate / clone / urlEncode) ------------
    AptHashItem* GetFirstItem() const;                    // @0x82ADA0D0
    AptHashItem* GetNextItem(const AptHashItem* pCurrent) const;   // @0x82ADA130

    // ---- packed AS event-handler bitmask ------------------------------------
    int32_t HasEventHandler(int32_t nMask) const;         // @0x82AD5200
    void    SetEventHandler(int32_t nMask);               // @0x82AD51E0
    void    RemoveEventHandler(int32_t nMask);            // @0x82AD51F0
    // Maintain the event-handler mask from a member name being set/cleared.
    void    UpdateObjectMethods(const AptValue* pKeyValue, const EAStringC* pKeyName,
                                bool bRemoving);           // @0x82ADA2F8

    // Release every held value/key + free the table (the dtor + the
    // AptValueWithHash teardown call this). @0x82AEB458
    void      DestroyGCPointers();

    // Release every held value/key, KEEPING the (now empty) bucket array, and call
    // the value Releases (ClearData) or skip the table free (ClearDataNoDelete).
    void      ClearData();                                // @0x82AEB620
    void      ClearDataNoDelete();                        // @0x82AEB720

private:
    void         FirstAllocation();                       // @0x82AE5190
    void         Expand();                                // @0x82AEED80
    AptHashItem* HashFindKey(const EAStringC& key) const; // @0x82ADA188
    void         HashSet(const EAStringC& key, AptValue* pValue); // shared core
    void         SetAt(int32_t nIndex, AptValue* pValue);        // @0x82AD9EE8
    void         OverwriteAt(int32_t nIndex, AptValue* pValue);  // shared core
    AptValue*    GetAt(int32_t nIndex) const;             // shared core
    void         UnsetPrototype();                        // @0x82ADA080
    void         Unset__Proto__();                        // shared core (X360 sibling)
};

// ===========================================================================
// EATech Apt -- AptNativeHash (the ActionScript property table).
//
// Reconstructed from the X360 ARTIST.XEX (the authoritative spine), cross-checked
// against the PS3 EXTERNAL ELF for the shared insert/lookup core. X360 addresses:
//   ctor 0x82AD9EA0 / FirstAllocation 0x82AE5190 / Expand 0x82AEED80 /
//   HashFindKey 0x82ADA188 / Set 0x82AF4B08 / Unset 0x82AEB528 /
//   SetAt 0x82AD9EE8 / IsEmpty 0x82AD51D0 / GetPrototype 0x82BBD940 /
//   SetPrototype 0x82ADA010 / Set__Proto__ 0x82AD9F50 / UnsetPrototype 0x82ADA080 /
//   DestroyGCPointers 0x82AEB458 / ClearData 0x82AEB620 / ClearDataNoDelete
//   0x82AEB720 / RegisterReferences 0x82ADA3C0 / GetFirstItem 0x82ADA0D0 /
//   GetNextItem 0x82ADA130 / HasEventHandler 0x82AD5200 / SetEventHandler
//   0x82AD51E0 / RemoveEventHandler 0x82AD51F0 / UpdateObjectMethods 0x82ADA2F8 /
//   'scalar deleting destructor' 0x82AF0918.
//
// Bucket key states: unused (m_pData == null, from the FirstAllocation memset),
// tombstone (the empty-string sentinel s_EmptyInternalData == X360 unk_82F72FF8,
// left by a delete), occupied (a real key). The probe is a bounded +/-8-bucket
// linear scan around the home bucket; on a full window with no reusable slot the
// table doubles (Expand) and the insert retries. Values are AddRef/Release'd
// through the AptValue vtable (AddRef=vtbl[0], Release=vtbl[1]).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AddRef/Release (value refs)
#include "SDKs/EATech/include/Apt/AptDefine.h"            // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"                // DOGMA_PoolManager
#include "SDKs/Packages/Apt/2.00.00/source/AptValue/AptSprite.h"  // SpriteMembersIndex (member-name gperf)

#include <cstring>   // memset / strcmp
#include <cstdio>    // snprintf (the dead-bucket report below)

// ---------------------------------------------------------------------------
// [FLAG PC bring-up guard -- 2026-08-29, the CrashNav map-screen exit AV]
//
// DestroyGCPointers below makes a VIRTUAL call (Release) through every non-null
// bucket value. On this build a bucket can outlive its value: measured on the map
// screen's exit teardown, the 'ControlledTextBox_mc' entry of a movie-clip property
// hash still pointed at an AptCIH that AptAnimationTarget::CleanRemList had already
// released and returned to the Apt pool, so the virtual call read the DOGMA free-list
// link out of word 0 and executed it (EXECUTING 0x0000000018000000, reported from
// AptNativeHash::DestroyGCPointers+0x6C).
//
// Every real AptValue vtable lives in the executable image, so "vtable outside the
// image" is a sound liveness test. The guard drops the dead bucket instead of calling
// into it, and REPORTS each hit so the underlying missing reference stays visible
// rather than becoming a silent crash. Delete this when the unbalanced Release that
// drops the node's refcount to the list-only value is found (see the wave notes).
// ---------------------------------------------------------------------------
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

namespace
{
    struct AptHashImageRange
    {
        uintptr_t muBase;
        uintptr_t muEnd;
        AptHashImageRange() : muBase(0), muEnd(0)
        {
            muBase = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
            if (muBase == 0)
                return;
            const IMAGE_DOS_HEADER* lpDos = reinterpret_cast<const IMAGE_DOS_HEADER*>(muBase);
            if (lpDos->e_magic != IMAGE_DOS_SIGNATURE)
                return;
            const IMAGE_NT_HEADERS* lpNt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
                muBase + static_cast<uintptr_t>(lpDos->e_lfanew));
            if (lpNt->Signature == IMAGE_NT_SIGNATURE)
                muEnd = muBase + lpNt->OptionalHeader.SizeOfImage;
        }
    };
    const AptHashImageRange& AptHashImage()
    {
        static AptHashImageRange sRange;
        return sRange;
    }

    bool AptHashPlausiblePointer(const void* lpPointer)
    {
        const uintptr_t lu = reinterpret_cast<uintptr_t>(lpPointer);
        return lu >= 0x10000u && lu < (1ull << 47) && (lu & 7u) == 0u;
    }

    // A live AptValue: a plausible object whose vtable, and whose Release slot inside
    // that vtable, both land in the executable image.
    bool AptHashValueLooksLive(const void* lpValue)
    {
        const AptHashImageRange& lrImage = AptHashImage();
        if (lrImage.muEnd == 0)
            return true;                     // cannot judge -- behave as the console does
        if (!AptHashPlausiblePointer(lpValue))
            return false;
        if (IsBadReadPtr(lpValue, sizeof(void*)))
            return false;
        const void* lpVTable = *reinterpret_cast<void* const*>(lpValue);
        const uintptr_t luVTable = reinterpret_cast<uintptr_t>(lpVTable);
        if (luVTable < lrImage.muBase || luVTable >= lrImage.muEnd)
            return false;
        if (IsBadReadPtr(lpVTable, 2 * sizeof(void*)))
            return false;
        const uintptr_t luRelease =
            reinterpret_cast<uintptr_t>(reinterpret_cast<void* const*>(lpVTable)[1]);
        return luRelease >= lrImage.muBase && luRelease < lrImage.muEnd;
    }

    void AptHashReportDeadValue(const void* lpHash, int liBucket, int liCapacity,
                                const char* lpcKey, const void* lpValue)
    {
        static int siLeft = 24;
        if (siLeft <= 0)
            return;
        --siLeft;
        uintptr_t luVTable = 0;
        if (AptHashPlausiblePointer(lpValue) && !IsBadReadPtr(lpValue, sizeof(void*)))
            luVTable = reinterpret_cast<uintptr_t>(*reinterpret_cast<void* const*>(lpValue));
        const uintptr_t luBase = AptHashImage().muBase;
        char lacLine[512];
        std::snprintf(lacLine, sizeof(lacLine),
            "[apt-dead-value] hash=0x%llX bucket=%d/%d key='%s' value=0x%llX vtable=0x%llX "
            "(the bucket outlived its AptValue -- guard dropped it instead of calling Release)\n",
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(lpHash)),
            liBucket, liCapacity, (lpcKey ? lpcKey : "<null>"),
            static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(lpValue)),
            static_cast<unsigned long long>(luVTable >= luBase ? luVTable - luBase : luVTable));
        CgsDev::Log::WriteToLog(lacLine);
    }
}

// ---------------------------------------------------------------------------
// dword_82143BA8 (X360 .rdata): member-index -> event-handler-bit lookup used by
// UpdateObjectMethods. Indexed by (resolved member index - 200); a value of -1
// means "this member is not an AS event handler". The table is generated/static
// .rdata and is NOT recoverable from the per-function exports (no .rdata dump),
// exactly like the gperf asso_values/wordlist tables of the sibling member-index
// classes -- declared opaque here and populated from a binary dump before link.
// The per-TU gate is `cl /c`, which this declaration satisfies.
// ---------------------------------------------------------------------------
extern const int32_t gAptMemberIndexToEventBit[];   // X360 dword_82143BA8

// ---------------------------------------------------------------------------
// ctor @0x82AD9EA0 -- store the capacity (rounded up to a power of two), null the
// table + slots + event mask.
// ---------------------------------------------------------------------------
AptNativeHash::AptNativeHash(int32_t nCapacity)
{
    mnCapacity         = nCapacity;
    mpTable            = nullptr;
    mp__Proto__        = nullptr;
    mpPrototype        = nullptr;
    mnEventHandlerMask = 0;

    if (((nCapacity - 1) & nCapacity) != 0)   // not already a power of two
    {
        int32_t nRounded = 1;
        for (nRounded = 1; nRounded < nCapacity; nRounded *= 2)
            ;
        mnCapacity = nRounded;
    }
}

// X360: there is no standalone ~AptNativeHash; the scalar-deleting destructor
// (@0x82AF0918) and the AptValueWithHash teardown reach the same teardown through
// DestroyGCPointers. Modelled as the C++ destructor for embedded-by-value use.
AptNativeHash::~AptNativeHash()
{
    if (mpTable)
        DestroyGCPointers();
}

// FirstAllocation @0x82AE5190 -- allocate + zero the bucket array.
void AptNativeHash::FirstAllocation()
{
    const size_t nBytes = sizeof(AptHashItem) * mnCapacity;
    mpTable = static_cast<AptHashItem*>(gpNonGCPoolManager->Allocate(nBytes));
    memset(mpTable, 0, nBytes);
}

bool AptNativeHash::IsEmpty() const { return mpTable == nullptr; }   // @0x82AD51D0

AptValue* AptNativeHash::GetAt(int32_t nIndex) const { return mpTable[nIndex].mpValue; }

// OverwriteAt -- claim a fresh bucket's value slot (no old value to release).
void AptNativeHash::OverwriteAt(int32_t nIndex, AptValue* pValue)
{
    pValue->AddRef();
    mpTable[nIndex].mpValue = pValue;
}

// SetAt @0x82AD9EE8 -- replace an existing bucket's value: AddRef the new, Release
// the old, store the new.
void AptNativeHash::SetAt(int32_t nIndex, AptValue* pValue)
{
    AptValue* pOld = mpTable[nIndex].mpValue;
    pValue->AddRef();
    if (pOld)
        pOld->Release();
    mpTable[nIndex].mpValue = pValue;
}

// HashFindKey @0x82ADA188 -- locate key's bucket (or null), bounded +/-8 probe.
AptHashItem* AptNativeHash::HashFindKey(const EAStringC& key) const
{
    const int cap  = mnCapacity;
    const int home = key.GetHashValue() & (cap - 1);

    AptHashItem* homeItem = &mpTable[home];
    if (homeItem->mKey.m_pData == nullptr)
        return nullptr;                                   // unused home -> absent
    if (!homeItem->mKey.IsEmpty() && homeItem->mKey.EqualNoCaseHash(key))
        return homeItem;

    int lo = home - 8;
    int hi;
    if (home - 8 < 0)
    {
        lo = 0;
        hi = (cap <= 16) ? (cap - 1) : 16;
    }
    else
    {
        hi = home + 8;
        if (home + 8 > cap - 1)
        {
            hi = cap - 1;
            lo = (cap - 17 < 0) ? 0 : (cap - 17);
        }
    }

    for (int idx = home + 1; idx <= hi; ++idx)
    {
        AptHashItem* item = &mpTable[idx];
        if (item->mKey.m_pData == nullptr)
            return nullptr;
        if (!item->mKey.IsEmpty() && item->mKey.EqualNoCaseHash(key))
            return item;
    }
    for (int idx = home - 1; idx >= lo; --idx)
    {
        AptHashItem* item = &mpTable[idx];
        if (item->mKey.m_pData == nullptr)
            break;
        if (!item->mKey.IsEmpty() && item->mKey.EqualNoCaseHash(key))
            return item;
    }
    return nullptr;
}

// HashSet (shared core) -- insert/replace key->value, growing on a full window.
void AptNativeHash::HashSet(const EAStringC& key, AptValue* pValue)
{
    int cap  = mnCapacity;
    int home = key.GetHashValue() & (cap - 1);
    AptHashItem* homeItem = &mpTable[home];

    while (homeItem->mKey.m_pData != nullptr)             // home occupied or tombstoned
    {
        int reuse = home;
        if (!homeItem->mKey.IsEmpty())                    // home holds a real key
        {
            if (homeItem->mKey.EqualNoCaseHash(key)) { SetAt(home, pValue); return; }
            cap   = mnCapacity;
            reuse = -1;
        }

        int lo = home - 8;
        int hi;
        if (home - 8 < 0)
        {
            lo = 0;
            hi = (cap <= 16) ? (cap - 1) : 16;
        }
        else
        {
            hi = home + 8;
            if (home + 8 > cap - 1)
            {
                hi = cap - 1;
                lo = (cap - 17 < 0) ? 0 : (cap - 17);
            }
        }

        for (int idx = home + 1; idx <= hi; ++idx)
        {
            AptHashItem* item = &mpTable[idx];
            if (item->mKey.m_pData == nullptr)            // empty -> occupy fresh
            {
                item->mKey.m_pData = key.m_pData;
                item->mKey.IncreaseInternalRefCount();
                OverwriteAt(idx, pValue);
                return;
            }
            if (item->mKey.IsEmpty())                     // tombstone -> reuse candidate
            {
                if (reuse == -1) reuse = idx;
            }
            else if (item->mKey.EqualNoCaseHash(key)) { SetAt(idx, pValue); return; }
        }
        for (int idx = home - 1; idx >= lo; --idx)
        {
            AptHashItem* item = &mpTable[idx];
            if (item->mKey.m_pData == nullptr)
            {
                item->mKey.m_pData = key.m_pData;
                item->mKey.IncreaseInternalRefCount();
                OverwriteAt(idx, pValue);
                return;
            }
            if (item->mKey.IsEmpty())
            {
                if (reuse == -1) reuse = idx;
            }
            else if (item->mKey.EqualNoCaseHash(key)) { SetAt(idx, pValue); return; }
        }

        if (reuse != -1)                                  // reuse a tombstone slot
        {
            mpTable[reuse].mKey = key;                    // operator=: drop tombstone, take ref
            OverwriteAt(reuse, pValue);
            return;
        }

        Expand();                                         // grow + rehash, then retry
        cap      = mnCapacity;
        home     = key.GetHashValue() & (cap - 1);
        homeItem = &mpTable[home];
    }

    // home unused -> occupy fresh
    homeItem->mKey.m_pData = key.m_pData;
    homeItem->mKey.IncreaseInternalRefCount();
    OverwriteAt(home, pValue);
}

// Expand @0x82AEED80 -- build a fresh table at double capacity, rehash every live
// (non-tombstone, occupied) entry into it, then swap the new table into `this`.
// The old table is handed to the temporary so its DestroyGCPointers releases the
// now-duplicated keys/values and frees the old block (the X360 calls
// DestroyGCPointers on the swapped-out temporary).
void AptNativeHash::Expand()
{
    AptNativeHash bigger(2 * mnCapacity);
    bigger.FirstAllocation();

    const int    oldCap = mnCapacity;
    AptHashItem* oldTab = mpTable;
    for (int i = 0; i < oldCap; ++i)
    {
        AptHashItem* e = &oldTab[i];
        if (e->mKey.m_pData != nullptr && !e->mKey.IsEmpty())
            bigger.HashSet(e->mKey, e->mpValue);
    }

    // Swap the new table into `this`; give `bigger` the old table so its teardown
    // releases the duplicated keys/values and frees the old block.
    AptHashItem* newTab = bigger.mpTable;
    const int    newCap = bigger.mnCapacity;
    bigger.mnCapacity = oldCap;
    bigger.mpTable    = oldTab;
    mnCapacity        = newCap;
    mpTable           = newTab;
    bigger.DestroyGCPointers();                           // frees oldTab; nulls bigger.mpTable
    // The X360 issues a second DestroyGCPointers when the swapped-out table was
    // non-null; the first call has already nulled bigger.mpTable, so the guarded
    // repeat is a no-op (and bigger's destructor at scope end likewise no-ops).
}

// Set @0x82AF4B08 -- public store, routing the magic keys to the fast-slots. A
// null value routes to Unset (the X360 deletes the key when the value is null).
void AptNativeHash::Set(const EAStringC& key, AptValue* pValue)
{
    if (!pValue)
    {
        Unset(key);
        return;
    }
    if (key.IsEmpty())                                    // empty-string key ignored
        return;

    const uint16_t h = key.UpdateHashValue();            // compute the cached hash if needed
    if (h == 1689 && key.EqualNoCase(gAptKeyPrototype))      { SetPrototype(pValue);  return; }
    if (h == 27581 && key.EqualNoCase(StringPool::saConstant[0])) { Set__Proto__(pValue); return; }

    if (!mpTable)
        FirstAllocation();
    HashSet(key, pValue);
}

// Unset @0x82AEB528 -- remove key: drop it from the table (tombstone the slot,
// release its value/key) if present, otherwise clear the matching proto fast-slot.
void AptNativeHash::Unset(const EAStringC& key)
{
    if (key.IsEmpty())                                    // empty-string key ignored
        return;

    const uint16_t h = key.UpdateHashValue();

    if (mpTable)
    {
        AptHashItem* item = HashFindKey(key);
        if (item)
        {
            EAStringC::DecreaseInternalRefCount(item->mKey.m_pData);
            AptValue* pValue = item->mpValue;
            item->mKey.m_pData = reinterpret_cast<EAStringC::DebugDataC*>(EAStringC::s_EmptyInternalData);   // tombstone
            if (pValue)
                pValue->Release();
            item->mpValue = nullptr;
            return;
        }
    }

    if (h == 1689 && key.EqualNoCase(gAptKeyPrototype))
        UnsetPrototype();
    else if (h == 27581 && key.EqualNoCase(StringPool::saConstant[0]))
        Unset__Proto__();
}

// Lookup (shared core) -- table first, then the proto fast-slots.
AptValue* AptNativeHash::Lookup(const EAStringC& key) const
{
    const uint16_t h = key.UpdateHashValue();
    if (mpTable)
    {
        AptHashItem* item = HashFindKey(key);
        if (item)
            return item->mpValue;
    }
    if (h == 1689)
        return key.EqualNoCase(gAptKeyPrototype) ? mpPrototype : nullptr;
    if (h == 27581)
        return key.EqualNoCase(StringPool::saConstant[0]) ? mp__Proto__ : nullptr;
    return nullptr;
}

// ---- proto / __proto__ fast-slots ----------------------------------------
AptValue* AptNativeHash::Get__Proto__() const { return mp__Proto__; }
AptValue* AptNativeHash::GetPrototype() const { return mpPrototype; }   // @0x82BBD940

void AptNativeHash::Set__Proto__(AptValue* pValue)   // @0x82AD9F50
{
    if (pValue)      pValue->AddRef();
    if (mp__Proto__) mp__Proto__->Release();
    mp__Proto__ = pValue;
}

void AptNativeHash::SetPrototype(AptValue* pValue)   // @0x82ADA010
{
    if (pValue)      pValue->AddRef();
    if (mpPrototype) mpPrototype->Release();
    mpPrototype = pValue;
}

void AptNativeHash::UnsetPrototype()   // @0x82ADA080
{
    if (mpPrototype) { mpPrototype->Release(); mpPrototype = nullptr; }
}

void AptNativeHash::Unset__Proto__()
{
    if (mp__Proto__) { mp__Proto__->Release(); mp__Proto__ = nullptr; }
}

// ---- bucket iteration -----------------------------------------------------
// GetFirstItem @0x82ADA0D0 -- the first occupied (non-unused, non-tombstone)
// bucket, or null if the table is empty/unallocated.
AptHashItem* AptNativeHash::GetFirstItem() const
{
    AptHashItem* tab = mpTable;
    if (tab == nullptr)
        return nullptr;
    const int cap = mnCapacity;
    if (cap <= 0)
        return nullptr;

    for (int i = 0; i < cap; ++i)
    {
        AptHashItem* item = &tab[i];
        if (item->mKey.m_pData != nullptr && !item->mKey.IsEmpty())
            return item;
    }
    return nullptr;
}

// GetNextItem @0x82ADA130 -- the next occupied bucket strictly after pCurrent, or
// null when the end of the table is reached.
AptHashItem* AptNativeHash::GetNextItem(const AptHashItem* pCurrent) const
{
    AptHashItem* tab = mpTable;
    if (tab == nullptr)
        return nullptr;

    AptHashItem* item = const_cast<AptHashItem*>(pCurrent) + 1;
    AptHashItem* end  = &tab[mnCapacity];
    if (item >= end)
        return nullptr;

    for (; item < end; ++item)
    {
        if (item->mKey.m_pData != nullptr && !item->mKey.IsEmpty())
            return item;
    }
    return nullptr;
}

// ---- packed AS event-handler bitmask --------------------------------------
int32_t AptNativeHash::HasEventHandler(int32_t nMask) const   // @0x82AD5200
{
    return mnEventHandlerMask & nMask;
}

void AptNativeHash::SetEventHandler(int32_t nMask)            // @0x82AD51E0
{
    mnEventHandlerMask |= nMask;
}

void AptNativeHash::RemoveEventHandler(int32_t nMask)        // @0x82AD51F0
{
    mnEventHandlerMask &= ~nMask;
}

// UpdateObjectMethods @0x82ADA2F8 -- when an AS member named by pKeyValue is set
// (bRemoving=false) or cleared (bRemoving=true), fold/unfold the corresponding
// event-handler bit into the mask. Member kinds that are themselves dynamic
// callbacks (a CharacterInstHandle value carrying the bound-method flag, or a
// CIHNone value) are skipped: the mask only tracks the recognised on<Event>
// handler names. The member name is resolved through the sprite-member gperf
// index; only resolved indices >= 200 map to event bits via the .rdata table.
//   a2 (pKeyValue) -- the AptValue being assigned; its low-7 type bits and one
//                     packed flag bit gate the early-out.
//   a3 (pKeyName)  -- the EAStringC member name (the X360 reads its m_pData, then
//                     the char buffer at +8 / the size at +2 -- GetBuffer/GetLength).
void AptNativeHash::UpdateObjectMethods(const AptValue* pKeyValue,
                                        const EAStringC* pKeyName, bool bRemoving)
{
    // Skip when the value being assigned is itself a (bound) script-function
    // member -- the X360 tests the value-type index and one packed value flag:
    //     (type == 12 && mbIsDefined) || type == 37
    // Type 12 == AptVFT_CharacterInstHandle, 37 == AptVFT_CIHNone (the enum is
    // shared with X360). The X360 `extrwi r11,r11,1,4` reads the 1-bit field at
    // bit 27 of the packed value word == AptValue::mValueBitfield.mbIsDefined
    // (getIsDefined()): a CharacterInstHandle only counts as a bound method when
    // it is a defined value; CIHNone is always skipped.
    const AptVirtualFunctionTable_Indices eType = pKeyValue->getVtblIndex();
    if ((eType == AptVFT_CharacterInstHandle && pKeyValue->getIsDefined())
        || eType == AptVFT_CIHNone)
        return;

    // Resolve the member name (key string) through the sprite-member gperf index.
    const SpriteMembersIndex::Entry* pEntry =
        SpriteMembersIndex::in_word_set(pKeyName->GetBuffer(), pKeyName->GetLength());
    if (pEntry == nullptr)
        return;

    const int32_t nMemberIndex = static_cast<int32_t>(pEntry->muMemberIndex);
    if (nMemberIndex < 200)
        return;

    const int32_t nEventBit = gAptMemberIndexToEventBit[nMemberIndex - 200];
    if (nEventBit == -1)
        return;

    if (bRemoving)
        mnEventHandlerMask &= ~nEventBit;
    else
        mnEventHandlerMask |= nEventBit;
}

// RegisterReferences @0x82ADA3C0 -- the GC mark walk: hand every held AptValue
// reference (the proto fast-slots + each occupied bucket's value) to the
// collector's registration callback, attributed to pOwner.
void AptNativeHash::RegisterReferences(const AptValue* pOwner)
{
    // The callback is installed at Apt bring-up (AptInit.cpp: AptValue::
    // sReferenceRegistrationCb = &AptGC::sReferenceRegistrationCb, reproducing
    // the console's .data-initialised slot dword_8324E4E8); null-guarded so the
    // walk is inert before that bring-up.
    if (!AptValue::sReferenceRegistrationCb)
        return;

    if (mp__Proto__)
        AptValue::sReferenceRegistrationCb(pOwner, &mp__Proto__, "__proto__", 0);
    if (mpPrototype)
        AptValue::sReferenceRegistrationCb(pOwner, &mpPrototype, "prototype", 0);

    if (mpTable)
    {
        for (int i = 0; i < mnCapacity; ++i)
        {
            AptHashItem* e = &mpTable[i];
            if (e->mpValue)
                AptValue::sReferenceRegistrationCb(pOwner, &e->mpValue, e->mKey.c_str(), 0);
        }
    }
}

// DestroyGCPointers @0x82AEB458 -- clear the proto slots, then release every live
// bucket's value/key and free the table.
//
// ⭐ 2026-08-29 (main-menu wave): THE TABLE POINTER AND THE CAPACITY ARE RE-READ EVERY
// TIME, because the console re-reads them and this body is re-entrant through the
// Release() below. The X360 asm is explicit about it and it is not an artifact:
//
//     loc_82AEB49C:  lwz  r11, 4(r31)      ; mpTable      <- reloaded at the top of
//                    add  r11, r30, r11    ;                 EVERY iteration
//                    lwz  r10, 4(r11)      ; e->mpValue
//                    ... bctrl              ; e->mpValue->Release()
//                    lwz  r11, 4(r31)      ; mpTable      <- reloaded AFTER the call
//                    stw  r28, 4(r11)      ; e->mpValue = 0
//     loc_82AEB4D0:  lwz  r29, 4(r31)      ; mpTable      <- reloaded again for the key
//                    ...
//                    lwz  r11, 0(r31)      ; mnCapacity   <- reloaded for the loop test
//                    cmpw r27, r11 ; blt loc_82AEB49C
//     loc_82AEB500:  lwz  r10, 0(r31)      ; mnCapacity   <- reloaded for the free size
//                    lwz  r4, 4(r31)       ; mpTable      <- reloaded for the free
//
// The previous body cached `tab` and `cap` once. That is safe only if nothing inside
// Release() can touch THIS hash -- and it can: Release() on a bucket's AptValue runs the
// full ForceDelete composite (PreDestroy / DestroyGCPointers / deleting destructor), which
// re-enters the Apt teardown graph and can reach this same hash again (a re-entrant
// DestroyGCPointers empties it and frees mpTable, and Set/Expand can replace it). With the
// pointer cached, the rest of the loop then walks a block the pool has already handed back
// out, reading recycled bytes as `AptValue*` and calling through the DOGMA free-list link
// that now sits in word 0 -- which is exactly the map-screen exit AV measured this wave:
// EXECUTING 0x18000000 from AptNativeHash::DestroyGCPointers+0x6C (`call [rax+8]` ==
// mpValue->Release()), with the "value" and its "vtable" both pointing back into the Apt
// pool. Re-reading follows the live table the way the console does.
//
// The mid-loop `mpTable == 0` break is the one PC-side addition: on the console the reload
// simply picks up whatever the re-entrant call left, and a null there would fault. [FLAG PC
// bring-up guard]
void AptNativeHash::DestroyGCPointers()
{
    UnsetPrototype();
    Unset__Proto__();

    if (mpTable)
    {
        mnEventHandlerMask = 0;
        for (int i = 0; i < mnCapacity; ++i)      // mnCapacity re-read per iteration
        {
            if (mpTable == nullptr)
                break;                            // [FLAG PC bring-up guard] see the banner

            if (mpTable[i].mpValue)
            {
                // [FLAG PC bring-up guard -- 2026-08-29 map-screen exit AV] the console
                // calls straight through. On this build a bucket can still hold a value
                // the Apt GC has already handed back to the pool (measured: the CrashNav
                // map screen's 'ControlledTextBox_mc' entry, freed by
                // AptAnimationTarget::CleanRemList's refcount<=1 arm while this hash still
                // pointed at it) -- and Release() is a virtual call, so a recycled block's
                // free-list link is executed as code. Validate the vtable before the call,
                // and report every hit so the missing reference stays visible instead of
                // becoming a silent crash. See the wave notes for the open root cause.
                if (!AptHashValueLooksLive(mpTable[i].mpValue))
                {
                    AptHashReportDeadValue(this, i, mnCapacity,
                                           mpTable[i].mKey.c_str(), mpTable[i].mpValue);
                    mpTable[i].mpValue = nullptr;
                    if (mpTable[i].mKey.m_pData)
                    {
                        EAStringC::DecreaseInternalRefCount(mpTable[i].mKey.m_pData);
                        mpTable[i].mKey.m_pData = nullptr;
                    }
                    continue;
                }
                mpTable[i].mpValue->Release();
                if (mpTable == nullptr)
                    break;                        // [FLAG PC bring-up guard]
                mpTable[i].mpValue = nullptr;     // console: reload mpTable, then store
            }
            if (mpTable[i].mKey.m_pData)          // real key or tombstone
            {
                EAStringC::DecreaseInternalRefCount(mpTable[i].mKey.m_pData);   // guards the tombstone
                mpTable[i].mKey.m_pData = nullptr;
            }
        }
        if (mpTable)
        {
            gpNonGCPoolManager->Deallocate(mpTable, sizeof(AptHashItem) * mnCapacity);
            mpTable = nullptr;
        }
    }
}

// ClearData @0x82AEB620 -- empty the table (release values/keys), free the bucket
// array, clear the proto fast-slots and the event mask. Unlike DestroyGCPointers,
// the table walk only touches buckets whose key slot is occupied/tombstoned.
void AptNativeHash::ClearData()
{
    if (mpPrototype) { mpPrototype->Release(); mpPrototype = nullptr; }
    if (mp__Proto__) { mp__Proto__->Release(); mp__Proto__ = nullptr; }

    AptHashItem* tab = mpTable;
    if (tab)
    {
        const int cap = mnCapacity;
        for (int i = 0; i < cap; ++i)
        {
            AptHashItem* e = &tab[i];
            if (e->mKey.m_pData)                          // occupied or tombstone
            {
                if (e->mpValue)
                {
                    e->mpValue->Release();
                    e->mpValue = nullptr;
                }
                EAStringC::DecreaseInternalRefCount(e->mKey.m_pData);
                e->mKey.m_pData = nullptr;
            }
        }
        gpNonGCPoolManager->Deallocate(tab, sizeof(AptHashItem) * cap);
        mpTable = nullptr;
    }
    mnEventHandlerMask = 0;
}

// ClearDataNoDelete @0x82AEB720 -- as ClearData but KEEP the bucket array (do not
// free it / null mpTable); the table is left allocated but empty for reuse.
void AptNativeHash::ClearDataNoDelete()
{
    if (mpPrototype) { mpPrototype->Release(); mpPrototype = nullptr; }
    if (mp__Proto__) { mp__Proto__->Release(); mp__Proto__ = nullptr; }

    AptHashItem* tab = mpTable;
    if (tab)
    {
        const int cap = mnCapacity;
        for (int i = 0; i < cap; ++i)
        {
            AptHashItem* e = &tab[i];
            if (e->mKey.m_pData)                          // occupied or tombstone
            {
                if (e->mpValue)
                {
                    e->mpValue->Release();
                    e->mpValue = nullptr;
                }
                EAStringC::DecreaseInternalRefCount(e->mKey.m_pData);
                e->mKey.m_pData = nullptr;
            }
        }
    }
    mnEventHandlerMask = 0;
}

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

// XDK size optimization matches ARTIST at 0x82AD9F50 and 0x82ADA010.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptString/StringPool.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AddRef/Release (value refs)
#include "SDKs/EATech/include/Apt/AptDefine.h"            // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"                // DOGMA_PoolManager
#include "SDKs/Packages/Apt/2.00.00/source/AptValue/AptSprite.h"  // SpriteMembersIndex (member-name gperf)

#include <cstring>   // memset / strcmp

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
    : mnCapacity(nCapacity)
    , mpTable(0)
    , mp__Proto__(0)
    , mpPrototype(0)
    , mnEventHandlerMask(0)
{
    if ((mnCapacity & (mnCapacity - 1)) != 0)
    {
        int32_t nRounded = 1;
        while (nRounded < mnCapacity)
            nRounded <<= 1;
        mnCapacity = nRounded;
    }
}


// FirstAllocation @0x82AE5190 -- allocate + zero the bucket array.
void AptNativeHash::FirstAllocation()
{
    mpTable = static_cast<AptHashItem*>(gpNonGCPoolManager->Allocate(mnCapacity * sizeof(AptHashItem)));
    memset(mpTable, 0, mnCapacity * sizeof(AptHashItem));
}

bool AptNativeHash::IsEmpty() const { return mpTable == 0; }   // @0x82AD51D0

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
    const uint32_t home = key.m_pData->m_uHash & (mnCapacity - 1);
    if (!mpTable[home].mKey.IsValid())
        return 0;
    if (!mpTable[home].mKey.IsEmpty() && mpTable[home].mKey.EqualNoCaseHash(key))
        return &mpTable[home];

    const int nProbes = 8;
    int nFirst = home - nProbes;
    int nLast;
    if (nFirst < 0)
    {
        nFirst = 0;
        nLast = 2 * nProbes;
        if (nLast >= mnCapacity)
            nLast = mnCapacity - 1;
    }
    else
    {
        nLast = home + nProbes;
        if (nLast > mnCapacity - 1)
        {
            nLast = mnCapacity - 1;
            nFirst = nLast - 2 * nProbes;
            if (nFirst < 0)
                nFirst = 0;
        }
    }

    int iBucket = home;
    int nRemaining = nLast - home;
    while (nRemaining--)
    {
        ++iBucket;
        if (!mpTable[iBucket].mKey.IsValid())
            return 0;
        if (!mpTable[iBucket].mKey.IsEmpty() && mpTable[iBucket].mKey.EqualNoCaseHash(key))
            return &mpTable[iBucket];
    }

    iBucket = home;
    nRemaining = home - nFirst;
    while (nRemaining--)
    {
        --iBucket;
        if (!mpTable[iBucket].mKey.IsValid())
            return 0;
        if (!mpTable[iBucket].mKey.IsEmpty() && mpTable[iBucket].mKey.EqualNoCaseHash(key))
            return &mpTable[iBucket];
    }
    return 0;
}

// HashSet (shared core) -- insert/replace key->value, growing on a full window.
void AptNativeHash::HashSet(const EAStringC& key, AptValue* pValue)
{
    int cap  = mnCapacity;
    int home = key.GetHashValue() & (cap - 1);
    AptHashItem* homeItem = &mpTable[home];

    while (homeItem->mKey.m_pData != 0)             // home occupied or tombstoned
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
            if (item->mKey.m_pData == 0)            // empty -> occupy fresh
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
            if (item->mKey.m_pData == 0)
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

    for (int i = 0; i < mnCapacity; ++i)
    {
        EAStringC* pKey = &mpTable[i].mKey;
        if (!pKey->IsValid() || pKey->IsEmpty())
            continue;
        AptValue* pValue = mpTable[i].mpValue;
        bigger.HashSet(*pKey, pValue);
    }

    AptHashItem* pOldTable = mpTable;
    mpTable = bigger.mpTable;
    bigger.mpTable = pOldTable;
    const int nNewCapacity = bigger.mnCapacity;
    bigger.mnCapacity = mnCapacity;
    mnCapacity = nNewCapacity;
    bigger.DestroyGCPointers();
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
    if (h == 1689 && key.EqualNoCase(StringPool::saConstant[StringPool::KU_NAME_PROTOTYPE]))      { SetPrototype(pValue);  return; }
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
            item->mpValue = 0;
            return;
        }
    }

    if (h == 1689 && key.EqualNoCase(StringPool::saConstant[StringPool::KU_NAME_PROTOTYPE]))
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
        return key.EqualNoCase(StringPool::saConstant[StringPool::KU_NAME_PROTOTYPE]) ? mpPrototype : 0;
    if (h == 27581)
        return key.EqualNoCase(StringPool::saConstant[0]) ? mp__Proto__ : 0;
    return 0;
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
    if (mpPrototype) { mpPrototype->Release(); mpPrototype = 0; }
}

void AptNativeHash::Unset__Proto__()
{
    if (mp__Proto__) { mp__Proto__->Release(); mp__Proto__ = 0; }
}

// ---- bucket iteration -----------------------------------------------------
// GetFirstItem @0x82ADA0D0 -- the first occupied (non-unused, non-tombstone)
// bucket, or null if the table is empty/unallocated.
AptHashItem* AptNativeHash::GetFirstItem() const
{
    if (IsEmpty())
        return 0;
    for (int i = 0; i < mnCapacity; ++i)
    {
        if (mpTable[i].mKey.IsValid() && !mpTable[i].mKey.IsEmpty())
            return &mpTable[i];
    }
    return 0;
}

// GetNextItem @0x82ADA130 -- the next occupied bucket strictly after pCurrent, or
// null when the end of the table is reached.
AptHashItem* AptNativeHash::GetNextItem(const AptHashItem* pCurrent) const
{
    if (IsEmpty())
        return 0;

    AptHashItem* pItem = const_cast<AptHashItem*>(pCurrent);
    while (++pItem < &mpTable[mnCapacity])
    {
        if (pItem->mKey.IsValid() && !pItem->mKey.IsEmpty())
            return pItem;
    }
    return 0;
}

// ---- packed AS event-handler bitmask --------------------------------------
int32_t AptNativeHash::HasEventHandler(int32_t nMask) const   // @0x82AD5200
{
    return mnEventHandlerMask & nMask;
}

// UpdateObjectMethods @0x82ADA2F8 updates the named listener bit for an
// ordinary object. Character handles maintain their event mask separately.
void AptNativeHash::UpdateObjectMethods(const AptValue* pContext,
                                        const EAStringC* pKeyName, int bRemoving)
{
    if (pContext->isCIH())
        return;

    // Resolve the member name (key string) through the sprite-member gperf index.
    const SpriteMembersIndex::Entry* pEntry =
        SpriteMembersIndex::in_word_set(pKeyName->GetBuffer(), pKeyName->GetLength());
    if (pEntry == 0)
        return;

    const int32_t nMemberIndex = static_cast<int32_t>(pEntry->muMemberIndex);
    if (nMemberIndex < 200)
        return;

    const int32_t nEventBit = gAptMemberIndexToEventBit[nMemberIndex - 200];
    if (nEventBit == -1)
        return;

    if (bRemoving)
        RemoveEventHandler(nEventBit);
    else
        SetEventHandler(nEventBit);
}

// RegisterReferences @0x82ADA3C0 -- the GC mark walk: hand every held AptValue
// reference (the proto fast-slots + each occupied bucket's value) to the
// collector's registration callback, attributed to pOwner.
void AptNativeHash::RegisterReferences(const AptValue* pOwner)
{
    if (mp__Proto__)
        AptValue::sReferenceRegistrationCb(pOwner, &mp__Proto__, StringPool::saConstant[0].GetBuffer(), 0);
    if (mpPrototype)
        AptValue::sReferenceRegistrationCb(pOwner, &mpPrototype, StringPool::saConstant[StringPool::KU_NAME_PROTOTYPE].GetBuffer(), 0);

    if (!mpTable)
        return;
    for (int i = 0; i < mnCapacity; ++i)
    {
        if (mpTable[i].mpValue)
            AptValue::sReferenceRegistrationCb(pOwner, &mpTable[i].mpValue, mpTable[i].mKey.GetBuffer(), 0);
    }
}

// ARTIST 0x82AEB458 reloads the table and capacity after releasing values.
// Release can reenter the Apt object graph.
void AptNativeHash::DestroyGCPointers()
{
    UnsetPrototype();
    Unset__Proto__();
    if (!mpTable)
        return;

    mnEventHandlerMask = 0;
    for (int i = 0; i < mnCapacity; ++i)
    {
        if (mpTable[i].mpValue)
        {
            mpTable[i].mpValue->Release();
            mpTable[i].mpValue = 0;
        }
        if (mpTable[i].mKey.IsValid())
        {
            EAStringC& key = mpTable[i].mKey;
            EAStringC::DecreaseInternalRefCount(key.m_pData);
            key.m_pData = 0;
        }
    }
    gpNonGCPoolManager->Deallocate(mpTable, sizeof(AptHashItem) * mnCapacity);
    mpTable = 0;
}

// ClearData @0x82AEB620 -- empty the table (release values/keys), free the bucket
// array, clear the proto fast-slots and the event mask. Unlike DestroyGCPointers,
// the table walk only touches buckets whose key slot is occupied/tombstoned.
void AptNativeHash::ClearData()
{
    if (mpPrototype)
    {
        mpPrototype->Release();
        mpPrototype = 0;
    }
    if (mp__Proto__)
    {
        mp__Proto__->Release();
        mp__Proto__ = 0;
    }

    if (mpTable)
    {
        for (int i = 0; i < mnCapacity; ++i)
        {
            if (mpTable[i].mKey.IsValid())
            {
                if (mpTable[i].mpValue)
                {
                    mpTable[i].mpValue->Release();
                    mpTable[i].mpValue = 0;
                }
                EAStringC& key = mpTable[i].mKey;
                EAStringC::DecreaseInternalRefCount(key.m_pData);
                key.m_pData = 0;
            }
        }
        gpNonGCPoolManager->Deallocate(mpTable, sizeof(AptHashItem) * mnCapacity);
        mpTable = 0;
    }
    mnEventHandlerMask = 0;
}

// ClearDataNoDelete @0x82AEB720 -- as ClearData but KEEP the bucket array (do not
// free it / null mpTable); the table is left allocated but empty for reuse.
void AptNativeHash::ClearDataNoDelete()
{
    if (mpPrototype)
    {
        mpPrototype->Release();
        mpPrototype = 0;
    }
    if (mp__Proto__)
    {
        mp__Proto__->Release();
        mp__Proto__ = 0;
    }

    if (mpTable)
    {
        for (int i = 0; i < mnCapacity; ++i)
        {
            if (mpTable[i].mKey.IsValid())
            {
                if (mpTable[i].mpValue)
                {
                    mpTable[i].mpValue->Release();
                    mpTable[i].mpValue = 0;
                }
                EAStringC& key = mpTable[i].mKey;
                EAStringC::DecreaseInternalRefCount(key.m_pData);
                key.m_pData = 0;
            }
        }
    }
    mnEventHandlerMask = 0;
}

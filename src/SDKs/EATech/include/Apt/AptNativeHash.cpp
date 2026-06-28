// ===========================================================================
// EATech Apt -- AptNativeHash (the ActionScript property table).
// DECOMPILED from the PS3 EXTERNAL ELF (cross-checked vs X360 ARTIST).
//
//   ctor 0x7E3FD8 / dtor 0x7F8638 / FirstAllocation 0x7EFB34 / Expand 0x7F7EB8 /
//   HashFindKey 0x7E8188 / HashSet 0x7F7FA4 / Set 0x800484 / Lookup 0x7F8DF8 /
//   SetAt 0x7E40EC / OverwriteAt 0x7E4238 / GetAt 0x7DF2D0 / IsEmpty 0x7DF28C /
//   DestroyGCPointers 0x7F7D88 / proto slots 0x7DF27C/0x7DF284/0x7DF8AC/0x7DF94C/
//   0x7E402C/0x7E408C.
//
// Bucket key states: unused (m_pData == null, from the FirstAllocation memset),
// tombstone (the empty string, left by a delete), occupied (a real key). The
// probe is a bounded +/-8-bucket linear scan around the home bucket; on a full
// window with no reusable slot the table doubles (Expand) and the insert retries.
// Values are AddRef/Release'd through the AptValue vtable.
//
// Deletion (Unset/UnsetAt/ClearData) is deferred, so tombstones never arise at
// runtime; the tombstone handling below is reconstructed faithfully regardless.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptNativeHash.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"   // AddRef/Release (value refs)
#include "SDKs/EATech/include/Apt/AptDefine.h"            // gpNonGCPoolManager
#include "SDKs/EATech/Apt/DogmaAllocator.h"                // DOGMA_PoolManager

#include <cstring>   // memset

// ---------------------------------------------------------------------------
// ctor @0x7E3FD8 -- store the capacity (rounded up to a power of two), null the
// table + slots.
// ---------------------------------------------------------------------------
AptNativeHash::AptNativeHash(int32_t nCapacity)
{
    mnCapacity  = nCapacity;
    mnField4    = 0;
    mpTable     = nullptr;
    mp__Proto__ = nullptr;
    mpPrototype = nullptr;

    if (((nCapacity - 1) & nCapacity) != 0)   // not already a power of two
    {
        int p = 1;
        if (nCapacity > 1)
            for (p = 1; p < nCapacity; p *= 2)
                ;
        mnCapacity = p;
    }
}

AptNativeHash::~AptNativeHash()   // @0x7F8638
{
    if (mpTable)
        DestroyGCPointers();
}

// FirstAllocation @0x7EFB34 -- allocate + zero the bucket array.
void AptNativeHash::FirstAllocation()
{
    const size_t nBytes = sizeof(AptHashItem) * mnCapacity;
    mpTable = static_cast<AptHashItem*>(gpNonGCPoolManager->Allocate(nBytes));
    memset(mpTable, 0, nBytes);
}

bool AptNativeHash::IsEmpty() const { return mpTable == nullptr; }   // @0x7DF28C

AptValue* AptNativeHash::GetAt(int32_t nIndex) const { return mpTable[nIndex].mpValue; }   // @0x7DF2D0

// OverwriteAt @0x7E4238 -- claim a fresh bucket's value slot (no old value).
void AptNativeHash::OverwriteAt(int32_t nIndex, AptValue* pValue)
{
    pValue->AddRef();
    mpTable[nIndex].mpValue = pValue;
}

// SetAt @0x7E40EC -- replace an existing bucket's value (release the old).
void AptNativeHash::SetAt(int32_t nIndex, AptValue* pValue)
{
    AptValue* pOld = mpTable[nIndex].mpValue;
    pValue->AddRef();
    if (pOld)
        pOld->Release();
    mpTable[nIndex].mpValue = pValue;
}

// HashFindKey @0x7E8188 -- locate key's bucket (or null), bounded +/-8 probe.
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

// HashSet @0x7F7FA4 -- insert/replace key->value, growing on a full window.
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

// Expand @0x7F7EB8 -- double the table and rehash the live entries into it, then
// hand the old table to a temporary whose teardown releases + frees it.
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
    // releases the now-duplicated keys/values and frees the old block.
    AptHashItem* newTab = bigger.mpTable;
    const int    newCap = bigger.mnCapacity;
    bigger.mnCapacity = oldCap;
    bigger.mpTable    = oldTab;
    mnCapacity        = newCap;
    mpTable           = newTab;
    bigger.DestroyGCPointers();                           // frees oldTab; nulls bigger.mpTable
    // bigger's destructor at scope end then sees a null table and no-ops.
}

// Set @0x800484 -- public store, routing the magic keys to the fast-slots.
void AptNativeHash::Set(const EAStringC& key, AptValue* pValue)
{
    if (!pValue)
    {
        // FLAG: Set(key, null) == Unset(key) on console; the deletion path is
        // deferred, so a null-value Set is a no-op here. No current caller Sets null.
        return;
    }
    if (key.IsEmpty())                                    // empty-string key ignored
        return;

    const uint16_t h = key.UpdateHashValue();            // compute hash if needed
    if (h == 1689 && key.EqualNoCase(gAptKeyPrototype)) { SetPrototype(pValue);  return; }
    if (h == 27581 && key.EqualNoCase(StringPool::saConstant)) { Set__Proto__(pValue); return; }

    if (!mpTable)
        FirstAllocation();
    HashSet(key, pValue);
}

// Lookup @0x7F8DF8 -- table first, then the proto fast-slots.
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
        return key.EqualNoCase(StringPool::saConstant) ? mp__Proto__ : nullptr;
    return nullptr;
}

// ---- proto / __proto__ fast-slots ----------------------------------------
AptValue* AptNativeHash::Get__Proto__() const { return mp__Proto__; }   // @0x7DF27C
AptValue* AptNativeHash::GetPrototype() const { return mpPrototype; }   // @0x7DF284

void AptNativeHash::Set__Proto__(AptValue* pValue)   // @0x7DF8AC
{
    if (pValue)     pValue->AddRef();
    if (mp__Proto__) mp__Proto__->Release();
    mp__Proto__ = pValue;
}

void AptNativeHash::SetPrototype(AptValue* pValue)   // @0x7DF94C
{
    if (pValue)     pValue->AddRef();
    if (mpPrototype) mpPrototype->Release();
    mpPrototype = pValue;
}

void AptNativeHash::UnsetPrototype()   // @0x7E402C
{
    if (mpPrototype) { mpPrototype->Release(); mpPrototype = nullptr; }
}

void AptNativeHash::Unset__Proto__()   // @0x7E408C
{
    if (mp__Proto__) { mp__Proto__->Release(); mp__Proto__ = nullptr; }
}

// DestroyGCPointers @0x7F7D88 -- release the slots + every live bucket's
// value/key, then free the table.
void AptNativeHash::DestroyGCPointers()
{
    UnsetPrototype();
    Unset__Proto__();

    AptHashItem* tab = mpTable;
    if (tab)
    {
        const int cap = mnCapacity;
        mnField4 = 0;
        for (int i = 0; i < cap; ++i)
        {
            AptHashItem* e = &tab[i];
            if (e->mpValue)
            {
                e->mpValue->Release();
                e->mpValue = nullptr;
            }
            if (e->mKey.m_pData)                          // real key or tombstone
            {
                EAStringC::DecreaseInternalRefCount(e->mKey.m_pData);   // guards the tombstone
                e->mKey.m_pData = nullptr;
            }
        }
        gpNonGCPoolManager->Deallocate(tab, sizeof(AptHashItem) * cap);
        mpTable = nullptr;
    }
}

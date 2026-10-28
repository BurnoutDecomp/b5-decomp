// ===========================================================================
// EATech Apt -- StringPool out-of-line bodies: the interned `__proto__` key
// (saConstant) + the temporary-string-pool teardown (ClearTemporaryPool).
//
// X360 ClearTemporaryPool @0x82AD8E20 (PS3 EXTERNAL _ZN10StringPool18ClearTemporary
// PoolEv @0x7E60FC). saConstant == X360 dword_8324E580 / PS3 _ZN10StringPool10sa
// ConstantE -- the interned "__proto__" property key the AS member-op fast path
// compares names against (AptNativeHash::Set/Lookup, hash 27581).
//
// EA SDK identifiers kept verbatim (CXX_NAMING_CONVENTIONS external-API exception).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptString/StringPool.h"   // StringPool (saConstant + ClearTemporaryPool + Initialize)
#include "SDKs/EATech/include/Apt/AptValue/AptString.h"     // AptString (the pooled node) + gpNonGCPoolManager
#include "SDKs/EATech/include/Apt/AptValue/AptGCReleaseVector.h" // gValuesToRelease (Teardown drains it)
#include "SDKs/EATech/Apt/DogmaAllocator.h"                 // DOGMA_PoolManager (StringPool::Initialize bucket array)

#include <xtl.h>   // _InterlockedExchange (the Apt string-pool spin lock)
#include <cstring>    // memset / strcmp (bucket-array clear + pool key compare)

// ARTIST CRT initializer82C71F10 builds these writable 0x108-byte records.
// No allocator is needed before StringPool::Initialize publishes their addresses.
static EAStringC::StaticStringHelperT sStringPoolData[StringPool::KU_CONSTANT_COUNT] = {
    {{1, 9, 1, 0}, "__proto__"},
    {{1, 6, 1, 0}, "_alpha"},
    {{1, 13, 1, 0}, "_currentframe"},
    {{1, 5, 1, 0}, "_down"},
    {{1, 11, 1, 0}, "_droptarget"},
    {{1, 10, 1, 0}, "_focusrect"},
    {{1, 13, 1, 0}, "_framesloaded"},
    {{1, 7, 1, 0}, "_global"},
    {{1, 7, 1, 0}, "_height"},
    {{1, 12, 1, 0}, "_highquality"},
    {{1, 5, 1, 0}, "_left"},
    {{1, 5, 1, 0}, "_name"},
    {{1, 8, 1, 0}, "_quality"},
    {{1, 6, 1, 0}, "_right"},
    {{1, 9, 1, 0}, "_rotation"},
    {{1, 13, 1, 0}, "_soundbuftime"},
    {{1, 7, 1, 0}, "_target"},
    {{1, 12, 1, 0}, "_totalframes"},
    {{1, 5, 1, 0}, "_type"},
    {{1, 3, 1, 0}, "_up"},
    {{1, 4, 1, 0}, "_url"},
    {{1, 8, 1, 0}, "_visible"},
    {{1, 6, 1, 0}, "_width"},
    {{1, 2, 1, 0}, "_x"},
    {{1, 7, 1, 0}, "_xmouse"},
    {{1, 7, 1, 0}, "_xscale"},
    {{1, 2, 1, 0}, "_y"},
    {{1, 7, 1, 0}, "_ymouse"},
    {{1, 7, 1, 0}, "_yscale"},
    {{1, 2, 1, 0}, "aa"},
    {{1, 2, 1, 0}, "ab"},
    {{1, 5, 1, 0}, "array"},
    {{1, 2, 1, 0}, "ba"},
    {{1, 2, 1, 0}, "bb"},
    {{1, 7, 1, 0}, "boolean"},
    {{1, 6, 1, 0}, "center"},
    {{1, 5, 1, 0}, "color"},
    {{1, 10, 1, 0}, "controller"},
    {{1, 4, 1, 0}, "date"},
    {{1, 5, 1, 0}, "error"},
    {{1, 5, 1, 0}, "false"},
    {{1, 8, 1, 0}, "function"},
    {{1, 2, 1, 0}, "ga"},
    {{1, 2, 1, 0}, "gb"},
    {{1, 6, 1, 0}, "getRGB"},
    {{1, 12, 1, 0}, "getTransform"},
    {{1, 4, 1, 0}, "left"},
    {{1, 8, 1, 0}, "loadvars"},
    {{1, 9, 1, 0}, "movieclip"},
    {{1, 4, 1, 0}, "none"},
    {{1, 4, 1, 0}, "null"},
    {{1, 6, 1, 0}, "number"},
    {{1, 6, 1, 0}, "object"},
    {{1, 6, 1, 0}, "onData"},
    {{1, 9, 1, 0}, "onDragOut"},
    {{1, 10, 1, 0}, "onDragOver"},
    {{1, 12, 1, 0}, "onEnterFrame"},
    {{1, 9, 1, 0}, "onKeyDown"},
    {{1, 7, 1, 0}, "onKeyUp"},
    {{1, 6, 1, 0}, "onLoad"},
    {{1, 11, 1, 0}, "onMouseDown"},
    {{1, 11, 1, 0}, "onMouseMove"},
    {{1, 9, 1, 0}, "onMouseUp"},
    {{1, 12, 1, 0}, "onMouseWheel"},
    {{1, 7, 1, 0}, "onPress"},
    {{1, 9, 1, 0}, "onRelease"},
    {{1, 16, 1, 0}, "onReleaseOutside"},
    {{1, 9, 1, 0}, "onRollOut"},
    {{1, 10, 1, 0}, "onRollOver"},
    {{1, 8, 1, 0}, "onUnload"},
    {{1, 9, 1, 0}, "prototype"},
    {{1, 2, 1, 0}, "ra"},
    {{1, 2, 1, 0}, "rb"},
    {{1, 5, 1, 0}, "right"},
    {{1, 6, 1, 0}, "setRGB"},
    {{1, 12, 1, 0}, "setTransform"},
    {{1, 5, 1, 0}, "sound"},
    {{1, 6, 1, 0}, "string"},
    {{1, 5, 1, 0}, "super"},
    {{1, 10, 1, 0}, "textformat"},
    {{1, 4, 1, 0}, "this"},
    {{1, 4, 1, 0}, "true"},
    {{1, 9, 1, 0}, "undefined"},
    {{1, 4, 1, 0}, "xMax"},
    {{1, 4, 1, 0}, "xMin"},
    {{1, 3, 1, 0}, "XML"},
    {{1, 4, 1, 0}, "yMax"},
    {{1, 4, 1, 0}, "yMin"},
};
EAStringC StringPool::saConstant[StringPool::KU_CONSTANT_COUNT];

// The string-pool bucket array + count (off_8324E4F4 / dword_8324E4F8). Owned here.
// The bucket array is a flat AptString*[nCount] hash table: each slot is the head of
// a singly-linked chain threaded through AptString::mpNext (the +0xC link).
namespace
{
    void*        gpAptStringPoolBuckets = 0;   // off_8324E4F4
    unsigned int gnAptStringPoolCount   = 0;         // dword_8324E4F8

    inline AptString** AptStringPoolBuckets()
    {
        return reinterpret_cast<AptString**>(gpAptStringPoolBuckets);
    }
}

// The string-pool TABLE lock (X360 unk_8324E7D4 == XB1 dword_14147A5A0): ONE lock
// guards the bucket table on every rung (GetFromPool/FindOrCreate, RemoveFromPool/
// ReleaseString, Teardown); the free-list lock (unk_8324E8E8) below is separate.
// The console brackets the mutations with the lwarx/stwcx. interrupt-masked
// test-and-set idiom; the XB1 x64 build spins _InterlockedCompareExchange /
// _InterlockedExchange on the same word (sub_140838AE0 / sub_14083F2A0) -- the
// interlocked TAS below is that exact idiom.
namespace
{
    volatile long gStringPoolTableLock = 0;
    inline void StringPoolTableLock_Acquire()
    {
        while (_InterlockedCompareExchange(&gStringPoolTableLock, 1, 0) != 0) {}
    }
    inline void StringPoolTableLock_Release()
    {
        _InterlockedExchange(&gStringPoolTableLock, 0);
    }
}

// ARTIST @0x82AE3630: publish the static names, then allocate the bucket table.
void StringPool::Initialize(int nBucketCount)
{
    StringPoolTableLock_Acquire();
    for (int i = 0; i < StringPool::KU_CONSTANT_COUNT; ++i)
        saConstant[i] = sStringPoolData[i];

    gpAptStringPoolBuckets = gpNonGCPoolManager->Allocate(sizeof(void*) * static_cast<unsigned>(nBucketCount));
    std::memset(gpAptStringPoolBuckets, 0, sizeof(void*) * static_cast<unsigned>(nBucketCount));
    gnAptStringPoolCount = static_cast<unsigned int>(nBucketCount);
    StringPoolTableLock_Release();
}

// ---------------------------------------------------------------------------
// FindOrCreate -- the pool-string INTERNER (XB1 sub_140838AE0; the x64-primary
// Apt rung). Every serialized constant-pool string resolves through here (the
// _parseStream Push/DefineDictionary entry resolution), NOT through a plain
// temporary AptString::Create: an interned string enters the string-pool bucket
// chain ONCE with a chain-held reference (AddRef on insert) plus a GC root, so
// it outlives the deferred-release drain for the life of the pool. A temp
// Create'd copy is queued in gValuesToRelease when its node is a RECYCLED
// free-list allocation and dies at the next drain, dangling every dictionary
// slot that held it (the 2026-07-09 'BuildName'/'Initialize' pool-string death
// storm: onLoad's method names resolved to recycled corpses, so component
// registration broke after the first drain).
//
// TWIN PROOF (2026-08-07 audit): XB1 sub_140838AE0 IS StringPool::GetFromPool
// @0x82AF2F68 on the x64 rung. Same lock word (dword_14147A5A0 == X360
// unk_8324E7D4), same probe (bucket = hash % count; the 16-bit hash cached at
// record+6 short-circuits, the byte compare decides), same miss path (fresh
// node + cached hash + bucket-head link + vtbl[0] AddRef + GCRootCount+1 under
// the GC flag lock), same clamped incGCRoot on a hit. The hash the XB1 inlines
// (h = 403 * (h ^ tolower(c)), seed 0x9DC5, zero collapses to 0x4567) is
// exactly EAStringC::CalculateHashValue's 16-bit fold (403 == low-16 of the
// FNV prime 0x01000193, 0x9DC5 == low-16 of the FNV basis), which the X360
// body calls by name. ONE body (GetFromPool below) serves both names; the XB1
// probes the table unconditionally (StringPool::Initialize, from
// AptCommonInitialize, precedes the first intern on every rung), so no
// pre-Initialize arm exists to model.
// ---------------------------------------------------------------------------
AptString* StringPool::FindOrCreate(const char* pName)
{
    return GetFromPool(pName);
}

// ---------------------------------------------------------------------------
// ReleaseString (XB1 sub_14083F2A0) -- HOMED 2026-07-10 (retiring the {} stub):
// the INVERSE of FindOrCreate's hit path, called by _parseStream's unresolve
// direction per released pool string.
//
// TWIN PROOF (2026-08-07 audit): XB1 sub_14083F2A0 IS StringPool::RemoveFromPool
// @0x82AD8C98 on the x64 rung. Same lock word (dword_14147A5A0 == X360
// unk_8324E7D4), same pinned check (GC-root count 63 left alone), same decGCRoot
// (the (v4-1)<<18 merge under the GC flag lock), and on the last rooted use
// (pre-decrement count 1) the same bucket unlink -- bucket derived from the
// 16-bit hash CACHED in the string record (XB1 *(m_pData+6) % count; every
// interned node stores it on insert) -- then the chain's owning Release (vtbl[1],
// the XB1 (*(*a1+8))(a1)). One body (RemoveFromPool below) serves both names.
// ---------------------------------------------------------------------------
void StringPool::ReleaseString(AptString* pString)
{
    if (pString == 0)   // host guard: the engine never unresolves a null slot
        return;
    RemoveFromPool(pString);
}

// ---------------------------------------------------------------------------
// The AptString recycle free-list head (X360 off_8324E4FC / PS3 StringPool::
// spFirstFree). Its single DEFINITION lives in AptGlobals.cpp (the globals home);
// declared extern here (this TU's ClearTemporaryPool empties it, and the built EATech
// AptString.cpp's Create/Destroy pop/push it). Defining it here too was a LNK2005 dup.
// ---------------------------------------------------------------------------
extern AptString* gpStringPoolFreeList;   // off_8324E4FC (defined in AptGlobals.cpp)

// The string-pool FREE-LIST spin lock (X360 unk_8324E8E8 / PS3
// AptMutexStringPoolFirstFree) -- a separate lock from the bucket-table lock above.
// The console brackets the free-list teardown with the lwarx/stwcx. interrupt-
// masked test-and-set idiom; the interlocked TAS below is that idiom's direct
// host equivalent (the XB1 x64 build spins _Interlocked* on its lock words the
// same way).
namespace
{
    volatile long gStringPoolFreeListLock = 0;
    inline void StringPoolFreeListLock_Acquire()
    {
        while (_InterlockedExchange(&gStringPoolFreeListLock, 1) != 0) {}
    }
    inline void StringPoolFreeListLock_Release()
    {
        _InterlockedExchange(&gStringPoolFreeListLock, 0);
    }
}

// ---------------------------------------------------------------------------
// ClearTemporaryPool @0x82AD8E20 -- release every pooled (recycled) temporary
// string node back to the heap, emptying the free list. The GC teardown's final
// step (AptGC::CleanUnreachable / CleanAll / AptCommonShutdown).
//
// X360 walk (under the pool lock): for each node off the free-list head, save its
// mpNext (+0xC), call the node's vtable +0x28 (an empty STUB -- no-op), then call
// the head's scalar-deleting-destructor (vtable +0x38, arg 1 == `delete this`),
// advance head = saved mpNext, repeat until the list is empty. The STUB call is a
// no-op; `delete pNode` IS the scalar-deleting-destructor (~AptString() then the
// AptString operator delete -> gpNonGCPoolManager->Deallocate).
// ---------------------------------------------------------------------------
void StringPool::ClearTemporaryPool()
{
    StringPoolFreeListLock_Acquire();

    AptString* pNode = gpStringPoolFreeList;
    if (pNode)
    {
        do
        {
            AptString* const pNext = pNode->GetNext();   // v8 = result[3] (mpNext @ +0xC)
            // X360 vtable +0x28 == STUB (no-op); omitted.
            delete pNode;                                // X360 vtable +0x38(,1): scalar deleting dtor
            gpStringPoolFreeList = pNext;                // head = saved mpNext
            pNode = pNext;
        }
        while (pNode);
    }

    StringPoolFreeListLock_Release();
}

// ---------------------------------------------------------------------------
// GetFromPool @0x82AF2F68 -- intern an AS name into the string pool.
//
// Hash pName (EAStringC::CalculateHashValue) to a bucket, then walk that bucket's
// mpNext chain for a node whose cached hash + buffer match. On a HIT, GC-root the
// existing node (incGCRoot, unless it is already permanently pinned at MAX_GCROOT)
// and return it. On a MISS, allocate a fresh pooled AptString holding pName, cache
// its hash, prepend it to the chain, AddRef + incGCRoot it, and return it.
//
// The whole probe/insert runs under the pool-table lock (X360 unk_8324E7D4 ==
// XB1 dword_14147A5A0 -- see the FindOrCreate twin proof above).
// ---------------------------------------------------------------------------
AptString* StringPool::GetFromPool(const char* pName)
{
    StringPoolTableLock_Acquire();

    const unsigned int uHash  = EAStringC::CalculateHashValue(pName);   // u16 (masked)
    const unsigned int uIndex = uHash % gnAptStringPoolCount;
    AptString** const  paBuckets = AptStringPoolBuckets();

    // Probe the bucket chain: hash short-circuit, then a full string compare.
    AptString* pNode = paBuckets[uIndex];
    while (pNode != 0)
    {
        if (pNode->GetInternalString()->GetHashValue() == uHash
            && std::strcmp(pNode->GetInternalString()->GetBuffer(), pName) == 0)
            break;
        pNode = pNode->mpNext;
    }

    if (pNode == 0)
    {
        // Miss: build a fresh pooled node holding pName. Create("") hands back an
        // empty node; assign a temp EAStringC built from pName into its embedded
        // string, then cache that string's hash (the X360 recomputes + stores it).
        pNode = AptString::Create("");
        {
            EAStringC strKey(pName);
            *pNode->GetInternalString() = strKey;
        }
        pNode->GetInternalString()->CalculateHashValue();

        // Prepend to the bucket chain.
        pNode->mpNext      = paBuckets[uIndex];
        paBuckets[uIndex]  = pNode;

        pNode->AddRef();      // X360 vtable slot 0
        pNode->incGCRoot();   // pin against the collector
    }
    else if (pNode->getGCRoot() != AptValue::MAX_GCROOT)
    {
        // Hit: take another GC root (unless already permanently pinned).
        pNode->incGCRoot();
    }

    StringPoolTableLock_Release();
    return pNode;
}

// ---------------------------------------------------------------------------
// RemoveFromPool @0x82AD8C98 -- drop one GC-root reference to a pooled AptString.
//
// Under the pool-table lock: a value whose root count is MAX_GCROOT is a permanently
// pinned constant -- left untouched. Otherwise decGCRoot; and when the original root
// count was exactly 1 (so it has now reached 0) the node is unlinked from its bucket
// chain and Release()'d (X360 vtable slot +4). Any higher root count just decrements.
// ---------------------------------------------------------------------------
AptString* StringPool::RemoveFromPool(AptString* pValue)
{
    StringPoolTableLock_Acquire();

    const unsigned int uOrigRoot = pValue->getGCRoot();
    if (uOrigRoot == AptValue::MAX_GCROOT)
    {
        // Permanently pinned: never pooled-removed.
        StringPoolTableLock_Release();
        return pValue;
    }

    pValue->decGCRoot();
    if (uOrigRoot == 1)
    {
        // Last root gone: unlink from the bucket chain, then Release the node.
        const unsigned int uHash   = pValue->GetInternalString()->GetHashValue();
        const unsigned int uIndex  = uHash % gnAptStringPoolCount;
        AptString** const  paBuckets = AptStringPoolBuckets();

        if (paBuckets[uIndex] == pValue)
        {
            paBuckets[uIndex] = pValue->mpNext;
        }
        else
        {
            AptString* pCur = paBuckets[uIndex];
            while (pCur->mpNext != pValue)
                pCur = pCur->mpNext;
            pCur->mpNext = pValue->mpNext;
        }

        pValue->Release();   // X360 vtable slot +4
    }

    StringPoolTableLock_Release();
    return pValue;
}

// ---------------------------------------------------------------------------
// Teardown @0x82AE3720 -- the Apt-shutdown counterpart of Initialize.
//
// Under the pool-table lock: for every bucket, Release() each node in its mpNext
// chain; when a bucket held any nodes, drain the GC deferred-release vector
// (gValuesToRelease) so the just-Released values are actually reclaimed. Then free
// the bucket array back to the pool, zero the count, and reset the interned AS-name
// table (saConstant) to empty strings (X360 stores the empty-string sentinel into
// each of the 88 slots).
// ---------------------------------------------------------------------------
void StringPool::Teardown()
{
    StringPoolTableLock_Acquire();

    AptString** const  paBuckets = AptStringPoolBuckets();
    const unsigned int nCount    = gnAptStringPoolCount;

    for (unsigned int i = 0; i < nCount; ++i)
    {
        bool bReleasedAny = false;
        for (AptString* pNode = paBuckets[i]; pNode != 0; )
        {
            AptString* const pNext = pNode->mpNext;
            pNode->Release();   // X360 vtable slot +4
            pNode = pNext;
            bReleasedAny = true;
        }
        if (bReleasedAny)
            if (gpValuesToRelease != 0)
                gpValuesToRelease->ReleaseValues();
    }

    gpNonGCPoolManager->Deallocate(gpAptStringPoolBuckets, sizeof(void*) * nCount);
    gnAptStringPoolCount = 0;

    // Reset the static names without releasing their non-pool-owned records.
    for (int i = 0; i < StringPool::KU_CONSTANT_COUNT; ++i)
        saConstant[i].ClearConstantValue();

    StringPoolTableLock_Release();
}

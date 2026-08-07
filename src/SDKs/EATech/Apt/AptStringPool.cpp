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

#include <intrin.h>   // _InterlockedExchange (the Apt string-pool spin lock)
#include <cstring>    // memset / strcmp (bucket-array clear + pool key compare)

// ---------------------------------------------------------------------------
// saConstant -- the interned AS-name TABLE (X360 dword_8324E580, 88 entries).
//
// The 88 names are GROUND TRUTH from the TARGET binary: the 264-byte
// StaticStringHelperT record block sStringPoolData @0x82F733FC (stride 264,
// end 0x82F78EBC => 88 records) is written by the CRT initializer
// sub_82C71F10 (each record: {u16 refCount=1, u16 length, u16 flags=1, u16 0,
// char data[256]}); every literal below was recovered from that initializer's
// store set ([19] "_up" = the packed dword 0x5F757000, [85] "XML" =
// 0x584D4C00, [66] "onReleaseOutside" via its copy loop; the rest are its
// strcpy/qword stores in record order). StringPool::Initialize then points
// saConstant[i] at record i (PS3 0x7F7A70's operator= loop) -- collapsed here
// to interning each literal at Initialize (the same observable content).
// ---------------------------------------------------------------------------
EAStringC StringPool::saConstant[StringPool::KU_CONSTANT_COUNT] = {
    EAStringC("__proto__"),
    EAStringC("_alpha"),
    EAStringC("_currentframe"),
    EAStringC("_down"),
    EAStringC("_droptarget"),
    EAStringC("_focusrect"),
    EAStringC("_framesloaded"),
    EAStringC("_global"),
    EAStringC("_height"),
    EAStringC("_highquality"),
    EAStringC("_left"),
    EAStringC("_name"),
    EAStringC("_quality"),
    EAStringC("_right"),
    EAStringC("_rotation"),
    EAStringC("_soundbuftime"),
    EAStringC("_target"),
    EAStringC("_totalframes"),
    EAStringC("_type"),
    EAStringC("_up"),
    EAStringC("_url"),
    EAStringC("_visible"),
    EAStringC("_width"),
    EAStringC("_x"),
    EAStringC("_xmouse"),
    EAStringC("_xscale"),
    EAStringC("_y"),
    EAStringC("_ymouse"),
    EAStringC("_yscale"),
    EAStringC("aa"),
    EAStringC("ab"),
    EAStringC("array"),
    EAStringC("ba"),
    EAStringC("bb"),
    EAStringC("boolean"),
    EAStringC("center"),
    EAStringC("color"),
    EAStringC("controller"),
    EAStringC("date"),
    EAStringC("error"),
    EAStringC("false"),
    EAStringC("function"),
    EAStringC("ga"),
    EAStringC("gb"),
    EAStringC("getRGB"),
    EAStringC("getTransform"),
    EAStringC("left"),
    EAStringC("loadvars"),
    EAStringC("movieclip"),
    EAStringC("none"),
    EAStringC("null"),
    EAStringC("number"),
    EAStringC("object"),
    EAStringC("onData"),
    EAStringC("onDragOut"),
    EAStringC("onDragOver"),
    EAStringC("onEnterFrame"),
    EAStringC("onKeyDown"),
    EAStringC("onKeyUp"),
    EAStringC("onLoad"),
    EAStringC("onMouseDown"),
    EAStringC("onMouseMove"),
    EAStringC("onMouseUp"),
    EAStringC("onMouseWheel"),
    EAStringC("onPress"),
    EAStringC("onRelease"),
    EAStringC("onReleaseOutside"),
    EAStringC("onRollOut"),
    EAStringC("onRollOver"),
    EAStringC("onUnload"),
    EAStringC("prototype"),
    EAStringC("ra"),
    EAStringC("rb"),
    EAStringC("right"),
    EAStringC("setRGB"),
    EAStringC("setTransform"),
    EAStringC("sound"),
    EAStringC("string"),
    EAStringC("super"),
    EAStringC("textformat"),
    EAStringC("this"),
    EAStringC("true"),
    EAStringC("undefined"),
    EAStringC("xMax"),
    EAStringC("xMin"),
    EAStringC("XML"),
    EAStringC("yMax"),
    EAStringC("yMin")
};

// The shared fixed-size pool the bucket array is carved from (off_8324D808).
extern DOGMA_PoolManager* gpAptPseudoDataPool;

// The string-pool bucket array + count (off_8324E4F4 / dword_8324E4F8). Owned here.
// The bucket array is a flat AptString*[nCount] hash table: each slot is the head of
// a singly-linked chain threaded through AptString::mpNext (the +0xC link).
namespace
{
    void*        gpAptStringPoolBuckets = nullptr;   // off_8324E4F4
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
        while (_InterlockedExchange(&gStringPoolTableLock, 1) != 0) {}
    }
    inline void StringPoolTableLock_Release()
    {
        _InterlockedExchange(&gStringPoolTableLock, 0);
    }
}

// ---------------------------------------------------------------------------
// StringPool::Initialize @0x82AE3630 -- allocate the interned AS-name string table +
// the string-pool bucket array. Called once by AptCommonInitialize (AptInit.cpp) at
// the Apt bring-up. Homed here (this TU owns the full StringPool.h + no AptNativeHash.h
// mini-StringPool collision; AptInit.cpp reaches it through the AptStringPool_Initialize
// free wrapper below to avoid pulling the full StringPool.h into its interpreter-header
// include set).
//
// X360 (under the interned-table spin lock unk_8324E7D4): walk the 256-entry AS-name
// table dword_8324E580, pointing each entry at its 264-byte StaticStringHelperT record
// in the rodata block unk_82F733FC (stride 0x108), then off_8324E4F4 = pool->Allocate(
// 4 * nCount); memset(0); dword_8324E4F8 = nCount.
//
// The AS-name table CONTENTS are RECOVERED (no open rodata gap remains): the 88
// literals were read out of the CRT initializer sub_82C71F10's store set into the
// record block sStringPoolData @0x82F733FC and are statically interned at the
// saConstant definition above; this Initialize collapses the console's per-record
// pointer assignment to that static interning (the same observable content). The
// STRUCTURAL bucket-array allocation below is faithful.
// `nBucketCount` == the config bucket count (config word[11]).
// ---------------------------------------------------------------------------
void StringPool::Initialize(int nBucketCount)
{
    // (The 88 AS names are statically interned at the saConstant definition above --
    // the same proven EAStringC(const char*) pattern the old single __proto__ key
    // used; the console's Initialize-time record assignment collapses to it.)
    gpAptStringPoolBuckets = gpAptPseudoDataPool->Allocate(sizeof(void*) * static_cast<unsigned>(nBucketCount));
    if (gpAptStringPoolBuckets != nullptr)
        std::memset(gpAptStringPoolBuckets, 0, sizeof(void*) * static_cast<unsigned>(nBucketCount));
    gnAptStringPoolCount = static_cast<unsigned int>(nBucketCount);
}

// AptStringPool_Initialize -- the free wrapper AptCommonInitialize calls (so AptInit.cpp
// need not include the full StringPool.h alongside the interpreter headers, which carry
// AptNativeHash.h's incompatible mini `class StringPool`).
// PC-only header-decoupling forwarder: the console calls StringPool::Initialize @0x82AE3630
// directly; this wrapper only keeps AptNativeHash.h's incompatible mini `class StringPool`
// out of AptInit.cpp.
// FLAG PC-platform leaf: header-decoupling forwarder, no console counterpart.
void AptStringPool_Initialize(int nBucketCount)
{
    StringPool::Initialize(nBucketCount);
}

// AptStringPool_Teardown / AptStringPool_ClearTemporaryPool -- the free wrappers the Apt
// shutdown (AptInit.cpp AptCommonShutdown) reaches StringPool::Teardown @0x82AE3720 /
// StringPool::ClearTemporaryPool @0x82AD8E20 through, for the same reason as
// AptStringPool_Initialize above (AptInit.cpp cannot pull in the full StringPool.h).
// FLAG PC-platform leaf: header-decoupling forwarder, no console counterpart.
void AptStringPool_Teardown()
{
    StringPool::Teardown();
}

// FLAG PC-platform leaf: header-decoupling forwarder, no console counterpart.
void AptStringPool_ClearTemporaryPool()
{
    StringPool::ClearTemporaryPool();
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

// The free forwarder the _parseStream TU calls (same header-decoupling pattern as
// AptStringPool_Initialize above).
// FLAG PC-platform leaf: header-decoupling forwarder, no console counterpart.
AptString* AptStringPool_FindOrCreate(const char* pName)
{
    return StringPool::FindOrCreate(pName);
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
    if (pString == nullptr)   // host guard: the engine never unresolves a null slot
        return;
    RemoveFromPool(pString);
}

// The free forwarder the _parseStream TU calls (the same header-decoupling
// pattern as the pair above).
// FLAG PC-platform leaf: header-decoupling forwarder, no console counterpart.
void AptStringPoolReleaseString(AptString* pString)
{
    StringPool::ReleaseString(pString);
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
    while (pNode != nullptr)
    {
        if (pNode->GetInternalString()->GetHashValue() == uHash
            && std::strcmp(pNode->GetInternalString()->GetBuffer(), pName) == 0)
            break;
        pNode = pNode->mpNext;
    }

    if (pNode == nullptr)
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
        for (AptString* pNode = paBuckets[i]; pNode != nullptr; )
        {
            AptString* const pNext = pNode->mpNext;
            pNode->Release();   // X360 vtable slot +4
            pNode = pNext;
            bReleasedAny = true;
        }
        if (bReleasedAny)
            gValuesToRelease.ReleaseValues();
    }

    gpAptPseudoDataPool->Deallocate(gpAptStringPoolBuckets, sizeof(void*) * nCount);
    gnAptStringPoolCount = 0;

    // Reset the interned AS-name table to empty (X360 writes the empty-string
    // sentinel into each slot; the object-model equivalent is an empty EAStringC).
    for (int i = 0; i < StringPool::KU_CONSTANT_COUNT; ++i)
        saConstant[i] = EAStringC();

    StringPoolTableLock_Release();
}

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
#include "SDKs/EATech/Apt/DogmaAllocator.h"                 // DOGMA_PoolManager (StringPool::Initialize bucket array)

#include <intrin.h>   // _InterlockedExchange (the Apt string-pool spin lock)
#include <cstring>    // memset (StringPool::Initialize bucket-array clear)

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
namespace
{
    void*        gpAptStringPoolBuckets = nullptr;   // off_8324E4F4
    unsigned int gnAptStringPoolCount   = 0;         // dword_8324E4F8
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
// FLAG (rodata): the AS-name table CONTENTS come from the un-recovered rodata block
// unk_82F733FC (the same table AptGlobals.cpp defines as gAptASNameTable + FLAGs as
// engine/rodata-generated). The string-record population is FLAG'd (left to the
// constant-string data); the STRUCTURAL bucket-array allocation below is faithful.
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

// ---------------------------------------------------------------------------
// The AptString recycle free-list head (X360 off_8324E4FC / PS3 StringPool::
// spFirstFree). Its single DEFINITION lives in AptGlobals.cpp (the globals home);
// declared extern here (this TU's ClearTemporaryPool empties it, and the built EATech
// AptString.cpp's Create/Destroy pop/push it). Defining it here too was a LNK2005 dup.
// ---------------------------------------------------------------------------
extern AptString* gpStringPoolFreeList;   // off_8324E4FC (defined in AptGlobals.cpp)

// FLAG: the string-pool free-list spin lock (X360 unk_8324E8E8 / PS3
// AptMutexStringPoolFirstFree). The console brackets the free-list teardown with
// the lwarx/stwcx. interrupt-masked test-and-set idiom; modelled as a host-portable
// interlocked TAS (uncontended on the single-thread bring-up path).
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

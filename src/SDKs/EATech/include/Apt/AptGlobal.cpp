// ===========================================================================
// EATech Apt -- AptGlobal: the ActionScript "_global" object.
//
// Reconstructed from the X360 ARTIST.XEX:
//     AptGlobal::AptGlobal          @ 0x82AF0530
//     AptGlobal::~AptGlobal         @ 0x82AF05E8
//     AptGlobal::objectMemberLookup @ 0x82AE23B8
//     AptGlobal::objectMemberSet    @ 0x82AF5500
//     AptGlobal::operator new       @ 0x82AE6538
//     AptGlobal::operator delete    @ 0x82AF0590
// See AptGlobal.h for the shape / layout derivation.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptGlobal.h"
#include "SDKs/EATech/include/Apt/AptValue/AptValue.h"
#include "SDKs/EATech/include/Apt/AptString/EAString.h"      // EAStringC key
#include "SDKs/EATech/Apt/DogmaAllocator.h"                  // DOGMA_PoolManager::Allocate
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"           // AptValueGC_PoolManager + gAptValueGCSizeOffset
#include "SDKs/EATech/Apt/AptValueGCAllocator.h"             // AptValueGC_MemItem
#include "SDKs/EATech/include/Apt/AptDefine.h"               // gpGCPoolManager

// The two global scope tables _global member resolution consults, built by
// AptValueInitialize (AptInit.cpp): gpAptGlobalFallback is the AptGlobal fallback
// scope (off_8324E380), gpAptNativeGlobals aliases the _global extension object
// (off_8324E37C) -- null only before that bring-up runs, when _global resolution
// is inert. (See AptGlobal.h.)
AptValueWithHash* gpAptNativeGlobals  = 0;   // X360 off_8324E37C
AptValueWithHash* gpAptGlobalFallback = 0;   // X360 off_8324E380

// ---------------------------------------------------------------------------
// operator new @ 0x82AE6538
//
// Allocate the AptGlobal block from the GC pool and mark its AptValueGC_MemItem
// "allocated" flag. The X360 calls DOGMA_PoolManager::Allocate directly on the
// GC pool (off_8324D834 == gpGCPoolManager), then SetIsAllocated(block,
// gAptValueGCSizeOffset /*byte_8324D804*/, 1). Guarded for a null pool before
// AptAllocatorInitialize @0x82ADD118 (AptInit.cpp) wires gpGCPoolManager.
// ---------------------------------------------------------------------------
void* AptGlobal::operator new(size_t size)
{
    if (gpGCPoolManager == 0)
        return 0;

    void* lpBlock = gpGCPoolManager->Allocate(size);
    if (lpBlock != 0)
        reinterpret_cast<AptValueGC_MemItem*>(lpBlock)
            ->SetIsAllocated(gAptValueGCSizeOffset, true);
    return lpBlock;
}

// ---------------------------------------------------------------------------
// operator delete @ 0x82AF0590
//
// Free the GC block; on a successful free clear the AptValueGC_MemItem
// "allocated" flag. The X360 does DOGMA_PoolManager::Deallocate(gpGCPoolManager,
// p, size) then, iff it freed, SetIsAllocated(p, gAptValueGCSizeOffset, 0) --
// exactly AptValueGC_PoolManager::DeallocateAptValueGC. Guarded for a null pool.
// ---------------------------------------------------------------------------
void AptGlobal::operator delete(void* p, size_t size)
{
    if (gpGCPoolManager != 0)
        gpGCPoolManager->DeallocateAptValueGC(p, size);
}

// ---------------------------------------------------------------------------
// ctor @ 0x82AF0530
//
// AptObject(AptVFT_Global, 11) -- the X360 inlined the trivial AptObject base
// ctor (AptValueWithHash(AptVFT_Global, 11) then clear mClassFlags), so the asm
// calls AptValueWithHash::AptValueWithHash(this, 17, 11) and zeroes the flags
// word at +0x1C directly. Then the object is pinned as a GC root.
// ---------------------------------------------------------------------------
AptGlobal::AptGlobal()
    : AptObject(AptVFT_Global, 11)
{
    setGCRoot(1);
}

// ---------------------------------------------------------------------------
// dtor @ 0x82AF05E8
//
// Empty: the X360 stores the vtable then tail-calls ~AptObject (which tears down
// the property hash). The C++ compiler emits that chain for an empty body.
// ---------------------------------------------------------------------------
AptGlobal::~AptGlobal()
{
}

// ---------------------------------------------------------------------------
// objectMemberLookup @ 0x82AE23B8
//
// Resolve a member of _global by name: look in the native-globals table first;
// if that misses (null) or the found value is not a defined value, fall back to
// the secondary global table.
//
// FLAG PC-platform leaf: the X360 brackets the global-table access with an
// interrupt-masking atomic test-and-set spinlock (mfmsr/mtmsree/lwarx/stwcx. on
// the lock global unk_8324E71C). On the single-threaded PC bring-up path the TAS
// is elided (threading primitive, not an engine method -- the same unk_8324E71C
// treatment AptInit.cpp applies); the lock would only serialise concurrent
// readers of the observable lookup below.
// ---------------------------------------------------------------------------
AptValue* AptGlobal::objectMemberLookup(AptValue* const /*pThis*/,
                                        const AptNativeString* const pName) const
{
    // X360: AptNativeHash::Lookup(off_8324E37C + 8 /*its mHash*/, pName).
    AptValue* pFound = (gpAptNativeGlobals != 0) ? gpAptNativeGlobals->Lookup(*pName) : 0;

    // X360 condition: `!result || ((result->mnValueData >> 27) & 1) == 0`. Bit 4
    // from the MSB of the big-endian value word is mbIsDefined -- i.e. retry when
    // the native hit is absent or not a defined value.
    if (pFound == 0 || !pFound->getIsDefined())
    {
        if (gpAptGlobalFallback != 0)
            pFound = gpAptGlobalFallback->Lookup(*pName);
    }

    return pFound;
}

// ---------------------------------------------------------------------------
// objectMemberSet @ 0x82AF5500
//
// Assign a member of _global: store it in this object's own property hash, but
// only if the name is not already a native global (a native global must not be
// shadowed by an assignment). Always reports success.
//
// FLAG PC-platform leaf: same single-threaded note as objectMemberLookup -- the
// X360's interrupt-masked TAS around the registry read is elided (threading
// primitive) on the single-threaded PC path.
// ---------------------------------------------------------------------------
bool AptGlobal::objectMemberSet(AptValue* const /*pThis*/,
                                const AptNativeString* const pName,
                                AptValue* const pValue)
{
    // X360: if (!AptNativeHash::Lookup(off_8324E37C + 8, pName)) Set(this->mHash, pName, pValue).
    if (gpAptNativeGlobals == 0 || gpAptNativeGlobals->Lookup(*pName) == 0)
        Set(*pName, pValue);

    return true;
}

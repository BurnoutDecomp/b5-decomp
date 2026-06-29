// ===========================================================================
// EATech Apt -- AptGC.   DECOMPILED from the X360 ARTIST.XEX.
//   sReferenceRegistrationCb @0x82AD9C80 / CleanAll @0x82AE4A40.
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptGC.h"
#include "SDKs/EATech/include/Apt/AptValue/AptGCReleaseVector.h"   // the deferred-release vector
#include "SDKs/EATech/include/Apt/AptValue/AptInteger.h"          // AptInteger::ClearPool
#include "SDKs/EATech/include/Apt/AptValue/AptFloat.h"            // AptFloat::ClearPool
#include "SDKs/EATech/include/Apt/AptString/StringPool.h"         // StringPool::ClearTemporaryPool
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"                // AptValueGC_PoolManager

// ---------------------------------------------------------------------------
// The Apt GC globals. FLAG: defined by the Apt GC startup data (AptInit); declared
// here so this TU compiles + links against them.
//   gValuesToRelease -- the deferred-release vector instance (X360 off_8324E51C).
//   gAptValueGCPool  -- the live-AptValue pool manager  (X360 off_8324D834).
// ---------------------------------------------------------------------------
extern AptGCReleaseVector      gValuesToRelease;
extern AptValueGC_PoolManager  gAptValueGCPool;

// ---------------------------------------------------------------------------
// sReferenceRegistrationCb @0x82AD9C80 -- mark-walk callback.
// ---------------------------------------------------------------------------
void* AptGC::sReferenceRegistrationCb(const AptValue* /*pOwner*/, void* pSlot,
                                      const char* /*pDebugName*/, int /*bFlag*/)
{
    AptValue* pValue = *static_cast<AptValue**>(pSlot);
    if (!pValue->getGCMark())
    {
        pValue->setGCMark(true);
        pValue->RegisterReferences();   // recurse: visit the value's own held refs
    }
    return pValue;
}

// ---------------------------------------------------------------------------
// CleanAll @0x82AE4A40 -- tear down every live Apt value at shutdown.
// ---------------------------------------------------------------------------
void AptGC::CleanAll()
{
    // 1. Flush anything queued for deferred release.
    gValuesToRelease.ReleaseValues();

    // 2. Pre-destroy every live value (drop its GC pointers) with refcount-driven
    //    deletion suspended, so the graph stays walkable while it is dismantled.
    const bool bWasSuspended = AptValue::sbSuspendRefcountDeletions;
    AptValue::sbSuspendRefcountDeletions = true;
    for (AptValue* pValue = gAptValueGCPool.GetFirstAptValue(); pValue;
         pValue = gAptValueGCPool.GetNextAptValue(pValue))
    {
        pValue->PreDestroy();
        pValue->DestroyGCPointers();
    }
    AptValue::sbSuspendRefcountDeletions = bWasSuspended;

    // 3. Flush again (PreDestroy may have queued more), then delete every value
    //    (fetching the next link before deleting the current one).
    gValuesToRelease.ReleaseValues();
    for (AptValue* pValue = gAptValueGCPool.GetFirstAptValue(); pValue; )
    {
        AptValue* pNext = gAptValueGCPool.GetNextAptValue(pValue);
        pValue->DeleteThis();
        pValue = pNext;
    }

    // 4. Final flush, then clear the value free-lists / temporary string pool.
    gValuesToRelease.ReleaseValues();
    AptInteger::ClearPool();
    AptFloat::ClearPool();
    StringPool::ClearTemporaryPool();
}

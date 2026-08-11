// ===========================================================================
// EATech Apt -- AptDefine.cpp: the global value pools + the non-GC array alloc
// helpers (leak AptDefine.h:480-481, 539-540).
// ===========================================================================

#include "SDKs/EATech/include/Apt/AptDefine.h"
#include "SDKs/EATech/Apt/DogmaAllocator.h"          // DOGMA_PoolManager (Allocate/Deallocate)
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"   // AptValueGC_PoolManager (gpGCPoolManager type)

// The Apt value pools. They are wired to their backing DOGMA pools by the Apt
// runtime startup: AptInit.cpp's AptAllocatorInitialize @0x82ADD118 constructs
// both pool objects (via the host base-alloc hook) and WireAllocatorGlobals
// stores them here. They stay null only across the pre-init window, which the
// allocation-path null guards cover.
DOGMA_PoolManager*      gpNonGCPoolManager = 0;
AptValueGC_PoolManager* gpGCPoolManager    = 0;

// AptNonGCAllocSaveSize / AptNonGCFreeSavedSize -- the non-GC operator new[]
// path. The leak declares them but ships no body; reconstructed as the canonical
// size-prefixed pool allocation (store nSize in a leading header so delete[],
// which only gets the pointer, can recover the size DOGMA::Deallocate needs).
void* AptNonGCAllocSaveSize(size_t nSize)
{
    if (gpNonGCPoolManager == 0)
        return 0;
    size_t* lpBlock = static_cast<size_t*>(gpNonGCPoolManager->Allocate(nSize + sizeof(size_t)));
    if (lpBlock == 0)
        return 0;
    *lpBlock = nSize;
    return lpBlock + 1;
}

void AptNonGCFreeSavedSize(void* p)
{
    if (p == 0 || gpNonGCPoolManager == 0)
        return;
    size_t* lpHeader = static_cast<size_t*>(p) - 1;
    gpNonGCPoolManager->Deallocate(lpHeader, *lpHeader + sizeof(size_t));
}

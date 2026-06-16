#include "ppmalloc/EAGeneralAllocator.h"

#include <windows.h>

// EA::Allocator::GeneralAllocator (PPMalloc). Layout reconstructed in the header from 
// Spore; this TU holds the implementation. Reconstructed so far: ctor/dtor (the vtable's
// home), the PPMAutoMutex RAII lock, and MallocAligned (lock + delegate). The dlmalloc CORE
// (MallocInternal @ 0x82B502C0, 435 lines: fast-bin/bin/top-chunk/core-block allocation) and
// MallocAlignedInternal are reconstructed next from the X360 pseudocode.
namespace EA
{
namespace Allocator
{
    GeneralAllocator::GeneralAllocator()
        : mbInitialized(false)
    {
        // Real field setup happens in Init(); reconstructed with the allocation bodies.
    }

    GeneralAllocator::~GeneralAllocator()
    {
    }

    // @ 0x82B4DF08 - enter the allocator's mutex (a CRITICAL_SECTION) if present. The X360 also
    // bumps a recursion counter inside the mutex; the CRITICAL_SECTION tracks its own recursion.
    GeneralAllocator::PPMAutoMutex::PPMAutoMutex(void* pMutex)
        : mpMutex(pMutex)
    {
        if (mpMutex)
            EnterCriticalSection(static_cast<CRITICAL_SECTION*>(mpMutex));
    }

    GeneralAllocator::PPMAutoMutex::~PPMAutoMutex()
    {
        if (mpMutex)
            LeaveCriticalSection(static_cast<CRITICAL_SECTION*>(mpMutex));
    }

    // @ 0x82B515B0 - lock the heap (PPMAutoMutex), delegate to the aligned-alloc core, unlock on
    // return (the X360 inlines the PPMAutoMutex dtor; here RAII does it).
    void* GeneralAllocator::MallocAligned(size_t nSize, size_t nAlignment, size_t nAlignmentOffset, int nAllocationFlags)
    {
        PPMAutoMutex lock(mpMutex);
        return MallocAlignedInternal(nSize, nAlignment, nAlignmentOffset, nAllocationFlags);
    }
}
}

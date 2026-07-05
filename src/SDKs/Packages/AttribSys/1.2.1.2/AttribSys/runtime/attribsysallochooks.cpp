#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/attribsysallochooks.h"

#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h"    // GetAttribSysAllocator
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h" // AttribSysPackageAllocator::Malloc
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttribHashMapTablePolicy.h" // HashMapTablePolicy::FreeWithCensusIf

// AttribSys generic allocation hooks -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//   Attrib::Alloc @ 0x821F0478
//   Attrib::Free  @ 0x82804270

namespace Attrib
{
    // @ 0x821F0478 -- the AttribSys library's generic allocation hook. Account the
    // request (AllocationAccounting(size, 1)), short-circuit a zero-byte request to
    // NULL, then Malloc the bytes from the static AttribSys package allocator with
    // flags 0. The X360 Mallocs from &dword_83011B94 == AttribSysMemoryManager::
    // sAttribSysAllocator; GetAttribSysAllocator() runs the same sbHasLinearAllocator
    // assert (CgsAttribSysMemoryManager.h:211) the asm emits inline and returns that
    // same instance. The hook takes only the size; the Malloc flags argument is the
    // literal 0 the asm stores (li r5,0), not a passed-through parameter.
    void* Alloc(size_t lnSize)
    {
        Attrib::AllocationAccounting(static_cast<s32>(lnSize), 1);

        if (lnSize == 0)
            return NULL;

        CgsAttribSys::AttribSysPackageAllocator* lpAllocator =
            CgsAttribSys::AttribSysMemoryManager::GetAttribSysAllocator();
        return lpAllocator->Malloc(lnSize, 0);
    }

    // @ 0x82804270 -- the AttribSys library's generic deallocation hook. Decrement the
    // shared live-byte census (dword_83011BFC == HashMapTablePolicy::smCurrentMemory) by
    // lnSize, refresh the high-water mark (dword_83011BF8 == smPeakMemory) against the
    // post-decrement total, then -- only when both the block and the size are non-zero --
    // hand the block back to the AttribSys package allocator. This is byte-for-byte the
    // census-and-free sequence of HashMapTablePolicy::FreeWithCensusIf, so it is delegated
    // there to keep the census statics defined exactly once.
    void* Free(void* lpBlock, size_t lnSize, const char* lpcTag)
    {
        return Attrib::HashMapTablePolicy::FreeWithCensusIf(lpBlock, lnSize, lpcTag);
    }
}

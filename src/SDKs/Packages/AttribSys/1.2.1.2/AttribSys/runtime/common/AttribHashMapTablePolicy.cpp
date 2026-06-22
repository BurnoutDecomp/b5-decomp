#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/AttribHashMapTablePolicy.h"

#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h"   // GetAttribSysAllocator
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h" // AttribSysPackageAllocator::Free

namespace Attrib
{
// ---- static census ---------------------------------------------------------
// The two file-scope byte counters the X360 keeps beside the AttribSys database
// pointer (dword_83011BFC / dword_83011BF8). Zero-initialised at load.
u32 HashMapTablePolicy::smCurrentMemory = 0;
u32 HashMapTablePolicy::smPeakMemory    = 0;

// @ 0x82804690 -- hash-map bucket-array release.
//
// Store-for-store from the X360: decrement the live-byte census by liSize, refresh
// the peak against the post-decrement total (the X360 reuses one census-update
// sequence for both alloc and free, so the high-water store is left in on the free
// path), then -- only when both the block and the size are non-zero -- hand the
// block back to the AttribSys package allocator under the "Attrib::HashMapTable"
// diagnostic tag. Returns the package allocator's free result.
void* HashMapTablePolicy::Free(void* lpBlock, size_t liSize)
{
    void* lpResult = lpBlock;

    const u32 luNewCurrent = smCurrentMemory - static_cast<u32>(liSize);
    smCurrentMemory = luNewCurrent;
    if (smCurrentMemory > smPeakMemory)
        smPeakMemory = luNewCurrent;

    if (lpBlock != NULL && liSize != 0)
    {
        CgsAttribSys::AttribSysPackageAllocator* lpAllocator =
            CgsAttribSys::AttribSysMemoryManager::GetAttribSysAllocator();
        lpAllocator->Free(lpBlock, static_cast<s32>(liSize), "Attrib::HashMapTable");
        lpResult = lpBlock;
    }

    return lpResult;
}
}

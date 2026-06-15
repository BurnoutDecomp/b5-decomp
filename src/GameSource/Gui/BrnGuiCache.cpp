#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX. StateLoadingHelper tracks how many of
// its watched resources are pending an unload. Increment/Decrement adjust the count
// and then run a debug consistency check: the count must equal the number of resources
// actually in a pending-unload state (LOAD_CANCELLED / UNLOAD_REQUESTED / UNLOADING).
// The consistency loop feeds CGS_ASSERT, which is a no-op in this build (CgsAssert.h),
// matching the project convention for the X360 assert machinery.

namespace BrnGui
{
    namespace
    {
        u32 CountRealPendingUnloads(const StateLoadingHelper::ResourceInfo* lpResources, u32 luCount)
        {
            u32 luRealPending = 0;
            for (u32 i = 0; i < luCount; ++i)
            {
                StateLoadingHelper::EResourceState leState = lpResources[i].meState;
                if (leState == StateLoadingHelper::E_STATE_LOAD_CANCELLED
                    || leState == StateLoadingHelper::E_STATE_UNLOAD_REQUESTED
                    || leState == StateLoadingHelper::E_STATE_UNLOADING)
                {
                    ++luRealPending;
                }
            }
            return luRealPending;
        }
    }

    // @ 0x824EC008
    void StateLoadingHelper::IncrementUnloadPending()
    {
        ++muPendingUnloadCount;
        CGS_ASSERT(muPendingUnloadCount <= KU_MAX_RESOURCES_TO_WATCH,
                   "muPendingUnloadCount <= KU_MAX_RESOURCES_TO_WATCH");

        const u32 luRealPending = CountRealPendingUnloads(maResources, KU_MAX_RESOURCES_TO_WATCH);
        CGS_ASSERT(muPendingUnloadCount == luRealPending,
                   "Pending Unload count does not equal real pending unload count. "
                   "This is skipable but might crash with out of memory");
        (void)luRealPending;
    }

    // @ 0x824EC1E8
    void StateLoadingHelper::DecrementUnloadPending()
    {
        --muPendingUnloadCount;
        CGS_ASSERT(muPendingUnloadCount <= KU_MAX_RESOURCES_TO_WATCH,
                   "muPendingUnloadCount <= KU_MAX_RESOURCES_TO_WATCH");

        const u32 luRealPending = CountRealPendingUnloads(maResources, KU_MAX_RESOURCES_TO_WATCH);
        CGS_ASSERT(muPendingUnloadCount == luRealPending,
                   "Pending Unload count does not equal real pending unload count. "
                   "This is skipable but might crash with out of memory");
        (void)luRealPending;
    }
}

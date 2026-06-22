// Tiny embed check: proves jobs.h is self-contained and the EA::Jobs namespace-
// level helpers (AtomicStore/SetAllocator/TicksToSeconds/ToJobThreadId) are usable
// by name with the proven X360 signatures. Compile-only; no linkage.

#include "SDKs/EATech/eajobs/jobs.h"

namespace
{
    void EaJobsFuncsEmbedCheck()
    {
        using namespace EA::Jobs;

        u32 luSlot = 0;
        u32* lpuSlot = AtomicStore(&luSlot, 0x1234u);
        (void)lpuSlot;

        int liAllocator = 0;
        SetAllocator(&liAllocator);

        f32 lfSeconds = TicksToSeconds(49875000ull);
        (void)lfSeconds;

        u32 luJobThread = ToJobThreadId(7u);
        (void)luJobThread;
    }
}

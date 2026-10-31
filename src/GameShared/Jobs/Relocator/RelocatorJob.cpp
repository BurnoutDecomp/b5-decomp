#include "GameShared/Jobs/Relocator/RelocatorJob.h"
#include <xtl.h>

RelocatorJob gaRelocatorJobs[KI_NUM_RELOCATOR_JOBS];

// ARTIST 82AD2BF8. The overlap arm advances only the source: each chunk
// overwrites the same destination, as the original 82AD2C9C..B0 does.
void RelocatorJob::Execute(void* lpvJobData, u32 luJobId)
{
    CgsMemory::RelocatorJobData* lpData = static_cast<CgsMemory::RelocatorJobData*>(lpvJobData);
    mpJobData = lpData;
    if (lpData->mpTestParams != 0)
    {
        ExecuteTestHijacks(lpData->mpTestParams, luJobId);
        return;
    }

    for (s32 liOp = 0; liOp < lpData->miNumOps; ++liOp)
    {
        const CgsMemory::RelocateOp& lrOp = lpData->mpOps[liOp];
        char* lpcSource = static_cast<char*>(lrOp.mpSource);
        const u32 luSource = static_cast<u32>(reinterpret_cast<uintptr_t>(lpcSource));
        const u32 luDest = static_cast<u32>(reinterpret_cast<uintptr_t>(lrOp.mpDest));
        const u32 luSize = lrOp.muSize;
        if (static_cast<s32>(luDest + luSize - 1u) <= static_cast<s32>(luSource) ||
            static_cast<s32>(luDest) >= static_cast<s32>(luSource + luSize - 1u))
        {
            XMemCpy(lrOp.mpDest, lpcSource, luSize);
        }
        else
        {
            s32 liRemaining = static_cast<s32>(luSize);
            while (liRemaining > 0)
            {
                s32 liChunk = lpData->miBounceBufferSize;
                if (liRemaining <= liChunk)
                    liChunk = liRemaining;
                XMemCpy(lpData->mpBounceBuffer, lpcSource, static_cast<u32>(liChunk));
                XMemCpy(lrOp.mpDest, lpData->mpBounceBuffer, static_cast<u32>(liChunk));
                liRemaining -= liChunk;
                lpcSource += liChunk;
            }
        }
    }
}

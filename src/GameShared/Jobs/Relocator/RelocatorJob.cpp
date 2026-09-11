#include "GameShared/Jobs/Relocator/RelocatorJob.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstring>   // memcpy

// RelocatorJob::Execute - the copy body of the relocation job.

RelocatorJob gaRelocatorJobs[KI_NUM_RELOCATOR_JOBS];

void RelocatorJob::Execute(void* lpvJobData)
{
    CgsMemory::RelocatorJobData* lpData = static_cast<CgsMemory::RelocatorJobData*>(lpvJobData);
    mpJobData = lpData;

    // GATE: the data-stream hijack body this flag selects is not reconstructed. Nothing in
    // this build sets the flag - Relocator::Execute clears it on every pass - so the gate is
    // unreachable today; if a producer ever appears it names the missing body instead of
    // silently copying nothing.
    if (lpData->muTestHijacks != 0)
    {
        CGS_ASSERT(false, "Relocator test hijacks are not implemented\n");
        return;
    }

    for (s32 liOp = 0; liOp < lpData->miNumOps; ++liOp)
    {
        const CgsMemory::RelocateOp& lrOp = lpData->mpOps[liOp];

        char* lpcSource = static_cast<char*>(lrOp.mpSource);
        char* lpcDest   = static_cast<char*>(lrOp.mpDest);
        const s32 liSize = static_cast<s32>(lrOp.muSize);

        // Disjoint spans copy straight through. The two comparisons are the console's, as
        // written: destination end at or below the source start, or destination at or after
        // the source end.
        if (lpcDest + liSize - 1 <= lpcSource || lpcDest >= lpcSource + liSize - 1)
        {
            memcpy(lpcDest, lpcSource, static_cast<size_t>(liSize));
            continue;
        }

        // Overlapping: bounce the block through the staging buffer a chunk at a time. Only
        // the read cursor advances - the console re-reads the op's destination every chunk,
        // so each chunk lands at the same destination address. Reproduced as written.
        s32   liRemaining = liSize;
        char* lpcCursor   = lpcSource;
        while (liRemaining > 0)
        {
            s32 liChunk = lpData->miBounceBufferSize;
            if (liRemaining <= liChunk)
            {
                liChunk = liRemaining;
            }

            memcpy(lpData->mpBounceBuffer, lpcCursor, static_cast<size_t>(liChunk));
            memcpy(lrOp.mpDest, lpData->mpBounceBuffer, static_cast<size_t>(liChunk));

            liRemaining -= liChunk;
            lpcCursor   += liChunk;
        }
    }
}

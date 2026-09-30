#include "GameShared/Jobs/Relocator/RelocatorJob.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstring>   // memcpy
#include <cstdint>

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
        const u32 luSize = lrOp.muSize;
        if (luSize == 0)
            continue;

        // FLAG PC-platform leaf: compare native addresses, not unrelated C++
        // pointers. Half-open spans correctly classify even one-byte overlaps.
        const uintptr_t luSource = reinterpret_cast<uintptr_t>(lpcSource);
        const uintptr_t luDest = reinterpret_cast<uintptr_t>(lpcDest);
        if (luDest + luSize <= luSource || luDest >= luSource + luSize)
        {
            memcpy(lpcDest, lpcSource, luSize);
            continue;
        }

        // Native correctness repair to ARTIST 82AD2C7C..82AD2CB8: the original
        // advances only the source and overwrites the beginning of the destination
        // on every chunk. Real car resources exceed the 1 MiB bounce buffer.
        // Advance both sides and copy backward when a higher destination overlaps
        // unread source bytes. Preserve the bounded temporary-buffer contract.
        CGS_ASSERT(lpData->mpBounceBuffer != nullptr && lpData->miBounceBufferSize > 0,
                   "Overlapping relocation requires a non-empty bounce buffer\n");
        if (lpData->mpBounceBuffer == nullptr || lpData->miBounceBufferSize <= 0)
            return;
        u32 luRemaining = luSize;
        while (luRemaining != 0)
        {
            const u32 luCapacity = static_cast<u32>(lpData->miBounceBufferSize);
            const u32 luChunk = luRemaining < luCapacity ? luRemaining : luCapacity;
            const u32 luOffset = luDest > luSource ? luRemaining - luChunk : luSize - luRemaining;
            memcpy(lpData->mpBounceBuffer, lpcSource + luOffset, luChunk);
            memcpy(lpcDest + luOffset, lpData->mpBounceBuffer, luChunk);
            luRemaining -= luChunk;
        }
    }
}

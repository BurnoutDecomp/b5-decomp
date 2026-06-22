#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamResultPoster.h"

#include <cstring>  // std::memcpy (models the Xbox XMemCpy block-copy intrinsic)

namespace CgsMemory
{
    // X360 0x828680F0.
    // Atomically reserves liResultCount slots at the end of the reader's result
    // stream, then block-copies the records in. The reader's packed status word
    // (mEncodedStatus) carries numResults[bits 0-23], dataBufferUsed[bits 24-47]
    // and numUsers[bits 48-51]; this only advances the numResults field while
    // preserving the rest, via a compare-and-swap retry loop.
    //
    // Returns the index of the first reserved slot (the pre-increment numResults),
    // or -1 if there is no room (numResults + count would exceed miMaxResults).
    //
    // Asm member accesses (through this->mpReader):
    //   *(reader+0)  = mEncodedStatus    (64-bit packed status; ldarx/stdcx CAS)
    //   *(reader+20) = miMaxResults      (capacity check)
    //   *(reader+16) = miResultSize      (stride; copy skipped when 0)
    //   *(reader+8)  = mpcResultBuffer   (copy destination base)
    // XMemCpy(dest = miResultSize*firstSlot + mpcResultBuffer, src = lpResults,
    //         count = miResultSize*liResultCount).
    s32 DataStreamResultPoster::AddResults(void* lpResults, s32 liResultCount)
    {
        DataStreamResultReader* lpReader = mpReader;

        u32 luFirstSlot;
        for (;;)
        {
            const u64 luCurrEncoded = lpReader->mEncodedStatus.GetValue();

            luFirstSlot = static_cast<u32>(luCurrEncoded &
                                           DataStreamResultReader::KU_NUM_RESULTS_MASK);
            const u32 luNewNumResults = luFirstSlot + static_cast<u32>(liResultCount);
            if (luNewNumResults > static_cast<u32>(lpReader->miMaxResults))
            {
                return -1;
            }

            // Replace only the numResults field; keep dataBufferUsed + numUsers.
            const u64 luNewEncoded =
                (luCurrEncoded & ~DataStreamResultReader::KU_NUM_RESULTS_MASK) |
                (static_cast<u64>(luNewNumResults) &
                 DataStreamResultReader::KU_NUM_RESULTS_MASK);

            if (lpReader->mEncodedStatus.SetValueConditional(luNewEncoded, luCurrEncoded))
            {
                break;
            }
        }

        const s32 liResultSize = lpReader->miResultSize;
        if (liResultSize != 0)
        {
            std::memcpy(lpReader->mpcResultBuffer + liResultSize * luFirstSlot,
                        lpResults,
                        static_cast<size_t>(liResultSize * liResultCount));
        }

        return static_cast<s32>(luFirstSlot);
    }
}

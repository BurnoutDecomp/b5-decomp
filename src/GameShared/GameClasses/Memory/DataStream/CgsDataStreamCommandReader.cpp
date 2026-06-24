#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamCommandReader.h"

#include <cstring>  // std::memcpy (models the Xbox XMemCpy block-copy intrinsic)

namespace CgsMemory
{
    // X360 0x82867920.
    // Atomically claims the next command slot from the poster's stream and copies
    // it out. The poster's packed status word (mEncodedStatus) carries
    // nextCommand[bits 0-23] and numUsers[bits 24-27]; ReadCom advances only the
    // nextCommand field via a compare-and-swap retry loop while preserving the
    // numUsers bits, then block-copies the reserved record.
    //
    // Returns 1 when the stream is drained (the claimed index would reach or
    // exceed the poster's posted command count, miNumCommands); otherwise copies
    // the record, writes the slot index through lpuOutIndex if non-NULL, and
    // returns 0.
    //
    // Asm member accesses (through this->mpPoster):
    //   *(poster+0)  = mEncodedStatus    (64-bit packed status; ldarx/stdcx CAS)
    //   *(poster+20) = miNumCommands     (posted-command count; the read bound)
    //   *(poster+16) = miCommandSize     (record stride; also the copy count)
    //   *(poster+8)  = mpcCommandBuffer  (copy source base)
    // XMemCpy(dest = lpDest, src = miCommandSize*index + mpcCommandBuffer,
    //         count = miCommandSize).
    s32 DataStreamCommandReader::ReadCom(void* lpDest, u32* lpuOutIndex)
    {
        DataStreamCommandPoster* lpPoster = mpPoster;

        u32 luIndex;
        for (;;)
        {
            const u64 luCurrEncoded = lpPoster->mEncodedStatus.GetValue();

            luIndex = static_cast<u32>(luCurrEncoded &
                                       DataStreamCommandPoster::KU_NEXT_COMMAND_MASK);

            if (luIndex >= static_cast<u32>(lpPoster->miNumCommands))
            {
                return 1;
            }

            // Advance only the nextCommand field; keep the numUsers bits. The
            // X360 build of the new value (rlwinm 0,4,7) keeps just bits 24-27 of
            // the low word, so the upper 32 bits are cleared -- mirrored here by
            // masking the current value with KU_NUM_USERS_MASK (0x0F000000).
            const u64 luNewEncoded =
                (luCurrEncoded & DataStreamCommandPoster::KU_NUM_USERS_MASK) |
                (static_cast<u64>(luIndex + 1) &
                 DataStreamCommandPoster::KU_NEXT_COMMAND_MASK);

            if (lpPoster->mEncodedStatus.SetValueConditional(luNewEncoded, luCurrEncoded))
            {
                break;
            }
        }

        const s32 liCommandSize = lpPoster->miCommandSize;
        std::memcpy(lpDest,
                    lpPoster->mpcCommandBuffer + liCommandSize * luIndex,
                    static_cast<usize>(liCommandSize));

        if (lpuOutIndex)
        {
            *lpuOutIndex = luIndex;
        }

        return 0;
    }
}

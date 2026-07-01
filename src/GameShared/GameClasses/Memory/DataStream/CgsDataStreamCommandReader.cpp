#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamCommandReader.h"

#include <cstring>  // std::memcpy (models the Xbox XMemCpy block-copy intrinsic)

namespace CgsMemory
{
    // X360 0x82867830.
    // Binds this reader to lpPoster and registers it as a user of the poster's
    // stream. numUsers is packed into bits 24-27 of the poster's mEncodedStatus
    // (KU_NUM_USERS_BIT/MASK); this atomically increments just that nibble via a
    // compare-and-swap retry loop, preserving the nextCommand field (bits 0-23)
    // untouched. Also sets miLastCommandIndex = -1 (X360 sentinel init; see header).
    //
    // Same CAS idiom as ReadCom / DataStreamResultPoster::AddResults, using the
    // committed Futex::AtomicUint64 (GetValue/SetValueConditional) rather than
    // the raw ldarx/stdcx the X360 asm inlines.
    void DataStreamCommandReader::Construct(DataStreamCommandPoster* lpPoster)
    {
        mpPoster = lpPoster;
        miLastCommandIndex = -1;

        for (;;)
        {
            const u64 luCurrEncoded = lpPoster->mEncodedStatus.GetValue();

            const u32 luNumUsers = static_cast<u32>(
                (luCurrEncoded >> DataStreamCommandPoster::KU_NUM_USERS_BIT) &
                DataStreamCommandPoster::KU_NUM_USERS_MAX);

            const u64 luNewEncoded =
                (luCurrEncoded & ~DataStreamCommandPoster::KU_NUM_USERS_MASK) |
                (((static_cast<u64>(luNumUsers) + 1) &
                  DataStreamCommandPoster::KU_NUM_USERS_MAX) <<
                 DataStreamCommandPoster::KU_NUM_USERS_BIT);

            if (lpPoster->mEncodedStatus.SetValueConditional(luNewEncoded, luCurrEncoded))
            {
                break;
            }
        }
    }

    // X360 0x828678B0.
    // Unregisters this reader as a user of its bound poster's stream: atomically
    // decrements the poster's packed numUsers nibble via the same compare-and-
    // swap retry loop as Construct, preserving the nextCommand field.
    void DataStreamCommandReader::Destruct()
    {
        DataStreamCommandPoster* lpPoster = mpPoster;

        for (;;)
        {
            const u64 luCurrEncoded = lpPoster->mEncodedStatus.GetValue();

            const u32 luNumUsers = static_cast<u32>(
                (luCurrEncoded >> DataStreamCommandPoster::KU_NUM_USERS_BIT) &
                DataStreamCommandPoster::KU_NUM_USERS_MAX);

            const u64 luNewEncoded =
                (luCurrEncoded & ~DataStreamCommandPoster::KU_NUM_USERS_MASK) |
                (((static_cast<u64>(luNumUsers) - 1) &
                  DataStreamCommandPoster::KU_NUM_USERS_MAX) <<
                 DataStreamCommandPoster::KU_NUM_USERS_BIT);

            if (lpPoster->mEncodedStatus.SetValueConditional(luNewEncoded, luCurrEncoded))
            {
                break;
            }
        }
    }

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

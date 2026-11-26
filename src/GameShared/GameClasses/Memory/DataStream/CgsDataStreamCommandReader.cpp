#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamCommandReader.h"

#include <xtl.h>
#include "GameShared/GameClasses/Core/X360/CgsCoreX360.h"  // CgsCore::MemCpy

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

        bool lbSet;
        do
        {
            const u64 luCurrent = mpPoster->mEncodedStatus.GetValue();
            u8 lucNumUsers;
            u32 luIndex;
            DataStreamCommandPoster::DecodeStatus(luCurrent, &lucNumUsers, &luIndex);
            const u64 luNext = DataStreamCommandPoster::EncodeStatus(static_cast<u8>(lucNumUsers + 1), luIndex);
            lbSet = mpPoster->mEncodedStatus.SetValueConditional(luNext, luCurrent);
        } while (!lbSet);
    }

    // X360 0x828678B0.
    // Unregisters this reader as a user of its bound poster's stream: atomically
    // decrements the poster's packed numUsers nibble via the same compare-and-
    // swap retry loop as Construct, preserving the nextCommand field.
    void DataStreamCommandReader::Destruct()
    {
        bool lbSet;
        do
        {
            const u64 luCurrent = mpPoster->mEncodedStatus.GetValue();
            u8 lucNumUsers;
            u32 luIndex;
            DataStreamCommandPoster::DecodeStatus(luCurrent, &lucNumUsers, &luIndex);
            const u64 luNext = DataStreamCommandPoster::EncodeStatus(static_cast<u8>(lucNumUsers - 1), luIndex);
            lbSet = mpPoster->mEncodedStatus.SetValueConditional(luNext, luCurrent);
        } while (!lbSet);
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
        u32 luIndex;
        u64 luCurrent;
        u64 luNext;
        do
        {
            luCurrent = mpPoster->mEncodedStatus.GetValue();
            u8 lucNumUsers;
            DataStreamCommandPoster::DecodeStatus(luCurrent, &lucNumUsers, &luIndex);
            if (luIndex >= static_cast<u32>(mpPoster->miNumCommands))
                return 1;

            luNext = DataStreamCommandPoster::EncodeStatus(lucNumUsers, luIndex + 1);
        } while (!mpPoster->mEncodedStatus.SetValueConditional(luNext, luCurrent));

        const s32 liCommandSize = mpPoster->miCommandSize;
        CgsCore::MemCpy(lpDest, mpPoster->mpcCommandBuffer + liCommandSize * luIndex, liCommandSize);
        if (lpuOutIndex)
            *lpuOutIndex = luIndex;
        return 0;
    }
    // ARTIST 0x82867A00: reserve one contiguous batch before copying records.
    DataStreamCommandReader::EReadCommandStatus DataStreamCommandReader::ReadCommands(void* lpDest, s32 liMaxCommands,
                                               s32* lpiReadCount, s32* lpiFirstIndex)
    {
        DataStreamCommandPoster* lpPoster = mpPoster;
        u32 luIndex, luCount;
        for (;;)
        {
            const u64 luCurrent = lpPoster->mEncodedStatus.GetValue();
            luIndex = static_cast<u32>(luCurrent & DataStreamCommandPoster::KU_NEXT_COMMAND_MASK);
            const u32 luPosted = static_cast<u32>(lpPoster->miNumCommands);
            if (luIndex >= luPosted)
                return E_READ_FINISHED;
            luCount = luPosted - luIndex;
            if (luCount > static_cast<u32>(liMaxCommands))
                luCount = static_cast<u32>(liMaxCommands);
            const u64 luNext = (luCurrent & DataStreamCommandPoster::KU_NUM_USERS_MASK) |
                               (static_cast<u64>(luIndex + luCount) & DataStreamCommandPoster::KU_NEXT_COMMAND_MASK);
            if (lpPoster->mEncodedStatus.SetValueConditional(luNext, luCurrent))
                break;
        }
        const u32 luSize = static_cast<u32>(lpPoster->miCommandSize);
        CgsCore::MemCpy(lpDest, lpPoster->mpcCommandBuffer + luSize * luIndex, luSize * luCount);
        *lpiReadCount = static_cast<s32>(luCount);
        if (lpiFirstIndex)
            *lpiFirstIndex = static_cast<s32>(luIndex);
        return E_READ_SUCCESS;
    }

}

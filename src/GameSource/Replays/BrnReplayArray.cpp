#include "GameSource/Replays/BrnReplayArray.h"

#include "GameSource/Replays/BrnReplayBaseSerialiser.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Containers/CgsBitArray.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX (BrnReplays::BrnReplayArray<u32,4>).
//   Read  @0x826512A8 : length byte, then key-frame verbatim OR delta {index,value} records.
//   Write @0x82654118 : length byte, then key-frame verbatim OR delta records (changed prefix
//                       bits + appended tail).
// Assert file/line strings cite Replays/BrnReplayArray.h (:152/:163/:233/:247/:256) and
// Containers/CgsBitArray.h (:222); CGS_ASSERT records this file's __FILE__/__LINE__ and the
// messages are preserved verbatim (the streamed value + trailing "\n" are dropped).

namespace BrnReplays
{
    // @0x826512A8
    template <typename T, u8 N>
    void BrnReplayArray<T, N>::Read(BaseSerialiser* lpSerialiser)
    {
        u8 lu8Length = 0;
        lpSerialiser->ReadByte(&lu8Length);
        CGS_ASSERT(lu8Length <= N, "Bad array size: ");
        muLength = lu8Length;

        if (lpSerialiser->IsKeyFrame())
        {
            lpSerialiser->Read(maElements, static_cast<s32>(sizeof(T)) * lu8Length);
            return;
        }

        u8 lu8NumChanged = 0;
        lpSerialiser->ReadByte(&lu8NumChanged);
        CGS_ASSERT(lu8NumChanged <= muLength, "luNumChangedElements <= muLength");

        for (u32 luRecord = 0; luRecord < lu8NumChanged; ++luRecord)
        {
            ReplayArrayUpdateRecord lRecord;
            lpSerialiser->Read(&lRecord, sizeof(lRecord));

            const u32 luIndex = lRecord.mu8Index;
            CGS_ASSERT(luIndex < muLength, "Bad update index: ");
            maElements[luIndex] = static_cast<T>(lRecord.muValue);
        }
    }

    // @0x82654118
    template <typename T, u8 N>
    void BrnReplayArray<T, N>::Write(BaseSerialiser* lpSerialiser,
                                    const T* lpPrevData,
                                    u8 lu8PrevLength)
    {
        CGS_ASSERT(muLength <= N, "Bad array length: ");

        lpSerialiser->WriteByte(&muLength);

        if (lpSerialiser->IsKeyFrame())
        {
            lpSerialiser->Write(maElements, static_cast<s32>(sizeof(T)) * muLength);
            return;
        }

        CGS_ASSERT(lu8PrevLength <= N, "Bad previous array length: ");

        // Compare the shared prefix of the live and previous arrays; record a "changed" bit
        // for every element that differs. The changed set is an inlined CgsContainers::BitArray<N>.
        const u32 luCompareCount = (muLength >= lu8PrevLength) ? lu8PrevLength : muLength;
        CgsContainers::BitArray<N> lChangedBits;
        lChangedBits.UnSetAll();
        u32 luNumChanged = 0;
        for (u32 luElement = 0; luElement < luCompareCount; ++luElement)
        {
            if (maElements[luElement] != lpPrevData[luElement])
            {
                CGS_ASSERT(luElement < N, "Index: ");
                lChangedBits.SetBit(luElement);
                ++luNumChanged;
            }
        }

        // Record count byte = changed-in-prefix + appended tail (muLength - luCompareCount).
        const u8 lu8NumRecords =
            static_cast<u8>(luNumChanged - luCompareCount + muLength);
        lpSerialiser->WriteByte(&lu8NumRecords);

        // Emit one record per set changed bit, walking the mask low-to-high (bounded to N).
        for (s32 liScan = lChangedBits.GetFirstNonZeroBit();
             liScan != CgsContainers::BitArray<N>::KI_INVALID_BITINDEX;
             liScan = lChangedBits.GetNextNonZeroBit(liScan))
        {
            ReplayArrayUpdateRecord lRecord;
            lRecord.mu8Index = static_cast<u8>(liScan);
            lRecord.muValue  = static_cast<u32>(maElements[liScan]);
            lpSerialiser->Write(&lRecord, sizeof(lRecord));
        }

        // Append every element beyond the previous length: index runs prevLength.., value is the
        // live element (X360 restores the tail start from var_FC == lu8PrevLength).
        if (lu8PrevLength < muLength)
        {
            u8 lu8Index = lu8PrevLength;
            for (u32 luElement = lu8PrevLength; luElement < muLength; ++luElement)
            {
                ReplayArrayUpdateRecord lRecord;
                lRecord.mu8Index = lu8Index;
                lRecord.muValue  = static_cast<u32>(maElements[luElement]);
                lpSerialiser->Write(&lRecord, sizeof(lRecord));
                lu8Index = static_cast<u8>(luElement + 1);
            }
        }
    }

    // The only attested instantiation of the delta serialisers.
    template void BrnReplayArray<u32, 4>::Read(BaseSerialiser*);
    template void BrnReplayArray<u32, 4>::Write(BaseSerialiser*, const u32*, u8);
}

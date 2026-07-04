// ============================================================================
// CgsSceneManager::CoarseQueryResultBuffer<N>
//   Home: GameShared/GameClasses/SceneManager/SpatialPartitionModule/
//         CgsCoarseQueryResultBuffer.h
//
// Canonical (DWARF) home for CgsSceneManager::CoarseQueryResultBuffer<N>, the
// fixed-capacity coarse-query result staging buffer used by the spatial-partition
// module. Results are pushed 16-bit-at-a-time into a flat u16 buffer, grouped into
// "batches" delimited by a header word (high bit 0x8000 = batch start, low 15 bits =
// attempted count). Layout + member names/offsets are DWARF-authoritative
// (CgsCoarseQueryResultBuffer.h:51..116); method shapes are DWARF-attested and gated
// on the X360 ledger (this wave bodies 6 of them; the remaining declares are the
// Construct/Destruct/Clear/Begin/GetFreeSpace/GetNumBatches/GetNumResultsAttempted
// siblings, declare-only here until their own TUs land).
//
// LAYOUT (DWARF, base = u8*; the <16384> instantiation is honest size 32920):
//   +0      uint16_t mau16Buffer[16448]                  (16384 capacity + 64 header/slack)
//   +32896  int32_t  miTotalNumResults
//   +32900  int32_t  miTotalBufferSize                   (current write cursor, in u16 slots)
//   +32904  int32_t  miCurrentResultsStartPosition       (start slot of the in-flight batch)
//   +32908  int32_t  miNumResultsAttemptedThisBatch
//   +32912  uint32_t muNumBatches
//   +32916  bool     mbInABatch                          (+1..+3 tail pad -> size 32920)
//
// The buffer array length 16448 (not the 16384 capacity) is DWARF-attested
// (CgsCoarseQueryResultBuffer.h:110 uint16_t[16448] mau16Buffer) and cross-checked by
// the honest 32920-byte size at the foreign use site (CgsSpatialPartitionManagerIO.h
// mCoarseResultBuffer / CoarseQueryResultBufferDefault[32920]): 32920 = 16448*2 + 24.
// PushResult/PushResults cap writes at KU_MaxResults (0x4000 == 16384) from the asm.
// ============================================================================
#pragma once

#include "types.hpp"

#include <cstring>   // std::memcpy (PushResults)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace CgsSceneManager
{
    // CgsCoarseQueryResultBuffer.h:51 (DWARF). Template capacity is the pushable-result
    // limit (0x4000 for the shipping <16384> instantiation); the backing array is 64 slots
    // larger to hold per-batch header words.
    template <u32 KU_MaxResults>
    struct CoarseQueryResultBuffer
    {
    public:
        void Construct();                                                     // :137 (declare-only)
        void Destruct();                                                      // :155 (declare-only)
        void Clear();                                                         // :174 (declare-only)

        s32        GetFreeSpace() const;                                      // :365 (declare-only)
        u16*       GetResultsBatch();                                         // :198  X360 0x828AD8E0
        void       BeginResultsBatch();                                      // :217 (declare-only)
        void       EndResultsBatch();                                        // :244  X360 0x828ADA10
        bool       PushResult(u16 lu16Result);                               // :268  X360 0x828ADAC0
        bool       PushResults(const u16* lpResults, u32 luNumResults);      // :298  X360 0x828ADB98
        s32        GetNumResultsWritten() const;                             // :326  X360 0x828ADC60
        s32        GetNumResultsAttempted() const;                           // :346 (declare-only)
        u32        GetNumBatches() const;                                    // :381 (declare-only)
        const u16* GetBatchByOffset(u32 luOffset, u32* lpNextBatchOffsetOut, // :407  X360 0x828ADD40
                                    u32* lpNumResultsOut) const;

    private:
        // DWARF-attested members (names + byte offsets). KU_MaxResults + 64 == 16448 for <16384>.
        u16 mau16Buffer[KU_MaxResults + 64];        // +0
        s32 miTotalNumResults;                      // +32896 (0x8080)
        s32 miTotalBufferSize;                      // +32900 (0x8084)
        s32 miCurrentResultsStartPosition;          // +32904 (0x8088)
        s32 miNumResultsAttemptedThisBatch;         // +32908 (0x808C)
        u32 muNumBatches;                           // +32912 (0x8090)
        bool mbInABatch;                            // +32916 (0x8094)
    };

    // ------------------------------------------------------------------------
    // Generic method bodies (shared by every instantiation; each per-instance
    // ledger TU is a thin explicit instantiation). X360 addresses per method.
    // ------------------------------------------------------------------------

    // X360 0x828AD8E0. &mau16Buffer[miCurrentResultsStartPosition] (non-const).
    template <u32 KU_MaxResults>
    u16* CoarseQueryResultBuffer<KU_MaxResults>::GetResultsBatch()
    {
        CGS_ASSERT(mbInABatch,
                   "GetResultsBatch called outside of a BeginResultsBatch/EndResultsBatch pair");

        return &mau16Buffer[miCurrentResultsStartPosition];
    }

    // X360 0x828ADA10. Back-patch the batch header word (written by BeginResultsBatch one slot
    // before the batch start) with this batch's attempted-result count and the batch-start high bit.
    template <u32 KU_MaxResults>
    void CoarseQueryResultBuffer<KU_MaxResults>::EndResultsBatch()
    {
        CGS_ASSERT(mbInABatch,
                   "EndResultsBatch called without a matching call to BeginResultsBatch");

        const u16 lu16Count = static_cast<u16>(miNumResultsAttemptedThisBatch & 0xFFFF);
        mbInABatch = false;
        ++muNumBatches;
        mau16Buffer[miCurrentResultsStartPosition - 1] = static_cast<u16>(lu16Count | 0x8000);
    }

    // X360 0x828ADAC0. Push a single 16-bit result (capping at KU_MaxResults).
    template <u32 KU_MaxResults>
    bool CoarseQueryResultBuffer<KU_MaxResults>::PushResult(u16 lu16Result)
    {
        CGS_ASSERT(mbInABatch,
                   "PushResult called outside of a BeginResultsBatch/EndResultsBatch pair");

        ++miNumResultsAttemptedThisBatch;
        if (miTotalBufferSize >= static_cast<s32>(KU_MaxResults))
        {
            return false;
        }

        // X360 emitted a dcbz128 cache-line prezero here; no observable effect on the host, dropped.
        mau16Buffer[miTotalBufferSize] = lu16Result;
        ++miTotalBufferSize;
        ++miTotalNumResults;
        return true;
    }

    // X360 0x828ADB98. Push a run of 16-bit results (capping at KU_MaxResults).
    template <u32 KU_MaxResults>
    bool CoarseQueryResultBuffer<KU_MaxResults>::PushResults(const u16* lpResults, u32 luNumResults)
    {
        CGS_ASSERT(mbInABatch,
                   "PushResults called outside of a BeginResultsBatch/EndResultsBatch pair");

        const s32 liStart = miTotalBufferSize;
        miNumResultsAttemptedThisBatch += luNumResults;

        if (static_cast<u32>(liStart + luNumResults) > KU_MaxResults)
        {
            return false;
        }

        memcpy(&mau16Buffer[liStart], lpResults, 2 * luNumResults);
        miTotalBufferSize += luNumResults;
        miTotalNumResults += luNumResults;
        return true;
    }

    // X360 0x828ADC60. miTotalBufferSize - miCurrentResultsStartPosition (const).
    template <u32 KU_MaxResults>
    s32 CoarseQueryResultBuffer<KU_MaxResults>::GetNumResultsWritten() const
    {
        CGS_ASSERT(mbInABatch,
                   "GetNumResultsWritten called outside of a BeginResultsBatch/EndResultsBatch pair");

        return miTotalBufferSize - miCurrentResultsStartPosition;
    }

    // X360 0x828ADD40. Walk one batch out of the flat buffer at luOffset (const).
    template <u32 KU_MaxResults>
    const u16* CoarseQueryResultBuffer<KU_MaxResults>::GetBatchByOffset(
        u32 luOffset, u32* lpNextBatchOffsetOut, u32* lpNumResultsOut) const
    {
        CGS_ASSERT(lpNextBatchOffsetOut, "lpNextBatchOffsetOut");
        CGS_ASSERT(lpNumResultsOut, "lpNumResultsOut");

        const u16* lpHeader = &mau16Buffer[luOffset];
        const u32  luNumResults = *lpHeader & 0x7FFF;

        // The batch header word must have its high (batch-start) bit set. Reproduced store-for-store
        // from the X360 (the check ORs in 0x8000, so it is effectively a non-empty-buffer guard).
        CGS_ASSERT(miTotalBufferSize > 0 || (*lpHeader | 0x8000),
                   "Bad offset requested. Element is not a batch start offset");

        if (luOffset < static_cast<u32>(miTotalBufferSize))
        {
            *lpNextBatchOffsetOut = luNumResults + luOffset + 1;
            *lpNumResultsOut = luNumResults;
            return lpHeader + 1;
        }

        return nullptr;
    }
}

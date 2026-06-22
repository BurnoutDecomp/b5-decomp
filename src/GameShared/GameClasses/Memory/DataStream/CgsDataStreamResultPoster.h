#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamResultReader.h"

// CgsMemory::DataStreamResultPoster
// ---------------------------------------------------------------------------
// The posting side that feeds a DataStreamResultReader: callers append fixed-
// stride result records (and optionally variable-size data) which the consuming
// reader plays back. The poster is a thin wrapper that owns nothing but a back
// pointer to the reader (mpReader @ +0); the actual buffers and the packed
// 64-bit lock-free status word live on the reader. The poster mutates the
// reader's status word via an atomic compare-and-swap loop (the reader's
// Futex::AtomicUint64 mEncodedStatus), then block-copies the new records into
// the reader's result buffer.
//
// X360 home (reconstructed in this pass):
//   AddResults @ 0x828680F0
//
// Layout: a single member mpReader @ +0 (verified against the AddResults
// pseudocode: `r10 = *this` reads mpReader, then dereferences the reader's
// members through it). Member access is by name throughout.
namespace CgsMemory
{
    struct DataStreamResultPoster
    {
        // --- API (CgsDataStreamResultPoster.h:56-83) -------------------------
        // Reconstructed in this pass.
        s32 AddResults(void* lpResults, s32 liResultCount);

        // Declared-only (not reconstructed in this TU pass).
        void  Construct(DataStreamResultReader* lpReader);
        void  Destruct();
        void* AllocateData(s32 liSize, s32 liAlignment);
        s32   AddResult(void* lpResult);
        u32   GetResultSize() const;
        u32   GetMaxResultsInBuffer() const;

    private:
        // Declared-only reader-status helpers (CgsDataStreamResultPoster.h:95/98).
        // AddResults inlines the same get / compare-and-swap directly against the
        // reader's mEncodedStatus, so these are not reconstructed in this pass.
        u64  GetReaderEncodedStatus() const;
        bool SetReaderEncodedStatusConditional(u64 luNewValue, u64 luCurrValue);

        // --- member (CgsDataStreamResultPoster.h:92) -------------------------
        DataStreamResultReader* mpReader;  // +0  the reader this poster feeds
    };
}

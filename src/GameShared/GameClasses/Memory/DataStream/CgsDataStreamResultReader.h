#pragma once

#include "types.hpp"
#include "SDKs/EATech/eathread/Futex.h"  // Futex::AtomicUint64 (minimal slice)

// CgsMemory::DataStreamResultReader
// ---------------------------------------------------------------------------
// Reads back the fixed-stride result records a data-stream command produced.
// The reader owns two 128-byte-aligned, 128-byte-multiple buffers (a result
// buffer of fixed-size records and a variable-size data buffer) plus a packed
// 64-bit status word (mEncodedStatus) that the streaming side updates lock-free.
//
// X360 homes:
//   Construct   @ 0x8286A0D0
//   Begin       @ 0x828681D8
//   ReadResult  @ 0x82868418
//
// Layout is X360-authoritative (verified against the pseudocode this->[n]
// member accesses): mEncodedStatus@0 (8B), mpcResultBuffer@8, miResultBufferSize@12,
// miResultSize@16, miMaxResults@20, mpcDataBuffer@24, miDataBufferSize@28,
// mbStreaming@32, miNextResult@36, miNumResultsAtEnd@40.
namespace CgsMemory
{
    // Forward decl: the poster feeds this reader and mutates its packed status
    // word / result buffer directly (see CgsDataStreamResultPoster).
    struct DataStreamResultPoster;

    struct DataStreamResultReader
    {
        // The poster appends results straight into this reader's buffers and
        // CAS-updates its packed status; it needs access to the private members
        // below. (Additive: layout/sizeof unchanged.)
        friend struct DataStreamResultPoster;

        // Result of a single ReadResult() (CgsDataStreamResultReader.h:57).
        enum EReadResultStatus
        {
            E_READ_SUCCESS  = 0,
            E_READ_EMPTY    = 1,
            E_READ_FINISHED = 2,
            E_READ_COUNT    = 3,
        };

        // Packed-status bit layout (CgsDataStreamResultReader.h:44-54).
        // Verbatim from DWARF; the encode/decode helpers (EncodeStatus /
        // DecodeStatus, declared below) are the only users and are not yet
        // reconstructed. NOTE: the DWARF's KU_DATA_BUFFER_USED_MASK (0xFF000000,
        // 8 bits) does not span KU_DATA_BUFFER_USED_MAX (0xFFFFFF, 24 bits) from
        // KU_DATA_BUFFER_USED_BIT (24); transcribed as attested. (FLAG)
        static const u64 KU_NUM_RESULTS_BIT      = 0;           // h:44 (no value in DWARF; mask starts at bit 0)
        static const u64 KU_DATA_BUFFER_USED_BIT = 24;          // h:45
        static const u64 KU_NUM_USERS_BIT        = 48;          // h:46

        static const u64 KU_NUM_RESULTS_MAX      = 16777215u;   // h:48  0x00FFFFFF
        static const u64 KU_DATA_BUFFER_USED_MAX = 16777215u;   // h:49  0x00FFFFFF
        static const u64 KU_NUM_USERS_MAX        = 15u;         // h:50  0x0000000F

        static const u64 KU_NUM_RESULTS_MASK      = 16777215u;          // h:52  0x00FFFFFF
        static const u64 KU_DATA_BUFFER_USED_MASK = 4278190080u;        // h:53  0xFF000000 (see note)
        static const u64 KU_NUM_USERS_MASK        = (u64)15u << 48;     // h:54 (no value in DWARF; derived from BIT 48 + MAX 0xF)

        // --- lifecycle / API (CgsDataStreamResultReader.h:73-94) -------------
        // Reconstructed below.
        void Construct(void* lpResultBuffer, s32 liResultBufferSize, s32 liResultSize,
                       void* lpDataBuffer, s32 liDataBufferSize);
        void Begin();
        EReadResultStatus ReadResult(void* lpBuffer);

        // Declared-only (not reconstructed in this TU pass).
        void Destruct();
        void End();
        s32  GetResultSize() const;

    private:
        // Declared-only status (un)pack helpers (CgsDataStreamResultReader.h:119/127).
        u64  EncodeStatus(u8 lucNumUsers, u32 luNumResults, u32 luBufferUsed);
        void DecodeStatus(const u64& lruEncoded, u8* lpucNumUsers, u32* lpuNumResults, u32* lpuBufferUsed);

        // --- members (CgsDataStreamResultReader.h:99-112) --------------------
        Futex::AtomicUint64 mEncodedStatus;     // +0  packed (numUsers|bufferUsed|numResults)
        char*               mpcResultBuffer;    // +8  fixed-stride result records
        s32                 miResultBufferSize; // +12 result buffer size (bytes)
        s32                 miResultSize;       // +16 one result's stride (bytes)
        s32                 miMaxResults;       // +20 results that fit (bufSize / resultSize)
        char*               mpcDataBuffer;      // +24 variable-size data buffer
        s32                 miDataBufferSize;   // +28 data buffer size (bytes)
        bool                mbStreaming;        // +32 true between Begin() and End()
        s32                 miNextResult;       // +36 read cursor (next record index)
        s32                 miNumResultsAtEnd;  // +40 record count latched at stream end
    };
}

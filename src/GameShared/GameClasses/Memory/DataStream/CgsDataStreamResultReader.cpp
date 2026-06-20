#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamResultReader.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

#include <cstring>  // std::memcpy (models the Xbox XMemCpy block-copy intrinsic)

namespace CgsMemory
{
    // X360 0x8286A0D0.
    // Validates that both buffers are 128-byte aligned and 128-byte-multiple in
    // size (and that the result stride is a 128-byte multiple), stores the buffer
    // pointers/sizes, derives miMaxResults = resultBufferSize / resultSize, clears
    // the packed status word and the read cursor/count, and leaves !mbStreaming.
    void DataStreamResultReader::Construct(void* lpResultBuffer, s32 liResultBufferSize,
                                           s32 liResultSize, void* lpDataBuffer,
                                           s32 liDataBufferSize)
    {
        // Asm order: result-buffer ptr (guarded by resultSize!=0), data-buffer ptr,
        // result-buffer size, data-buffer size, result size. Each fires when the
        // value is NOT 128-aligned / NOT a 128 multiple; assert condition is the
        // negation (== 0).
        if (liResultSize != 0)
        {
            CGS_ASSERT((reinterpret_cast<uintptr_t>(lpResultBuffer) % 128) == 0,
                       "Result buffer MUST be on a 128 byte boundary\n");
        }
        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpDataBuffer) % 128) == 0,
                   "Data buffer MUST be on a 128 byte boundary\n");
        CGS_ASSERT((liResultBufferSize % 128) == 0,
                   "Result buffer MUST be a multiple of 128 bytes\n");
        CGS_ASSERT((liDataBufferSize % 128) == 0,
                   "Data buffer MUST be a multiple of 128 bytes\n");
        CGS_ASSERT((liResultSize % 128) == 0,
                   "Result size MUST be a multiple of 128 bytes\n");

        mbStreaming = false;                    // *(this+32) = 0

        // De-atomized: the X360 ldarx/stdcx + mfmsr loop stores 0 into the packed
        // status word. The committed EA::Thread::AtomicInt has both operator=(ValueType)
        // and SetValue; the explicit named SetValue makes the de-atomic store
        // unambiguous (the DWARF hint shows the original Construct using operator=,
        // which is equivalent). (FLAG: de-atomic, semantic parity.)
        mEncodedStatus.SetValue(0);

        miNextResult      = 0;                  // this[9]
        miNumResultsAtEnd = 0;                  // this[10]

        mpcResultBuffer    = static_cast<char*>(lpResultBuffer); // this[2]
        miResultBufferSize = liResultBufferSize;                 // this[3]
        miResultSize       = liResultSize;                       // this[4]
        mpcDataBuffer      = static_cast<char*>(lpDataBuffer);   // this[6]
        miDataBufferSize   = liDataBufferSize;                   // this[7]

        if (liResultSize <= 0)
        {
            // Sentinel max when the stride is non-positive (avoids the divide).
            // -1879048193 == 0x8FFFFFFF.
            miMaxResults = -1879048193;                          // this[5]
        }
        else
        {
            miMaxResults = liResultBufferSize / liResultSize;    // this[5]
        }
    }

    // X360 0x828681D8.
    // Opens a read pass: must not already be streaming; resets the read cursor and
    // count, marks streaming, and lock-free clears the packed status word.
    void DataStreamResultReader::Begin()
    {
        CGS_ASSERT(!mbStreaming, "Already streaming\n");

        miNextResult      = 0;   // this[9]
        miNumResultsAtEnd = 0;   // this[10]
        mbStreaming       = true; // *(this+32) = 1

        // De-atomized: the X360 body is a GetValue + SetValueConditional retry
        // loop (the mfmsr / ldarx-stdcx machinery) whose net effect is to reset
        // the status word to 0. Mirrors the two attested calls; with the current
        // value as the condition the CAS always succeeds, so this equals
        // mEncodedStatus = 0. (FLAG: de-atomic, semantic parity.)
        mEncodedStatus.SetValueConditional(0, mEncodedStatus.GetValue());
    }

    // X360 0x82868418.
    // Copies the next fixed-stride result record into lpBuffer. Must NOT be called
    // while streaming. Returns E_READ_FINISHED once the cursor reaches the latched
    // count, else E_READ_SUCCESS (advancing the cursor).
    DataStreamResultReader::EReadResultStatus DataStreamResultReader::ReadResult(void* lpBuffer)
    {
        CGS_ASSERT(!mbStreaming, "Can't read while streaming\n");

        const s32 liCursor = miNextResult;          // *(this+36)
        if (liCursor >= miNumResultsAtEnd)          // *(this+40)
        {
            return E_READ_FINISHED;                 // result = 2
        }

        const s32 liResultSize = miResultSize;      // *(this+16)
        if (liResultSize > 0)
        {
            // XMemCpy(lpBuffer, mpcResultBuffer + resultSize*cursor, resultSize),
            // modelled as std::memcpy per the committed convention.
            std::memcpy(lpBuffer,
                        mpcResultBuffer + (liResultSize * liCursor),
                        static_cast<usize>(liResultSize));
        }
        ++miNextResult;                             // ++*(this+36)
        return E_READ_SUCCESS;                      // result = 0
    }
}

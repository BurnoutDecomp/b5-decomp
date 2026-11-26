#include "GameShared/GameClasses/Memory/DataStream/CgsDataStreamResultReader.h"
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\memory\\DataStream/CgsDataStreamResultReader.cpp"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"  // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"  // CgsDev::StrStream (End's users-connected assert)

#include <xtl.h>  // native lwsync

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
            CGS_ASSERT_AT((reinterpret_cast<uintptr_t>(lpResultBuffer) % 128) == 0,
                       "Result buffer MUST be on a 128 byte boundary\n", CGS_ARTIST_FILE, 46);
        }
        CGS_ASSERT_AT((reinterpret_cast<uintptr_t>(lpDataBuffer) % 128) == 0,
                   "Data buffer MUST be on a 128 byte boundary\n", CGS_ARTIST_FILE, 47);
        CGS_ASSERT_AT((liResultBufferSize % 128) == 0,
                   "Result buffer MUST be a multiple of 128 bytes\n", CGS_ARTIST_FILE, 48);
        CGS_ASSERT_AT((liDataBufferSize % 128) == 0,
                   "Data buffer MUST be a multiple of 128 bytes\n", CGS_ARTIST_FILE, 49);
        CGS_ASSERT_AT((liResultSize % 128) == 0,
                   "Result size MUST be a multiple of 128 bytes\n", CGS_ARTIST_FILE, 50);

        mbStreaming = false;                    // *(this+32) = 0

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
        __lwsync();
        CGS_ASSERT_STREAM_AT(!mbStreaming, "Already streaming\n", CGS_ARTIST_FILE, 104);

        miNextResult      = 0;   // this[9]
        miNumResultsAtEnd = 0;   // this[10]
        mbStreaming       = true; // *(this+32) = 1

        // Retry when a producer changes the status between the load and CAS.
        while (!mEncodedStatus.SetValueConditional(0, mEncodedStatus.GetValue())) {}
    }

    // X360 0x82868418.
    // Copies the next fixed-stride result record into lpBuffer. Must NOT be called
    // while streaming. Returns E_READ_FINISHED once the cursor reaches the latched
    // count, else E_READ_SUCCESS (advancing the cursor).
    DataStreamResultReader::EReadResultStatus DataStreamResultReader::ReadResult(void* lpBuffer)
    {
        CGS_ASSERT_STREAM_AT(!mbStreaming, "Can't read while streaming\n", CGS_ARTIST_FILE, 166);

        const s32 liCursor = miNextResult;
        if (liCursor < miNumResultsAtEnd)
        {
            const s32 liResultSize = miResultSize;
            if (liResultSize > 0)
                XMemCpy(lpBuffer, mpcResultBuffer + liResultSize * liCursor, liResultSize);
            ++miNextResult;
            return E_READ_SUCCESS;
        }
        return E_READ_FINISHED;
    }

    // Closes the read pass: must be streaming, and every producer user must have
    // disconnected. Latches the packed result count (low 24 bits of the status word)
    // as the readable record count, clears the streaming flag, then fences so the
    // reader's ReadResult pass observes the latched state.
    void DataStreamResultReader::End()
    {
        CGS_ASSERT_AT(mbStreaming, "Not streaming\n", CGS_ARTIST_FILE, 137);

        const u64 luStatus     = mEncodedStatus.GetValue();
        const u32 luNumResults = static_cast<u32>(luStatus & KU_NUM_RESULTS_MASK);
        const u32 luNumUsers   = static_cast<u32>((luStatus >> KU_NUM_USERS_BIT) & KU_NUM_USERS_MAX);
        if (luNumUsers != 0)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Attempted to end stream while " << static_cast<s32>(luNumUsers) << " users connected\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage,
                CGS_ASSERT_LOCATION(CGS_ARTIST_FILE, 143));
            CgsDev::Assert::EndAssert();
        }

        miNumResultsAtEnd = static_cast<s32>(luNumResults);   // *(this+40)
        mbStreaming       = false;                            // *(this+32) = 0
        __lwsync();
    }
}

#undef CGS_ARTIST_FILE

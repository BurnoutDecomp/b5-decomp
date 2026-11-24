#include "GameShared/Jobs/DecompressionJob/CgsDecompressor.h"
#include "GameShared/Jobs/CgsJobAssertBuffer.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"   // CgsMemory::HeapMalloc::Malloc/Free
#include "GameShared/GameClasses/Core/CgsAssert.h"         // CGS_ASSERT (DecompressionJobEntry range check)

#include <cstring>
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include <xtl.h> // OutputDebugStringA (the X360 calls it directly on an inflate error)

// Native EAThread id is the32-bit XDK thread handle (ARTIST82B42560).
namespace EA { namespace Thread { void* GetThreadId(); } }

// ARTIST byte_83015F20 is shared by native job assertion paths.
char byte_83015F20[8192];

namespace CgsResource
{
    namespace
    {
        // ARTIST83248280, six contexts at0x180-byte strides.
        Decompressor gaDecompressors[6];

        // The version string passed by ARTIST at 0x82ACCA6C and 0x82ACCB28.
        const char KAC_ARTIST_ZLIB_VERSION[] = "1.1.3"; // 0x82142708
    }
    CGS_STATIC_ASSERT(sizeof(z_stream) == 0x38, "native zlib stream size");
    CGS_STATIC_ASSERT(sizeof(DecompressionJobStatus) == 0x80, "native decompression status size");
    CGS_STATIC_ASSERT(sizeof(Decompressor) == 0x180, "native decompressor context stride");
    // 0x82ACC9C0 -- zlib zalloc trampoline. zlib passes the z_stream::opaque (our `this`) and the
    // item count/size; allocate items*size from the job's heap at 4-byte alignment.
    void* Decompressor::CompressorAllocateCallback(void* lpOpaque, uInt luItems, uInt luSize)
    {
        Decompressor* lpSelf = static_cast<Decompressor*>(lpOpaque);
        return lpSelf->mpHeapMalloc->Malloc(static_cast<s32>(luItems * luSize), 4);
    }

    // 0x82ACC9D8 -- zlib zfree trampoline. Free the block through the job's heap.
    void Decompressor::CompressorFreeCallback(void* lpOpaque, void* lpAddress)
    {
        Decompressor* lpSelf = static_cast<Decompressor*>(lpOpaque);
        lpSelf->mpHeapMalloc->Free(lpAddress);
    }

    // 0x82ACC9E0 -- prime the working stream for the FIRST entry and inflateInit_ it.
    s32 Decompressor::BeginDecompressingFirstEntry()
    {
        std::memset(&mStatus.mDecompressionStream, 0, sizeof(z_stream));

        const CompressedData& lEntry = mpEntries[muCurrentEntry];

        mStatus.mDecompressionStream.opaque = this;
        mStatus.mDecompressionStream.zfree = &Decompressor::CompressorFreeCallback;
        mStatus.mDecompressionStream.zalloc = &Decompressor::CompressorAllocateCallback;
        mStatus.muAmountRead = 0;
        mStatus.muAmountWritten = 0;
        mStatus.miLastInflateResult = Z_STREAM_END;
        mStatus.mDecompressionStream.next_out = static_cast<Bytef*>(lEntry.mpDestinationBuffer);
        mStatus.mDecompressionStream.avail_out = lEntry.muDestinationSize;

        mStatus.miLastInflateResult = inflateInit_(&mStatus.mDecompressionStream, KAC_ARTIST_ZLIB_VERSION, sizeof(z_stream));
        return mStatus.miLastInflateResult;
    }

    // 0x82ACCA90 -- advance to the next entry, then prime the working stream and inflateInit_ it.
    s32 Decompressor::BeginDecompressingNextEntry()
    {
        ++muCurrentEntry;
        std::memset(&mStatus.mDecompressionStream, 0, sizeof(z_stream));

        const CompressedData& lEntry = mpEntries[muCurrentEntry];

        mStatus.mDecompressionStream.opaque = this;
        mStatus.miLastInflateResult = Z_STREAM_END;
        mStatus.mDecompressionStream.zfree = &Decompressor::CompressorFreeCallback;
        mStatus.mDecompressionStream.zalloc = &Decompressor::CompressorAllocateCallback;
        mStatus.muAmountRead = 0;
        mStatus.muAmountWritten = 0;
        mStatus.mDecompressionStream.next_out = static_cast<Bytef*>(lEntry.mpDestinationBuffer);
        mStatus.mDecompressionStream.avail_out = lEntry.muDestinationSize;

        mStatus.miLastInflateResult = inflateInit_(&mStatus.mDecompressionStream, KAC_ARTIST_ZLIB_VERSION, sizeof(z_stream));
        return mStatus.miLastInflateResult;
    }

    // 0x82ACCB48 -- run the whole entry list. Restore the working stream from the saved snapshot,
    // inflate each compressed entry into its destination (Z_SYNC_FLUSH), reporting decompression
    // errors, then write the working stream back to the snapshot.
    void Decompressor::Execute(DecompressionJobData* lpJobData)
    {
        mpJobData = lpJobData;

        muNumEntries = lpJobData->muNumEntries;

        std::memcpy(&mStatus, lpJobData->mpStatus, sizeof(mStatus));

        const s32 liSavedResult = mStatus.miLastInflateResult;
        mpHeapMalloc  = lpJobData->mpHeapMalloc;
        mpEntries     = lpJobData->mpEntries;
        muCurrentEntry = 0;

        if (liSavedResult == Z_STREAM_END)
        {
            BeginDecompressingFirstEntry();
        }
        for (u32 i = 1; i < muNumEntries; ++i)
        {
            const CompressedData& lEntry = mpEntries[muCurrentEntry];
            mStatus.mDecompressionStream.next_in = static_cast<Bytef*>(lEntry.mpSourceBuffer);
            mStatus.mDecompressionStream.avail_in = lEntry.muSourceSize;

            const int liResult = inflate(&mStatus.mDecompressionStream, Z_SYNC_FLUSH);
            mStatus.miLastInflateResult = liResult;
            // asm: two equality compares (result != Z_OK && result != Z_STREAM_END);
            // the Hex-Rays `>= 2` rendering only holds because it typed result UNSIGNED,
            // so negative zlib error codes (Z_DATA_ERROR etc.) must also report.
            if (liResult != Z_OK && liResult != Z_STREAM_END)
            {
                OutputDebugStringA("Error during decompression\n");
            }
            inflateEnd(&mStatus.mDecompressionStream);
            BeginDecompressingNextEntry();
        }

        // Final (or only) entry.
        const CompressedData& lFinal = mpEntries[muCurrentEntry];
        mStatus.mDecompressionStream.next_in = static_cast<Bytef*>(lFinal.mpSourceBuffer);
        mStatus.mDecompressionStream.avail_in = lFinal.muSourceSize;

        const int liResult = inflate(&mStatus.mDecompressionStream, Z_SYNC_FLUSH);
        mStatus.miLastInflateResult = liResult;
        // asm: result != Z_OK && result != Z_STREAM_END (signed-correct; see above).
        if (liResult != Z_OK && liResult != Z_STREAM_END)
        {
            OutputDebugStringA("Error during decompression\n");
        }
        if (mStatus.miLastInflateResult == Z_STREAM_END)
        {
            inflateEnd(&mStatus.mDecompressionStream);
        }

        std::memcpy(lpJobData->mpStatus, &mStatus, sizeof(mStatus));
    }

    // ARTIST82ACCCA0: r4 is the job descriptor; context selection uses native thread handles.
    void DecompressionJobEntry(EA::Jobs::Param, EA::Jobs::Param lParam1,
                               EA::Jobs::Param, EA::Jobs::Param)
    {
        const u64 luThreadId = static_cast<u64>(reinterpret_cast<u32>(EA::Thread::GetThreadId()));
        const u64 luJobThreadIndex = (luThreadId - 0xF8000088ull) >> 2;
        if (luJobThreadIndex >= 6)
        {
            CgsDev::StrStream lMessage(byte_83015F20, sizeof(byte_83015F20));
            const u64 luReportedThreadId = static_cast<u64>(reinterpret_cast<u32>(EA::Thread::GetThreadId()));
            lMessage << "SPU Id out of range: " << luReportedThreadId << "\n";
            const char* lpcSource =
                "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\jobs\\decompressionjob\\DecompressionJob.cpp";
            CGS_JOB_ASSERT_LOG(lpcSource, 56, lMessage.GetBuffer());
            __asm { mfmsr r12 }
            __asm { ori r12, r12, 0x400 }
            __asm { mtmsr r12 }
            CGS_JOB_ASSERT_LOG(lpcSource, 56, lMessage.GetBuffer());
            __asm { mfmsr r12 }
            __asm { ori r12, r12, 0x400 }
            __asm { mtmsr r12 }
        }
        gaDecompressors[static_cast<u32>(luJobThreadIndex)].Execute(
            static_cast<DecompressionJobData*>(lParam1.mpValue));
    }
}

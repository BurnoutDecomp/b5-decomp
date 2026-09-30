#include "GameShared/Jobs/DecompressionJob/CgsDecompressor.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"   // CgsMemory::HeapMalloc::Malloc/Free
#include "GameShared/GameClasses/Core/CgsAssert.h"         // CGS_ASSERT (DecompressionJobEntry range check)

#include <cstring>   // std::memset / std::memcpy (models the Xbox memset/XMemCpy intrinsics)
#include <windows.h> // OutputDebugStringA (the X360 calls it directly on an inflate error)

namespace CgsResource
{
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
        // FLAG PC-platform leaf: use the native zlib structure and library ABI.
        std::memset(&mStream, 0, sizeof(mStream));

        const CompressedData& lEntry = mpEntries[muCurrentEntry];

        mStream.opaque = this;
        mStream.zfree  = &Decompressor::CompressorFreeCallback;
        mStream.zalloc = &Decompressor::CompressorAllocateCallback;
        muAmountRead    = 0;
        muAmountWritten = 0;
        miLastInflateResult = 1;
        mStream.next_out  = static_cast<Bytef*>(lEntry.mpDestinationBuffer);
        mStream.avail_out = lEntry.muDestinationSize;

        miLastInflateResult = inflateInit_(&mStream, ZLIB_VERSION, sizeof(mStream));
        return miLastInflateResult;
    }

    // 0x82ACCA90 -- advance to the next entry, then prime the working stream and inflateInit_ it.
    s32 Decompressor::BeginDecompressingNextEntry()
    {
        ++muCurrentEntry;
        std::memset(&mStream, 0, sizeof(mStream));

        const CompressedData& lEntry = mpEntries[muCurrentEntry];

        mStream.opaque = this;
        miLastInflateResult = 1;
        mStream.zfree  = &Decompressor::CompressorFreeCallback;
        mStream.zalloc = &Decompressor::CompressorAllocateCallback;
        muAmountRead    = 0;
        muAmountWritten = 0;
        mStream.next_out  = static_cast<Bytef*>(lEntry.mpDestinationBuffer);
        mStream.avail_out = lEntry.muDestinationSize;

        miLastInflateResult = inflateInit_(&mStream, ZLIB_VERSION, sizeof(mStream));
        return miLastInflateResult;
    }

    // 0x82ACCB48 -- run the whole entry list. Restore the working stream from the saved snapshot,
    // inflate each compressed entry into its destination (Z_SYNC_FLUSH), reporting decompression
    // errors, then write the working stream back to the snapshot.
    void* Decompressor::Execute(DecompressionJobData* lpJobData)
    {
        mpJobData = lpJobData;

        muNumEntries = lpJobData->muNumEntries;

        // FLAG PC-platform leaf: the original snapshot spans 128 console
        // bytes. Copy its named state so native pointer widths cannot truncate
        // it or overwrite the following worker fields.
        mStream = lpJobData->mpStatus->mDecompressionStream;
        muAmountRead = lpJobData->mpStatus->muAmountRead;
        muAmountWritten = lpJobData->mpStatus->muAmountWritten;
        miLastInflateResult = lpJobData->mpStatus->miLastInflateResult;

        const s32 liSavedResult = miLastInflateResult;
        mpHeapMalloc  = lpJobData->mpHeapMalloc;
        mpEntries     = lpJobData->mpEntries;
        muCurrentEntry = 0;

        if (liSavedResult == 1)
        {
            BeginDecompressingFirstEntry();
        }

        for (u32 i = 1; i < muNumEntries; ++i)
        {
            const CompressedData& lEntry = mpEntries[muCurrentEntry];
            mStream.next_in  = static_cast<Bytef*>(lEntry.mpSourceBuffer);
            mStream.avail_in = lEntry.muSourceSize;

            const int liResult = inflate(&mStream, Z_SYNC_FLUSH);
            miLastInflateResult = liResult;
            // asm: two equality compares (result != Z_OK && result != Z_STREAM_END);
            // the Hex-Rays `>= 2` rendering only holds because it typed result UNSIGNED,
            // so negative zlib error codes (Z_DATA_ERROR etc.) must also report.
            if (liResult != Z_OK && liResult != Z_STREAM_END)
            {
                OutputDebugStringA("Error during decompression\n");
            }
            inflateEnd(&mStream);
            BeginDecompressingNextEntry();
        }

        // Final (or only) entry.
        const CompressedData& lFinal = mpEntries[muCurrentEntry];
        mStream.next_in  = static_cast<Bytef*>(lFinal.mpSourceBuffer);
        mStream.avail_in = lFinal.muSourceSize;

        const int liResult = inflate(&mStream, Z_SYNC_FLUSH);
        miLastInflateResult = liResult;
        // asm: result != Z_OK && result != Z_STREAM_END (signed-correct; see above).
        if (liResult != Z_OK && liResult != Z_STREAM_END)
        {
            OutputDebugStringA("Error during decompression\n");
        }
        if (miLastInflateResult == 1)
        {
            inflateEnd(&mStream);
        }

        lpJobData->mpStatus->mDecompressionStream = mStream;
        lpJobData->mpStatus->muAmountRead = muAmountRead;
        lpJobData->mpStatus->muAmountWritten = muAmountWritten;
        lpJobData->mpStatus->miLastInflateResult = miLastInflateResult;
        return lpJobData->mpStatus;
    }

    // ARTIST82ACCCA0: four-parameter EA job ABI; r4 is the data parameter.
    // Only the platform-specific workspace selection changes on the host.
    void DecompressionJobEntry(EA::Jobs::Param, EA::Jobs::Param lParam1,
                               EA::Jobs::Param, EA::Jobs::Param)
    {
        // ARTIST82ACCCAC saves r4: data is the SECOND EA::Jobs parameter.
        auto* lpData = static_cast<DecompressionJobData*>(lParam1.mpValue);
        // FLAG PC-platform leaf: native thread ids do not encode X360 slots.
        // The owning interface supplies a stable workspace; modern zlib's
        // inflateStateCheck also requires its z_stream address to stay fixed.
        CGS_ASSERT(lpData && lpData->mpNativeWorker, "No native decompression worker\n");
        if (lpData && lpData->mpNativeWorker)
            lpData->mpNativeWorker->Execute(lpData);
    }
}

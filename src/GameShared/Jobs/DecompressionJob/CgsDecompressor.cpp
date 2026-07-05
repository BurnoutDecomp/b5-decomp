#include "GameShared/Jobs/DecompressionJob/CgsDecompressor.h"
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"   // CgsMemory::HeapMalloc::Malloc/Free
#include "GameShared/GameClasses/Core/CgsAssert.h"         // CGS_ASSERT (DecompressionJobEntry range check)

#include <cstring>   // std::memset / std::memcpy (models the Xbox memset/XMemCpy intrinsics)
#include <windows.h> // OutputDebugStringA (the X360 calls it directly on an inflate error)

// Honest externs for the DecompressionJobEntry @0x82ACCCA0 thread-id -> slot mapping.
// EA::Thread::GetThreadId is the EATech thread API (declared-not-defined here). gaDecompressorPool
// is the X360 global decompressor pool (dword_83248280); its per-slot element type/stride is NOT
// fully attested (the 384-byte stride disagrees with sizeof(Decompressor)=0x110), so it is modelled
// as an extern byte-addressed pool so the call arithmetic stays faithful. Both are declared-only.
namespace EA { namespace Thread { u32 GetThreadId(); } }
extern unsigned char gaDecompressorPool[];

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
        std::memset(&mStream, 0, 56);

        const CompressedData& lEntry = mpEntries[muCurrentEntry];

        mStream.opaque = this;
        mStream.zfree  = &Decompressor::CompressorFreeCallback;
        mStream.zalloc = &Decompressor::CompressorAllocateCallback;
        muAmountRead    = 0;
        muAmountWritten = 0;
        miLastInflateResult = 1;
        mStream.next_out  = static_cast<Bytef*>(lEntry.mpDestinationBuffer);
        mStream.avail_out = lEntry.muDestinationSize;

        miLastInflateResult = inflateInit_(&mStream, "1.1.3", 56);
        return miLastInflateResult;
    }

    // 0x82ACCA90 -- advance to the next entry, then prime the working stream and inflateInit_ it.
    s32 Decompressor::BeginDecompressingNextEntry()
    {
        ++muCurrentEntry;
        std::memset(&mStream, 0, 56);

        const CompressedData& lEntry = mpEntries[muCurrentEntry];

        mStream.opaque = this;
        miLastInflateResult = 1;
        mStream.zfree  = &Decompressor::CompressorFreeCallback;
        mStream.zalloc = &Decompressor::CompressorAllocateCallback;
        muAmountRead    = 0;
        muAmountWritten = 0;
        mStream.next_out  = static_cast<Bytef*>(lEntry.mpDestinationBuffer);
        mStream.avail_out = lEntry.muDestinationSize;

        miLastInflateResult = inflateInit_(&mStream, "1.1.3", 56);
        return miLastInflateResult;
    }

    // 0x82ACCB48 -- run the whole entry list. Restore the working stream from the saved snapshot,
    // inflate each compressed entry into its destination (Z_SYNC_FLUSH), reporting decompression
    // errors, then write the working stream back to the snapshot.
    void* Decompressor::Execute(DecompressionJobData* lpJobData)
    {
        mpJobData = lpJobData;

        muNumEntries = lpJobData->muNumEntries;

        // Restore the 128-byte saved working-stream region from the job status snapshot.
        std::memcpy(&mStream, lpJobData->mpStatus, 128);

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

        return std::memcpy(lpJobData->mpStatus, &mStream, 128);
    }

    // X360 0x82ACCCA0 -- EA::Jobs local-job entry point for the decompression job. The scheduler
    // invokes it on a worker thread with the job's data pointer (r4). It maps the calling worker
    // thread to a decompressor/SPU slot via thread-id arithmetic, range-checks the slot, then runs
    // that slot's Decompressor over the job data.
    //
    // FLAG (low confidence): the per-slot pool is the global array dword_83248280 indexed at a
    // 384-byte stride. That stride is X360-attested but disagrees with sizeof(Decompressor) (0x110);
    // the global element type/stride is NOT fully attested, so it is modelled as an extern
    // byte-addressed pool so the call arithmetic stays faithful. Reproduces the assert + Execute
    // tail call verbatim.
    void DecompressionJobEntry(void* lpvJobData)
    {
        DecompressionJobData* lpJobData = static_cast<DecompressionJobData*>(lpvJobData);

        // slot = (threadId + (0xE0FFFFEF << 3)) >> 2  -- opaque X360 thread-id -> slot mapping.
        const u32 luThreadId = static_cast<u32>(EA::Thread::GetThreadId());
        const u64 luSlot     = (static_cast<u64>(luThreadId) + (0xE0FFFFEFull << 3)) >> 2;

        CGS_ASSERT(luSlot < 6, "SPU Id out of range: ");   // DecompressionJob.cpp:56 (id + '\n' streamed separately)

        // gaDecompressorPool: extern per-slot pool, 384-byte X360 stride (element type unattested).
        Decompressor* lpDecompressor =
            reinterpret_cast<Decompressor*>(&gaDecompressorPool[static_cast<u32>(luSlot) * 384]);
        lpDecompressor->Execute(lpJobData);
    }
}

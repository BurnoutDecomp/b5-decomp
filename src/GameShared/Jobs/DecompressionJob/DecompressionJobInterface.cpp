#include "GameShared/Jobs/DecompressionJob/DecompressionJobInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CgsDev::Assert Begin/Fire/End triad
#include "SDKs/EATech/eajobs/job_types.h"                     // EA::Jobs::JOB_ENVIRONMENT_LOCAL

#include <cstring>   // std::memset (models the X360 memset intrinsic)

// GameShared/Jobs/DecompressionJob/DecompressionJobInterface.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the spine) + the DecFIGS DWARF. The streaming
// front-end batches CompressedData entries and flushes them to a worker thread as one EA::Jobs
// decompression job. Each method asserts its pre-conditions exactly as the X360 build does --
// every assert is a plain literal message fired through the CgsDev::Assert Begin/Fire/End triad
// with the X360 file/line (the X360 streamed each literal through a CgsDev::StrStream into the
// assert buffer; the message carries no runtime value, so the literal is forwarded directly,
// matching the committed CgsMemoryMap precedent). The asserts only report -- control falls through
// regardless, exactly as the asm does (no early return on a failed assert).

namespace
{
    // The X360 embeds this exact path string in every assert in this TU.
    const char* const KPC_ASSERT_FILE =
        "..\\..\\..\\GameShared\\Jobs\\DecompressionJob/DecompressionJobInterface.cpp";
}

namespace CgsResource
{

// DecompressionJobInterface.cpp:40 / X360 0x828DB0C0
void DecompressionJobInterface::Construct(EA::Jobs::JobScheduler* lpScheduler,
                                          CompressedData*         lpEntries,
                                          u32                     luMaxEntries)
{
    if (!lpScheduler)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("No scheduler given\n", KPC_ASSERT_FILE, 42);
        CgsDev::Assert::EndAssert();
    }
    if (!lpEntries)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("No entry list given\n", KPC_ASSERT_FILE, 43);
        CgsDev::Assert::EndAssert();
    }

    mpScheduler   = lpScheduler;
    meStage       = E_DJS_IDLE;       // X360 stores 0 at +0x4DC
    muMaxEntries  = luMaxEntries;     // a4 -> +0x4D4
    mpEntries     = lpEntries;        // a3 -> +0x4D0

    // Build the heap over the interface's own 128-KiB buffer (X360 HeapMalloc::Construct
    // gets &mHeap, macMallocBuffer, 0x20000).
    mHeap.Construct(macMallocBuffer, KU_COMPRESSION_BUFFER_SIZE);
}

// DecompressionJobInterface.cpp:81 / X360 0x828DB1E8
void DecompressionJobInterface::BeginStream()
{
    if (meStage == E_DJS_FLUSHING)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Invalid stage to begin stream\n", KPC_ASSERT_FILE, 84);
        CgsDev::Assert::EndAssert();
    }

    muNumEntries = 0;
    meStage      = E_DJS_ADDING_ENTRIES;

    // Clear the descriptor + the saved-stream snapshot. The X360 zeros 128 bytes for each (the
    // job-data block is a fixed 128-byte slot on the spine); here the descriptor is its real
    // 16-byte size and the snapshot its 128 bytes -- the worker reads the same fields either way.
    std::memset(&mJobData, 0, sizeof(mJobData));
    std::memset(&mJobStatus, 0, sizeof(mJobStatus));

    // The saved snapshot starts a fresh inflate (miLastInflateResult == 1 == Z_STREAM_END drives
    // the first BeginDecompressingFirstEntry in Decompressor::Execute); the byte counters reset.
    mJobStatus.muAmountRead       = 0;
    mJobStatus.muAmountWritten    = 0;
    mJobStatus.miLastInflateResult = 1;
}

// DecompressionJobInterface.cpp:113 / X360 0x828DB2C0
bool DecompressionJobInterface::RunFlushJobs()
{
    if (meStage != E_DJS_ADDING_ENTRIES)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Invalid stage to begin flush\n", KPC_ASSERT_FILE, 115);
        CgsDev::Assert::EndAssert();
    }

    // Nothing to flush if no entries were created.
    if (muNumEntries == 0)
    {
        return false;
    }

    // Fill the job descriptor the worker consumes.
    mJobData.mpStatus     = &mJobStatus;
    mJobData.mpEntries    = mpEntries;
    mJobData.mpHeapMalloc = &mHeap;

    // The X360 drops the last entry from the flush count when its source buffer is still empty
    // (the in-progress entry was created but never given a source): the entry at index
    // (muNumEntries - 1) is excluded so only complete entries are decompressed.
    const CompressedData& lLastEntry = mpEntries[muNumEntries - 1];
    if (lLastEntry.mpSourceBuffer != 0)
    {
        mJobData.muNumEntries = muNumEntries;
    }
    else
    {
        mJobData.muNumEntries = muNumEntries - 1;
    }

    // If that leaves no complete entries, there is nothing to flush.
    if (mJobData.muNumEntries == 0)
    {
        return false;
    }

    meStage = E_DJS_FLUSHING;

    mJob.Clear();
    mJob.SetCode(EA::Jobs::JOB_ENVIRONMENT_LOCAL,
                 reinterpret_cast<const void*>(&DecompressionJobEntry), 0);
    mJob.SetData(&mJobData, sizeof(mJobData));
    mJob.SetName("Decompression");
    mpScheduler->AddJobs(&mJob, 1);
    return true;
}

// DecompressionJobInterface.cpp:249 / X360 0x828DB658
void DecompressionJobInterface::EndStream()
{
    if (meStage != E_DJS_ADDING_ENTRIES)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Invalid stage to end stream\n", KPC_ASSERT_FILE, 251);
        CgsDev::Assert::EndAssert();
    }
    if (mbEntryInProgress)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Can not end stream while creating an entry\n", KPC_ASSERT_FILE, 252);
        CgsDev::Assert::EndAssert();
    }

    meStage = E_DJS_IDLE;
}

// DecompressionJobInterface.cpp:268 / X360 0x828DB760
void DecompressionJobInterface::CreateEntry(void* lpDestBuffer, u32 luDestBufferSize)
{
    if (meStage != E_DJS_ADDING_ENTRIES)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Invalid stage to create an entry\n", KPC_ASSERT_FILE, 270);
        CgsDev::Assert::EndAssert();
    }
    if (mbEntryInProgress)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Already creating an entry\n", KPC_ASSERT_FILE, 271);
        CgsDev::Assert::EndAssert();
    }
    if (muNumEntries >= muMaxEntries)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Out of space for new entries", KPC_ASSERT_FILE, 272);
        CgsDev::Assert::EndAssert();
    }
    if (!lpDestBuffer)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Can not create entry without dest buffer\n", KPC_ASSERT_FILE, 273);
        CgsDev::Assert::EndAssert();
    }
    if (luDestBufferSize == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Entry must have more than 0 bytes for destination!\n", KPC_ASSERT_FILE, 274);
        CgsDev::Assert::EndAssert();
    }
    if ((luDestBufferSize & 0xF) != 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "Entry must be at least 16 byte aligned to decompress on a job or DMAs will fail\n",
            KPC_ASSERT_FILE, 275);
        CgsDev::Assert::EndAssert();
    }

    // Open a fresh entry at the next slot: record the destination span, clear its (not yet
    // supplied) source span, and flag an entry in progress. The X360 sets mbEntryInProgress
    // first, then writes the slot and bumps muNumEntries.
    mbEntryInProgress = true;

    CompressedData& lEntry = mpEntries[muNumEntries];
    lEntry.mpDestinationBuffer = lpDestBuffer;
    lEntry.muDestinationSize   = luDestBufferSize;
    lEntry.mpSourceBuffer      = 0;
    lEntry.muSourceSize        = 0;

    ++muNumEntries;
}

// DecompressionJobInterface.cpp:302 / X360 0x828DBA58
void DecompressionJobInterface::AppendToEntry(void* lpSourceBuffer, u32 luSourceBufferSize)
{
    if (meStage != E_DJS_ADDING_ENTRIES)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Invalid stage to create an entry\n", KPC_ASSERT_FILE, 304);
        CgsDev::Assert::EndAssert();
    }
    if (!mbEntryInProgress)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Not creating an entry\n", KPC_ASSERT_FILE, 305);
        CgsDev::Assert::EndAssert();
    }
    if (muNumEntries == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Should have at least 1 entry in list to append to", KPC_ASSERT_FILE, 306);
        CgsDev::Assert::EndAssert();
    }
    if (!lpSourceBuffer)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Can not create entry without source buffer\n", KPC_ASSERT_FILE, 307);
        CgsDev::Assert::EndAssert();
    }
    if (luSourceBufferSize == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Entry must have more than 0 bytes for source!\n", KPC_ASSERT_FILE, 308);
        CgsDev::Assert::EndAssert();
    }

    CompressedData& lEntry = mpEntries[muNumEntries - 1];
    if (lEntry.mpSourceBuffer != 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Entry already has source buffer\n", KPC_ASSERT_FILE, 316);
        CgsDev::Assert::EndAssert();
    }
    if (lEntry.muSourceSize != 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Entry already has source size\n", KPC_ASSERT_FILE, 317);
        CgsDev::Assert::EndAssert();
    }

    // Attach the source (compressed) span to the entry in progress.
    lEntry.mpSourceBuffer = lpSourceBuffer;
    lEntry.muSourceSize   = luSourceBufferSize;
}

// DecompressionJobInterface.cpp:334 / X360 0x828DBD80
void DecompressionJobInterface::FinishEntry()
{
    if (meStage != E_DJS_ADDING_ENTRIES)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Invalid stage to create an entry\n", KPC_ASSERT_FILE, 336);
        CgsDev::Assert::EndAssert();
    }
    if (!mbEntryInProgress)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Not creating an entry\n", KPC_ASSERT_FILE, 337);
        CgsDev::Assert::EndAssert();
    }
    if (muNumEntries == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Should have at least 1 entry in list to append to", KPC_ASSERT_FILE, 338);
        CgsDev::Assert::EndAssert();
    }

    mbEntryInProgress = false;
}

}

#pragma once

#include "types.hpp"
#include "GameShared/Jobs/DecompressionJob/CgsDecompressor.h" // CompressedData, DecompressionJobData/Status, DecompressionJobEntry
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"      // CgsMemory::HeapMalloc (mHeap, embedded by value)
#include "SDKs/EATech/eajobs/job.h"                            // EA::Jobs::Job (mJob, embedded by value)
#include "SDKs/EATech/eajobs/job_scheduler.h"                  // EA::Jobs::JobScheduler (mpScheduler)

// GameShared/Jobs/DecompressionJob/DecompressionJobInterface.h
//
// CgsResource::DecompressionJobInterface -- the streaming front-end that batches a list of
// compressed resource entries and flushes them to a worker thread as one EA::Jobs decompression
// job. The bundle loader streams entries through it: BeginStream resets the batch, CreateEntry /
// AppendToEntry / FinishEntry build up one CompressedData record at a time, RunFlushJobs hands the
// accumulated batch to the job scheduler, and EndStream closes the stream.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the spine) + the DecFIGS DWARF
// (DecompressionJobInterface.h:56). The X360 object layout (member ORDER + the embedded job/heap)
// is read off the asm; the x64 sizes fall out from the named members. Confirmed offsets:
//   mpScheduler        +0x000   (*a1     = scheduler)
//   mJobData           +0x080   (a1+32 -> &mJobStatus, mpEntries, count, &mHeap; 128B reserved)
//   mJobStatus         +0x100   (a1+64,  the 128-byte saved-stream snapshot)
//   mJob               +0x180   (a1+96,  EA::Jobs::Job, 848 bytes)
//   mpEntries          +0x4D0   (a1[308])
//   muMaxEntries       +0x4D4   (a1[309])
//   muNumEntries       +0x4D8   (a1[310])
//   meStage            +0x4DC   (a1[311])
//   mbEntryInProgress  +0x4E0   (a1+1248, lbz/stb -> bool)
//   mHeap              +0x4E4   (a1+313, CgsMemory::HeapMalloc, sizeof 0x520 on X360)
//   macMallocBuffer    +0xA04   (a1+641, the 0x20000 (128 KiB) buffer the heap manages)
//
// DWARF/X360 DELTA: the PS3 DWARF spells the heap as macMallocBuffer[65536] + maMallocNodes[256]
// (an inline IndexedNodeHeap); the X360 ARTIST build uses a CgsMemory::HeapMalloc over a 128-KiB
// buffer (HeapMalloc::Construct(macMallocBuffer, KU_COMPRESSION_BUFFER_SIZE) @ Construct). The
// X360 spine is authoritative. The PS3-only Destruct()/WaitForFlushJobs(bool) are NOT in the X360
// function set (inlined into callers there), so they are omitted -- only the seven X360-attested
// methods are declared.

namespace CgsResource
{
    // DecompressionJobInterface.h:39 -- which phase of the streaming batch we are in.
    enum EDecompressionJobStage
    {
        E_DJS_IDLE           = 0,   // no stream open
        E_DJS_ADDING_ENTRIES = 1,   // a stream is open and entries are being added
        E_DJS_FLUSHING       = 2,   // the batch has been handed to the job scheduler
        E_DJS_COUNT          = 3
    };

    class DecompressionJobInterface
    {
    public:
        // DecompressionJobInterface.h:59 -- the size of the heap buffer the interface owns
        // (HeapMalloc::Construct passes 0x20000 in Construct's asm).
        static const u32 KU_COMPRESSION_BUFFER_SIZE = 131072;

        // DecompressionJobInterface.cpp:40 / X360 0x828DB0C0 -- adopt the scheduler + entry list,
        // start IDLE, and build the heap over the owned buffer.
        void Construct(EA::Jobs::JobScheduler* lpScheduler, CompressedData* lpEntries, u32 luMaxEntries);

        // DecompressionJobInterface.cpp:81 / X360 0x828DB1E8 -- open a streaming batch: reset the
        // entry count, clear the saved job status, enter E_DJS_ADDING_ENTRIES.
        void BeginStream();

        // DecompressionJobInterface.cpp:113 / X360 0x828DB2C0 -- flush the accumulated batch to the
        // job scheduler as one decompression job. Returns true if a job was submitted.
        bool RunFlushJobs();

        // DecompressionJobInterface.cpp:249 / X360 0x828DB658 -- close the streaming batch.
        void EndStream();

        // DecompressionJobInterface.cpp:268 / X360 0x828DB760 -- start a new entry with its
        // destination (uncompressed) buffer + size.
        void CreateEntry(void* lpDestBuffer, u32 luDestBufferSize);

        // DecompressionJobInterface.cpp:302 / X360 0x828DBA58 -- attach the source (compressed)
        // buffer + size to the entry in progress.
        void AppendToEntry(void* lpSourceBuffer, u32 luSourceBufferSize);

        // DecompressionJobInterface.cpp:334 / X360 0x828DBD80 -- mark the entry in progress complete.
        void FinishEntry();

    private:
        EA::Jobs::JobScheduler* mpScheduler;        // +0x000 the scheduler RunFlushJobs submits to
        DecompressionJobData    mJobData;           // +0x080 the descriptor handed to the job (128B slot)
        DecompressionJobStatus  mJobStatus;         // +0x100 the saved 128-byte stream snapshot
        EA::Jobs::Job           mJob;               // +0x180 the decompression job (848B)
        CompressedData*         mpEntries;          // +0x4D0 the caller's entry array
        u32                     muMaxEntries;       // +0x4D4 capacity of mpEntries
        u32                     muNumEntries;       // +0x4D8 entries created so far this stream
        EDecompressionJobStage  meStage;            // +0x4DC current streaming phase
        bool                    mbEntryInProgress;  // +0x4E0 a CreateEntry has not yet been FinishEntry'd
        CgsMemory::HeapMalloc   mHeap;              // +0x4E4 the heap the job's zlib callbacks use
        u8                      macMallocBuffer[KU_COMPRESSION_BUFFER_SIZE]; // +0xA04 the heap's backing buffer
    };
}

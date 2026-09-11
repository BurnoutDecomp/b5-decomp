#pragma once

#include "types.hpp"
#include "SDKs/EATech/eajobs/job.h"   // EA::Jobs::Job (mJob, embedded by value)

// CgsMemory::Relocator - the block-copy engine the resource pool's emergency defragmenter
// and the replay GPU disk-write stream both drive. A caller fills a RelocationParams with a
// list of (source, destination, size) ops and calls Execute; the Relocator latches that list
// into its job-data block, submits one job, and the job body performs the copies. Update
// polls (or blocks on) that job and reports whether the engine is idle again.
//
// Layout (struct offsets are the console's; field widths follow the x64 PC target, so the
// byte offsets are not reproduced verbatim - every member is reached by name):
//   +0x000 the two running latches
//   +0x010 mJob         (EA::Jobs::Job, embedded by value)
//   +0x380 mJobData     (the 128-byte block Job::SetData attaches)
//   sizeof == 1024 on the console; RelocationParams is a separate 16-byte object the caller
//   owns (the pool module embeds one immediately after its Relocator).

namespace CgsMemory
{
    // One copy op. The producer writes absolute addresses, not offsets.
    struct RelocateOp
    {
        void* mpSource;       // +0x0
        void* mpDest;         // +0x4
        u32   muSize;         // +0x8
        u32   muMemorySpace;  // +0xC  0 = main memory, 3 = graphics memory
    };

    // The per-pass parameter block. Execute copies the first four words of it into the job
    // data block, so the job reads a snapshot and the caller may refill the params freely.
    struct RelocationParams
    {
        RelocateOp* mpOps;               // +0x0  the op array (muNumOps entries)
        s32         miNumOps;            // +0x4  filled in by the producer just before Execute
        void*       mpBounceBuffer;      // +0x8  staging buffer for overlapping copies
        s32         miBounceBufferSize;  // +0xC  chunk size of the bounce copy
    };

    // The job's data block. The first four words are the params snapshot; mbTestHijacks
    // selects the job's alternate (data-stream hijack) body and Execute always clears it.
    struct RelocatorJobData
    {
        RelocateOp* mpOps;               // +0x00
        s32         miNumOps;            // +0x04
        void*       mpBounceBuffer;      // +0x08
        s32         miBounceBufferSize;  // +0x0C
        u32         muTestHijacks;       // +0x10
        u8          mPad_0014[108];      // +0x14  to the 128-byte block Job::SetData attaches
    };

    class Relocator
    {
    public:
        // EA::Jobs::Job has no default constructor, so the embedded job is named where it is
        // declared; the name is the one Execute wires onto it anyway. Construct() is still
        // what resets the engine, and every caller runs it.
        Relocator() : mJob("Relocator") {}

        // Reset both running latches. Returns this (the console's copy-construct-style return).
        Relocator* Construct();

        // Latch lpParams into the job data block and submit the copy job. Asserts if the
        // engine is still marked running.
        void Execute(RelocationParams* lpParams);

        // Poll the copy job. lbClearExternalLatch drops the secondary latch first;
        // lbBlock waits for the job rather than leaving it running. Returns true once
        // both latches are clear, i.e. the engine is idle.
        bool Update(bool lbClearExternalLatch, bool lbBlock);

        // ---- Layout (see the note above) ----------------------------------------------
        bool          mbRunning;            // +0x000 a job is submitted and not yet reaped
        bool          mbRunningExternally;  // +0x001 a second busy latch: Construct and Update
                                            //        clear it and Execute's "already running"
                                            //        check tests it, but nothing in this build
                                            //        ever sets it.
        EA::Jobs::Job mJob;                 // +0x010 the one copy job
        RelocatorJobData mJobData;          // +0x380 the block attached to that job
    };
}
